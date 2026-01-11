// ============================================================================
//  Module: cmd_decoder_fifo (Fixed)
//  Description: A simple FIFO to queue requests for the accelerator.
//               The re-assembly logic has been removed and moved to the
//               bus_interface module, simplifying this module significantly.
//  Standard: Verilog-2001
// ============================================================================
`timescale 1ns / 1ps

`include "bf_defines.vh"

module cmd_decoder_fifo (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- CPU Command Input (from Bus Interface) ---
    input                           i_cpu_cmd_valid,    // Trigger to push into FIFO
    input       [`REQ_PKT_WIDTH-1:0]  i_cpu_cmd_packet,   // CHANGED: Receives a full 64-bit packet

    // --- FIFO Output (to BF Control Unit) ---
    output wire [`REQ_PKT_WIDTH-1:0]  o_req_pkt,
    output wire                       o_fifo_not_empty,

    // --- FIFO Control (from BF Control Unit) ---
    input                           i_fifo_rd_en,

    // --- Status Output (to Bus Interface) ---
    output wire                       o_fifo_full
);

//-----------------------------------------------------------------------------
// Local Parameters
//-----------------------------------------------------------------------------
parameter FIFO_DEPTH = 16;
parameter FIFO_AW    = 4;

//-----------------------------------------------------------------------------
// Internal Registers and Wires
//-----------------------------------------------------------------------------
reg [`REQ_PKT_WIDTH-1:0]  r_fifo_mem [0:FIFO_DEPTH-1];
reg [FIFO_AW-1:0]         r_wr_ptr;
reg [FIFO_AW-1:0]         r_rd_ptr;
reg [FIFO_AW:0]           r_item_count;

wire                      w_fifo_push;
wire                      w_fifo_pop;

//-----------------------------------------------------------------------------
// FIFO Core Logic
//-----------------------------------------------------------------------------
assign w_fifo_push = i_cpu_cmd_valid;
assign w_fifo_pop  = i_fifo_rd_en && o_fifo_not_empty;

// Write Operation
always @(posedge i_clk) begin
    if (w_fifo_push && !o_fifo_full) begin
        r_fifo_mem[r_wr_ptr] <= i_cpu_cmd_packet; // FIXED: Use the direct 64-bit input
    end
end

// Write Pointer Logic
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_wr_ptr <= {FIFO_AW{1'b0}};
    end else begin
        if (w_fifo_push && !o_fifo_full) begin
            r_wr_ptr <= r_wr_ptr + 1;
        end
    end
end

// Read Operation
assign o_req_pkt = r_fifo_mem[r_rd_ptr];

// Read Pointer Logic
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_rd_ptr <= {FIFO_AW{1'b0}};
    end else begin
        if (w_fifo_pop) begin
            r_rd_ptr <= r_rd_ptr + 1;
        end
    end
end

// Item Counter Logic
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_item_count <= {(FIFO_AW+1){1'b0}};
    end else begin
        case ({w_fifo_push && !o_fifo_full, w_fifo_pop})
            2'b01: r_item_count <= r_item_count - 1; // Pop only
            2'b10: r_item_count <= r_item_count + 1; // Push only
            2'b11: r_item_count <= r_item_count;     // Simultaneous push and pop
            default: r_item_count <= r_item_count;   // No change
        endcase
    end
end

// Status Signal Generation
assign o_fifo_full      = (r_item_count == FIFO_DEPTH);
assign o_fifo_not_empty = (r_item_count != 0);

endmodule
