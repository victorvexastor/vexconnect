/* main.c — VexConnect mesh relay node
 *
 * Usage:
 *   ./vexconnect --listen /tmp/vex1.sock                    # Start node 1
 *   ./vexconnect --listen /tmp/vex2.sock --peer /tmp/vex1.sock  # Start node 2, connect to 1
 *   ./vexconnect --listen /tmp/vex3.sock --peer /tmp/vex2.sock  # Node 3 → 2 → 1 (mesh!)
 *
 * Then type messages in any terminal. They hop through the mesh.
 */

#include "vex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <getopt.h>

/* Forward declaration */
int vex_transport_unix_read(vex_peer_t *peer, uint8_t *buf, size_t buf_len);

static vex_node_t node;

static void handle_signal(int sig) {
    (void)sig;
    node.running = 0;
    printf("\n[VexConnect] Shutting down...\n");
}

static void print_banner(void) {
    printf("\n"
           "  ╦  ╦┌─┐─┐ ┬╔═╗┌─┐┌┐┌┌┐┌┌─┐┌─┐┌┬┐\n"
           "  ╚╗╔╝├┤ ┌┴┬┘║  │ │││││││├┤ │   │ \n"
           "   ╚╝ └─┘┴ └─╚═╝└─┘┘└┘┘└┘└─┘└─┘ ┴ \n"
           "  Free mesh. No tower. No ISP. No permission.\n"
           "  v0.1 — github.com/victorvexastor/vexconnect\n\n");
}

static void print_usage(const char *prog) {
    printf("Usage: %s [options]\n\n"
           "Options:\n"
           "  --listen PATH    Unix socket path to listen on (required)\n"
           "  --peer PATH      Connect to another node's socket (repeatable)\n"
           "  --name NAME      Node display name\n"
           "  --ttl N          Default TTL (default: 7)\n"
           "  --no-relay       Don't relay packets (receive only)\n"
           "  --stats          Print stats every 30s\n"
           "  --help           Show this help\n"
           "  --version        Show version\n\n"
           "Interactive commands:\n"
           "  <message>        Broadcast a message to the mesh\n"
           "  /peers           List connected peers\n"
           "  /stats           Show relay statistics\n"
           "  /quit            Exit\n\n",
           prog);
}

static void print_stats(vex_node_t *n) {
    time_t uptime = time(NULL) - n->started_at;
    int hours = (int)(uptime / 3600);
    int mins = (int)((uptime % 3600) / 60);

    printf("\n[STATS] Node: %s | Uptime: %dh%dm\n", n->node_name, hours, mins);
    printf("[STATS] Sent: %llu | Received: %llu | Relayed: %llu | Dropped: %llu\n",
           (unsigned long long)n->packets_sent,
           (unsigned long long)n->packets_received,
           (unsigned long long)n->packets_relayed,
           (unsigned long long)n->packets_dropped);

    int active = 0;
    for (int i = 0; i < VEX_MAX_PEERS; i++)
        if (n->peers[i].active) active++;
    printf("[STATS] Peers: %d active\n\n", active);
}

static void print_peers(vex_node_t *n) {
    int count = 0;
    printf("\n[PEERS]\n");
    for (int i = 0; i < VEX_MAX_PEERS; i++) {
        if (n->peers[i].active) {
            time_t ago = time(NULL) - n->peers[i].last_seen;
            printf("  %s (fd=%d, last seen %lds ago)\n",
                   n->peers[i].name, n->peers[i].fd, (long)ago);
            count++;
        }
    }
    if (count == 0) printf("  (no peers connected)\n");
    printf("\n");
}

int main(int argc, char *argv[]) {
    const char *listen_path = NULL;
    const char *peer_paths[VEX_MAX_PEERS];
    int peer_count = 0;
    const char *name = NULL;
    int ttl = VEX_DEFAULT_TTL;
    int relay = 1;
    int show_stats = 0;

    static struct option long_opts[] = {
        {"listen",   required_argument, 0, 'l'},
        {"peer",     required_argument, 0, 'p'},
        {"name",     required_argument, 0, 'n'},
        {"ttl",      required_argument, 0, 't'},
        {"no-relay", no_argument,       0, 'r'},
        {"stats",    no_argument,       0, 's'},
        {"help",     no_argument,       0, 'h'},
        {"version",  no_argument,       0, 'v'},
        {0, 0, 0, 0}
    };

    int c;
    while ((c = getopt_long(argc, argv, "l:p:n:t:rshv", long_opts, NULL)) != -1) {
        switch (c) {
            case 'l': listen_path = optarg; break;
            case 'p':
                if (peer_count < VEX_MAX_PEERS)
                    peer_paths[peer_count++] = optarg;
                break;
            case 'n': name = optarg; break;
            case 't': ttl = atoi(optarg); break;
            case 'r': relay = 0; break;
            case 's': show_stats = 1; break;
            case 'v': printf("VexConnect v0.1\n"); return 0;
            case 'h':
            default:
                print_usage(argv[0]);
                return c == 'h' ? 0 : 1;
        }
    }

    if (!listen_path) {
        fprintf(stderr, "Error: --listen PATH is required\n\n");
        print_usage(argv[0]);
        return 1;
    }

    /* Signal handlers */
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    print_banner();

    /* Initialize node */
    vex_mesh_init(&node);
    if (name) strncpy(node.node_name, name, sizeof(node.node_name) - 1);
    node.default_ttl = (uint8_t)ttl;
    node.relay_enabled = relay;

    /* Start listening */
    if (vex_transport_unix_init(&node, listen_path) != 0) {
        fprintf(stderr, "Failed to start listener\n");
        return 1;
    }

    /* Connect to specified peers */
    for (int i = 0; i < peer_count; i++) {
        vex_transport_unix_connect(&node, peer_paths[i]);
    }

    char id_hex[9];
    vex_hex(node.sign_pk, 4, id_hex);
    printf("[VexConnect] Node %s ready (id: %s)\n", node.node_name, id_hex);
    printf("[VexConnect] Peers: %d | Relay: %s\n",
           node.peer_count, relay ? "ON" : "OFF");
    printf("[VexConnect] Type a message and press Enter to broadcast.\n");
    printf("[VexConnect] Commands: /peers /stats /quit\n\n> ");
    fflush(stdout);

    /* Main loop */
    time_t last_stats = time(NULL);
    time_t last_prune = time(NULL);

    while (node.running) {
        /* Poll for stdin + peer data */
        struct pollfd fds[VEX_MAX_PEERS + 2];
        int nfds = 0;

        /* stdin */
        fds[nfds].fd = STDIN_FILENO;
        fds[nfds].events = POLLIN;
        nfds++;

        /* listen socket */
        if (node.listen_fd >= 0) {
            fds[nfds].fd = node.listen_fd;
            fds[nfds].events = POLLIN;
            nfds++;
        }

        /* peer sockets */
        for (int i = 0; i < VEX_MAX_PEERS; i++) {
            if (node.peers[i].active) {
                fds[nfds].fd = node.peers[i].fd;
                fds[nfds].events = POLLIN;
                nfds++;
            }
        }

        int ready = poll(fds, nfds, 1000);  /* 1 second timeout */
        if (ready < 0) continue;

        /* Check stdin */
        if (fds[0].revents & POLLIN) {
            char input[512];
            if (fgets(input, sizeof(input), stdin)) {
                /* Strip newline */
                size_t len = strlen(input);
                if (len > 0 && input[len - 1] == '\n') input[--len] = '\0';
                if (len == 0) { printf("> "); fflush(stdout); continue; }

                /* Commands */
                if (strcmp(input, "/quit") == 0 || strcmp(input, "/q") == 0) {
                    node.running = 0;
                    continue;
                }
                if (strcmp(input, "/peers") == 0) {
                    print_peers(&node);
                    printf("> "); fflush(stdout);
                    continue;
                }
                if (strcmp(input, "/stats") == 0) {
                    print_stats(&node);
                    printf("> "); fflush(stdout);
                    continue;
                }

                /* Send message */
                vex_mesh_send(&node, input);
                printf("> ");
                fflush(stdout);
            }
        }

        /* Check listen socket for new connections */
        if (node.listen_fd >= 0) {
            for (int i = 0; i < nfds; i++) {
                if (fds[i].fd == node.listen_fd && (fds[i].revents & POLLIN)) {
                    vex_transport_unix_accept(&node);
                }
            }
        }

        /* Check peer sockets for incoming data */
        for (int i = 0; i < VEX_MAX_PEERS; i++) {
            if (!node.peers[i].active) continue;

            uint8_t buf[VEX_MAX_PACKET];
            int n = vex_transport_unix_read(&node.peers[i], buf, sizeof(buf));
            if (n > 0) {
                vex_mesh_receive(&node, buf, (size_t)n, node.peers[i].fd);
            } else if (n < 0) {
                vex_log("TRANSPORT", "Peer %s disconnected", node.peers[i].name);
                node.peer_count--;
            }
        }

        /* Periodic maintenance */
        time_t now = time(NULL);
        if (now - last_prune > 10) {
            vex_seen_prune(&node.seen);
            last_prune = now;
        }
        if (show_stats && now - last_stats > 30) {
            print_stats(&node);
            printf("> "); fflush(stdout);
            last_stats = now;
        }
    }

    /* Cleanup */
    if (node.listen_fd >= 0) close(node.listen_fd);
    for (int i = 0; i < VEX_MAX_PEERS; i++) {
        if (node.peers[i].active) close(node.peers[i].fd);
    }

    printf("[VexConnect] Node %s offline. %llu packets relayed.\n",
           node.node_name, (unsigned long long)node.packets_relayed);
    return 0;
}
