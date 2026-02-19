/* crypto.c — Key management and encryption for VexConnect */

#include "vex.h"
#include "tweetnacl.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

/* Derive mesh-wide shared key from service UUID (deterministic) */
void vex_crypto_derive_mesh_key(vex_node_t *node) {
    /* Hash the service UUID to get a shared symmetric key
     * All nodes derive the same key — enables broadcast relay */
    const char *uuid = VEX_SERVICE_UUID;
    uint8_t hash[64];
    crypto_hash(hash, (const uint8_t *)uuid, strlen(uuid));
    memcpy(node->mesh_key, hash, 32);
}

/* Initialize crypto — generate or load keys */
int vex_crypto_init(vex_node_t *node) {
    /* Generate identity keypair (Ed25519 for signing) */
    crypto_sign_keypair(node->sign_pk, node->sign_sk);

    /* Generate box keypair (X25519 for encryption) */
    crypto_box_keypair(node->box_pk, node->box_sk);

    /* Derive shared mesh key */
    vex_crypto_derive_mesh_key(node);

    return 0;
}

/* Save keys to file */
int vex_crypto_save_keys(const vex_node_t *node, const char *path) {
    char filepath[512];
    FILE *f;

    /* Create directory if needed */
    mkdir(path, 0700);

    /* Save signing keypair */
    snprintf(filepath, sizeof(filepath), "%s/identity.key", path);
    f = fopen(filepath, "wb");
    if (!f) {
        vex_log("CRYPTO", "Failed to save identity key: %s", strerror(errno));
        return -1;
    }
    fwrite(node->sign_pk, 1, 32, f);
    fwrite(node->sign_sk, 1, 64, f);
    fclose(f);
    chmod(filepath, 0600);

    /* Save box keypair */
    snprintf(filepath, sizeof(filepath), "%s/ephemeral.key", path);
    f = fopen(filepath, "wb");
    if (!f) {
        vex_log("CRYPTO", "Failed to save ephemeral key: %s", strerror(errno));
        return -1;
    }
    fwrite(node->box_pk, 1, 32, f);
    fwrite(node->box_sk, 1, 32, f);
    fclose(f);
    chmod(filepath, 0600);

    vex_log("CRYPTO", "Keys saved to %s", path);
    return 0;
}

/* Load keys from file. Returns 0 on success, -1 if not found (generate new) */
int vex_crypto_load_keys(vex_node_t *node, const char *path) {
    char filepath[512];
    FILE *f;

    /* Load signing keypair */
    snprintf(filepath, sizeof(filepath), "%s/identity.key", path);
    f = fopen(filepath, "rb");
    if (!f) return -1;

    if (fread(node->sign_pk, 1, 32, f) != 32 ||
        fread(node->sign_sk, 1, 64, f) != 64) {
        fclose(f);
        return -1;
    }
    fclose(f);

    /* Load box keypair */
    snprintf(filepath, sizeof(filepath), "%s/ephemeral.key", path);
    f = fopen(filepath, "rb");
    if (!f) {
        /* Regenerate ephemeral if missing */
        crypto_box_keypair(node->box_pk, node->box_sk);
    } else {
        if (fread(node->box_pk, 1, 32, f) != 32 ||
            fread(node->box_sk, 1, 32, f) != 32) {
            fclose(f);
            crypto_box_keypair(node->box_pk, node->box_sk);
        } else {
            fclose(f);
        }
    }

    vex_crypto_derive_mesh_key(node);
    vex_log("CRYPTO", "Keys loaded from %s", path);
    return 0;
}

/* Encrypt a broadcast message using the shared mesh key.
 * Output: nonce(24) || ciphertext
 * Returns 0 on success */
int vex_crypto_encrypt_broadcast(const vex_node_t *node,
                                  const uint8_t *plain, uint16_t len,
                                  uint8_t *cipher, uint16_t *cipher_len) {
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint8_t padded[crypto_secretbox_ZEROBYTES + VEX_MAX_PAYLOAD];
    uint8_t encrypted[crypto_secretbox_ZEROBYTES + VEX_MAX_PAYLOAD];

    if (len > VEX_MAX_PAYLOAD - crypto_secretbox_NONCEBYTES - crypto_secretbox_BOXZEROBYTES)
        return -1;

    /* Generate random nonce */
    randombytes(nonce, crypto_secretbox_NONCEBYTES);

    /* Pad plaintext with ZEROBYTES leading zeros */
    memset(padded, 0, crypto_secretbox_ZEROBYTES);
    memcpy(padded + crypto_secretbox_ZEROBYTES, plain, len);

    /* Encrypt */
    if (crypto_secretbox(encrypted, padded,
                         crypto_secretbox_ZEROBYTES + len,
                         nonce, node->mesh_key) != 0)
        return -1;

    /* Output: nonce || ciphertext (without BOXZEROBYTES padding) */
    memcpy(cipher, nonce, crypto_secretbox_NONCEBYTES);
    memcpy(cipher + crypto_secretbox_NONCEBYTES,
           encrypted + crypto_secretbox_BOXZEROBYTES,
           len + crypto_secretbox_ZEROBYTES - crypto_secretbox_BOXZEROBYTES);

    *cipher_len = crypto_secretbox_NONCEBYTES + len +
                  crypto_secretbox_ZEROBYTES - crypto_secretbox_BOXZEROBYTES;
    return 0;
}

/* Decrypt a broadcast message using the shared mesh key.
 * Input: nonce(24) || ciphertext
 * Returns 0 on success */
int vex_crypto_decrypt_broadcast(const vex_node_t *node,
                                  const uint8_t *cipher, uint16_t len,
                                  uint8_t *plain, uint16_t *plain_len) {
    uint8_t nonce[crypto_secretbox_NONCEBYTES];
    uint8_t padded_cipher[crypto_secretbox_BOXZEROBYTES + VEX_MAX_PAYLOAD];
    uint8_t decrypted[crypto_secretbox_ZEROBYTES + VEX_MAX_PAYLOAD];

    if (len <= crypto_secretbox_NONCEBYTES) return -1;

    uint16_t ct_len = len - crypto_secretbox_NONCEBYTES;

    /* Extract nonce */
    memcpy(nonce, cipher, crypto_secretbox_NONCEBYTES);

    /* Pad ciphertext with BOXZEROBYTES leading zeros */
    memset(padded_cipher, 0, crypto_secretbox_BOXZEROBYTES);
    memcpy(padded_cipher + crypto_secretbox_BOXZEROBYTES,
           cipher + crypto_secretbox_NONCEBYTES, ct_len);

    /* Decrypt */
    if (crypto_secretbox_open(decrypted, padded_cipher,
                              crypto_secretbox_BOXZEROBYTES + ct_len,
                              nonce, node->mesh_key) != 0)
        return -1;

    *plain_len = ct_len - (crypto_secretbox_ZEROBYTES - crypto_secretbox_BOXZEROBYTES);
    memcpy(plain, decrypted + crypto_secretbox_ZEROBYTES, *plain_len);
    return 0;
}
