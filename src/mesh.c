/* mesh.c — Core mesh routing logic */

#include "vex.h"
#include "tweetnacl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Initialize mesh node */
int vex_mesh_init(vex_node_t *node) {
    memset(node, 0, sizeof(*node));

    node->default_ttl = VEX_DEFAULT_TTL;
    node->scan_interval = VEX_SCAN_INTERVAL;
    node->relay_enabled = 1;
    node->running = 1;
    node->started_at = time(NULL);
    node->listen_fd = -1;

    /* Initialize dedup cache */
    vex_seen_init(&node->seen);

    /* Initialize crypto (generate or load keys) */
    char keypath[256];
    snprintf(keypath, sizeof(keypath), "%s/.vexconnect", getenv("HOME") ? getenv("HOME") : "/tmp");

    if (vex_crypto_load_keys(node, keypath) != 0) {
        vex_log("MESH", "No existing keys found, generating new identity");
        vex_crypto_init(node);
        vex_crypto_save_keys(node, keypath);
    }

    /* Generate node name from public key if not set */
    if (node->node_name[0] == '\0') {
        char hex[9];
        vex_hex(node->sign_pk, 4, hex);
        snprintf(node->node_name, sizeof(node->node_name), "vex-%s", hex);
    }

    vex_log("MESH", "Node %s online (TTL=%d)", node->node_name, node->default_ttl);
    return 0;
}

/* Send a message into the mesh (broadcast) */
int vex_mesh_send(vex_node_t *node, const char *message) {
    vex_packet_t pkt;
    uint8_t wire[VEX_MAX_PACKET];
    uint8_t encrypted[VEX_MAX_PAYLOAD];
    uint16_t encrypted_len;

    size_t msg_len = strlen(message);
    if (msg_len > VEX_MAX_PAYLOAD - 100) {  /* Leave room for crypto overhead */
        vex_log("MESH", "Message too long (%zu bytes)", msg_len);
        return -1;
    }

    /* Encrypt the message */
    if (vex_crypto_encrypt_broadcast(node, (const uint8_t *)message, (uint16_t)msg_len,
                                      encrypted, &encrypted_len) != 0) {
        vex_log("MESH", "Encryption failed");
        return -1;
    }

    /* Build packet */
    pkt.version = VEX_VERSION;
    pkt.ttl = node->default_ttl;
    pkt.flags = VEX_FLAG_ENCRYPTED | VEX_FLAG_BROADCAST;
    pkt.payload_len = encrypted_len;
    memcpy(pkt.payload, encrypted, encrypted_len);

    /* Generate packet ID */
    vex_packet_make_id(encrypted, encrypted_len, pkt.packet_id);

    /* Mark as seen (don't process our own packets) */
    vex_seen_add(&node->seen, pkt.packet_id);

    /* Encode to wire format */
    int wire_len = vex_packet_encode(&pkt, wire, sizeof(wire));
    if (wire_len < 0) {
        vex_log("MESH", "Packet encode failed");
        return -1;
    }

    /* Send to all peers */
    int sent = vex_transport_send_to_all(node, wire, (size_t)wire_len, -1);
    node->packets_sent++;

    char id_hex[17];
    vex_hex(pkt.packet_id, 8, id_hex);
    vex_log("MESH", "Sent [%s] TTL=%d → %d peers (%zu bytes)",
            id_hex, pkt.ttl, sent, msg_len);

    return sent;
}

/* Process a received packet — decrypt, display, relay */
int vex_mesh_receive(vex_node_t *node, const uint8_t *raw, size_t len, int source_fd) {
    vex_packet_t pkt;

    /* Decode */
    if (vex_packet_decode(raw, len, &pkt) != 0) {
        node->packets_dropped++;
        return -1;
    }

    /* Version check */
    if (pkt.version != VEX_VERSION) {
        node->packets_dropped++;
        return -1;
    }

    char id_hex[17];
    vex_hex(pkt.packet_id, 8, id_hex);

    /* Dedup check */
    if (vex_seen_check(&node->seen, pkt.packet_id)) {
        /* Already seen — drop silently */
        node->packets_dropped++;
        return 0;
    }

    /* Mark as seen */
    vex_seen_add(&node->seen, pkt.packet_id);
    node->packets_received++;

    /* Decrypt and display */
    if (pkt.flags & VEX_FLAG_ENCRYPTED) {
        uint8_t plaintext[VEX_MAX_PAYLOAD];
        uint16_t plain_len;

        if (vex_crypto_decrypt_broadcast(node, pkt.payload, pkt.payload_len,
                                          plaintext, &plain_len) == 0) {
            plaintext[plain_len] = '\0';
            printf("\r[MESH] ← %s (TTL=%d, hops=%d)\n> ",
                   (char *)plaintext, pkt.ttl, node->default_ttl - pkt.ttl);
            fflush(stdout);
        } else {
            vex_log("MESH", "Decryption failed for packet %s", id_hex);
        }
    }

    /* Relay to other peers */
    if (node->relay_enabled) {
        return vex_mesh_relay(node, raw, len, source_fd);
    }

    return 0;
}

/* Relay a packet to all peers except the source */
int vex_mesh_relay(vex_node_t *node, const uint8_t *raw, size_t len, int source_fd) {
    vex_packet_t pkt;

    if (vex_packet_decode(raw, len, &pkt) != 0) return -1;

    /* TTL check */
    if (pkt.ttl <= 1) {
        /* End of the line */
        return 0;
    }

    /* Decrement TTL and re-encode */
    pkt.ttl--;

    uint8_t wire[VEX_MAX_PACKET];
    int wire_len = vex_packet_encode(&pkt, wire, sizeof(wire));
    if (wire_len < 0) return -1;

    /* Forward to all peers except source */
    int relayed = vex_transport_send_to_all(node, wire, (size_t)wire_len, source_fd);
    node->packets_relayed++;

    char id_hex[17];
    vex_hex(pkt.packet_id, 8, id_hex);
    vex_log("MESH", "Relay [%s] TTL=%d → %d peers", id_hex, pkt.ttl, relayed);

    return relayed;
}
