#include "st_bf.h"
#include <string.h>
#include <stdio.h>

// --- 数据结构定义 ---
typedef struct {
    uint8_t bit_array[BLOOM_FILTER_MAX_SIZE];
    size_t size_in_bytes;
    size_t size_in_bits;
} BloomFilter;

// --- 全局静态布隆过滤器实例 ---
// API 函数操作的都是这一个单一实例。
static BloomFilter g_bloom_filter;

// --- 私有哈希函数 (static，仅在此文件内部可见) ---

// 哈希函数1 - DJB2算法
static uint32_t hash_djb2(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }
    return hash;
}

// 哈希函数2 - SDBM算法
static uint32_t hash_sdbm(const char *str) {
    uint32_t hash = 0;
    int c;
    while ((c = *str++)) {
        hash = c + (hash << 6) + (hash << 16) - hash;
    }
    return hash;
}

// 哈希函数3 - FNV-1a算法
static uint32_t hash_fnv1a(const char *str) {
    uint32_t hash = 0x811c9dc5;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 0x01000193;
    }
    return hash;
}

// --- 公共 API 函数实现 ---

int bloom_init(size_t size_in_bytes) {
    if (size_in_bytes == 0 || size_in_bytes > BLOOM_FILTER_MAX_SIZE) {
        return -1; // 无效大小
    }
    g_bloom_filter.size_in_bytes = size_in_bytes;
    g_bloom_filter.size_in_bits = size_in_bytes * 8;
    // 为安全起见，清空整个静态缓冲区，而不仅仅是使用的部分。
    memset(g_bloom_filter.bit_array, 0, sizeof(g_bloom_filter.bit_array));
    return 0;
}

void bloom_add(const char *str) {
    uint32_t hashes[HASH_FUNC_COUNT];
    hashes[0] = hash_djb2(str) % g_bloom_filter.size_in_bits;
    hashes[1] = hash_sdbm(str) % g_bloom_filter.size_in_bits;
    hashes[2] = hash_fnv1a(str) % g_bloom_filter.size_in_bits;

    for (int i = 0; i < HASH_FUNC_COUNT; i++) {
        uint32_t byte_index = hashes[i] / 8;
        uint8_t bit_index = hashes[i] % 8;
        g_bloom_filter.bit_array[byte_index] |= (1 << bit_index);
    }
}

int bloom_contains(const char *str) {
    uint32_t hashes[HASH_FUNC_COUNT];
    hashes[0] = hash_djb2(str) % g_bloom_filter.size_in_bits;
    hashes[1] = hash_sdbm(str) % g_bloom_filter.size_in_bits;
    hashes[2] = hash_fnv1a(str) % g_bloom_filter.size_in_bits;

    for (int i = 0; i < HASH_FUNC_COUNT; i++) {
        uint32_t byte_index = hashes[i] / 8;
        uint8_t bit_index = hashes[i] % 8;
        if ((g_bloom_filter.bit_array[byte_index] & (1 << bit_index)) == 0) {
            return 0; // 绝对不存在
        }
    }
    return 1; // 可能存在
}

void bloom_stats(void) {
    size_t set_bits = 0;
    for (size_t i = 0; i < g_bloom_filter.size_in_bytes; i++) {
        for (int j = 0; j < 8; j++) {
            if ((g_bloom_filter.bit_array[i] >> j) & 1) {
                set_bits++;
            }
        }
    }
    printf("--- Bloom Filter Stats ---\n");
    printf("Size: %u bytes (%u bits)\n", (unsigned)g_bloom_filter.size_in_bytes, (unsigned)g_bloom_filter.size_in_bits);
    printf("Set Bits: %u\n", (unsigned)set_bits);
    printf("Load Factor: %u%%\n", (unsigned)(set_bits * 100 / g_bloom_filter.size_in_bits));
}
