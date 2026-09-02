/*
------------------------------------------------------------
Author: Subhajit Halder
Date Created: 2026-08-26
Module: Utilities
File: password_hash.c
About: Self-contained SHA-256 (public-domain algorithm, no external
       crypto library — consistent with this project's no-3rd-party-
       dependency convention) plus random salting for password
       storage. Only pal_* wrappers are used, no direct C stdlib
       calls, per pal.h's "PAL is the only module allowed to use the
       standard C library" rule.

       Not a substitute for a vetted crypto library (no constant-time
       compare, salt entropy is only as good as pal_rand() reseeded
       with pal_time_seed()) — but a large improvement over the
       plaintext storage this replaces for a hobby/learning project.
Revisions:
- 2026-08-26  Initial implementation
------------------------------------------------------------
*/

#include "pal.h"
#include "password_hash.h"

#define SHA256_DIGEST_BYTES 32
#define SALT_BYTES            8

/* ================= SHA-256 (public domain algorithm) ================= */

typedef struct {
    unsigned int  state[8];
    unsigned long long bitlen;
    unsigned char data[64];
    unsigned int  datalen;
} sha256_ctx;

static const unsigned int SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha256_transform(sha256_ctx* ctx, const unsigned char* data)
{
    unsigned int m[64];
    unsigned int a, b, c, d, e, f, g, h, t1, t2;
    int i, j;

    for (i = 0, j = 0; i < 16; i++, j += 4)
        m[i] = ((unsigned int)data[j] << 24) | ((unsigned int)data[j + 1] << 16) |
               ((unsigned int)data[j + 2] << 8) | ((unsigned int)data[j + 3]);

    for (; i < 64; i++)
    {
        unsigned int s0 = ROTR(m[i - 15], 7) ^ ROTR(m[i - 15], 18) ^ (m[i - 15] >> 3);
        unsigned int s1 = ROTR(m[i - 2], 17) ^ ROTR(m[i - 2], 19) ^ (m[i - 2] >> 10);
        m[i] = m[i - 16] + s0 + m[i - 7] + s1;
    }

    a = ctx->state[0]; b = ctx->state[1]; c = ctx->state[2]; d = ctx->state[3];
    e = ctx->state[4]; f = ctx->state[5]; g = ctx->state[6]; h = ctx->state[7];

    for (i = 0; i < 64; i++)
    {
        unsigned int S1 = ROTR(e, 6) ^ ROTR(e, 11) ^ ROTR(e, 25);
        unsigned int ch = (e & f) ^ (~e & g);
        unsigned int S0 = ROTR(a, 2) ^ ROTR(a, 13) ^ ROTR(a, 22);
        unsigned int maj = (a & b) ^ (a & c) ^ (b & c);

        t1 = h + S1 + ch + SHA256_K[i] + m[i];
        t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    ctx->state[0] += a; ctx->state[1] += b; ctx->state[2] += c; ctx->state[3] += d;
    ctx->state[4] += e; ctx->state[5] += f; ctx->state[6] += g; ctx->state[7] += h;
}

static void sha256_init(sha256_ctx* ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
}

static void sha256_update(sha256_ctx* ctx, const unsigned char* data, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64)
        {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void sha256_final(sha256_ctx* ctx, unsigned char* hash)
{
    unsigned int i = ctx->datalen;
    int j;

    if (ctx->datalen < 56)
    {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0;
    }
    else
    {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        pal_memset(ctx->data, 0, 56);
    }

    ctx->bitlen += (unsigned long long)ctx->datalen * 8;
    ctx->data[63] = (unsigned char)(ctx->bitlen);
    ctx->data[62] = (unsigned char)(ctx->bitlen >> 8);
    ctx->data[61] = (unsigned char)(ctx->bitlen >> 16);
    ctx->data[60] = (unsigned char)(ctx->bitlen >> 24);
    ctx->data[59] = (unsigned char)(ctx->bitlen >> 32);
    ctx->data[58] = (unsigned char)(ctx->bitlen >> 40);
    ctx->data[57] = (unsigned char)(ctx->bitlen >> 48);
    ctx->data[56] = (unsigned char)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    for (i = 0; i < 4; i++)
        for (j = 0; j < 8; j++)
            hash[j * 4 + i] = (unsigned char)((ctx->state[j] >> (24 - i * 8)) & 0xff);
}

static void sha256(const unsigned char* data, int len, unsigned char* out)
{
    sha256_ctx ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

/* ================= HEX ENCODE / DECODE ================= */

static const char HEX_DIGITS[] = "0123456789abcdef";

static void bytes_to_hex(const unsigned char* in, int len, char* out)
{
    int i;
    for (i = 0; i < len; i++)
    {
        out[i * 2]     = HEX_DIGITS[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = HEX_DIGITS[in[i] & 0xF];
    }
    out[len * 2] = '\0';
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int hex_to_bytes(const char* in, int hexlen, unsigned char* out)
{
    int i;
    if (hexlen % 2 != 0) return -1;
    for (i = 0; i < hexlen / 2; i++)
    {
        int hi = hex_nibble(in[i * 2]);
        int lo = hex_nibble(in[i * 2 + 1]);
        if (hi < 0 || lo < 0) return -1;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 0;
}

/* ================= SALT ================= */

static int rng_seeded = 0;

static void random_salt(unsigned char* buf, int n)
{
    int i;
    if (!rng_seeded)
    {
        pal_srand(pal_time_seed());
        rng_seeded = 1;
    }
    for (i = 0; i < n; i++)
        buf[i] = (unsigned char)(pal_rand() & 0xFF);
}

/* ================= SALTED HASH ================= */

static void hash_with_salt(const unsigned char* salt, const char* password, unsigned char* out_digest)
{
    unsigned char buf[SALT_BYTES + 256];   /* generous cap for a typed password */
    int plen = (int)pal_strlen(password);
    int total = SALT_BYTES + plen;
    if (total > (int)sizeof(buf)) total = (int)sizeof(buf);

    pal_memcpy(buf, salt, SALT_BYTES);
    if (total > SALT_BYTES)
        pal_memcpy(buf + SALT_BYTES, password, total - SALT_BYTES);

    sha256(buf, total, out_digest);
}

void password_hash(const char* password, char* out)
{
    unsigned char salt[SALT_BYTES];
    unsigned char digest[SHA256_DIGEST_BYTES];
    char salt_hex[SALT_BYTES * 2 + 1];
    char digest_hex[SHA256_DIGEST_BYTES * 2 + 1];

    random_salt(salt, SALT_BYTES);
    hash_with_salt(salt, password, digest);

    bytes_to_hex(salt, SALT_BYTES, salt_hex);
    bytes_to_hex(digest, SHA256_DIGEST_BYTES, digest_hex);

    pal_strcpy(out, salt_hex);
    pal_strcat(out, ":");
    pal_strcat(out, digest_hex);
}

int password_verify(const char* password, const char* stored)
{
    const char* colon = pal_strchr(stored, ':');
    unsigned char salt[SALT_BYTES];
    unsigned char digest[SHA256_DIGEST_BYTES];
    char digest_hex[SHA256_DIGEST_BYTES * 2 + 1];
    int salt_hex_len;

    if (!colon) return 0;

    salt_hex_len = (int)(colon - stored);
    if (salt_hex_len != SALT_BYTES * 2) return 0;
    if (hex_to_bytes(stored, salt_hex_len, salt) != 0) return 0;

    hash_with_salt(salt, password, digest);
    bytes_to_hex(digest, SHA256_DIGEST_BYTES, digest_hex);

    return pal_strcmp(digest_hex, colon + 1) == 0;
}
