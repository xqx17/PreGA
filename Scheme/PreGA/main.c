#include "scheme.h"
#include <stdio.h>
#include <string.h>

// --- WCH 平台驱动 & 库 ---
#include "ch32v30x.h"
#include "debug.h"
#include "ch32v30x_rng.h"
#include "risc_time.h"

#define NUM_DEVICES_IN_GROUP 5

int main() {
    uint32_t start,end,elapse;
    // --- CH32V 初始化 ---
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    Delay_Init();
    USART_Printf_Init(115200);
    systick_Init(); // 初始化 SysTick (用于 Get_counter())
    // 初始化硬件随机数生成器
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, ENABLE);
    RNG_Cmd(ENABLE);
    // 等待 RNG 准备就绪
    while(RNG_GetFlagStatus(RNG_FLAG_DRDY) == RESET);

    printf("Scheme simulation started (using SM3 on CH32V307VCT6)...\r\n");
    printf("========================================\r\n");

    // --- Phase 1: System Initialization (by TA) ---
    start = Get_counter();
    printf("[TA] 1. System initialization...\r\n");
    uint8_t master_key[HASH_LEN];
    ta_initialize_system(master_key);

    IoTDevice devices[NUM_DEVICES_IN_GROUP];
    for (int i = 0; i < NUM_DEVICES_IN_GROUP; i++) {
        ta_register_device(&devices[i], i + 1, master_key);
    }

    //初始化 EdgeNode 和 TimeBloomFilter ---
    EdgeNode edge_node;

    // 初始化时间布隆过滤器 (需在堆上分配内存，确保堆空间足够)
    // 参数: bloom_size=1024, windows=5, threshold=0.8, range=60
    edge_node.tbf = create_time_bloom_filter(1024, 5, 0.8, 60);

    if (edge_node.tbf == NULL) {
        printf("Error: Failed to create TBF (Heap full?)\r\n");
        while(1); // 停止运行
    }

    //调用预计算函数
    ta_precompute_group_point(&edge_node, devices, NUM_DEVICES_IN_GROUP, master_key);

    end = Get_counter();
    elapse = end - start;
    printf("Initialization need :%lu\n",elapse);


    printf("========================================\r\n");

    // --- Phase 2: Device Message Signing ---
    printf("[Device] Devices preparing to sign and send messages...\r\n");
    start = Get_counter();
    MessageTuple message_batch[NUM_DEVICES_IN_GROUP];

    for (int i = 0; i < NUM_DEVICES_IN_GROUP; i++) {
        char msg_content[128];
        sprintf(msg_content, "Data from device %d", devices[i].id);
        device_sign_message(&message_batch[i], &devices[i], msg_content);
    }
    printf("\r\n");
    end = Get_counter();
    elapse = (end - start)/NUM_DEVICES_IN_GROUP;
    printf("Message Signing need :%lu\n",elapse);


    printf("========================================\r\n");

    // --- Phase 3: Edge Node Batch Verification ---
    printf("[ES] Edge Node received a batch of %d messages, starting batch verification...\r\n", NUM_DEVICES_IN_GROUP);
    start = Get_counter();

    // 调用批量验证
    int result = edgenode_batch_verify(&edge_node, message_batch, NUM_DEVICES_IN_GROUP);

    if (result) {
        printf("Result: Batch verification successful!\r\n");
    } else {
        printf("Result: Batch verification failed!\r\n");
    }
    end = Get_counter();
    elapse = end - start;
    printf("Batch Verification need :%lu\n",elapse);

    // Keep the program running
    while(1) {
    }

    return 0;
}
