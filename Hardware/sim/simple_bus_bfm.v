// ============================================================================
//  Module: simple_bus_bfm
//  Description: 一个simple_bus协议的总线功能模型 (BFM)。
//               它模拟一个CPU主设备，提供任务来发起总线上的读写事务。
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

`include "../rtl/bf_defines.vh"

module simple_bus_bfm (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- Bus Master Interface (to DUT's Slave Port) ---
    output reg  [`ADDR_WIDTH-1:0]   o_s_addr,
    output reg  [`DATA_WIDTH-1:0]   o_s_wdata,
    output reg                      o_s_wen,
    output reg                      o_s_ren,
    input       [`DATA_WIDTH-1:0]   i_s_rdata,
    input                           i_s_ready
);

//-----------------------------------------------------------------------------
// Initialization
//-----------------------------------------------------------------------------
initial begin
    o_s_addr  = `ADDR_WIDTH'h0;
    o_s_wdata = `DATA_WIDTH'h0;
    o_s_wen   = 1'b0;
    o_s_ren   = 1'b0;
end

//-----------------------------------------------------------------------------
// Task: bus_write
// Description: 执行一次32位写事务。
//-----------------------------------------------------------------------------
task bus_write;
    input [`ADDR_WIDTH-1:0] addr;
    input [`DATA_WIDTH-1:0] data;
begin
    @(posedge i_clk);
    o_s_addr  = addr;
    o_s_wdata = data;
    o_s_wen   = 1'b1;
    o_s_ren   = 1'b0;

    while (!i_s_ready) begin
        @(posedge i_clk);
    end

    @(posedge i_clk);
    o_s_wen = 1'b0;
    
    //$display("[BFM] Write: addr=0x%h, data=0x%h", addr, data);
end
endtask

//-----------------------------------------------------------------------------
// Task: bus_read
// Description: 执行一次32位读事务。
//-----------------------------------------------------------------------------
task bus_read;
    input  [`ADDR_WIDTH-1:0] addr;
    output [`DATA_WIDTH-1:0] data;
    reg    [`DATA_WIDTH-1:0] read_data_temp;
begin
    @(posedge i_clk);
    o_s_addr = addr;
    o_s_wen  = 1'b0;
    o_s_ren  = 1'b1;

    while (!i_s_ready) begin
        @(posedge i_clk);
    end

    @(posedge i_clk);
    read_data_temp = i_s_rdata;

    @(posedge i_clk);
    o_s_ren = 1'b0;

    data = read_data_temp;

    // if(addr == 32'h08)begin
    //     $display("read status");
    //     $display("status_val:%h", i_s_rdata);
    //     $display("status_val_1:%h", read_data_temp);
    // end

end
endtask

//-----------------------------------------------------------------------------
// Task: send_bf_request
// Description: 使用两次32位写操作发送一个请求包。
//-----------------------------------------------------------------------------
task send_bf_request;
    input [2:0]               op_type;
    input [`ADDR_WIDTH-1:0]   mem_addr;
    input [`DATA_WIDTH-1:0]   len;
    reg   [`DATA_WIDTH-1:0]   high_word;
    reg   [`DATA_WIDTH-1:0]   low_word;
begin
    low_word  = mem_addr;
    high_word = {op_type, len[28:0]};

    //$display("[BFM] Sending Request: Op=%b, Addr=0x%h, Len=%d", op_type, mem_addr, len);

    bus_write(32'h00, low_word);
    bus_write(32'h04, high_word);
end
endtask

//-----------------------------------------------------------------------------
// Task: poll_status_for_done
// Description: 轮询状态寄存器直到busy标志位变低。
//-----------------------------------------------------------------------------
task poll_status_for_done;
    reg [`DATA_WIDTH-1:0] status_val;
    reg busy_bit;
    integer timeout_counter;
begin
    //$display("[BFM] Polling for job completion...");
    timeout_counter = 0;

    // 先至少等待一个周期，给硬件反应时间
    @(posedge i_clk);

    // 第一次读取状态
    bus_read(32'h08, status_val);
    busy_bit = status_val[1];

    while (busy_bit && timeout_counter < 500) begin
        //$display("[BFM] Polling... Accelerator is busy (status=0x%h)", status_val);
        @(posedge i_clk);
        bus_read(32'h08, status_val);
        busy_bit = status_val[1];
        timeout_counter = timeout_counter + 1;
    end

    if (timeout_counter >= 500) begin
        //$display("[BFM] ERROR: Timeout waiting for job completion!");
    end else begin
        //$display("[BFM] Job completed (status=0x%h).", status_val);
    end
end
endtask

//-----------------------------------------------------------------------------
// Task: configure_key
// Description: 写入64位密钥。
//-----------------------------------------------------------------------------
task configure_key;
    input [63:0] key;
begin
    //$display("[BFM] Configuring 64-bit crypto key...");
    bus_write(32'h0C, key[31:0]);
    bus_write(32'h10, key[63:32]);
end
endtask

endmodule
