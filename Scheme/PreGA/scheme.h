#ifndef SCHEME_H
#define SCHEME_H

#include <stdio.h>
#include <stdint.h>
#include <stddef.h> // for size_t
#include "sm3.h"    // 替换为 SM3
#include "bf.h"     // [新增] 引入布隆过滤器

// --- 常量定义 ---
#define HASH_LEN 32                     // SM3 哈希长度 (32 字节)
#define NUM_SUBKEYS 16                  // 秘密密钥向量的长度n
#define MAX_GROUP_SIZE 10               // 边缘节点支持的最大批处理数量

// --- 核心数据结构 ---

// 密钥向量 (SK)
typedef struct {
    uint8_t key[NUM_SUBKEYS][HASH_LEN];
} SecretKeyVector;

// 时间向量 (C)
typedef struct {
    uint8_t c[NUM_SUBKEYS][HASH_LEN];
} TimeVector;

// 物联网设备
typedef struct {
    int id;
    uint8_t system_master_key[HASH_LEN];
    SecretKeyVector sk;
    TimeVector time_vec;
} IoTDevice;

// 匿名签名 (Sig)
typedef struct {
    uint8_t sig[NUM_SUBKEYS][HASH_LEN];
} Signature;

// 消息元组
typedef struct {
    char message[128];
    uint32_t timestamp; // 使用 SysTick 计数器
    uint32_t nonce;     // 使用 TRNG 生成
    Signature signature;
    uint8_t contribution_value[HASH_LEN]; // C_i
} MessageTuple;

// 边缘节点 [修改]
typedef struct {
    // 移除旧的数组，改为持有布隆过滤器指针
    // uint8_t group_verification_points[100][HASH_LEN];
    TimeBloomFilter* tbf;
} EdgeNode;


// --- 函数接口声明 ---

void print_hash(const uint8_t* hash);

void ta_initialize_system(uint8_t* master_key_out);

void ta_register_device(IoTDevice* device, int id, const uint8_t* master_key);

// [修改] 参数改为 EdgeNode* node
void ta_precompute_group_point(EdgeNode* node, IoTDevice* devices, int num_devices, const uint8_t* master_key);

void device_evolve_keys(IoTDevice* device);

void device_sign_message(MessageTuple* tuple_out, IoTDevice* device, const char* message);

int edgenode_batch_verify(const EdgeNode* node, const MessageTuple* batch, int batch_size);

#endif // SCHEME_H
