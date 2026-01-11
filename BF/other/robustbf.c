#include "robustbf.h"
#include <stdlib.h>
#include <string.h>

// MurmurHash3 32-bit implementation
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

/**
 * @brief Internal helper to generate all required hashes and fingerprint.
 * We use two base hashes to generate k+2 independent hashes.
 * h_i(x) = hash1(x) + i * hash2(x)
 */
static void _get_hashes_and_fingerprint(const robustbf_t* filter, const void* data, size_t len,
                                        uint32_t* row_index, uint32_t* col_indices, fingerprint_t* fingerprint) {
    uint32_t hash1 = murmurhash3_32(data, len, 0xAAAAAAAA); // Seed 1
    uint32_t hash2 = murmurhash3_32(data, len, 0xBBBBBBBB); // Seed 2

    *row_index = hash1 % filter->rows;
    
    // The fingerprint is derived from hash2. We mask it to get 8 bits.
    // An 8-bit fingerprint cannot be 0, as 0 indicates an empty slot.
    *fingerprint = (fingerprint_t)(hash2 & 0xFF);
    if (*fingerprint == 0) {
        *fingerprint = 1; // Remap 0 to 1
    }

    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        col_indices[i] = (hash1 + i * hash2) % filter->cols;
    }
}

robustbf_t* robustbf_create(uint32_t rows, uint32_t cols, uint32_t num_hashes) {
    if (rows == 0 || cols == 0 || num_hashes == 0) {
        return NULL;
    }

    robustbf_t* filter = (robustbf_t*)malloc(sizeof(robustbf_t));
    if (!filter) {
        return NULL;
    }

    filter->rows = rows;
    filter->cols = cols;
    filter->num_hashes = num_hashes;
    filter->elements = 0;

    size_t total_cells = (size_t)rows * cols;
    filter->filter_data = (fingerprint_t*)calloc(total_cells, sizeof(fingerprint_t));

    if (!filter->filter_data) {
        free(filter);
        return NULL;
    }
    return filter;
}

void robustbf_destroy(robustbf_t* filter) {
    if (filter) {
        free(filter->filter_data);
        free(filter);
    }
}

void robustbf_add(robustbf_t* filter, const void* data, size_t len) {
    uint32_t row_idx;
    fingerprint_t fp;
    uint32_t* col_indices = (uint32_t*)malloc(filter->num_hashes * sizeof(uint32_t));
    if (!col_indices) return;

    _get_hashes_and_fingerprint(filter, data, len, &row_idx, col_indices, &fp);

    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        size_t index = (size_t)row_idx * filter->cols + col_indices[i];
        filter->filter_data[index] = fp;
    }

    filter->elements++;
    free(col_indices);
}

int robustbf_check(const robustbf_t* filter, const void* data, size_t len) {
    uint32_t row_idx;
    fingerprint_t fp;
    uint32_t* col_indices = (uint32_t*)malloc(filter->num_hashes * sizeof(uint32_t));
    if (!col_indices) return 0; // Or handle error appropriately

    _get_hashes_and_fingerprint(filter, data, len, &row_idx, col_indices, &fp);

    int result = 1; // Assume it exists initially
    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        size_t index = (size_t)row_idx * filter->cols + col_indices[i];
        if (filter->filter_data[index] != fp) {
            result = 0; // Found a mismatch, so it definitely doesn't exist
            break;
        }
    }

    free(col_indices);
    return result;
}