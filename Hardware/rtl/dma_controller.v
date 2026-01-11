// ============================================================================
//  Module: dma_controller (Final Fix)
//  Description: A Direct Memory Access (DMA) controller. This final version
//               corrects the loop termination logic to ensure the exact
//               number of bytes specified by 'i_len' is transferred.
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

`include "bf_defines.vh"

module dma_controller (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- Control Interface (from BF Control Unit) ---
    input                           i_start,
    input       [`ADDR_WIDTH-1:0]   i_src_addr,
    input       [`DATA_WIDTH-1:0]   i_len,
    output reg                      o_done,

    // --- Master Bus Interface (to Main Memory) ---
    output reg  [`ADDR_WIDTH-1:0]   o_m_addr,
    output reg                      o_m_req,
    output reg                      o_m_wen,
    input       [`DATA_WIDTH-1:0]   i_m_rdata,
    input                           i_m_ready,

    // --- Local Data Buffer Interface ---
    output reg  [`ADDR_WIDTH-1:0]   o_buf_waddr,
    output reg  [`DATA_WIDTH-1:0]   o_buf_wdata,
    output reg                      o_buf_wen
);

//-----------------------------------------------------------------------------
// FSM State Definition
//-----------------------------------------------------------------------------
parameter DMA_IDLE      = 2'b00;
parameter DMA_REQ_MEM   = 2'b01;
parameter DMA_WAIT_MEM  = 2'b10;
parameter DMA_WRITE_BUF = 2'b11;

//-----------------------------------------------------------------------------
// Internal Registers
//-----------------------------------------------------------------------------
reg [1:0]                 r_current_state;
reg [1:0]                 r_next_state;

reg [`ADDR_WIDTH-1:0]     r_current_mem_addr;
reg [`ADDR_WIDTH-1:0]     r_current_buf_addr;
reg [`DATA_WIDTH-1:0]     r_bytes_transferred; // CHANGED: Count up, not down

//-----------------------------------------------------------------------------
// State Register Logic
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_current_state <= DMA_IDLE;
    end else begin
        r_current_state <= r_next_state;
    end
end

//-----------------------------------------------------------------------------
// Internal Counter and Address Register Logic
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_current_mem_addr  <= `ADDR_WIDTH'h0;
        r_current_buf_addr  <= `ADDR_WIDTH'h0;
        r_bytes_transferred <= `DATA_WIDTH'h0;
    end else begin
        if (i_start) begin
            r_current_mem_addr  <= i_src_addr;
            r_current_buf_addr  <= `ADDR_WIDTH'h0;
            r_bytes_transferred <= `DATA_WIDTH'h0;
        end
        // Update counters on a successful write to the buffer
        else if (r_current_state == DMA_WRITE_BUF) begin
            r_current_mem_addr  <= r_current_mem_addr + 4;
            r_current_buf_addr  <= r_current_buf_addr + 4;
            r_bytes_transferred <= r_bytes_transferred + 4;
        end
    end
end

//-----------------------------------------------------------------------------
// Next State and Output Logic (Combinational)
//-----------------------------------------------------------------------------
always @(*) begin
    // Default values
    r_next_state = r_current_state;
    o_done       = 1'b0;
    o_m_req      = 1'b0;
    o_m_wen      = 1'b0;
    o_m_addr     = r_current_mem_addr;
    o_buf_wen    = 1'b0;
    o_buf_waddr  = r_current_buf_addr;
    o_buf_wdata  = i_m_rdata;

    case (r_current_state)
        DMA_IDLE: begin
            if (i_start) begin
                if (i_len > 0) begin
                    r_next_state = DMA_REQ_MEM;
                end else begin
                    o_done = 1'b1;
                    r_next_state = DMA_IDLE;
                end
            end
        end

        DMA_REQ_MEM: begin
            o_m_req = 1'b1;
            o_m_wen = 1'b0; // It's a read request
            r_next_state = DMA_WAIT_MEM;
        end

        DMA_WAIT_MEM: begin
            if (i_m_ready) begin
                r_next_state = DMA_WRITE_BUF;
            end
        end

        DMA_WRITE_BUF: begin
            o_buf_wen = 1'b1;
            // **FIXED**: Check if the *next* transfer will exceed the total length.
            // The current transfer is completing now.
            if (r_bytes_transferred + 4 >= i_len) begin
                o_done = 1'b1;
                r_next_state = DMA_IDLE;
            end else begin
                r_next_state = DMA_REQ_MEM;
            end
        end

        default: begin
            r_next_state = DMA_IDLE;
        end
    endcase
end

endmodule
