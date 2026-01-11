// ============================================================================
//  Module: bf_accel_wrapper (Fixed)
//  Description: The top-level module for the BFAccel IP core.
//               Connections updated to reflect the new internal interface.
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

`include "bf_defines.vh"

module bf_accel_wrapper (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- Slave Interface (from CPU/simple_bus) ---
    input       [`ADDR_WIDTH-1:0]   i_s_addr,
    input       [`DATA_WIDTH-1:0]   i_s_wdata,
    input                           i_s_wen,
    input                           i_s_ren,
    output      [`DATA_WIDTH-1:0]   o_s_rdata,
    output                          o_s_ready,

    // --- Master Interface (to simple_bus/Main Memory) ---
    output      [`ADDR_WIDTH-1:0]   o_m_addr,
    output      [`DATA_WIDTH-1:0]   o_m_wdata,
    output                          o_m_wen,
    output                          o_m_req,
    input       [`DATA_WIDTH-1:0]   i_m_rdata,
    input                           i_m_ready
);

//-----------------------------------------------------------------------------
// Internal Wires for connecting Bus Interface and Core
//-----------------------------------------------------------------------------
wire                        w_cpu_cmd_valid;
wire [`REQ_PKT_WIDTH-1:0]   w_cpu_cmd_packet; // CHANGED
wire                        w_key_we;
wire [63:0]                 w_key_wdata;

wire                        w_fifo_full;
wire                        w_accel_busy;
wire                        w_check_result; // NEW

//-----------------------------------------------------------------------------
// Module Instantiations
//-----------------------------------------------------------------------------

// 1. Bus Interface Module Instantiation
bus_interface u_bus_if (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_s_addr           (i_s_addr),
    .i_s_wdata          (i_s_wdata),
    .i_s_wen            (i_s_wen),
    .i_s_ren            (i_s_ren),
    .o_s_rdata          (o_s_rdata),
    .o_s_ready          (o_s_ready),
    .o_cpu_cmd_valid    (w_cpu_cmd_valid),
    .o_cpu_cmd_packet   (w_cpu_cmd_packet), // CHANGED
    .i_fifo_full        (w_fifo_full),
    .o_key_we           (w_key_we),
    .o_key_wdata        (w_key_wdata),
    .i_accel_busy       (w_accel_busy),
    .i_check_result     (w_check_result)
);

// 2. Core Logic Module Instantiation
bf_accel_core u_core (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_cpu_cmd_valid    (w_cpu_cmd_valid),
    .i_cpu_cmd_packet   (w_cpu_cmd_packet), // CHANGED
    .i_key_we           (w_key_we),
    .i_key_wdata        (w_key_wdata),
    .o_fifo_full        (w_fifo_full),
    .o_accel_busy       (w_accel_busy),
    .o_check_result     (w_check_result), // NEW
    .o_m_addr           (o_m_addr),
    .o_m_wdata          (o_m_wdata),
    .o_m_wen            (o_m_wen),
    .o_m_req            (o_m_req),
    .i_m_rdata          (i_m_rdata),
    .i_m_ready          (i_m_ready)
);

endmodule
