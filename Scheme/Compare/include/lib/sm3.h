#ifndef SM3_H
#define SM3_H
#include <stdint.h>
#include <stddef.h>

// SM3 上下文
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t  data[64];
    size_t   datalen;
} SM3_CTX;

// 接口
void sm3_init(SM3_CTX *ctx);
void sm3_update(SM3_CTX *ctx, const uint8_t *data, size_t len);
void sm3_final(SM3_CTX *ctx, uint8_t digest[32]);

#endif
