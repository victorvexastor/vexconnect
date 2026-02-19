/* transport_unix.c — Unix socket transport for development/testing
 * Simulates mesh connections without BLE hardware.
 * Two+ instances connect via Unix domain sockets. */

#include "vex.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

/* Initialize as a listening node on a Unix socket */
int vex_transport_unix_init(vex_node_t *node, const char *sock_path) {
    struct sockaddr_un addr;

    node->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (node->listen_fd < 0) {
        vex_log("TRANSPORT", "socket() failed: %s", strerror(errno));
        return -1;
    }

    /* Remove existing socket file */
    unlink(sock_path);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (bind(node->listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        vex_log("TRANSPORT", "bind() failed: %s", strerror(errno));
        close(node->listen_fd);
        return -1;
    }

    if (listen(node->listen_fd, 5) < 0) {
        vex_log("TRANSPORT", "listen() failed: %s", strerror(errno));
        close(node->listen_fd);
        return -1;
    }

    /* Non-blocking for accept */
    fcntl(node->listen_fd, F_SETFL, O_NONBLOCK);

    vex_log("TRANSPORT", "Listening on %s", sock_path);
    return 0;
}

/* Accept a new peer connection (non-blocking) */
int vex_transport_unix_accept(vex_node_t *node) {
    int fd = accept(node->listen_fd, NULL, NULL);
    if (fd < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }

    /* Find empty peer slot */
    for (int i = 0; i < VEX_MAX_PEERS; i++) {
        if (!node->peers[i].active) {
            node->peers[i].fd = fd;
            node->peers[i].active = 1;
            node->peers[i].last_seen = time(NULL);
            snprintf(node->peers[i].name, sizeof(node->peers[i].name), "peer-%d", fd);
            node->peer_count++;

            /* Non-blocking for reads */
            fcntl(fd, F_SETFL, O_NONBLOCK);

            vex_log("TRANSPORT", "Accepted peer %s (fd=%d)", node->peers[i].name, fd);
            return 1;
        }
    }

    vex_log("TRANSPORT", "Max peers reached, rejecting connection");
    close(fd);
    return 0;
}

/* Connect to another node's Unix socket */
int vex_transport_unix_connect(vex_node_t *node, const char *sock_path) {
    struct sockaddr_un addr;

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sock_path, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        vex_log("TRANSPORT", "Connect to %s failed: %s", sock_path, strerror(errno));
        close(fd);
        return -1;
    }

    /* Find empty peer slot */
    for (int i = 0; i < VEX_MAX_PEERS; i++) {
        if (!node->peers[i].active) {
            node->peers[i].fd = fd;
            node->peers[i].active = 1;
            node->peers[i].last_seen = time(NULL);
            snprintf(node->peers[i].name, sizeof(node->peers[i].name), "peer@%s", sock_path);
            node->peer_count++;

            fcntl(fd, F_SETFL, O_NONBLOCK);

            vex_log("TRANSPORT", "Connected to %s (fd=%d)", sock_path, fd);
            return 0;
        }
    }

    close(fd);
    return -1;
}

/* Send data to a specific peer */
int vex_transport_send_to_peer(vex_peer_t *peer, const uint8_t *data, size_t len) {
    if (!peer->active || peer->fd < 0) return -1;

    /* Length-prefixed: 2 bytes big-endian length + payload */
    uint8_t header[2];
    header[0] = (uint8_t)(len >> 8);
    header[1] = (uint8_t)(len & 0xFF);

    ssize_t n = write(peer->fd, header, 2);
    if (n != 2) {
        peer->active = 0;
        close(peer->fd);
        return -1;
    }

    n = write(peer->fd, data, len);
    if (n != (ssize_t)len) {
        peer->active = 0;
        close(peer->fd);
        return -1;
    }

    return 0;
}

/* Send data to all active peers except one (source) */
int vex_transport_send_to_all(vex_node_t *node, const uint8_t *data, size_t len, int except_fd) {
    int sent = 0;
    for (int i = 0; i < VEX_MAX_PEERS; i++) {
        if (node->peers[i].active && node->peers[i].fd != except_fd) {
            if (vex_transport_send_to_peer(&node->peers[i], data, len) == 0) {
                sent++;
            }
        }
    }
    return sent;
}

/* Read a packet from a peer (non-blocking).
 * Returns bytes read, 0 if nothing available, -1 on error/disconnect */
int vex_transport_unix_read(vex_peer_t *peer, uint8_t *buf, size_t buf_len) {
    if (!peer->active) return -1;

    /* Read 2-byte length header */
    uint8_t header[2];
    ssize_t n = read(peer->fd, header, 2);
    if (n == 0) {
        /* Peer disconnected */
        peer->active = 0;
        close(peer->fd);
        return -1;
    }
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        peer->active = 0;
        close(peer->fd);
        return -1;
    }
    if (n != 2) return -1;

    uint16_t pkt_len = ((uint16_t)header[0] << 8) | header[1];
    if (pkt_len > buf_len || pkt_len > VEX_MAX_PACKET) return -1;

    /* Read payload */
    size_t total = 0;
    while (total < pkt_len) {
        n = read(peer->fd, buf + total, pkt_len - total);
        if (n <= 0) {
            peer->active = 0;
            close(peer->fd);
            return -1;
        }
        total += n;
    }

    peer->last_seen = time(NULL);
    return (int)pkt_len;
}
