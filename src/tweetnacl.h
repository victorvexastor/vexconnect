/* TweetNaCl - Minimal NaCl crypto library
 * Public domain - https://tweetnacl.cr.yp.to/
 * We only need crypto_box (X25519 + XSalsa20 + Poly1305)
 * and crypto_sign (Ed25519) declarations here.
 * Full implementation in tweetnacl.c */

#ifndef TWEETNACL_H
#define TWEETNACL_H

#define crypto_box_PUBLICKEYBYTES 32
#define crypto_box_SECRETKEYBYTES 32
#define crypto_box_NONCEBYTES 24
#define crypto_box_ZEROBYTES 32
#define crypto_box_BOXZEROBYTES 16
#define crypto_box_BEFORENMBYTES 32

#define crypto_sign_PUBLICKEYBYTES 32
#define crypto_sign_SECRETKEYBYTES 64
#define crypto_sign_BYTES 64

#define crypto_hash_BYTES 64
#define crypto_scalarmult_BYTES 32
#define crypto_secretbox_KEYBYTES 32
#define crypto_secretbox_NONCEBYTES 24
#define crypto_secretbox_ZEROBYTES 32
#define crypto_secretbox_BOXZEROBYTES 16

typedef unsigned char u8;
typedef unsigned long long u64;

int crypto_box_keypair(u8 *pk, u8 *sk);
int crypto_box(u8 *c, const u8 *m, u64 d, const u8 *n, const u8 *y, const u8 *x);
int crypto_box_open(u8 *m, const u8 *c, u64 d, const u8 *n, const u8 *y, const u8 *x);
int crypto_box_beforenm(u8 *k, const u8 *y, const u8 *x);

int crypto_secretbox(u8 *c, const u8 *m, u64 d, const u8 *n, const u8 *k);
int crypto_secretbox_open(u8 *m, const u8 *c, u64 d, const u8 *n, const u8 *k);

int crypto_sign_keypair(u8 *pk, u8 *sk);
int crypto_sign(u8 *sm, u64 *smlen, const u8 *m, u64 n, const u8 *sk);
int crypto_sign_open(u8 *m, u64 *mlen, const u8 *sm, u64 n, const u8 *pk);

int crypto_hash(u8 *out, const u8 *m, u64 n);
void randombytes(u8 *x, u64 xlen);

#endif
