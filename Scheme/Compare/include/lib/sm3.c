#include "sm3.h"

#define ROTL(x,n) (((x) << (n)) | ((x) >> (32-(n))))
#define P0(x)    ((x) ^ ROTL((x),9) ^ ROTL((x),17))
#define P1(x)    ((x) ^ ROTL((x),15) ^ ROTL((x),23))
#define FF(j,x,y,z) ((j)<16 ? ((x) ^ (y) ^ (z)) : (((x)&(y))|((x)&(z))|((y)&(z))))
#define GG(j,x,y,z) ((j)<16 ? ((x) ^ (y) ^ (z)) : (((x)&(y))|((~(x))&(z))))

// 常量表放只读区
static const uint32_t Tj[64] __attribute__((section(".rodata"))) = {
    0x79CC4519,0x79CC4519,0x79CC4519,0x79CC4519,
    0x79CC4519,0x79CC4519,0x79CC4519,0x79CC4519,
    0x79CC4519,0x79CC4519,0x79CC4519,0x79CC4519,
    0x79CC4519,0x79CC4519,0x79CC4519,0x79CC4519,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A,
    0x7A879D8A,0x7A879D8A,0x7A879D8A,0x7A879D8A
};

static void sm3_compress(SM3_CTX *ctx, const uint8_t block[64]) {
    uint32_t W[68], W1[64];
    uint32_t A=ctx->state[0],B=ctx->state[1],C_1=ctx->state[2],D=ctx->state[3],E_1=ctx->state[4],F=ctx->state[5],G=ctx->state[6],H=ctx->state[7];

    // 消息扩展
    for (int j=0; j<16; j++) {
        W[j] = (block[j*4]<<24)|(block[j*4+1]<<16)|(block[j*4+2]<<8)|block[j*4+3];
    }
    for (int j=16; j<68; j++) {
        W[j] = P1(W[j-16]^W[j-9]^ROTL(W[j-3],15)) ^ ROTL(W[j-13],7) ^ W[j-6];
    }
    for (int j=0; j<64; j++) {
        W1[j] = W[j] ^ W[j+4];
        uint32_t SS1 = ROTL((ROTL(A,12)+E_1+ROTL(Tj[j], j)),7);
        uint32_t SS2 = SS1 ^ ROTL(A,12);
        uint32_t TT1 = FF(j,A,B,C_1) + D + SS2 + W1[j];
        uint32_t TT2 = GG(j,E_1,F,G) + H + SS1 + W[j];
        D=C_1; C_1=ROTL(B,9); B=A; A=TT1;
        H=G; G=ROTL(F,19); F=E_1; E_1=P0(TT2);
    }
    ctx->state[0]^=A; ctx->state[1]^=B; ctx->state[2]^=C_1; ctx->state[3]^=D;
    ctx->state[4]^=E_1; ctx->state[5]^=F; ctx->state[6]^=G; ctx->state[7]^=H;
}

void sm3_init(SM3_CTX *ctx) {
    ctx->state[0]=0x7380166F;ctx->state[1]=0x4914B2B9;
    ctx->state[2]=0x172442D7;ctx->state[3]=0xDA8A0600;
    ctx->state[4]=0xA96F30BC;ctx->state[5]=0x163138AA;
    ctx->state[6]=0xE38DEE4D;ctx->state[7]=0xB0FB0E4E;
    ctx->datalen=0; ctx->bitlen=0;
}

void sm3_update(SM3_CTX *ctx, const uint8_t *data, size_t len) {
    while (len--) {
        ctx->data[ctx->datalen++] = *data++;
        if (ctx->datalen==64) {
            sm3_compress(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void sm3_final(SM3_CTX *ctx, uint8_t digest[32]) {
    size_t i = ctx->datalen;
    ctx->data[i++] = 0x80;
    if (i>56) {
        while (i<64) ctx->data[i++]=0;
        sm3_compress(ctx, ctx->data);
        i = 0;
    }
    while (i<56) ctx->data[i++]=0;
    ctx->bitlen += ctx->datalen * 8;
    for (int j=7; j>=0; j--) {
        ctx->data[i++] = (ctx->bitlen >> (j*8)) & 0xFF;
    }
    sm3_compress(ctx, ctx->data);
    for (i=0; i<8; i++) {
        digest[i*4]   = (ctx->state[i]>>24)&0xFF;
        digest[i*4+1] = (ctx->state[i]>>16)&0xFF;
        digest[i*4+2] = (ctx->state[i]>> 8)&0xFF;
        digest[i*4+3] = (ctx->state[i]    )&0xFF;
    }
}
