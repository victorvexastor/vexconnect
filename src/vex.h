/* VexConnect — Mesh relay node
 * Single header for all internal types and functions */

#ifndef VEX_H
#define VEX_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

/* ── Protocol constants ── */
#define VEX_VERSION       0x01
#define VEX_MAX_PACKET    512
#define VEX_HEADER_SIZE   11        /* version(1) + packet_id(8) + ttl(1) + flags(1) */
#define VEX_MAX_PAYLOAD   (VEX_MAX_PACKET - VEX_HEADER_SIZE)
#define VEX_DEFAULT_TTL   7
#define VEX_SEEN_CAPACITY 1000
#define VEX_SEEN_TTL_SEC  60
#define VEX_MAX_PEERS     32
#define VEX_BLE_MAX_CONN  5
#define VEX_SCAN_INTERVAL 15        /* seconds */
#define VEX_KEY_ROTATE    3600      /* seconds — ephemeral key rotation */

/* ── Flags ── */
#define VEX_FLAG_ENCRYPTED    (1 << 0)
#define VEX_FLAG_BROADCAST    (1 << 1)
#define VEX_FLAG_ACK_REQ      (1 << 2)

/* ── GATT UUIDs ── */
#define VEX_SERVICE_UUID  "0000vc01-0000-1000-8000-00805f9b34fb"
#define VEX_TX_UUID       "0000vc02-0000-1000-8000-00805f9b34fb"
#define VEX_RX_UUID       "0000vc03-0000-1000-8000-00805f9b34fb"
#define VEX_STATS_UUID    "0000vc04-0000-1000-8000-00805f9b34fb"

/* ── Packet ── */
typedef struct {
    uint8_t  version;
    uint8_t  packet_id[8];
    uint8_t  ttl;
    uint8_t  flags;
    uint8_t  payload[VEX_MAX_PAYLOAD];
    uint16_t payload_len;
} vex_packet_t;

/* ── Seen cache (deduplication) ── */
typedef struct {
    uint8_t  packet_id[8];
    time_t   timestamp;
    int      active;
} vex_seen_entry_t;

typedef struct {
    vex_seen_entry_t entries[VEX_SEEN_CAPACITY];
    int count;
} vex_seen_cache_t;

/* ── Peer ── */
typedef struct {
    int      fd;             /* socket fd or BLE handle */
    char     name[64];
    uint8_t  pubkey[32];
    int      active;
    int      rssi;
    time_t   last_seen;
} vex_peer_t;

/* ── Node state ── */
typedef struct {
    /* Identity */
    uint8_t  sign_pk[32];
    uint8_t  sign_sk[64];
    uint8_t  box_pk[32];
    uint8_t  box_sk[32];
    char     node_name[64];

    /* Mesh shared key (for broadcast encryption) */
    uint8_t  mesh_key[32];

    /* Peers */
    vex_peer_t peers[VEX_MAX_PEERS];
    int peer_count;

    /* Dedup */
    vex_seen_cache_t seen;

    /* Stats */
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t packets_relayed;
    uint64_t packets_dropped;
    time_t   started_at;

    /* Config */
    uint8_t  default_ttl;
    int      scan_interval;
    int      relay_enabled;
    int      lora_enabled;
    int      running;

    /* Transport */
    int      listen_fd;      /* for unix socket transport */
} vex_node_t;

/* ── packet.c ── */
int  vex_packet_encode(const vex_packet_t *pkt, uint8_t *buf, size_t buf_len);
int  vex_packet_decode(const uint8_t *buf, size_t len, vex_packet_t *pkt);
void vex_packet_make_id(const uint8_t *payload, uint16_t len, uint8_t *id_out);

/* ── seen.c ── */
void vex_seen_init(vex_seen_cache_t *cache);
int  vex_seen_check(vex_seen_cache_t *cache, const uint8_t *packet_id);  /* 1=seen, 0=new */
void vex_seen_add(vex_seen_cache_t *cache, const uint8_t *packet_id);
void vex_seen_prune(vex_seen_cache_t *cache);

/* ── crypto.c ── */
int  vex_crypto_init(vex_node_t *node);
int  vex_crypto_load_keys(vex_node_t *node, const char *path);
int  vex_crypto_save_keys(const vex_node_t *node, const char *path);
int  vex_crypto_encrypt_broadcast(const vex_node_t *node, const uint8_t *plain, uint16_t len,
                                   uint8_t *cipher, uint16_t *cipher_len);
int  vex_crypto_decrypt_broadcast(const vex_node_t *node, const uint8_t *cipher, uint16_t len,
                                   uint8_t *plain, uint16_t *plain_len);
void vex_crypto_derive_mesh_key(vex_node_t *node);

/* ── mesh.c ── */
int  vex_mesh_init(vex_node_t *node);
int  vex_mesh_send(vex_node_t *node, const char *message);
int  vex_mesh_relay(vex_node_t *node, const uint8_t *raw, size_t len, int source_fd);
int  vex_mesh_receive(vex_node_t *node, const uint8_t *raw, size_t len, int source_fd);

/* ── transport (unix socket for dev, BLE for production) ── */
int  vex_transport_unix_init(vex_node_t *node, const char *sock_path);
int  vex_transport_unix_accept(vex_node_t *node);
int  vex_transport_unix_connect(vex_node_t *node, const char *sock_path);
int  vex_transport_send_to_peer(vex_peer_t *peer, const uint8_t *data, size_t len);
int  vex_transport_send_to_all(vex_node_t *node, const uint8_t *data, size_t len, int except_fd);

/* ── util ── */
void vex_hex(const uint8_t *data, size_t len, char *out);
void vex_log(const char *component, const char *fmt, ...);
uint64_t vex_time_ms(void);

#endif
