#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

#define HASH_COUNT 3
#define DEFAULT_BLOOM_SIZE 1024
#define DEFAULT_TIME_WINDOW 3
#define DEFAULT_THRESHOLD_RATIO 0.8

#define GET_IDX(tbf, idx) ((idx) % (tbf)->time_window_size)

typedef uint8_t counter_t;

typedef struct time_window {
    int size;                  
    int time_position_front;   
    int time_position_end;      
    int current_time_position;  
} Time_Win;

typedef struct {
    int time_window_size;       
    int bloom_size;             
    float threshold_ratio;     
    int time_range;             
    
    Time_Win* tw;            
    counter_t **filter;       
    
    int *load_counts;           
    
    time_t *window_timestamps;  
} TimeBloomFilter;

uint32_t hash_djb2(const void *key, size_t length);
uint32_t hash_sdbm(const void *key, size_t length);
uint32_t hash_fnv1a(const void *key, size_t length);

uint32_t hash_djb2(const void *key, size_t length) {
    uint32_t hash = 5381;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) hash = ((hash << 5) + hash) + str[i];
    return hash;
}
uint32_t hash_sdbm(const void *key, size_t length) {
    uint32_t hash = 0;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) hash = str[i] + (hash << 6) + (hash << 16) - hash;
    return hash;
}
uint32_t hash_fnv1a(const void *key, size_t length) {
    uint32_t hash = 2166136261;
    const unsigned char *str = (const unsigned char *)key;
    for (size_t i = 0; i < length; i++) {
        hash ^= str[i];
        hash *= 16777619;
    }
    return hash;
}

TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range) {
    TimeBloomFilter* tbf = (TimeBloomFilter*)malloc(sizeof(TimeBloomFilter));
    if (!tbf) return NULL;
    
    tbf->bloom_size = bloom_size > 0 ? bloom_size : DEFAULT_BLOOM_SIZE;
    tbf->time_window_size = time_window_size > 0 ? time_window_size : DEFAULT_TIME_WINDOW;
    tbf->threshold_ratio = threshold_ratio > 0 ? threshold_ratio : DEFAULT_THRESHOLD_RATIO;
    tbf->time_range = time_range > 0 ? time_range : 60;

    tbf->tw = (Time_Win*)malloc(sizeof(Time_Win));
    tbf->tw->size = 1; 
    tbf->tw->time_position_front = 0;
    tbf->tw->time_position_end = 0;
    tbf->tw->current_time_position = 0;
    
    tbf->filter = (counter_t**)malloc(tbf->time_window_size * sizeof(counter_t*));
    tbf->load_counts = (int*)calloc(tbf->time_window_size, sizeof(int));
    tbf->window_timestamps = (time_t*)calloc(tbf->time_window_size, sizeof(time_t));
    
    if (!tbf->filter || !tbf->load_counts || !tbf->window_timestamps) return NULL;

    for (int i = 0; i < tbf->time_window_size; i++) {
        tbf->filter[i] = (counter_t*)calloc(tbf->bloom_size, sizeof(counter_t)); 
    }

    tbf->window_timestamps[0] = time(NULL);
    
    return tbf;
}

void reset_window(TimeBloomFilter* tbf, int idx) {
    memset(tbf->filter[idx], 0, tbf->bloom_size * sizeof(counter_t));
    tbf->load_counts[idx] = 0;
    tbf->window_timestamps[idx] = time(NULL);
}

void force_slide_window(TimeBloomFilter* tbf) {
    int old_end = tbf->tw->time_position_end;
    tbf->tw->time_position_end = GET_IDX(tbf, old_end + 1);
}

void check_and_cleanup_windows(TimeBloomFilter* tbf) {
    time_t now = time(NULL);

    while (tbf->tw->size > 0) {
        int end_idx = tbf->tw->time_position_end;
        double age = difftime(now, tbf->window_timestamps[end_idx]);
        
        if (age > tbf->time_range) {
            tbf->tw->time_position_end = GET_IDX(tbf, end_idx + 1);
            tbf->tw->size--;
            
            if (tbf->tw->size == 0) {
                tbf->tw->time_position_front = 0;
                tbf->tw->time_position_end = 0;
                tbf->tw->size = 1;
                reset_window(tbf, 0); 
                break; 
            }
        } else {
            break;
        }
    }
}


int is_current_window_full(TimeBloomFilter* tbf) {
    int curr = tbf->tw->current_time_position;
    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);
    return (tbf->load_counts[curr] >= threshold);
}

int insert_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    check_and_cleanup_windows(tbf);

    if (is_current_window_full(tbf)) {
        if (tbf->tw->size < tbf->time_window_size) {

            int next_front = GET_IDX(tbf, tbf->tw->time_position_front + 1);
            reset_window(tbf, next_front); 
            
            tbf->tw->time_position_front = next_front;
            tbf->tw->current_time_position = next_front;
            tbf->tw->size++;
        } else {
            force_slide_window(tbf); 
            
            int next_front = GET_IDX(tbf, tbf->tw->time_position_front + 1);
            reset_window(tbf, next_front);
            
            tbf->tw->time_position_front = next_front;
            tbf->tw->current_time_position = next_front;
        }
    }

    int curr = tbf->tw->current_time_position;
    
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;
    
    for (int i = 0; i < HASH_COUNT; i++) {
        int pos = hash_vals[i];
        if (tbf->filter[curr][pos] == 0) {
            tbf->load_counts[curr]++; 
        }
        if (tbf->filter[curr][pos] < UINT8_MAX) {
            tbf->filter[curr][pos]++;
        }
    }
    
    return 0;
}

int query_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    check_and_cleanup_windows(tbf);
    
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;
    
    for (int i = 0; i < tbf->tw->size; i++) {
        int idx = GET_IDX(tbf, tbf->tw->time_position_end + i);
        
        int found = 1;
        for (int h = 0; h < HASH_COUNT; h++) {
            if (tbf->filter[idx][hash_vals[h]] == 0) {
                found = 0;
                break;
            }
        }
        if (found) return 1;
    }
    return 0;
}

void free_time_bloom_filter(TimeBloomFilter* tbf) {
    if(!tbf) return;
    for(int i=0; i<tbf->time_window_size; i++) free(tbf->filter[i]);
    free(tbf->filter);
    free(tbf->load_counts);
    free(tbf->window_timestamps);
    free(tbf->tw);
    free(tbf);
}

void manual_age_windows(TimeBloomFilter* tbf, int seconds) {
    for(int i = 0; i < tbf->time_window_size; i++) {
        tbf->window_timestamps[i] -= seconds;
    }
}

void test_burst_traffic() {
    printf("==== Burst Traffic Window Dynamics Test ====\n");
    printf("Settings: BloomSize=50000, MaxWindows=20, TimeRange=1\n");
    printf("Duration: 30 time units\n\n");
    printf("Time(T)\tMsgs(N)\tWinCount\tTrend\n");
    printf("-------------------------------------------------------------\n");

    TimeBloomFilter* tbf = create_time_bloom_filter(50000, 20000, 0.8, 1);
    
    srand(time(NULL));

    int last_size = tbf->tw->size;
    int n_msgs = 0;
    int last_msg = 0;

    for (int t = 1; t <= 30; t++) {
        manual_age_windows(tbf, 1);

        if(t < 15){
            if (abs((n_msgs-last_msg)) < 50000) {
                n_msgs = 50000 + (rand() % 30000); 
            } else {
                n_msgs = 100 + (rand() % 400);
                last_msg = n_msgs;
            }
        } else {
            n_msgs = 60000 + (rand() % 20000);
        }

        for (int i = 0; i < n_msgs; i++) {
            char key[64];
            sprintf(key, "time_%d_msg_%d", t, i);
            insert_element(tbf, key, strlen(key));
        }

        int current_size = tbf->tw->size;
        
        printf("%02d\t%d\t\t%d\t\t", t, n_msgs, current_size);
        
        printf("[");
        int max_bar = 20; 
        for(int k=0; k<current_size && k<max_bar; k++) printf("#");
        for(int k=current_size; k<max_bar; k++) printf(" ");
        printf("] ");

        if (current_size > last_size) printf("(EXPAND)");
        else if (current_size < last_size) printf("(SHRINK)");
        else if (n_msgs > 50000 && current_size == 20) printf("(FORCE CYCLE)");
        
        printf("\n");

        last_size = current_size;
    }

    printf("-------------------------------------------------------------\n");
    free_time_bloom_filter(tbf);
}

int main() {
    test_burst_traffic();
    return 0;

}
