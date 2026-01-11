// ============================================================================
//  Module: bf_accel_core (Fixed)
//  Description: The core functional logic of the Bloom Filter Accelerator.
//               Interface updated to accept a full 64-bit command packet.
//  Standard: Verilog-2001
// ============================================================================

//DMA向主存请求数据，不向主存写数据

`timescale 1ns / 1ps

`include "bf_defines.vh"

module bf_accel_core (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- Internal Interface from Bus Interface ---
    input                           i_cpu_cmd_valid,
    input       [`REQ_PKT_WIDTH-1:0]  i_cpu_cmd_packet,   // CHANGED
    input                           i_key_we,
    input       [63:0]              i_key_wdata,
    output                          o_fifo_full,
    output                          o_accel_busy,
    output                          o_check_result, // NEW: Pass through check result

    // --- Master Interface (Pass-through to Main Memory for DMA) ---
    output      [`ADDR_WIDTH-1:0]   o_m_addr,
    output      [`DATA_WIDTH-1:0]   o_m_wdata,
    output                          o_m_wen,
    output                          o_m_req,
    input       [`DATA_WIDTH-1:0]   i_m_rdata,
    input                           i_m_ready
);

//-----------------------------------------------------------------------------
// Internal Wires and Registers
//-----------------------------------------------------------------------------

// --- Configuration Register ---
reg [63:0] r_crypto_key;

// --- Connections for Command Decoder and FIFO ---
wire [`REQ_PKT_WIDTH-1:0]   w_req_pkt_out;
wire                        w_fifo_not_empty;
wire                        w_fifo_rd_en;

// --- Connections for Control Unit (FSM) ---
wire                        w_dma_start;
wire [`ADDR_WIDTH-1:0]      w_dma_src_addr;
wire [`DATA_WIDTH-1:0]      w_dma_len;
wire                        w_dma_done;
wire                        w_dp_start;
wire [1:0]                  w_dp_op_type;
wire                        w_dp_done;
wire                        w_check_result; // NEW

// --- Connections for DMA Controller to Master Port ---
wire [`ADDR_WIDTH-1:0]      w_dma_m_addr;
//wire [`DATA_WIDTH-1:0]      w_dma_m_wdata;
wire                        w_dma_m_wen;
wire                        w_dma_m_req;

// --- Connections for Local Data Buffer (SRAM) ---
parameter LOCAL_BUF_DEPTH = 256;
parameter LOCAL_BUF_AW    = 8;
wire [`ADDR_WIDTH-1:0]      w_buf_waddr;
wire [`DATA_WIDTH-1:0]      w_buf_wdata;
wire                        w_buf_wen;
wire [`ADDR_WIDTH-1:0]      w_buf_raddr;
wire                        w_buf_ren;
wire [`DATA_WIDTH-1:0]      w_buf_rdata;

//-----------------------------------------------------------------------------
// Configuration Logic
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_crypto_key <= 64'h0;
    end else begin
        if (i_key_we) begin
            r_crypto_key <= i_key_wdata;
        end
    end
end

//-----------------------------------------------------------------------------
// Master Port Pass-through
//-----------------------------------------------------------------------------
//只发送读请求，不写数据
assign o_m_addr  = w_dma_m_addr;
assign o_m_wdata = `DATA_WIDTH'h0;
assign o_m_wen   = w_dma_m_wen;
assign o_m_req   = w_dma_m_req;

assign o_check_result = w_check_result;

//-----------------------------------------------------------------------------
// Sub-module Instantiations
//-----------------------------------------------------------------------------

// 1. Command Decoder and FIFO
cmd_decoder_fifo u_cmd_decoder_fifo (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_cpu_cmd_valid    (i_cpu_cmd_valid),
    .i_cpu_cmd_packet   (i_cpu_cmd_packet), // CHANGED
    .o_req_pkt          (w_req_pkt_out),
    .o_fifo_not_empty   (w_fifo_not_empty),
    .i_fifo_rd_en       (w_fifo_rd_en),
    .o_fifo_full        (o_fifo_full)
);

// 2. Main Control Unit (FSM)
bf_control_unit u_bf_control_unit (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_req_pkt          (w_req_pkt_out),
    .i_fifo_not_empty   (w_fifo_not_empty),
    .o_fifo_rd_en       (w_fifo_rd_en),
    .o_dma_start        (w_dma_start),
    .o_dma_src_addr     (w_dma_src_addr),
    .o_dma_len          (w_dma_len),
    .i_dma_done         (w_dma_done),
    .o_dp_start         (w_dp_start),
    .o_dp_op_type       (w_dp_op_type),
    .i_dp_done          (w_dp_done),
    .o_accel_busy       (o_accel_busy)
);

// 3. DMA Controller
dma_controller u_dma_controller (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_start            (w_dma_start),
    .i_src_addr         (w_dma_src_addr),
    .i_len              (w_dma_len),
    .o_done             (w_dma_done),
    .o_m_addr           (w_dma_m_addr),
    .o_m_req            (w_dma_m_req),
    .o_m_wen            (w_dma_m_wen),
    .i_m_rdata          (i_m_rdata),
    .i_m_ready          (i_m_ready),
    .o_buf_waddr        (w_buf_waddr),
    .o_buf_wdata        (w_buf_wdata),
    .o_buf_wen          (w_buf_wen)
);

// 4. Local Data Buffer (Dual-Port SRAM)
dual_port_sram #(
    .DATA_WIDTH ( `DATA_WIDTH ),
    .ADDR_WIDTH ( LOCAL_BUF_AW ),
    .DEPTH      ( LOCAL_BUF_DEPTH )
) u_local_buffer (
    .clk        (i_clk),
    .wea        (w_buf_wen),
    .addra      (w_buf_waddr[LOCAL_BUF_AW-1:0]),
    .dina       (w_buf_wdata),
    .addrb      (w_buf_raddr[LOCAL_BUF_AW-1:0]),
    .doutb      (w_buf_rdata)
);

// 5. Datapath Pipeline
bf_datapath u_bf_datapath (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_start            (w_dp_start),
    .i_op_type          (w_dp_op_type),
    .i_len              (w_dma_len),
    .o_done             (w_dp_done),
    .o_check_result     (w_check_result), // NEW
    .o_buf_raddr        (w_buf_raddr),
    .o_buf_ren          (w_buf_ren),
    .i_buf_rdata        (w_buf_rdata),
    .i_crypto_key       (r_crypto_key)
);

endmodule
