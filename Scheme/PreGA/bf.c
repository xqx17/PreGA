#include "./bf.h"
#include <string.h>     // For memset
#include <stdio.h>
#include "risc_time.h"// WCH Port: For printf

// =================================================================
//                 Private Constants
// =================================================================
#define DEFAULT_TIME_WINDOW 3
#define DEFAULT_THRESHOLD_RATIO 0.8

// =================================================================
//                 Global Static Instance of the Filter
// =================================================================
static TimeBloomFilter g_tbf;

// =================================================================
//                 Private Function Prototypes
// =================================================================
static int reduce_window_size(TimeBloomFilter* tbf);
static int slide_time_window(TimeBloomFilter* tbf);
static int expand_time_window(TimeBloomFilter* tbf);
static int should_expand_window(TimeBloomFilter* tbf);

static uint32_t hash_djb2(const void *key, size_t length);
static uint32_t hash_sdbm(const void *key, size_t length);
static uint32_t hash_fnv1a(const void *key, size_t length);


// =================================================================
//                 Hash Function Implementations
// =================================================================

static uint32_t hash_djb2(const void *key, size_t length) {
    uint32_t hash = 5381;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash % g_tbf.bloom_size; // Use bloom_size from the instance
}

static uint32_t hash_sdbm(const void *key, size_t length) {
    uint32_t hash = 0;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash = str[i] + (hash << 6) + (hash << 16) - hash;
    }
    return hash % g_tbf.bloom_size;
}

static uint32_t hash_fnv1a(const void *key, size_t length) {
    uint32_t hash = 2166136261;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }
    return hash % g_tbf.bloom_size;
}

double cycles_to_seconds_diff(uint64_t current, uint64_t previous) {
    if (current < previous) {
        return -1.0; 
    }
    uint64_t diff = current - previous;
    return (double)diff / 144000000;
}


// =================================================================
//                Public API Function Implementations
// =================================================================

TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range) {
    TimeBloomFilter* tbf = &g_tbf;

    tbf->bloom_size = (bloom_size > 0 && bloom_size <= BLOOM_SIZE) ? bloom_size : BLOOM_SIZE;
    tbf->time_window_size = (time_window_size > 0 && time_window_size <= TIME_WINDOW_SIZE) ? time_window_size : TIME_WINDOW_SIZE;
    tbf->is_expanded = 0;
    tbf->threshold_ratio = threshold_ratio > 0 ? threshold_ratio : DEFAULT_THRESHOLD_RATIO;
    tbf->time_range = time_range > 0 ? time_range : 60;

    tbf->tw.size = DEFAULT_TIME_WINDOW;
    tbf->tw.time_position_front = DEFAULT_TIME_WINDOW - 1;
    tbf->tw.time_position_end = 0;
    tbf->tw.current_time_position = tbf->tw.time_position_front;

    memset(tbf->filter, 0, sizeof(tbf->filter));
    // WCH Port: Use the high-precision cycle counter for the initial timestamp.
    tbf->tw.up_time = Get_counter();

    return tbf;
}

void check_and_update_time_window(TimeBloomFilter* tbf) {
    // WCH Port: Use the cycle counter and the corrected time diff function.
    uint64_t current_cycles = Get_counter ();
    
    if (cycles_to_seconds_diff(current_cycles, tbf->tw.up_time) > tbf->time_range) {
        if (!tbf->is_expanded) {
            slide_time_window(tbf);
        } else {
            reduce_window_size(tbf);
        }
    }
}

int insert_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    check_and_update_time_window(tbf);
    
    int filter_pos = should_expand_window(tbf);
    if(filter_pos < 0) {
        if (expand_time_window(tbf) != 0) {
            return -1; // Expansion failed
        }
        filter_pos = tbf->tw.current_time_position;
    } else {
        tbf->tw.current_time_position = filter_pos;
    }
    
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length);
    hash_vals[1] = hash_sdbm(data, length);
    hash_vals[2] = hash_fnv1a(data, length);
    
    for (int i = 0; i < HASH_COUNT; i++) {
        int pos = hash_vals[i] + 1;
        
        if (tbf->filter[filter_pos][pos] == 0) {
            tbf->filter[filter_pos][0]++;
        }
        
        if (tbf->filter[filter_pos][pos] < UINT8_MAX) {
            tbf->filter[filter_pos][pos]++;
        }
    }
    
    return 0;
}

int query_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    check_and_update_time_window(tbf);
    
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length);
    hash_vals[1] = hash_sdbm(data, length);
    hash_vals[2] = hash_fnv1a(data, length);
    
    for (int t = 0; t < tbf->tw.size; t++) {
        int current_filter_idx = (tbf->tw.time_position_end + t) % tbf->time_window_size;
        int found = 1;
        for (int i = 0; i < HASH_COUNT; i++) {
            int pos = hash_vals[i] + 1;
            if (tbf->filter[current_filter_idx][pos] == 0) {
                found = 0;
                break;
            }
        }
        if (found) return 1;
    }
    
    return 0;
}

int remove_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    check_and_update_time_window(tbf);
    
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length);
    hash_vals[1] = hash_sdbm(data, length);
    hash_vals[2] = hash_fnv1a(data, length);
    
    for (int t = 0; t < tbf->tw.size; t++) {
        int current_filter_idx = (tbf->tw.time_position_end + t) % tbf->time_window_size;
        int can_remove = 1;
        
        for (int i = 0; i < HASH_COUNT; i++) {
            int pos = hash_vals[i] + 1;
            if (tbf->filter[current_filter_idx][pos] == 0) {
                can_remove = 0;
                break;
            }
        }
        
        if (can_remove) {
            for (int i = 0; i < HASH_COUNT; i++) {
                int pos = hash_vals[i] + 1;
                tbf->filter[current_filter_idx][pos]--;
                
                if (tbf->filter[current_filter_idx][pos] == 0) {
                    tbf->filter[current_filter_idx][0]--;
                }
            }
            return 0;
        }
    }
    
    return -1;
}

void print_bloom_filter_status(TimeBloomFilter* tbf) {
    // WCH Port: Changed xprintf to printf and activated the debug output.
    printf("Time Bloom Filter Status:\r\n");
    printf("  Total Window Size (T): %d (current: %d)\r\n", tbf->time_window_size, tbf->tw.size);
    printf("  Is Expanded: %s\r\n", tbf->is_expanded ? "Yes" : "No");
    printf("  Current Position: %d\r\n", tbf->tw.current_time_position);
    
    printf("  Filter Usage:\r\n");
    for (int i = 0; i < tbf->tw.size; i++) {
        int current_filter_idx = (tbf->tw.time_position_end + i) % tbf->time_window_size;
        printf("    [%d] Count: %d/%d %s\r\n",
               current_filter_idx, tbf->filter[current_filter_idx][0], tbf->bloom_size,
               (current_filter_idx == tbf->tw.current_time_position) ? "(Current)" : "");
    }

    // WCH Port: Correctly calculate and print the age in seconds.
    uint64_t now_cycles = Get_counter();
    double age = cycles_to_seconds_diff(now_cycles, tbf->tw.up_time);
    printf("  Windows age: %d sec\r\n\r\n", (int)age);
}

// =================================================================
//              Private Helper Function Implementations
// =================================================================

static int should_expand_window(TimeBloomFilter* tbf) {
    int pos = tbf->tw.time_position_end;
    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);

    for(int i = 0; i < tbf->tw.size; i++){
        int current_idx = (pos + i) % tbf->time_window_size;
        if (tbf->filter[current_idx][0] < threshold) {
            return current_idx;
        }
    }
    return -1;
}

static int expand_time_window(TimeBloomFilter* tbf) {
    if (tbf->tw.size >= tbf->time_window_size) {
        return -1; // Cannot expand further
    }
    
    int new_size = tbf->tw.size + 1;
    int next_pos_front = (tbf->tw.time_position_front + 1) % tbf->time_window_size;
    
    memset(tbf->filter[next_pos_front], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
        
    // WCH Port: Update timestamp using cycle counter.
    tbf->tw.up_time = Get_counter();
    tbf->is_expanded = 1;
    tbf->tw.size = new_size;
    tbf->tw.current_time_position = next_pos_front;
    tbf->tw.time_position_front = next_pos_front;
    
    return 0;
}

static int reduce_window_size(TimeBloomFilter* tbf) {
    if (tbf->tw.size <= DEFAULT_TIME_WINDOW) {
        tbf->is_expanded = 0;
        return slide_time_window(tbf);
    }
    
    memset(tbf->filter[tbf->tw.time_position_end], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    
    tbf->tw.size--;
    tbf->tw.time_position_end = (tbf->tw.time_position_end + 1) % tbf->time_window_size;
    
    if (tbf->tw.size <= DEFAULT_TIME_WINDOW) {
        tbf->is_expanded = 0;
    }

    // WCH Port: Update timestamp using cycle counter.
    tbf->tw.up_time = Get_counter();
    
    return 0;
}

static int slide_time_window(TimeBloomFilter* tbf) {
    if (tbf->time_window_size < 1) return -1;
    
    int oldest_pos = tbf->tw.time_position_end;
    
    memset(tbf->filter[oldest_pos], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    
    // WCH Port: Update timestamp using cycle counter.
    tbf->tw.up_time = Get_counter();

    tbf->tw.time_position_end = (oldest_pos + 1) % tbf->time_window_size;
    tbf->tw.time_position_front = (tbf->tw.time_position_front + 1) % tbf->time_window_size;
    tbf->tw.current_time_position = tbf->tw.time_position_front;

    return 0;
}
