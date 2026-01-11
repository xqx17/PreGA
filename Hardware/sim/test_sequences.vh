// ============================================================================
//  File: test_sequences_perf.vh (Performance-extended)
//  Description: 在原�? test_sequences.vh 基础上增加�?�能测试，用于测�?
//               插入(ADD)、查�?(CHECK)和删�?(REMOVE) 1000 条消息所�?时间�?
// ============================================================================
`timescale 1ns / 1ps

`include "../rtl/bf_defines.vh"

// 辅助任务: �?查结果并打印PASS/FAIL
task check_result;
    input [31:0] actual_val;
    input [31:0] expected_val;
    input [255:0] message;
begin
    if (actual_val === expected_val) begin
        $display("[TB] PASS: %s (Got: 0x%h, Expected: 0x%h)", message, actual_val, expected_val);
    end else begin
        $display("[TB] FAIL: %s (Got: 0x%h, Expected: 0x%h)", message, actual_val, expected_val);
    end
end
endtask

// --- 新增: 初始化任�? ---
task initialize_dut_memories;
    integer i;
begin
    $display("\n[SEQ] --- Initializing DUT internal memories to zero ---");

    // 清零 dual_port_sram (本地数据缓存)
    // 路径: tb -> u_dut -> u_core -> u_local_buffer -> mem
    for (i = 0; i < u_dut.u_core.LOCAL_BUF_DEPTH; i = i + 1) begin
        u_dut.u_core.u_local_buffer.mem[i] = 0;
    end
    $display("[SEQ] dual_port_sram (local buffer) cleared.");

    // 清零 single_port_sram (私有计数器内�?)
    // 路径: tb -> u_dut -> u_core -> u_bf_datapath -> u_private_memory -> mem
    for (i = 0; i < u_dut.u_core.u_bf_datapath.PVT_MEM_DEPTH; i = i + 1) begin
        u_dut.u_core.u_bf_datapath.u_private_memory.mem[i] = 0;
    end
    $display("[SEQ] single_port_sram (private memory) cleared.");
    
    $display("[SEQ] --- DUT Memory Initialization Finished ---");
end
endtask

// 默认测量数量
//`define PERF_MSGS_DEFAULT 1000
`define PERF_MSGS_DEFAULT 1

// 性能测试主任务（N 可调�?
task run_perf_test_n_messages;
    input integer N;       // 要测试的消息数量
    input integer mode;    // 0: sequential, 1: batched
    integer i;
    time start_time, end_time;
    time elapsed_time;
    reg [31:0] status_val;
    reg found_bit;
    reg [31:0] base_addr;
    integer addr_stride;
begin
    $display("\n[PERF] --- Starting performance test: N=%0d, mode=%0d ---", N, mode);

    // 基础地址与步进，用于生成不同消息
    base_addr = 32'h1000;
    addr_stride = 32; // 每条消息间隔32字节（可调整�?

    // ---------- Phase 1: ADDs ----------
    $display("[PERF] Phase 1: ADD %0d messages", N);
    start_time = $time;
    if (mode == 0) begin
        // sequential: send one, poll until done, repeat
        for (i = 0; i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_ADD, base_addr + i * addr_stride, 16);
            u_bfm.poll_status_for_done;
        end
    end else begin
        // batched: send all back-to-back, then wait for completion of all
        for (i = 0; i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_ADD, base_addr + i * addr_stride, 16);
        end
        // 等待队列处理完成。实现方式依赖于 u_bfm 的能力：我们轮询 status 直到空闲�?
        u_bfm.poll_status_for_done; // �?次调用可能只保证队列中第�?个完成，�? poll_status_for_done 等待直到空闲则可�?
        // 为保险起见，短延迟后再检查一�?
        //# (CLK_PERIOD * 10);
    end
    end_time = $time;
    elapsed_time = end_time - start_time;
    $display("[PERF] Phase 1 finished: elapsed = %0t ps, cycles ~= %0d", elapsed_time, elapsed_time / CLK_PERIOD);

    // ---------- Phase 2: CHECKs ----------
    $display("[PERF] Phase 2: CHECK %0d messages", N);
    start_time = $time;
    if (mode == 0) begin
        for (i = 0; i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_CHECK, base_addr + i * addr_stride, 16);
            u_bfm.poll_status_for_done;
            // 可�?�：读取并确认found_bit，节约时间可注释�?
            u_bfm.bus_read(32'h08, status_val);
            found_bit = status_val[31];
            if (found_bit !== 1'b1) begin
                $display("[PERF] WARNING: CHECK failed for addr=0x%h (i=%0d)", base_addr + i * addr_stride, i);
            end
        end
    end else begin
        for (i = 0; i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_CHECK, base_addr + i * addr_stride, 16);
        end
        u_bfm.poll_status_for_done;
        //# (CLK_PERIOD * 10);
        // 批量模式下为了保证正确�?�，我们可以快�?�抽样几个点进行读取
        for (i = 0; i < 5 && i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_CHECK, base_addr + i * addr_stride, 16);
            u_bfm.poll_status_for_done;
            u_bfm.bus_read(32'h08, status_val);
            found_bit = status_val[31];
            if (found_bit !== 1'b1) begin
                $display("[PERF] WARNING (sample): CHECK failed for addr=0x%h (i=%0d)", base_addr + i * addr_stride, i);
            end
        end
    end
    end_time = $time;
    elapsed_time = end_time - start_time;
    $display("[PERF] Phase 2 finished: elapsed = %0t ps, cycles ~= %0d", elapsed_time, elapsed_time / CLK_PERIOD);

    // ---------- Phase 3: REMOVEs ----------
    $display("[PERF] Phase 3: REMOVE %0d messages", N);
    start_time = $time;
    if (mode == 0) begin
        for (i = 0; i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_REMOVE, base_addr + i * addr_stride, 16);
            u_bfm.poll_status_for_done;
        end
    end else begin
        for (i = 0; i < N; i = i + 1) begin
            u_bfm.send_bf_request(`OP_REMOVE, base_addr + i * addr_stride, 16);
        end
        u_bfm.poll_status_for_done;
        //# (CLK_PERIOD * 10);
    end
    end_time = $time;
    elapsed_time = end_time - start_time;
    $display("[PERF] Phase 3 finished: elapsed = %0t ps, cycles ~= %0d", elapsed_time, elapsed_time / CLK_PERIOD);

    $display("[PERF] --- Performance test finished (N=%0d, mode=%0d) ---", N, mode);
end
endtask

// 便捷包装任务：运行默�? 1000 条消息的测试
task run_perf_test_1000;
begin
    run_perf_test_n_messages(`PERF_MSGS_DEFAULT, 0); // sequential 模式
    # (CLK_PERIOD * 50);
    run_perf_test_n_messages(`PERF_MSGS_DEFAULT, 1); // batched 模式
end
endtask

// -----------------------------------------------------------------------------
// run_all_tests 修改：在原有功能测试后加入�?�能测试调用（可注释/取消�?
// -----------------------------------------------------------------------------

task run_all_tests;
begin
    //在所有测试开始前，先初始化DUT内部的存储器
    initialize_dut_memories;
    # (CLK_PERIOD * 5);

    // 性能测试: 1000 条消息（默认�?
    // 如果不希望自动运行�?�能测试，可注释下面两行
    $display("\n[SEQ] Now running performance tests (1000 msgs)...");
    run_perf_test_1000;
    # (CLK_PERIOD * 50);

    // run_test_zero_length;
    // # (CLK_PERIOD * 20);
end
endtask

// ============================================================================
// End of file
// ============================================================================










// // ============================================================================
// //  File: test_sequences.vh (Comprehensive)
// //  Description: 为BFAccel验证定义了一整套测试序列�?
// // ============================================================================
// `timescale 1ns / 1ps

// `include "../rtl/bf_defines.vh"

// // 辅助任务: �?查结果并打印PASS/FAIL
// task check_result;
//     input [31:0] actual_val;
//     input [31:0] expected_val;
//     input [255:0] message;
// begin
//     if (actual_val === expected_val) begin
//         $display("[TB] PASS: %s (Got: 0x%h, Expected: 0x%h)", message, actual_val, expected_val);
//     end else begin
//         $display("[TB] FAIL: %s (Got: 0x%h, Expected: 0x%h)", message, actual_val, expected_val);
//     end
// end
// endtask

// // --- 新增: 初始化任�? ---
// task initialize_dut_memories;
//     integer i;
// begin
//     $display("\n[SEQ] --- Initializing DUT internal memories to zero ---");

//     // 清零 dual_port_sram (本地数据缓存)
//     // 路径: tb -> u_dut -> u_core -> u_local_buffer -> mem
//     for (i = 0; i < u_dut.u_core.LOCAL_BUF_DEPTH; i = i + 1) begin
//         u_dut.u_core.u_local_buffer.mem[i] = 0;
//     end
//     $display("[SEQ] dual_port_sram (local buffer) cleared.");

//     // 清零 single_port_sram (私有计数器内�?)
//     // 路径: tb -> u_dut -> u_core -> u_bf_datapath -> u_private_memory -> mem
//     for (i = 0; i < u_dut.u_core.u_bf_datapath.PVT_MEM_DEPTH; i = i + 1) begin
//         u_dut.u_core.u_bf_datapath.u_private_memory.mem[i] = 0;
//     end
//     $display("[SEQ] single_port_sram (private memory) cleared.");
    
//     $display("[SEQ] --- DUT Memory Initialization Finished ---");
// end
// endtask


// // --- 测试序列 1: 基础功能验证 (ADD & CHECK) ---
// task run_test_basic_add_check;
//     reg [31:0] status_val;
//     reg found_bit;
// begin
//     $display("\n[SEQ] --- Starting Test 1: Basic Add and Check ---");
//     // 步骤 1: 配置密钥
//     u_bfm.configure_key(64'hDEADBEEF_CAFEBABE);
//     // 步骤 2: 发�?? 'ADD' 请求
//     u_bfm.send_bf_request(`OP_ADD, 32'h1000, 16);
//     // 步骤 3: 等待完成
//     u_bfm.poll_status_for_done;
//     // 步骤 4: 发�?? 'CHECK' 请求
//     u_bfm.send_bf_request(`OP_CHECK, 32'h1000, 16);
//     // 步骤 5: 等待完成
//     u_bfm.poll_status_for_done;
//     // 步骤 6: 读取结果并验�?
//     u_bfm.bus_read(32'h08, status_val);
//     found_bit = status_val[31]; // 假设结果在最高位
//     check_result(found_bit, 1'b1, "Element should be FOUND after ADD");
//     $display("[SEQ] --- Test 1 Finished ---");
// end
// endtask

// // --- 测试序列 2: 移除功能验证 (REMOVE & CHECK) ---
// task run_test_remove_check;
//     reg [31:0] status_val;
//     reg found_bit;
// begin
//     $display("\n[SEQ] --- Starting Test 2: Remove and Check ---");
//     // 步骤 1: 发�?? 'REMOVE' 请求
//     u_bfm.send_bf_request(`OP_REMOVE, 32'h1000, 16);
//     // 步骤 2: 等待完成
//     u_bfm.poll_status_for_done;
//     // 步骤 3: 再次发�?? 'CHECK' 请求
//     u_bfm.send_bf_request(`OP_CHECK, 32'h1000, 16);
//     // 步骤 4: 等待完成
//     u_bfm.poll_status_for_done;
//     // 步骤 5: 读取结果并验�?
//     u_bfm.bus_read(32'h08, status_val);
//     found_bit = status_val[31];
//     check_result(found_bit, 1'b0, "Element should NOT be found after REMOVE");
//     $display("[SEQ] --- Test 2 Finished ---");
// end
// endtask

// // --- 测试序列 3: �?查不存在的元�? ---
// task run_test_check_non_existent;
//     reg [31:0] status_val;
//     reg found_bit;
// begin
//     $display("\n[SEQ] --- Starting Test 3: Check Non-Existent Element ---");
//     // 步骤 1: �?查一个从未添加过的元�?
//     u_bfm.send_bf_request(`OP_CHECK, 32'h4000, 16);
//     // 步骤 2: 等待完成
//     u_bfm.poll_status_for_done;
//     // 步骤 3: 读取结果并验�?
//     u_bfm.bus_read(32'h08, status_val);
//     found_bit = status_val[31];
//     check_result(found_bit, 1'b0, "Non-existent element should NOT be found");
//     $display("[SEQ] --- Test 3 Finished ---");
// end
// endtask

// // --- 测试序列 4: FIFO 和流水线压力测试 ---
// task run_test_fifo_stress;
//     reg [31:0] status_val;
//     reg found_bit;
// begin
//     $display("\n[SEQ] --- Starting Test 4: FIFO and Pipeline Stress Test ---");
//     // 步骤 1: 背靠背发送三个不同命�?
//     $display("[SEQ] Sending 3 requests back-to-back...");
//     u_bfm.send_bf_request(`OP_ADD, 32'h2000, 32);   // 添加数据�?2
//     u_bfm.send_bf_request(`OP_ADD, 32'h3000, 16);   // 添加数据�?3
//     u_bfm.send_bf_request(`OP_CHECK, 32'h2000, 32); // �?查数据块2
//     // 步骤 2: 等待�?有排队的任务完成
//     u_bfm.poll_status_for_done;
//     // 步骤 3: 验证�?后一个命�?(CHECK)的结�?
//     u_bfm.bus_read(32'h08, status_val);
//     found_bit = status_val[31];
//     check_result(found_bit, 1'b1, "FIFO Test: Last CHECK command should succeed");
//     $display("[SEQ] --- Test 4 Finished ---");
// end
// endtask

// // --- 测试序列 5: 边界条件测试 (零长度命�?) ---
// task run_test_zero_length;
// begin
//     $display("\n[SEQ] --- Starting Test 5: Zero-Length Command Test ---");
//     // 步骤 1: 发�?�一个长度为0的命�?
//     u_bfm.send_bf_request(`OP_ADD, 32'h5000, 0);
//     // 步骤 2: 等待完成 (应该会很�?)
//     u_bfm.poll_status_for_done;
//     $display("[SEQ] --- Test 5 Finished (Completed gracefully) ---");
// end
// endtask


// // --- 总测试任�? ---
// task run_all_tests;
// begin
//     //在所有测试开始前，先初始化DUT内部的存储器
//     initialize_dut_memories;
//     # (CLK_PERIOD * 5);

//     run_test_basic_add_check;
//     # (CLK_PERIOD * 20);

//     run_test_remove_check;
//     # (CLK_PERIOD * 20);

//     run_test_check_non_existent;
//     # (CLK_PERIOD * 20);
    
//     run_test_fifo_stress;
//     # (CLK_PERIOD * 20);

//     // run_test_zero_length;
//     // # (CLK_PERIOD * 20);
// end
// endtask
