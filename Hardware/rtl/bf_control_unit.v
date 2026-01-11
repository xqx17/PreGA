// ============================================================================
//  Module: bf_control_unit (Final Fix)
//  Description: The main FSM for the BFAccel. This final version includes
//               a dedicated DEQUEUE state to create a fully robust, timing-safe
//               handshake with the command FIFO.
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

`include "bf_defines.vh"

module bf_control_unit (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- FIFO Interface ---
    input       [`REQ_PKT_WIDTH-1:0]  i_req_pkt,
    input                           i_fifo_not_empty,
    output reg                      o_fifo_rd_en,

    // --- DMA Control Interface ---
    output reg                      o_dma_start,
    output reg  [`ADDR_WIDTH-1:0]   o_dma_src_addr,
    output reg  [`DATA_WIDTH-1:0]   o_dma_len,
    input                           i_dma_done,

    // --- Datapath Control Interface ---
    output reg                      o_dp_start,
    output reg  [1:0]               o_dp_op_type,
    input                           i_dp_done,

    // --- Status Output ---
    output wire                     o_accel_busy
);

//-----------------------------------------------------------------------------
// FSM State Definition (Corrected for robust FIFO dequeue)
//-----------------------------------------------------------------------------
parameter FSM_IDLE      = 4'b0000;
parameter FSM_DEQUEUE   = 4'b0001; // NEW STATE: To safely read from FIFO
parameter FSM_LATCH_REQ = 4'b0010;
parameter FSM_START_DMA = 4'b0011;
parameter FSM_WAIT_DMA  = 4'b0100;
parameter FSM_START_DP  = 4'b0101;
parameter FSM_WAIT_DP   = 4'b0110;

//-----------------------------------------------------------------------------
// Internal Registers
//-----------------------------------------------------------------------------
reg [3:0]                 r_current_state;
reg [3:0]                 r_next_state;

reg [1:0]                 r_current_op;
reg [`ADDR_WIDTH-1:0]     r_current_addr;
reg [`DATA_WIDTH-1:0]     r_current_len;

//-----------------------------------------------------------------------------
// State Register Logic
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_current_state <= FSM_IDLE;
    end else begin
        r_current_state <= r_next_state;
    end
end

//-----------------------------------------------------------------------------
// Request Packet Register Logic (Now latches on a specific FSM state)
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_current_op   <= 2'b0;
        r_current_addr <= `ADDR_WIDTH'h0;
        r_current_len  <= `DATA_WIDTH'h0;
    end else begin
        // **FIXED**: Latch the data only when the FSM is in the DEQUEUE state.
        // This is synchronous and robust.
        if (r_current_state == FSM_DEQUEUE) begin
            r_current_op   <= i_req_pkt[`REQ_OP_RANGE];
            r_current_addr <= i_req_pkt[`REQ_ADDR_RANGE];
            r_current_len  <= i_req_pkt[`REQ_LEN_RANGE];
        end
    end
end

//-----------------------------------------------------------------------------
// Busy Signal Logic
//-----------------------------------------------------------------------------
assign o_accel_busy = (r_current_state != FSM_IDLE) || (i_fifo_not_empty); 

//-----------------------------------------------------------------------------
// Next State and Output Logic (Combinational)
//-----------------------------------------------------------------------------
always @(*) begin
    // Default values
    r_next_state   = r_current_state;
    o_fifo_rd_en   = 1'b0;
    o_dma_start    = 1'b0;
    o_dp_start     = 1'b0;
    
    o_dma_src_addr = r_current_addr;
    o_dma_len      = r_current_len;
    o_dp_op_type   = r_current_op;

    case (r_current_state)
        FSM_IDLE: begin
            if (i_fifo_not_empty) begin
                // FIFO has data, decide to read it in the next cycle.
                r_next_state = FSM_DEQUEUE;
            end
        end

        FSM_DEQUEUE: begin
            // In this state, we assert read enable for one full cycle.
            // The packet registers will latch the data on this clock edge.
            o_fifo_rd_en = 1'b1;
            r_next_state = FSM_LATCH_REQ;
        end

        FSM_LATCH_REQ: begin
            // Wait one cycle for the latched r_current_* registers to be stable.
            r_next_state = FSM_START_DMA;
        end

        FSM_START_DMA: begin
            o_dma_start  = 1'b1;
            r_next_state = FSM_WAIT_DMA;
        end

        FSM_WAIT_DMA: begin
            if (i_dma_done) begin
                r_next_state = FSM_START_DP;
            end
        end

        FSM_START_DP: begin
            o_dp_start   = 1'b1;
            r_next_state = FSM_WAIT_DP;
        end

        FSM_WAIT_DP: begin
            if (i_dp_done) begin
                r_next_state = FSM_IDLE;
            end
        end

        default: begin
            r_next_state = FSM_IDLE;
        end
    endcase
end

endmodule
