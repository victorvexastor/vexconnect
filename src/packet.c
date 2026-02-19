/* packet.c — VexConnect packet encoding/decoding */

#include "vex.h"
#include "tweetnacl.h"
#include <string.h>
#include <stdio.h>

/* Generate packet ID: first 8 bytes of SHA-512(payload + timestamp + random) */
void vex_packet_make_id(const uint8_t *payload, uint16_t len, uint8_t *id_out) {
    uint8_t hash_input[VEX_MAX_PAYLOAD + 16];
    uint8_t hash_output[64];
    uint8_t nonce[8];

    randombytes(nonce, 8);
    memcpy(hash_input, payload, len);
    memcpy(hash_input + len, nonce, 8);

    crypto_hash(hash_output, hash_input, len + 8);
    memcpy(id_out, hash_output, 8);
}

/* Encode packet to wire format. Returns bytes written, or -1 on error */
int vex_packet_encode(const vex_packet_t *pkt, uint8_t *buf, size_t buf_len) {
    size_t total = VEX_HEADER_SIZE + pkt->payload_len;

    if (total > buf_len || total > VEX_MAX_PACKET) return -1;
    if (pkt->version != VEX_VERSION) return -1;

    buf[0] = pkt->version;
    memcpy(buf + 1, pkt->packet_id, 8);
    buf[9] = pkt->ttl;
    buf[10] = pkt->flags;

    if (pkt->payload_len > 0) {
        memcpy(buf + VEX_HEADER_SIZE, pkt->payload, pkt->payload_len);
    }

    return (int)total;
}

/* Decode wire bytes to packet. Returns 0 on success, -1 on error */
int vex_packet_decode(const uint8_t *buf, size_t len, vex_packet_t *pkt) {
    if (len < VEX_HEADER_SIZE) return -1;

    pkt->version = buf[0];
    if (pkt->version != VEX_VERSION) return -1;

    memcpy(pkt->packet_id, buf + 1, 8);
    pkt->ttl = buf[9];
    pkt->flags = buf[10];

    pkt->payload_len = (uint16_t)(len - VEX_HEADER_SIZE);
    if (pkt->payload_len > VEX_MAX_PAYLOAD) return -1;

    if (pkt->payload_len > 0) {
        memcpy(pkt->payload, buf + VEX_HEADER_SIZE, pkt->payload_len);
    }

    return 0;
}
