#ifndef TBF_H
#define TBF_H

#include <stdint.h>
#include <stddef.h> 
#include <time.h> 

#define HASH_COUNT 3
#define BLOOM_SIZE 1024
#define TIME_WINDOW_SIZE 5


typedef uint8_t counter_t;

typedef struct time_window {
    time_t up_time;
    int size;
    int time_position_front;
    int time_position_end;
    int current_time_position; 
} Time_Win;

struct TimeBloomFilter {
    int time_window_size;
    int is_expanded;
    int bloom_size;
    float threshold_ratio;
    int saturation_threshold; 
    int time_range;
    Time_Win *tw;
    counter_t **filter;
};


typedef struct TimeBloomFilter TimeBloomFilter;


TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range);
void destroy_time_bloom_filter(TimeBloomFilter* tbf); 
int insert_element(TimeBloomFilter* tbf, const void* data, size_t length);
int query_element(TimeBloomFilter* tbf, const void* data, size_t length);
int remove_element(TimeBloomFilter* tbf, const void* data, size_t length);
void check_and_update_time_window(TimeBloomFilter* tbf);
void print_bloom_filter_status(TimeBloomFilter* tbf);

#endif // TBF_H
