#ifndef BLOOM_FILTER_H
#define BLOOM_FILTER_H

#include <stdint.h>
#include <stddef.h>

// --- 编译时配置 ---
// 位数组的最大大小（字节）。
// 这定义了静态内存分配的上限。
#define BLOOM_FILTER_MAX_SIZE 3072

// 使用的哈希函数数量。
#define HASH_FUNC_COUNT 3

/**
 * @brief 初始化全局布隆过滤器实例。
 * @param size_in_bytes 此实例所需的位数组大小。
 * 不得超过 BLOOM_FILTER_MAX_SIZE。
 * @return 成功返回 0，失败（如大小无效）返回 -1。
 */
int bloom_init(size_t size_in_bytes);

/**
 * @brief 将一个元素（C字符串）添加到全局布隆过滤器中。
 * @param str 要添加的以 null 结尾的字符串。
 */
void bloom_add(const char *str);

/**
 * @brief 检查一个元素是否可能存在于全局布隆过滤器中。
 * @param str 要检查的以 null 结尾的字符串。
 * @return 1 如果元素可能存在（可能是误报）。
 * 0 如果元素绝对不存在。
 */
int bloom_contains(const char *str);

/**
 * @brief 打印关于全局布隆过滤器当前状态的统计信息。
 * （例如：大小、已设置位数、负载因子）。
 * 注意：此函数依赖于 xprintf 的实现。
 */
void bloom_stats(void);

#endif // BLOOM_FILTER_H
