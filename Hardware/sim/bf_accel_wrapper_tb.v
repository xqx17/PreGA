// ============================================================================
//  Module: bf_accel_wrapper_tb
//  Description: 新的bf_accel_wrapper模块的顶层testbench�??
//               这个testbench验证完整的�?�封装好的IP核�??
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

`include "../rtl/bf_defines.vh"

module bf_accel_wrapper_tb;

//-----------------------------------------------------------------------------
// Testbench Parameters
//-----------------------------------------------------------------------------
parameter CLK_PERIOD = 20; // 100 MHz

//-----------------------------------------------------------------------------
// Testbench Signals
//-----------------------------------------------------------------------------
reg         clk;
reg         rst_n;


// BFM -> DUT Slave Port
wire [`ADDR_WIDTH-1:0]  s_addr_from_bfm;
wire [`DATA_WIDTH-1:0]  s_wdata_from_bfm;
wire                    s_wen_from_bfm;
wire                    s_ren_from_bfm;
wire [`DATA_WIDTH-1:0]  s_rdata_to_bfm;
wire                    s_ready_to_bfm;

// DUT Master Port -> Memory Model
wire [`ADDR_WIDTH-1:0]  m_addr_from_dut;
wire [`DATA_WIDTH-1:0]  m_wdata_from_dut;
wire                    m_wen_from_dut;
wire                    m_req_from_dut;
wire [`DATA_WIDTH-1:0]  m_rdata_to_dut;
wire                    m_ready_to_dut;

//-----------------------------------------------------------------------------
// Clock and Reset Generation
//-----------------------------------------------------------------------------
initial begin
    clk = 1'b0;
    forever #(CLK_PERIOD / 2) clk = ~clk;
    //forever #(0.5) clk = ~clk;
end

initial begin
    $display("========================================================");
    $display("= BFAccel Wrapper Testbench Starting...                =");
    $display("========================================================");

    rst_n = 1'b0;
    # (CLK_PERIOD * 5);
    rst_n = 1'b1;
    # (CLK_PERIOD * 5);

    run_all_tests;

    $display("\n========================================================");
    $display("= All Test Sequences Completed. Simulation Finished. =");
    $display("========================================================");
    $finish;
end

//-----------------------------------------------------------------------------
// DUT (Device Under Test) Instantiation
//-----------------------------------------------------------------------------
bf_accel_wrapper u_dut (
    .i_clk      (clk),
    .i_rst_n    (rst_n),
    .i_s_addr   (s_addr_from_bfm),
    .i_s_wdata  (s_wdata_from_bfm),
    .i_s_wen    (s_wen_from_bfm),
    .i_s_ren    (s_ren_from_bfm),
    .o_s_rdata  (s_rdata_to_bfm),
    .o_s_ready  (s_ready_to_bfm),
    .o_m_addr   (m_addr_from_dut),
    .o_m_wdata  (m_wdata_from_dut),
    .o_m_wen    (m_wen_from_dut),
    .o_m_req    (m_req_from_dut),
    .i_m_rdata  (m_rdata_to_dut),
    .i_m_ready  (m_ready_to_dut)
);

//-----------------------------------------------------------------------------
// BFM (Bus Functional Model) Instantiation
//-----------------------------------------------------------------------------
simple_bus_bfm u_bfm (
    .i_clk      (clk),
    .i_rst_n    (rst_n),
    .o_s_addr   (s_addr_from_bfm),
    .o_s_wdata  (s_wdata_from_bfm),
    .o_s_wen    (s_wen_from_bfm),
    .o_s_ren    (s_ren_from_bfm),
    .i_s_rdata  (s_rdata_to_bfm),
    .i_s_ready  (s_ready_to_bfm)
);

//-----------------------------------------------------------------------------
// Memory Model Instantiation
//-----------------------------------------------------------------------------
memory_model u_mem (
    .i_clk      (clk),
    .i_rst_n    (rst_n),
    .i_addr     (m_addr_from_dut),
    .i_wdata    (m_wdata_from_dut),
    .i_wen      (m_wen_from_dut),
    .i_req      (m_req_from_dut),
    .o_rdata    (m_rdata_to_dut),
    .o_ready    (m_ready_to_dut)
);

//-----------------------------------------------------------------------------
// Test Sequences
//-----------------------------------------------------------------------------
`include "test_sequences.vh"


endmodule
