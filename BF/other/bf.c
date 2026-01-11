#include "bf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define DEFAULT_TIME_WINDOW 3
#define DEFAULT_BLOOM_SIZE 1024
#define DEFAULT_THRESHOLD_RATIO 0.8f

static uint32_t hash_djb2(const void *key, size_t length) {
    uint32_t hash = 5381;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) hash = ((hash << 5) + hash) + str[i];
    return hash;
}

static uint32_t hash_sdbm(const void *key, size_t length) {
    uint32_t hash = 0;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) hash = str[i] + (hash << 6) + (hash << 16) - hash;
    return hash;
}

static uint32_t hash_fnv1a(const void *key, size_t length) {
    uint32_t hash = 2166136261U;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash ^= str[i];
        hash *= 16777619U;
    }
    return hash;
}

static void recompute_front(TimeBloomFilter* tbf) {
    if (!tbf || !tbf->tw) return;
    if (tbf->tw->size <= 0) {
        tbf->tw->time_position_front = (tbf->tw->time_position_end + tbf->time_window_size - 1) % tbf->time_window_size;
    } else {
        tbf->tw->time_position_front = (tbf->tw->time_position_end + tbf->tw->size - 1) % tbf->time_window_size;
    }
}


static int expand_time_window(TimeBloomFilter* tbf) {
    if (!tbf || !tbf->tw) return -1;
    if (tbf->tw->size >= tbf->time_window_size) return -1; // at capacity
    int new_index = (tbf->tw->time_position_end + tbf->tw->size) % tbf->time_window_size;
    memset(tbf->filter[new_index], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    tbf->tw->size++;
    recompute_front(tbf);
    tbf->is_expanded = 1;
    tbf->tw->up_time = time(NULL);
    return 0;
}

static int slide_time_window(TimeBloomFilter* tbf) {
    if (!tbf || !tbf->tw) return -1;
    if (tbf->time_window_size < 1) return -1;
    int oldest_pos = tbf->tw->time_position_end;
    memset(tbf->filter[oldest_pos], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    tbf->tw->time_position_end = (oldest_pos + 1) % tbf->time_window_size;
    recompute_front(tbf);
    tbf->tw->up_time = time(NULL);
    return 0;
}

static int reduce_window_size(TimeBloomFilter* tbf) {
    if (!tbf || !tbf->tw) return -1;
    if (tbf->tw->size <= 1) {
        return slide_time_window(tbf);
    }
    int oldest = tbf->tw->time_position_end;
    memset(tbf->filter[oldest], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    tbf->tw->time_position_end = (oldest + 1) % tbf->time_window_size;
    tbf->tw->size--;
    if (tbf->tw->size <= DEFAULT_TIME_WINDOW) {
        tbf->is_expanded = 0;
    }
    recompute_front(tbf);
    tbf->tw->up_time = time(NULL);
    return 0;
}

static int reduce_time_window(TimeBloomFilter* tbf) {
    if (!tbf) return -1;
    if (!tbf->is_expanded) return slide_time_window(tbf);
    return reduce_window_size(tbf);
}


TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range) {
    if (time_window_size <= 0) time_window_size = DEFAULT_TIME_WINDOW;
    if (bloom_size <= 0) bloom_size = DEFAULT_BLOOM_SIZE;
    if (threshold_ratio <= 0.0f) threshold_ratio = DEFAULT_THRESHOLD_RATIO;
    if (time_range <= 0) time_range = 60;

    TimeBloomFilter* tbf = (TimeBloomFilter*)calloc(1, sizeof(TimeBloomFilter));
    if (!tbf) return NULL;

    tbf->bloom_size = bloom_size;
    tbf->time_window_size = time_window_size;
    tbf->is_expanded = 0;
    tbf->threshold_ratio = threshold_ratio;
    tbf->time_range = time_range;
    tbf->saturation_threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);

    tbf->tw = (Time_Win*)calloc(1, sizeof(Time_Win));
    if (!tbf->tw) { free(tbf); return NULL; }

    int initial_logical = DEFAULT_TIME_WINDOW;
    if (initial_logical > tbf->time_window_size) initial_logical = tbf->time_window_size;
    if (initial_logical <= 0) initial_logical = 1;
    tbf->tw->size = initial_logical;
    tbf->tw->time_position_end = 0;
    recompute_front(tbf);

    tbf->filter = (counter_t**)malloc(tbf->time_window_size * sizeof(counter_t*));
    if (!tbf->filter) { free(tbf->tw); free(tbf); return NULL; }
    for (int i = 0; i < tbf->time_window_size; ++i) {
        tbf->filter[i] = (counter_t*)calloc(tbf->bloom_size + 1, sizeof(counter_t));
        if (!tbf->filter[i]) {
            for (int j = 0; j < i; ++j) free(tbf->filter[j]);
            free(tbf->filter); free(tbf->tw); free(tbf);
            return NULL;
        }
    }
    tbf->tw->up_time = time(NULL);
    return tbf;
}

void destroy_time_bloom_filter(TimeBloomFilter* tbf) {
    if (!tbf) return;
    if (tbf->tw) free(tbf->tw);
    if (tbf->filter) {
        for (int i = 0; i < tbf->time_window_size; ++i) {
            if (tbf->filter[i]) free(tbf->filter[i]);
        }
        free(tbf->filter);
    }
    free(tbf);
}

void check_and_update_time_window(TimeBloomFilter* tbf) {
    if (!tbf || !tbf->tw) return;
    time_t current_time = time(NULL);
    if (difftime(current_time, tbf->tw->up_time) > tbf->time_range) {
        reduce_time_window(tbf);
    }
}

int insert_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    if (!tbf || !tbf->tw || !data) return -1;
    check_and_update_time_window(tbf);
    if (tbf->tw->size == 0) return -1;

    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length);
    hash_vals[1] = hash_sdbm(data, length);
    hash_vals[2] = hash_fnv1a(data, length);

    for (int i = 0; i < HASH_COUNT; i++) {
        int col_pos = (hash_vals[i] % tbf->bloom_size) + 1;
        int logical_row_offset = (hash_vals[i] % tbf->tw->size);
        int physical_row_index = (tbf->tw->time_position_end + logical_row_offset) % tbf->time_window_size;

        if (tbf->filter[physical_row_index][col_pos] == 0) {
            if (tbf->filter[physical_row_index][0] < UINT8_MAX) tbf->filter[physical_row_index][0]++;
        }
        if (tbf->filter[physical_row_index][col_pos] < UINT8_MAX) {
            tbf->filter[physical_row_index][col_pos]++;
        }

        if (tbf->filter[physical_row_index][0] > tbf->saturation_threshold && !tbf->is_expanded) {
            expand_time_window(tbf);
        }
    }
    return 0;
}

int query_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    if (!tbf || !tbf->tw || !data) return 0;
    check_and_update_time_window(tbf);
    if (tbf->tw->size == 0) return 0;

    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length);
    hash_vals[1] = hash_sdbm(data, length);
    hash_vals[2] = hash_fnv1a(data, length);

    for (int i = 0; i < HASH_COUNT; i++) {
        int col_pos = (hash_vals[i] % tbf->bloom_size) + 1;
        int logical_row_offset = (hash_vals[i] % tbf->tw->size);
        int physical_row_index = (tbf->tw->time_position_end + logical_row_offset) % tbf->time_window_size;

        if (tbf->filter[physical_row_index][col_pos] == 0) {
            return 0; // definitely not present
        }
    }
    return 1;
}

int remove_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    if (!tbf || !tbf->tw || !data) return -1;
    check_and_update_time_window(tbf);
    if (tbf->tw->size == 0) return -1;

    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length);
    hash_vals[1] = hash_sdbm(data, length);
    hash_vals[2] = hash_fnv1a(data, length);
    
    int found_and_removed = 0;

    for (int i = 0; i < HASH_COUNT; i++) {
        int col_pos = (hash_vals[i] % tbf->bloom_size) + 1;
        int logical_row_offset = (hash_vals[i] % tbf->tw->size);
        int physical_row_index = (tbf->tw->time_position_end + logical_row_offset) % tbf->time_window_size;

        if (tbf->filter[physical_row_index][col_pos] > 0) {
            found_and_removed = 1; // At least one counter was positive
            tbf->filter[physical_row_index][col_pos]--;
            if (tbf->filter[physical_row_index][col_pos] == 0) {
                if (tbf->filter[physical_row_index][0] > 0) {
                    tbf->filter[physical_row_index][0]--;
                }
            }
        }
    }
    return found_and_removed ? 0 : -1;
}

void print_bloom_filter_status(TimeBloomFilter* tbf) {
    if (!tbf || !tbf->tw) {
        printf("TBF is NULL.\n");
        return;
    }
    printf("--- TBF Status ---\n");
    printf("  Bloom Size (B): %d | Time Window Capacity: %d\n", tbf->bloom_size, tbf->time_window_size);
    printf("  Logical Window Size: %d | Is Expanded: %s\n", tbf->tw->size, tbf->is_expanded ? "Yes" : "No");
    printf("  Oldest Window (end): %d | Newest Window (front): %d\n", tbf->tw->time_position_end, tbf->tw->time_position_front);
    printf("  Window Saturation Threshold: %d non-zero items (%.1f%%)\n", tbf->saturation_threshold, tbf->threshold_ratio * 100.0);
    printf("  Active Window Details:\n");
    for (int i = 0; i < tbf->tw->size; i++) {
        int physical_index = (tbf->tw->time_position_end + i) % tbf->time_window_size;
        printf("    - Logical Window %d (Physical Index %d): %u / %d non-zero items\n", i, physical_index, tbf->filter[physical_index][0], tbf->bloom_size);
    }
    printf("------------------\n");
}
