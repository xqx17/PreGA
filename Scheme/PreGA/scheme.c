#include "scheme.h"
#include "bf.h" 
#include <string.h>
#include <stdlib.h>


// --- WCH 平台驱动 & 库 ---
#include "ch32v30x.h"
#include "ch32v30x_rng.h"
#include "risc_time.h" 
#include "debug.h"

// --- 平台适配: 硬件真随机数生成器 (TRNG) ---
static void platform_trng(uint8_t *dest, unsigned size) {
    for (unsigned i = 0; i < size; i += 4) {
        // 等待 TRNG 数据就绪
        while (RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET);
        uint32_t random_word = RNG_GetRandomNumber();
        // 拷贝随机数 (注意处理不足4字节的情况)
        unsigned bytes_to_copy = (size - i < 4) ? (size - i) : 4;
        memcpy(dest + i, &random_word, bytes_to_copy);
    }
}

// 内部辅助函数：计算SM3哈希
static void compute_hash(uint8_t* hash_out, const uint8_t* data1, size_t len1, const uint8_t* data2, size_t len2) {
    SM3_CTX ctx;
    sm3_init(&ctx);
    sm3_update(&ctx, data1, len1);
    if (data2 != NULL && len2 > 0) {
        sm3_update(&ctx, data2, len2);
    }
    sm3_final(&ctx, hash_out);
}

void print_hash(const uint8_t* hash) {
    for (int i = 0; i < HASH_LEN; i++) {
        printf("%02x", hash[i]);
    }
}

void ta_initialize_system(uint8_t* master_key_out) {
    // 使用硬件 TRNG 生成主密钥
    platform_trng(master_key_out, HASH_LEN);
}

void ta_register_device(IoTDevice* device, int id, const uint8_t* master_key) {
    device->id = id;
    memcpy(device->system_master_key, master_key, HASH_LEN);
    // 使用硬件 TRNG 生成初始密钥向量 SK_i^0 和时间向量 C_i^0
    platform_trng((uint8_t*)device->sk.key, sizeof(device->sk.key));
    platform_trng((uint8_t*)device->time_vec.c, sizeof(device->time_vec.c));
}

void ta_precompute_group_point(EdgeNode* node, IoTDevice* devices, int num_devices, const uint8_t* master_key) {
    // 动态分配内存以支持不同批次大小
    uint8_t *aggregated_contributions = malloc(num_devices * HASH_LEN);
    if (!aggregated_contributions) {
        printf("Error: Failed to allocate memory for aggregated_contributions!\r\n");
        return;
    }

    // 遍历所有设备，计算其在当前时间步的贡献值
    for (int i = 0; i < num_devices; i++) {
        // 1. 模拟计算设备在当前时间步的密钥状态 (不修改原始设备状态)
        // TA 需要为时间步 t 计算密钥
        IoTDevice current_device_state = devices[i]; // 创建副本
        device_evolve_keys(&current_device_state);

        // 2. 计算其身份贡献值 C_i = H(H(sk_{i,1}^c), ..., H(sk_{i,n}^c))
        uint8_t *hashed_subkeys = malloc(NUM_SUBKEYS * HASH_LEN);
        if (!hashed_subkeys) {
            printf("Error: Failed to allocate memory for hashed_subkeys!\r\n");
            free(aggregated_contributions);
            return;
        }
        for (int j = 0; j < NUM_SUBKEYS; j++) {
            compute_hash(&hashed_subkeys[j * HASH_LEN], current_device_state.sk.key[j], HASH_LEN, NULL, 0);
        }

        uint8_t contribution_val[HASH_LEN];
        compute_hash(contribution_val, hashed_subkeys, NUM_SUBKEYS * HASH_LEN, NULL, 0);
        free(hashed_subkeys);

        // 3. 拼接所有设备的贡献值 (构造 C_agg)
        memcpy(&aggregated_contributions[i * HASH_LEN], contribution_val, HASH_LEN);
    }

    // 4. 计算最终的群体验证点 H_group = H(C_1 || C_2 || ... || C_k)
    uint8_t group_point[HASH_LEN];
    compute_hash(group_point, aggregated_contributions, num_devices * HASH_LEN, NULL, 0);

    // [关键修改] 5. 将群体验证点存入 TBF
    // 假设 EdgeNode 结构体中包含 TimeBloomFilter* tbf
    if (node && node->tbf) {
        insert_element(node->tbf, group_point, HASH_LEN);
        // printf("TA: Group verification point inserted into TBF.\r\n");
    } else {
        printf("Error: EdgeNode TBF not initialized!\r\n");
    }

    free(aggregated_contributions);
}


void device_evolve_keys(IoTDevice* device) {
    for (int j = 0; j < NUM_SUBKEYS; j++) {
        // C_{i,j}^t = H(s || C_{i,j}^{t-1}) [cite: 43]
        compute_hash(device->time_vec.c[j], device->system_master_key, HASH_LEN, device->time_vec.c[j], HASH_LEN);
        // sk_{i,j}^t = H(sk_{i,j}^{t-1} || C_{i,j}^t) [cite: 44]
        compute_hash(device->sk.key[j], device->sk.key[j], HASH_LEN, device->time_vec.c[j], HASH_LEN);
    }
}

void device_sign_message(MessageTuple* tuple_out, IoTDevice* device, const char* message) {
    // 1. 动态参数更新 (密钥演进) 
    device_evolve_keys(device);

    // 生成时间戳 T (使用SysTick计数器) 和 nonce R (使用TRNG)
    strncpy(tuple_out->message, message, sizeof(tuple_out->message) - 1);
    tuple_out->message[sizeof(tuple_out->message) - 1] = '\0';
    tuple_out->timestamp = Get_counter();
    uint32_t temp_nonce;
    platform_trng((uint8_t*)&temp_nonce, sizeof(temp_nonce));
    tuple_out->nonce = temp_nonce;

    // 2. 构造消息哈希根 t = H(M || T || R)
    SM3_CTX sha_ctx;
    sm3_init(&sha_ctx);
    sm3_update(&sha_ctx, (const uint8_t*)tuple_out->message, strlen(tuple_out->message));
    sm3_update(&sha_ctx, (const uint8_t*)&tuple_out->timestamp, sizeof(tuple_out->timestamp));
    sm3_update(&sha_ctx, (const uint8_t*)&tuple_out->nonce, sizeof(tuple_out->nonce));
    uint8_t t_hash[HASH_LEN];
    sm3_final(&sha_ctx, t_hash);

    // 3. 生成匿名签名 Sig 
    for (int j = 0; j < NUM_SUBKEYS; j++) {
        // 使用 t_hash 的比特位进行选择
        int bit = (t_hash[j] >> 7) & 1; // 简化处理：取每字节最高位作为 t_j
        if (bit == 1) { // t_j = 1, sigma = H(sk)
            compute_hash(tuple_out->signature.sig[j], device->sk.key[j], HASH_LEN, NULL, 0);
        } else { // t_j = 0, sigma = sk
            memcpy(tuple_out->signature.sig[j], device->sk.key[j], HASH_LEN);
        }
    }

    // 4. 计算身份贡献值 C_i = H(H(sk_{i,1}^c), ..., H(sk_{i,n}^c))
    uint8_t *hashed_subkeys = malloc(NUM_SUBKEYS * HASH_LEN);
     if (!hashed_subkeys) {
        printf("Error: Failed to allocate memory for hashed_subkeys in sign!\r\n");
        memset(tuple_out->contribution_value, 0, HASH_LEN);
        return;
    }
    for (int j = 0; j < NUM_SUBKEYS; j++) {
        compute_hash(&hashed_subkeys[j * HASH_LEN], device->sk.key[j], HASH_LEN, NULL, 0);
    }
    compute_hash(tuple_out->contribution_value, hashed_subkeys, NUM_SUBKEYS * HASH_LEN, NULL, 0);
    free(hashed_subkeys);
}

int edgenode_batch_verify(const EdgeNode* node, const MessageTuple* batch, int batch_size) {
    if (!node || !node->tbf) {
        printf("Error: EdgeNode or TBF not initialized.\r\n");
        return 0;
    }

    // 1. 消息批次预处理 (检查时间戳)
    //uint64_t current_time = Get_counter();
    //for (int i = 0; i < batch_size; i++) {
        // 简单的防重放/过期检查 (需根据时钟频率设定阈值)
        // if (current_time > batch[i].timestamp + ALLOWED_DELAY_CYCLES) {
        //     printf("Verification failed: Message timestamp expired.\r\n");
        //     return 0;
        // }
    //}

    // 2. 重构并校验群体验证点 [cite: 64]

    // 2.1 聚合收到的身份贡献值 C_agg = C_1 || ... || C_k [cite: 66]
    uint8_t *c_agg = malloc(batch_size * HASH_LEN);
    if (!c_agg) {
        printf("Error: Failed to allocate memory for c_agg!\r\n");
        return 0;
    }
    for (int i = 0; i < batch_size; i++) {
        memcpy(&c_agg[i * HASH_LEN], batch[i].contribution_value, HASH_LEN);
    }

    // 2.2 计算候选群体验证点 H'_group = H(C_agg) [cite: 68]
    uint8_t h_group_prime[HASH_LEN];
    compute_hash(h_group_prime, c_agg, batch_size * HASH_LEN, NULL, 0);

    // 2.3 [关键修改] 在 TBF 中查询 H'_group
    if (query_element(node->tbf, h_group_prime, HASH_LEN) == 0) {
        printf("Verification failed: Group verification point not found in TBF (Illegal Batch).\r\n");
        free(c_agg);
        return 0; // 拒绝整个批次
    }

    // 3. 批量验证签名完整性
    uint8_t *reconstructed_c_agg = malloc(batch_size * HASH_LEN);
    if (!reconstructed_c_agg) {
        printf("Error: Failed to allocate memory for reconstructed_c_agg!\r\n");
        free(c_agg);
        return 0;
    }

    for (int j = 0; j < batch_size; j++) {
        const MessageTuple* tuple = &batch[j];

        // 3.1 重新计算消息哈希根 t'_j = H(M_j || T_j || R_j) [cite: 72]
        SM3_CTX sha_ctx;
        sm3_init(&sha_ctx);
        sm3_update(&sha_ctx, (const uint8_t*)tuple->message, strlen(tuple->message));
        sm3_update(&sha_ctx, (const uint8_t*)&tuple->timestamp, sizeof(tuple->timestamp));
        sm3_update(&sha_ctx, (const uint8_t*)&tuple->nonce, sizeof(tuple->nonce));
        uint8_t t_hash_prime[HASH_LEN];
        sm3_final(&sha_ctx, t_hash_prime);

        // 3.2 恢复哈希后的子密钥 h'_{j,l} [cite: 76]
        uint8_t *h_prime = malloc(NUM_SUBKEYS * HASH_LEN);
        if (!h_prime) {
            free(c_agg);
            free(reconstructed_c_agg);
            return 0;
        }
        for (int l = 0; l < NUM_SUBKEYS; l++) {
            // 使用与签名时相同的比特选择逻辑
            int bit = (t_hash_prime[l] >> 7) & 1;
            if (bit == 1) { // t'_{j,l} = 1, sig[l] is H(sk)
                memcpy(&h_prime[l * HASH_LEN], tuple->signature.sig[l], HASH_LEN);
            } else { // t'_{j,l} = 0, sig[l] is sk
                // 收到的是 sk，计算 H(sk)
                compute_hash(&h_prime[l * HASH_LEN], tuple->signature.sig[l], HASH_LEN, NULL, 0);
            }
        }

        // 3.3 恢复贡献值 C_j' = H(h'_{j,1}, ..., h'_{j,n}) [cite: 77]
        uint8_t c_j_prime[HASH_LEN];
        compute_hash(c_j_prime, h_prime, NUM_SUBKEYS * HASH_LEN, NULL, 0);
        free(h_prime);

        // 聚合恢复的贡献值 (C'_agg = C'_1 || ... || C'_k) [cite: 80]
        memcpy(&reconstructed_c_agg[j * HASH_LEN], c_j_prime, HASH_LEN);
    }

    // 4. 最终决策 
    int verification_result = 0;
    if (memcmp(reconstructed_c_agg, c_agg, batch_size * HASH_LEN) == 0) {
        verification_result = 1; // Verification passed
    } else {
        printf("Verification failed: Reconstructed aggregated contribution value mismatch.\r\n");
        verification_result = 0; // Verification failed
    }

    // 清理内存
    free(c_agg);
    free(reconstructed_c_agg);

    return verification_result;
}
