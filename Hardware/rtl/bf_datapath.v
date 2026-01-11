// ============================================================================
//  Module: bf_datapath (next-state 版本, 修复 multi-driven net & latch 报告)
//  Description: o_check_result / r_element_found / r_hash_idx 均采用
//               next-state 风格，消除综合器 latch & multi-driven 报告
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps
`include "bf_defines.vh"

module bf_datapath (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- Control Interface ---
    input                           i_start,
    input       [1:0]               i_op_type,
    input       [`DATA_WIDTH-1:0]   i_len,
    output reg                      o_done,
    output reg                      o_check_result,

    // --- Local Data Buffer Interface ---
    output reg  [`ADDR_WIDTH-1:0]   o_buf_raddr,
    output reg                      o_buf_ren,
    input       [`DATA_WIDTH-1:0]   i_buf_rdata,

    // --- Configuration Input ---
    input       [63:0]              i_crypto_key
);

//-----------------------------------------------------------------------------
// FSM State Definition
//-----------------------------------------------------------------------------
parameter DP_FSM_IDLE         = 4'b0000;
parameter DP_FSM_HASH_START   = 4'b0001;
parameter DP_FSM_HASH_WAIT    = 4'b0010;
parameter DP_FSM_MEM_READ     = 4'b0011;
parameter DP_FSM_MEM_LATCH    = 4'b0100;
parameter DP_FSM_MEM_WRITE    = 4'b0101;
parameter DP_FSM_NEXT_HASH    = 4'b0110;
parameter DP_FSM_CHECK_NEXT   = 4'b0111;
parameter DP_FSM_FINISH       = 4'b1000;

//-----------------------------------------------------------------------------
// Internal Registers and Wires
//-----------------------------------------------------------------------------
reg [3:0]                   r_dp_current_state;
reg [3:0]                   r_dp_next_state;

reg [1:0]                   r_hash_idx;       // 寄存器
reg [1:0]                   r_hash_idx_next;  // 新增 next-state 变量

parameter PVT_MEM_AW        = 11;
parameter PVT_MEM_DEPTH     = 2048;

reg  [PVT_MEM_AW-1:0]       r_pvt_mem_addr_plain;
reg  [7:0]                  r_pvt_mem_wdata;
reg                         r_pvt_mem_wen;
wire [7:0]                  w_pvt_mem_rdata;

reg                         r_element_found;       // 寄存器
reg                         r_element_found_next;  // 新增 next-state 变量

reg [7:0]                   w_next_pvt_mem_wdata;

reg                         r_data_valid_dly;
reg                         r_hash_data_is_first_dly;
reg                         r_hash_data_is_last_dly;

wire [31:0]                 w_hash_res_0, w_hash_res_1, w_hash_res_2;
wire                        w_hash_valid;
reg                         r_hash_data_is_first;
reg                         r_hash_data_is_last;
wire [`ADDR_WIDTH-1:0]      w_scrambled_addr;
reg [`DATA_WIDTH-1:0]       r_elements_processed;
reg [`DATA_WIDTH-1:0]       r_total_elements;
reg [1:0]                   r_op_type;
reg [31:0]                  r_hash_results [0:2];
wire [7:0]                  w_modified_counter;

//-----------------------------------------------------------------------------
// FSM Sequential Logic
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_dp_current_state   <= DP_FSM_IDLE;
        r_element_found      <= 1'b0;
        r_hash_idx           <= 2'b00;
        o_check_result       <= 1'b0;
        r_elements_processed <= 32'h0;
        r_total_elements     <= 32'h0;
        r_op_type            <= 2'b0;
        o_buf_raddr          <= 32'h0;
    end else begin
        r_dp_current_state <= r_dp_next_state;
        r_element_found    <= r_element_found_next; // next-state 更新
        r_hash_idx         <= r_hash_idx_next;      // next-state 更新

        if (i_start) begin
            r_op_type            <= i_op_type;
            r_total_elements     <= i_len >> 2;
            r_elements_processed <= 32'h0;
            o_buf_raddr          <= 32'h0;
        end else if (r_dp_current_state == DP_FSM_CHECK_NEXT) begin
            if (r_elements_processed < r_total_elements - 1) begin
                r_elements_processed <= r_elements_processed + 1;
                o_buf_raddr          <= o_buf_raddr + 4;
            end
        end

        // o_check_result 逻辑
        if (r_dp_current_state == DP_FSM_FINISH) begin
            if (r_op_type == `OP_CHECK) begin
                o_check_result <= r_element_found;
                r_element_found <= 1'b0; // 完成后清零
            end
        end else if (r_dp_current_state != DP_FSM_IDLE) begin
            o_check_result <= 1'b0;
        end
    end
end

//-----------------------------------------------------------------------------
// Private memory write data register
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_pvt_mem_wdata <= 8'h0;
    end else begin
        r_pvt_mem_wdata <= w_next_pvt_mem_wdata;
    end
end

//-----------------------------------------------------------------------------
// Delay control signals for hash engine
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_data_valid_dly         <= 1'b0;
        r_hash_data_is_first_dly <= 1'b0;
        r_hash_data_is_last_dly  <= 1'b0;
    end else begin
        r_data_valid_dly         <= o_buf_ren;
        r_hash_data_is_first_dly <= r_hash_data_is_first;
        r_hash_data_is_last_dly  <= r_hash_data_is_last;
    end
end

//-----------------------------------------------------------------------------
// Hash results capture
//-----------------------------------------------------------------------------
always @(posedge i_clk) begin
    if (w_hash_valid) begin
        r_hash_results[0] <= w_hash_res_0;
        r_hash_results[1] <= w_hash_res_1;
        r_hash_results[2] <= w_hash_res_2;
    end
end

//-----------------------------------------------------------------------------
// FSM Combinational Logic
//-----------------------------------------------------------------------------
always @(*) begin
    r_dp_next_state       = r_dp_current_state;
    o_done                = 1'b0;
    o_buf_ren             = 1'b0;
    r_pvt_mem_wen         = 1'b0;
    r_pvt_mem_addr_plain  = {PVT_MEM_AW{1'b0}};
    r_hash_data_is_first  = 1'b0;
    r_hash_data_is_last   = 1'b0;

    r_element_found_next  = r_element_found;  // 默认保持
    r_hash_idx_next       = r_hash_idx;       // 默认保持
    w_next_pvt_mem_wdata  = r_pvt_mem_wdata;  // 默认保持

    case (r_dp_current_state)
        DP_FSM_IDLE: begin
            if (i_start) begin
                if (i_len > 0) begin
                    r_dp_next_state      = DP_FSM_HASH_START;
                    r_hash_idx_next      = 2'b00; // 初始化
                    if (i_op_type == `OP_CHECK)
                        r_element_found_next = 1'b1;
                    else
                        r_element_found_next = 1'b0;
                end else begin
                    r_dp_next_state = DP_FSM_FINISH;
                end
            end
        end

        DP_FSM_HASH_START: begin
            o_buf_ren            = 1'b1;
            r_hash_data_is_first = 1'b1;
            r_hash_data_is_last  = 1'b1;
            r_dp_next_state      = DP_FSM_HASH_WAIT;
        end
        
        DP_FSM_HASH_WAIT: begin
            if (w_hash_valid)
                r_dp_next_state = DP_FSM_MEM_READ;
        end

        DP_FSM_MEM_READ: begin
            r_pvt_mem_addr_plain = r_hash_results[r_hash_idx][PVT_MEM_AW-1:0] + 1;
            r_dp_next_state      = DP_FSM_MEM_LATCH;
        end

        DP_FSM_MEM_LATCH: begin
            r_pvt_mem_addr_plain = r_hash_results[r_hash_idx][PVT_MEM_AW-1:0] + 1;
            w_next_pvt_mem_wdata = w_modified_counter;
            if (r_op_type == `OP_CHECK)
                r_dp_next_state = DP_FSM_NEXT_HASH;
            else
                r_dp_next_state = DP_FSM_MEM_WRITE;
        end

        DP_FSM_MEM_WRITE: begin
            r_pvt_mem_addr_plain = r_hash_results[r_hash_idx][PVT_MEM_AW-1:0] + 1;
            r_pvt_mem_wen        = 1'b1;
            r_dp_next_state      = DP_FSM_NEXT_HASH;
        end

        DP_FSM_NEXT_HASH: begin
            if (r_hash_idx < 2) begin
                r_hash_idx_next  = r_hash_idx + 1;
                r_dp_next_state  = DP_FSM_MEM_READ;
            end else begin
                if (w_pvt_mem_rdata == 8'h00)
                    r_element_found_next = 1'b0;
                r_dp_next_state = DP_FSM_CHECK_NEXT;
            end
        end

        DP_FSM_CHECK_NEXT: begin
            if (r_elements_processed < r_total_elements - 1) begin
                r_dp_next_state = DP_FSM_HASH_START;
                r_hash_idx_next = 2'b00; // 再次初始化
                if (r_op_type == `OP_CHECK)
                    r_element_found_next = 1'b1;
                else
                    r_element_found_next = 1'b0;
            end else begin
                r_dp_next_state = DP_FSM_FINISH;
            end
        end
        
        DP_FSM_FINISH: begin
            o_done           = 1'b1;
            r_dp_next_state  = DP_FSM_IDLE;
        end

        default: r_dp_next_state = DP_FSM_IDLE;
    endcase
end

//-----------------------------------------------------------------------------
// Instantiation & Data Path
//-----------------------------------------------------------------------------
hash_engine u_hash_engine (
    .i_clk              (i_clk),
    .i_rst_n            (i_rst_n),
    .i_data_valid       (r_data_valid_dly),
    .i_data             (i_buf_rdata),
    .i_data_is_first    (r_hash_data_is_first_dly),
    .i_data_is_last     (r_hash_data_is_last_dly),
    .o_hash_valid       (w_hash_valid),
    .o_hash_0           (w_hash_res_0),
    .o_hash_1           (w_hash_res_1),
    .o_hash_2           (w_hash_res_2)
);

crypto_scrambler u_crypto_scrambler (
    .i_crypto_key       (i_crypto_key),
    .i_addr_plain       ({{(32-PVT_MEM_AW){1'b0}}, r_pvt_mem_addr_plain}),
    .o_addr_scrambled   (w_scrambled_addr)
);

assign w_modified_counter = (r_op_type == `OP_ADD)    ? ((w_pvt_mem_rdata == 8'hFF) ? 8'hFF : w_pvt_mem_rdata + 1) :
                            (r_op_type == `OP_REMOVE) ? ((w_pvt_mem_rdata == 8'h00) ? 8'h00 : w_pvt_mem_rdata - 1) :
                            w_pvt_mem_rdata;

single_port_sram #(
    .DATA_WIDTH ( 8 ),
    .ADDR_WIDTH ( PVT_MEM_AW ),
    .DEPTH      ( PVT_MEM_DEPTH )
) u_private_memory (
    .clk        (i_clk),
    .we         (r_pvt_mem_wen),
    .addr       (w_scrambled_addr[PVT_MEM_AW-1:0]),
    .din        (r_pvt_mem_wdata),
    .dout       (w_pvt_mem_rdata)
);

endmodule
