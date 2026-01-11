#include "ebf.h"
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
    case 3: k1 ^= tail[2] << 16;break;
    case 2: k1 ^= tail[1] << 8;break;
    case 1: k1 ^= tail[0];
        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;
        h1 ^= k1;
        break;
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
 * @brief Internal helper to generate fingerprint and k primary location hashes.
 */
static void _get_hashes_and_fingerprint(uint32_t num_hashes, const void* data, size_t len,
                                        uint32_t* primary_hashes, fingerprint_t* fingerprint) {
    uint32_t hash1 = murmurhash3_32(data, len, 0xAAAAAAAA);
    uint32_t hash2 = murmurhash3_32(data, len, 0xBBBBBBBB);

    // Fingerprint is derived from hash2 and cannot be 0.
    *fingerprint = (fingerprint_t)(hash2 & 0xFF);
    if (*fingerprint == 0) {
        *fingerprint = 1;
    }

    for (uint32_t i = 0; i < num_hashes; ++i) {
        primary_hashes[i] = hash1 + i * hash2;
    }
}

ebf_t* ebf_create(uint32_t rows, uint32_t cols, uint32_t num_hashes, uint32_t max_probes) {
    if (rows == 0 || cols == 0 || num_hashes == 0) {
        return NULL;
    }
    ebf_t* filter = (ebf_t*)malloc(sizeof(ebf_t));
    if (!filter) return NULL;

    filter->rows = rows;
    filter->cols = cols;
    filter->num_hashes = num_hashes;
    filter->max_probes = max_probes;
    filter->elements = 0;

    size_t total_cells = (size_t)rows * cols;
    filter->filter_data = (fingerprint_t*)calloc(total_cells, sizeof(fingerprint_t));
    if (!filter->filter_data) {
        free(filter);
        return NULL;
    }
    return filter;
}

void ebf_destroy(ebf_t* filter) {
    if (filter) {
        free(filter->filter_data);
        free(filter);
    }
}

int ebf_add(ebf_t* filter, const void* data, size_t len) {
    fingerprint_t fp;
    uint32_t* primary_hashes = (uint32_t*)malloc(filter->num_hashes * sizeof(uint32_t));
    if (!primary_hashes) return -1;

    _get_hashes_and_fingerprint(filter->num_hashes, data, len, primary_hashes, &fp);

    size_t total_cells = (size_t)filter->rows * filter->cols;
    int success_count = 0;

    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        uint32_t primary_index = primary_hashes[i] % total_cells;
        int inserted = 0;

        for (uint32_t probe = 0; probe < filter->max_probes; ++probe) {
            size_t current_index = (primary_index + probe) % total_cells;
            fingerprint_t existing_fp = filter->filter_data[current_index];

            // If the slot is empty or already contains our fingerprint, we can insert.
            if (existing_fp == 0 || existing_fp == fp) {
                filter->filter_data[current_index] = fp;
                inserted = 1;
                break;
            }
        }
        if (inserted) {
            success_count++;
        }
    }

    free(primary_hashes);
    if (success_count == filter->num_hashes) {
        filter->elements++;
        return 0; // Success
    }
    return -1; // Failure (filter is too full)
}

int ebf_check(const ebf_t* filter, const void* data, size_t len) {
    fingerprint_t fp;
    uint32_t* primary_hashes = (uint32_t*)malloc(filter->num_hashes * sizeof(uint32_t));
    if (!primary_hashes) return 0;

    _get_hashes_and_fingerprint(filter->num_hashes, data, len, primary_hashes, &fp);

    size_t total_cells = (size_t)filter->rows * filter->cols;
    int hashes_found = 0;

    for (uint32_t i = 0; i < filter->num_hashes; ++i) {
        uint32_t primary_index = primary_hashes[i] % total_cells;
        int found_this_hash = 0;

        for (uint32_t probe = 0; probe < filter->max_probes; ++probe) {
            size_t current_index = (primary_index + probe) % total_cells;
            fingerprint_t existing_fp = filter->filter_data[current_index];

            if (existing_fp == fp) {
                found_this_hash = 1;
                break; // Found it for this hash path
            }
            if (existing_fp == 0) {
                break; // Path ends here, it's not here
            }
        }

        if (found_this_hash) {
            hashes_found++;
        } else {
            // If even one hash path doesn't contain the fingerprint, it's a definitive "no".
            free(primary_hashes);
            return 0;
        }
    }

    free(primary_hashes);
    // It's a "maybe" if the fingerprint was found along all k paths.
    return (hashes_found == filter->num_hashes);
}
