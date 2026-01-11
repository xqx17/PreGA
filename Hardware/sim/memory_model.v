// ============================================================================
//  Module: memory_model
//  Description: 一个简单的主内存行为模型 (SRAM/DRAM)。
//               它在总线上作为从设备，响应来自主设备(如DMA)的读写请求。
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

`include "../rtl/bf_defines.vh"

module memory_model (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- Bus Slave Interface (from DUT's Master Port) ---
    input       [`ADDR_WIDTH-1:0]   i_addr,
    input       [`DATA_WIDTH-1:0]   i_wdata,
    input                           i_wen,
    input                           i_req,
    output reg  [`DATA_WIDTH-1:0]   o_rdata,
    output reg                      o_ready
);

//-----------------------------------------------------------------------------
// Memory Model Parameters
//-----------------------------------------------------------------------------
parameter MEM_SIZE_IN_BYTES = 64 * 1024; // 64 KB
parameter MEM_NUM_WORDS     = MEM_SIZE_IN_BYTES / 4;
parameter MEM_INIT_FILE     = "C:\\Users\\19122\\Desktop\\design\\BAC\\sim\\test_data.mem";

//-----------------------------------------------------------------------------
// Memory Storage Array
//-----------------------------------------------------------------------------
reg [`DATA_WIDTH-1:0] mem [0:MEM_NUM_WORDS-1];

//-----------------------------------------------------------------------------
// Memory Initialization
//-----------------------------------------------------------------------------
initial begin
    $display("[MEM] Initializing memory model...");
    $readmemh(MEM_INIT_FILE, mem);
    $display("[MEM] Memory initialized from file: %s", MEM_INIT_FILE);
end

//-----------------------------------------------------------------------------
// Bus Transaction Logic (with 1-cycle latency)
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        o_ready <= 1'b0;
        o_rdata <= `DATA_WIDTH'h0;
    end else begin
        if (i_req) begin
            // Master is making a request. Respond in the next cycle.
            o_ready <= 1'b1;
            if (i_wen) begin
                // It's a write request
                mem[i_addr[`ADDR_WIDTH-1:2]] <= i_wdata;
                //$display("[MEM] Write Request: addr=0x%h, data=0x%h", i_addr, i_wdata);
            end else begin
                // It's a read request. Data will be valid in the next cycle.
                o_rdata <= mem[i_addr[`ADDR_WIDTH-1:2]];
                //$display("[MEM] Read Request: addr=0x%h. Data will be available next cycle.", i_addr);
            end
        end else begin
            // No request, de-assert ready.
            o_ready <= 1'b0;
        end
    end
end

endmodule
