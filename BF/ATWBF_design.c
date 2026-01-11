#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>

// 哈希函数数量
#define HASH_COUNT 3

// 默认Bloom Filter大小
#define DEFAULT_BLOOM_SIZE 1024

// 默认时间窗口大小
#define DEFAULT_TIME_WINDOW 3

// 默认阈值比例(表示当有80%的计数器被使用时，该列Bloom Filter被视为饱和)
#define DEFAULT_THRESHOLD_RATIO 0.8

//测试用例数量
#define TEST_SIZE 60

// 计数器类型
typedef uint8_t counter_t;

typedef struct time_window
{

    //更新时间
    int up_time;

    //窗口大小
    int size;

    // 时间窗口位置
    int time_position_front;
    int time_position_end;

    //窗口当前活跃位置
    int current_time_position;

}Time_Win;

// 改进型Bloom Filter结构
typedef struct {
    // 时间窗口大小 T 
    int time_window_size;
    
    // 标记是否处于扩展状态 (如果为1，则下次窗口更新时会减少窗口数量)
    int is_expanded;
    
    // Bloom Filter大小 B
    int bloom_size;
    
    // 阈值比例
    float threshold_ratio;
    
    // 当前活跃时间窗口位置
    //int current_time_position;

    //时间窗口
    Time_Win* tw;
    
    // 二维Bloom Filter: TBF[T][B+1]
    // 其中T代表时间窗口，B代表Bloom Filter大小
    // 每列Bloom Filter的第一个元素为阈值过滤器，记录该列中非零计数器的个数
    counter_t **filter;
    
    // 时间窗口范围(秒)
    int time_range;
} TimeBloomFilter;

// 函数声明
int reduce_window_size(TimeBloomFilter* tbf);
int slide_time_window(TimeBloomFilter* tbf);
int expand_time_window(TimeBloomFilter* tbf);
int should_expand_window(TimeBloomFilter* tbf);
void check_and_update_time_window(TimeBloomFilter* tbf);

// 哈希函数声明
uint32_t hash_djb2(const void *key, size_t length);
uint32_t hash_sdbm(const void *key, size_t length);
uint32_t hash_fnv1a(const void *key, size_t length);

// 哈希函数实现
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

// 创建改进型Bloom Filter
TimeBloomFilter* create_time_bloom_filter(int bloom_size, int time_window_size, float threshold_ratio, int time_range) {
    TimeBloomFilter* tbf = (TimeBloomFilter*)malloc(sizeof(TimeBloomFilter));
    if (!tbf) return NULL;
    
    // 初始化参数
    tbf->bloom_size = bloom_size > 0 ? bloom_size : DEFAULT_BLOOM_SIZE;
    tbf->time_window_size = time_window_size > 0 ? time_window_size : DEFAULT_TIME_WINDOW;
    //tbf->default_window_size = DEFAULT_TIME_WINDOW; // 保证默认窗口大小固定
    tbf->is_expanded = 0;
    tbf->threshold_ratio = threshold_ratio > 0 ? threshold_ratio : DEFAULT_THRESHOLD_RATIO;
    tbf->time_range = time_range > 0 ? time_range : 60; // 默认60秒

    //初始化时间窗口
    tbf->tw = (Time_Win*)malloc(sizeof(Time_Win));
    //时间窗口大小为3
    tbf->tw->size = DEFAULT_TIME_WINDOW;
    tbf->tw->time_position_front = DEFAULT_TIME_WINDOW - 1;
    tbf->tw->time_position_end = 0;
    
    // 分配二维数组空间
    tbf->filter = (counter_t**)malloc(tbf->time_window_size * sizeof(counter_t*));
    if (!tbf->filter) {
        free(tbf);
        return NULL;
    }
    
    // 为每个时间窗口分配Bloom Filter空间(包括阈值过滤器)
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
   
    // 初始化创建时间
    tbf->tw->up_time = time(NULL);
    
    return tbf;
}

// 检查是否需要扩展时间窗口
// 返回值：
//          >= 0 不需要扩展
//          -1 需要扩展
int should_expand_window(TimeBloomFilter* tbf) {
    // 计算非饱和的Bloom Filter数量
    int pos = tbf->tw->time_position_end;
    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);

    //查询当前filter下的阈值计数器
    for(int i = 0; i < tbf->tw->size; i++){
        if (tbf->filter[pos+i][0] < threshold) {
            return pos + i;
        }
    }
    return -1;
}

// 扩展时间窗口
int expand_time_window(TimeBloomFilter* tbf) {
    // 增加时间窗口大小
    int new_size = tbf->tw->size + 1;
    int next_pos_front = tbf->tw->time_position_front + 1;
    
    // 初始化新创建的时间窗口
    memset(tbf->filter[next_pos_front], 0 ,(tbf->bloom_size + 1) * sizeof(counter_t));
     
    // 更新窗口时间
    tbf->tw->up_time = time(NULL);
    
    // 设置扩展状态标志
    tbf->is_expanded = 1;
    
    // 更新时间窗口大小
    tbf->tw->size = new_size;
    
    // 将当前位置移动到新窗口
    tbf->tw->current_time_position = next_pos_front;

    // 移动当前时间窗口前指针
    tbf->tw->time_position_front = next_pos_front;
    
    return 0;
}

// 根据是否扩展过窗口选择不同的窗口缩减策略
int reduce_time_window(TimeBloomFilter* tbf) {
    // 检查是否真的需要减少窗口
    if (!tbf->is_expanded) {
        // 如果没有扩展过，使用滑动窗口
        return slide_time_window(tbf);
    } 
    else {
        // 扩展过大小，缩减
        return reduce_window_size(tbf);
    }
}

// 仅缩减窗口大小，不滑动（用于扩展状态下的窗口更新）
int reduce_window_size(TimeBloomFilter* tbf) {
    if (tbf->tw->size <= DEFAULT_TIME_WINDOW) {
        // 已经是默认大小或更小，不需要减少，应该使用滑动窗口
        return slide_time_window(tbf);
    }
    
    // 释放最旧窗口
    memset(tbf->filter[tbf->tw->time_position_end], 0 ,(tbf->bloom_size + 1) * sizeof(counter_t));
    
    // 减小时间窗口大小
    tbf->tw->size--;
    
    tbf->tw->time_position_end = (tbf->tw->time_position_end + 1) % tbf->time_window_size;
    
    // 检查是否已经回到默认窗口大小，如果是则清除扩展标志
    if (tbf->tw->size <= DEFAULT_TIME_WINDOW) {
        tbf->is_expanded = 0;
    }

    //更新时间
    tbf->tw->up_time = time(NULL);
    
    return 0;
}

// 滑动时间窗口(删除最旧的窗口并添加新窗口)
// 返回值：
//          -1表示窗口出现问题；
//           0表示正常；
int slide_time_window(TimeBloomFilter* tbf) {
    if (tbf->time_window_size < 1) return -1; // 至少需要一个窗口
    
    // 找到最旧的窗口位置
    int oldest_pos = tbf->tw->time_position_end;
    int new_pos = tbf->tw->time_position_front;
    
    // 清除最旧窗口的计数器（重置为0）而不是释放内存
    memset(tbf->filter[oldest_pos], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    memset(tbf->filter[new_pos], 0, (tbf->bloom_size + 1) * sizeof(counter_t));
    
    // 更新该窗口的创建时间为当前时间
    tbf->tw->up_time = time(NULL);

    //窗口向前滑动
    tbf->tw->time_position_end = (oldest_pos + 1) % tbf->time_window_size;
    tbf->tw->time_position_front = (new_pos + 1) % tbf->time_window_size;

    //更新活跃窗口
    tbf->tw->current_time_position = tbf->tw->time_position_front;

    return 0;
}

// 检查并更新时间窗口
void check_and_update_time_window(TimeBloomFilter* tbf) {
    time_t current_time = time(NULL);
    
    // 检查是否有窗口超出时间范围
    if (difftime(current_time, tbf->tw->up_time) > tbf->time_range) {
        // 窗口超时，需要根据扩展状态决定如何更新
        reduce_time_window(tbf); // 通过改进的reduce_time_window函数来决定适当的更新策略
    }

}

// 检查可用的Bloom Filter数量(二维数组中的BF)
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

// 找到可用的Bloom Filter位置
int find_available_filter(TimeBloomFilter* tbf) {
    return should_expand_window(tbf);
}

// 插入元素
int insert_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    // 检查并更新时间窗口
    check_and_update_time_window(tbf);

    int threshold = (int)(tbf->bloom_size * tbf->threshold_ratio);
    
    // 找到可用的Bloom Filter
    int filter_pos = find_available_filter(tbf);
    if(filter_pos < 0) {
        expand_time_window(tbf);
        filter_pos = tbf->tw->current_time_position;
    }
    else{
        tbf->tw->current_time_position = filter_pos;
    }
    
    
    // 计算哈希值
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;
    
    // 更新计数器
    for (int i = 0; i < HASH_COUNT; i++) {
        int pos = hash_vals[i] + 1; // +1是因为位置0存储的是非零计数器的个数
        
        // 如果该位置的计数器从0变为1，更新非零计数器的个数
        //printf("%d\n",)
        if (tbf->filter[filter_pos][pos] == 0) {
            tbf->filter[filter_pos][0]++;
        }
        
        // 增加计数器，但不超过最大值
        if (tbf->filter[filter_pos][pos] < UINT8_MAX) {
            tbf->filter[filter_pos][pos]++;
        }
    }
    
    return 0;
}

// 查询元素
int query_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    // 检查并更新时间窗口
    check_and_update_time_window(tbf);
    
    // 计算哈希值
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;
    
    // 在所有时间窗口中查找
    for (int t = 0; t < tbf->tw->size; t++) {
        int found = 1;
        for (int i = 0; i < HASH_COUNT; i++) {
            int pos = hash_vals[i] + 1;
            if (tbf->filter[tbf->tw->time_position_end + t][pos] == 0) {
                found = 0;
                break;
            }
        }
        if (found) return 1; // 在某个时间窗口找到了元素
    }
    
    return 0; // 未找到元素
}

// 删除元素
int remove_element(TimeBloomFilter* tbf, const void* data, size_t length) {
    // 检查并更新时间窗口
    check_and_update_time_window(tbf);
    
    // 计算哈希值
    uint32_t hash_vals[HASH_COUNT];
    hash_vals[0] = hash_djb2(data, length) % tbf->bloom_size;
    hash_vals[1] = hash_sdbm(data, length) % tbf->bloom_size;
    hash_vals[2] = hash_fnv1a(data, length) % tbf->bloom_size;
    
    // 在所有时间窗口中查找和删除
    for (int t = 0; t < tbf->tw->size; t++) {
        int can_remove = 1;
        
        // 检查所有哈希位置是否都有计数
        for (int i = 0; i < HASH_COUNT; i++) {
            int pos = hash_vals[i] + 1;
            if (tbf->filter[tbf->tw->time_position_end + t][pos] == 0) {
                can_remove = 0;
                break;
            }
        }
        
        if (can_remove) {
            // 减少计数器
            for (int i = 0; i < HASH_COUNT; i++) {
                int pos = hash_vals[i] + 1;
                tbf->filter[tbf->tw->time_position_end + t][pos]--;
                
                // 如果计数器变为0，更新非零计数器的个数
                if (tbf->filter[tbf->tw->time_position_end + t][pos] == 0) {
                    tbf->filter[tbf->tw->time_position_end + t][0]--;
                }
            }
            return 0; // 成功删除
        }
    }
    
    return -1; // 元素不存在，无法删除
}

// 打印Bloom Filter的当前状态（用于调试）
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
    //printf("\ntime:%ld\n",now);
    double age = difftime(now, tbf->tw->up_time);
    printf("Windows age: %.6f sec\n",age);
    printf("\n");

}

// 释放改进型Bloom Filter资源
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

// 模拟时间流逝，强制触发窗口滑动
void simulate_time_passing(TimeBloomFilter* tbf, int seconds) {
    // 修改所有时间窗口的创建时间，使其提前指定的秒数
    time_t current = time(NULL);
    
    tbf->tw->up_time = current - (tbf->time_range + seconds);
}

//示例使用代码（可选）
int main() {
    clock_t start, end;
    // 创建一个时间Bloom Filter
    TimeBloomFilter* tbf = create_time_bloom_filter(128, 30, 0.6, 300000);
    
    // 插入元素并测试
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
    
    // 查询元素
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
    
    // 打印状态
    //print_bloom_filter_status(tbf);
    
    // 测试窗口扩展
    // 通过插入大量元素使窗口饱和
    printf("==== 3、窗口扩展测试 ====\n");
    for (int i = 0; i < 130; i++) {
        char elem[16];
        sprintf(elem, "elem_%d", i);
        insert_element(tbf, elem, strlen(elem));
    }
    // 再次打印状态，观察窗口是否扩展
    print_bloom_filter_status(tbf);

    //删除元素
    printf("==== 4、元素删除测试 ====\n");
    for (int i = 0; i < 50; i++) {
        char elem[10];
        sprintf(elem, "elem_%d", i);
        remove_element(tbf, elem, strlen(elem));
    }

    //打印BF状态
    print_bloom_filter_status(tbf);

    //模拟窗口滑动
    printf("==== 5、窗口滑动测试 ====\n");
    printf("==== 5.1 策略1 窗口数量过大 ====\n");
    simulate_time_passing(tbf,30);
    check_and_update_time_window(tbf);
    print_bloom_filter_status(tbf);

    printf("==== 5.2 策略2 滑动窗口 ====\n");
    simulate_time_passing(tbf,30);
    check_and_update_time_window(tbf);
    print_bloom_filter_status(tbf);
    
    // 释放资源
    free_time_bloom_filter(tbf);
    
    return 0;
}

