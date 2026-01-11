#include "mbf.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// MurmurHash3 32-bit implementation (a suitable Lhash function)
static uint32_t murmurhash3_32(const void* key, size_t len, uint32_t seed) {
    const uint8_t* data = (const uint8_t*)key;
    const int nblocks = len / 4;
    uint32_t h1 = seed;
    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);
    for (int i = -nblocks; i; i++) {
        uint32_t k1 = blocks[i];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;
        h1 ^= k1;
        h1 = (h1 << 13) | (h1 >> (32 - 13));
        h1 = h1 * 5 + 0xe6546b64;
    }

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);
    uint32_t k1 = 0;
    switch (len & 3) {
    case 3: k1 ^= tail[2] << 16;
    case 2: k1 ^= tail[1] << 8;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;
        h1 ^= k1;
    };

    h1 ^= len;
    h1 ^= h1 >> 16;
    h1 *= 0x85ebca6b;
    h1 ^= h1 >> 13;
    h1 *= 0xc2b2ae35;
    h1 ^= h1 >> 16;

    return h1;
}

// Helper function to check if a number is prime
static int is_prime(uint32_t n) {
    if (n <= 1) return 0;
    for (uint32_t i = 2; i * i <= n; i++) {
        if (n % i == 0) return 0;
    }
    return 1;
}

// Helper function to find the largest prime <= n
static uint32_t find_largest_prime(uint32_t n) {
    for (uint32_t i = n; i > 1; i--) {
        if (is_prime(i)) {
            return i;
        }
    }
    return 2; // Should not happen for n > 1
}

mbf_t* mbf_create(uint32_t v, uint32_t a, uint32_t b, uint32_t z) {
    if (v == 0 || a == 0 || b == 0 || z == 0 || z > 32) {
        return NULL;
    }

    mbf_t* filter = (mbf_t*)malloc(sizeof(mbf_t));
    if (!filter) {
        return NULL;
    }

    filter->v = v;
    filter->a = a;
    filter->b = b;
    filter->z = z;
    filter->p_z = find_largest_prime(z);
    filter->elements = 0;

    size_t total_cells = (size_t)v * a * b;
    filter->filter_data = (uint32_t*)calloc(total_cells, sizeof(uint32_t));

    if (!filter->filter_data) {
        free(filter);
        return NULL;
    }

    return filter;
}

void mbf_destroy(mbf_t* filter) {
    if (filter) {
        free(filter->filter_data);
        free(filter);
    }
}

void mbf_insert(mbf_t* filter, const void* data, size_t len, uint32_t seed0) {
    uint32_t lo = murmurhash3_32(data, len, seed0);

    uint32_t a_lo = lo % filter->a;
    uint32_t b_lo = lo % filter->b;
    uint32_t bit_pos = 1 << (lo % filter->p_z);
    size_t index = (0 * filter->a * filter->b) + (a_lo * filter->b) + b_lo;
    filter->filter_data[index] |= bit_pos;

    for (uint32_t i = 1; i < filter->v; ++i) {
        lo = murmurhash3_32(data, len, lo);
        a_lo = lo % filter->a;
        b_lo = lo % filter->b;
        bit_pos = 1 << (lo % filter->p_z);
        index = (i * filter->a * filter->b) + (a_lo * filter->b) + b_lo;
        filter->filter_data[index] |= bit_pos;
    }
    filter->elements++;
}

int mbf_check(const mbf_t* filter, const void* data, size_t len, uint32_t seed0) {
    uint32_t lo = murmurhash3_32(data, len, seed0);

    uint32_t a_lo = lo % filter->a;
    uint32_t b_lo = lo % filter->b;
    uint32_t bit_pos = 1 << (lo % filter->p_z);
    size_t index = (0 * filter->a * filter->b) + (a_lo * filter->b) + b_lo;
    if ((filter->filter_data[index] & bit_pos) == 0) {
        return 0;
    }

    for (uint32_t i = 1; i < filter->v; ++i) {
        lo = murmurhash3_32(data, len, lo);
        a_lo = lo % filter->a;
        b_lo = lo % filter->b;
        bit_pos = 1 << (lo % filter->p_z);
        index = (i * filter->a * filter->b) + (a_lo * filter->b) + b_lo;
        if ((filter->filter_data[index] & bit_pos) == 0) {
            return 0;
        }
    }
    return 1;
}