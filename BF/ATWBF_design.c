#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define HASH_COUNT 3

#define DEFAULT_BLOOM_SIZE 1024

#define DEFAULT_TIME_WINDOW 3

#define DEFAULT_THRESHOLD_RATIO 0.8

#define TEST_SIZE 60

typedef uint8_t counter_t;

typedef struct time_window
{
    int up_time;

    int size;

    int time_position_front;
    int time_position_end;

    int current_time_position;

}Time_Win;

typedef struct {
    int time_window_size;

    int is_expanded;
    
    int bloom_size;
    
    float threshold_ratio;
    
    Time_Win* tw;
    
    counter_t **filter;
    
    int time_range;
} TimeBloomFilter;

int reduce_window_size(TimeBloomFilter* tbf);
int slide_time_window(TimeBloomFilter* tbf);
int expand_time_window(TimeBloomFilter* tbf);
int should_expand_window(TimeBloomFilter* tbf);
void check_and_update_time_window(TimeBloomFilter* tbf);

uint32_t hash_djb2(const void *key, size_t length);
uint32_t hash_sdbm(const void *key, size_t length);
uint32_t hash_fnv1a(const void *key, size_t length);

uint32_t hash_djb2(const void *key, size_t length) {
    uint32_t hash = 5381;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash = ((hash << 5) + hash) + str[i];
    }
    return hash % DEFAULT_BLOOM_SIZE;
}

uint32_t hash_sdbm(const void *key, size_t length) {
    uint32_t hash = 0;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash = str[i] + (hash << 6) + (hash << 16) - hash;
    }
    return hash % DEFAULT_BLOOM_SIZE;
}

uint32_t hash_fnv1a(const void *key, size_t length) {
    uint32_t hash = 2166136261;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }
    return hash % DEFAULT_BLOOM_SIZE;
}

TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range) {
    TimeBloomFilter* tbf = (TimeBloomFilter*)malloc(sizeof(TimeBloomFilter));
    if (!tbf) return NULL;
    
    tbf->bloom_size = bloom_size > 0 ? bloom_size : DEFAULT_BLOOM_SIZE;
    tbf->time_window_size = time_window_size > 0 ? time_window_size : DEFAULT_TIME_WINDOW;
    //tbf->default_window_size = DEFAULT_TIME_WINDOW; 
    tbf->is_expanded = 0;
    tbf->threshold_ratio = threshold_ratio > 0 ? threshold_ratio : DEFAULT_THRESHOLD_RATIO;
    tbf->time_range = time_range > 0 ? time_range : 60; 

    tbf->tw = (Time_Win*)malloc(sizeof(Time_Win));

    tbf->tw->size = DEFAULT_TIME_WINDOW;
    tbf->tw->time_position_front = DEFAULT_TIME_WINDOW - 1;
    tbf->tw->time_position_end = 0;
    
    tbf->filter = (counter_t**)malloc(tbf->time_window_size * sizeof(counter_t*));
    if (!tbf->filter) {
        free(tbf);
        return NULL;
    }
    
    for (int i = 0; i < tbf->time_window_size; i++) {
        tbf->filter[i] = (counter_t*)calloc(tbf->bloom_size + 1, sizeof(counter_t));
        if (!tbf->filter[i]) {
            for (int j = 0; j < i; j++) {
                free(tbf->filter[j]);
            }
            free(tbf->filter);
            free(tbf);
            return NULL;
        }
    }   
   
    tbf->tw->up_time = time(NULL);
    
    return tbf;
}

int should_expand_window(TimeBloomFilter* tbf) {
    int pos = tbf->tw->time_position_end;
    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);

    for(int i = 0; i < tbf->tw->size; i++){
        if (tbf->filter[pos+i][0] < threshold) {
            return pos + i;
        }
    }
    return -1;
}

int expand_time_window(TimeBloomFilter* tbf) {
    int new_size = tbf->tw->size + 1;
    int next_pos_front = tbf->tw->time_position_front + 1;
    
    memset(tbf->filter[next_pos_front], 0 ,(tbf->bloom_size + 1) * sizeof(counter_t));
     
    tbf->tw->up_time = time(NULL);
    tbf->is_expanded = 1;
    tbf->tw->size = new_size;
    tbf->tw->current_time_position = next_pos_front;
    tbf->tw->time_position_front = next_pos_front;
    return 0;
}

int reduce_time_window(TimeBloomFilter* tbf) {
    if (!tbf->is_expanded) {
        return slide_time_window(tbf);
    } 
    else {
        return reduce_window_size(tbf);
    }
}

int reduce_window_size(TimeBloomFilter* tbf) {
    if (tbf->tw->size <= DEFAULT_TIME_WINDOW) {
        return slide_time_window(tbf);
    }
    
    memset(tbf->filter[tbf->tw->time_position_end], 0 ,(tbf->bloom_size + 1) * sizeof(counter_t));

    tbf->tw->size--;
    
    tbf->tw->time_position_end = (tbf->tw->time_position_end + 1) % tbf->time_window_size;
    
    if (tbf->tw->size <= DEFAULT_TIME_WINDOW) {
        tbf->is_expanded = 0;
    }

    tbf->tw->up_time = time(NULL);
    
    return 0;
}

int slide_time_window(TimeBloomFilter* tbf) {
    if (tbf->time_window_size < 1) return -1; 
    
    int oldest_pos = tbf->tw->time_position_end;
    int new_pos = tbf->tw->time_position_front;

    memset(tbf->filter[oldest_pos], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    memset(tbf->filter[new_pos], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    
    tbf->tw->up_time = time(NULL);

    tbf->tw->time_position_end = (oldest_pos + 1) % tbf->time_window_size;
    tbf->tw->time_position_front = (new_pos + 1) % tbf->time_window_size;

    tbf->tw->current_time_position = tbf->tw->time_position_front;

    return 0;
}

void check_and_update_time_window(TimeBloomFilter* tbf) {
    time_t current_time = time(NULL);
    
    if (difftime(current_time, tbf->tw->up_time) > tbf->time_range) {
        reduce_time_window(tbf); 
    }

}

int count_available_filters(TimeBloomFilter* tbf) {
    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);
    int count = 0;
    
    for (int i = 0; i < tbf->time_window_size; i++) {
        if (tbf->filter[i][0] < threshold) {
            count++;
        }
    }
    
    return count;
}

int find_available_filter(TimeBloomFilter* tbf) {
    return should_expand_window(tbf);
}


int insert_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    check_and_update_time_window(tbf);

    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);
    
    int filter_pos = find_available_filter(tbf);
    if(filter_pos < 0) {
        expand_time_window(tbf);
        filter_pos = tbf->tw->current_time_position;
    }
    else{
        tbf->tw->current_time_position = filter_pos;
    }
    
    
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;

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
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;
    
    for (int t = 0; t < tbf->tw->size; t++) {
        int found = 1;
        for (int i = 0; i < HASH_COUNT; i++) {
            int pos = hash_vals[i] + 1;
            if (tbf->filter[tbf->tw->time_position_end + t][pos] == 0) {
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
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;

    for (int t = 0; t < tbf->tw->size; t++) {
        int can_remove = 1;

        for (int i = 0; i < HASH_COUNT; i++) {
            int pos = hash_vals[i] + 1;
            if (tbf->filter[tbf->tw->time_position_end + t][pos] == 0) {
                can_remove = 0;
                break;
            }
        }
        
        if (can_remove) {
            for (int i = 0; i < HASH_COUNT; i++) {
                int pos = hash_vals[i] + 1;
                tbf->filter[tbf->tw->time_position_end + t][pos]--;
                
                if (tbf->filter[tbf->tw->time_position_end + t][pos] == 0) {
                    tbf->filter[tbf->tw->time_position_end + t][0]--;
                }
            }
            return 0; 
        }
    }
    
    return -1; 
}

void print_bloom_filter_status(TimeBloomFilter* tbf) {
    printf("Time Bloom Filter Status:\n");
    printf("Total Window Size (T): %d (current: %d)\n", tbf->time_window_size, tbf->tw->size);
    printf("Is Expanded: %s\n", tbf->is_expanded ? "Yes" : "No");
    printf("Current Position: %d\n", tbf->tw->current_time_position);
    
    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);
    int available_count = count_available_filters(tbf);
    
    printf("Total Available Filters: %d/%d (Threshold: %d)\n", 
           available_count, tbf->time_window_size, threshold);
    
    printf("Filter Usage:\n");
    for (int i = 0; i < tbf->tw->size; i++) {
        time_t now = time(NULL);
        double age = difftime(now, tbf->tw->up_time);
        
        printf("  [%d] Count: %d/%d,%.3f%s\n", 
               tbf->tw->time_position_end + i, tbf->filter[tbf->tw->time_position_end + i][0], tbf->bloom_size,
               age, (i == tbf->tw->current_time_position) ? "(Current)" : "");
    }
    time_t now = time(NULL);
    double age = difftime(now, tbf->tw->up_time);
    printf("Windows age: %.6f sec\n",age);
    printf("\n");

}

void free_time_bloom_filter(TimeBloomFilter* tbf) {
    if (!tbf) return;
    
    if (tbf->filter) {
        for (int i = 0; i < tbf->time_window_size; i++) {
            if (tbf->filter[i]) {
                free(tbf->filter[i]);
            }
        }
        free(tbf->filter);
    }
    
    free(tbf);
}

void simulate_time_passing(TimeBloomFilter* tbf, int seconds) {
    time_t current = time(NULL);
    
    tbf->tw->up_time = current - (tbf->time_range + seconds);
}

int main() {
    clock_t start, end;

    TimeBloomFilter* tbf = create_time_bloom_filter(128, 30, 0.6, 300000);
    
    printf("==== 1、插入元素测试 ====\n");
    char test_str[100][32];
    start = clock();
    for (int i = 0; i < TEST_SIZE;i++) {
        sprintf(test_str[i], "test_element_%d", i);
        insert_element(tbf, test_str[i], strlen(test_str[i]));
    }
    end = clock();
    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("运行时间: %.8f 秒\n", time_taken);


    print_bloom_filter_status(tbf);
    
    printf("==== 2、查询元素测试 ====\n");
    int result;
    start = clock();
    for (size_t i = 0; i < TEST_SIZE; i++) {
        result = query_element(tbf, test_str[i], strlen(test_str[i]));
        if(result == 0){
        	printf("Query result = %d false\n", result);
        } 
    }
    end = clock();
    time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("查询运行时间: %.8f 秒\n", time_taken);
    
    printf("==== 3、窗口扩展测试 ====\n");
    for (int i = 0; i < 130; i++) {
        char elem[16];
        sprintf(elem, "elem_%d", i);
        insert_element(tbf, elem, strlen(elem));
    }
    print_bloom_filter_status(tbf);

    printf("==== 4、元素删除测试 ====\n");
    for (int i = 0; i < 50; i++) {
        char elem[10];
        sprintf(elem, "elem_%d", i);
        remove_element(tbf, elem, strlen(elem));
    }

    print_bloom_filter_status(tbf);

    printf("==== 5、窗口滑动测试 ====\n");
    printf("==== 5.1 策略1 窗口数量过大 ====\n");
    simulate_time_passing(tbf,30);
    check_and_update_time_window(tbf);
    print_bloom_filter_status(tbf);

    printf("==== 5.2 策略2 滑动窗口 ====\n");
    simulate_time_passing(tbf,30);
    check_and_update_time_window(tbf);
    print_bloom_filter_status(tbf);
    
    free_time_bloom_filter(tbf);
    
    return 0;
}

