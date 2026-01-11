// ============================================================================
//  Module: bus_interface (Final Corrected Version)
//  Description: A standalone slave bus interface for the BFAccel. This final
//               version has the fully restored and correct read logic for the
//               status register and creates a robust pipeline for commands.
//  Standard: Verilog-20ool
// ============================================================================

`timescale 1ns / 1ps

`include "bf_defines.vh"

module bus_interface (
    // --- Global Signals ---
    input                           i_clk,
    input                           i_rst_n,

    // --- External Slave Bus Interface (from simple_bus) ---
    input       [`ADDR_WIDTH-1:0]   i_s_addr,
    input       [`DATA_WIDTH-1:0]   i_s_wdata,
    input                           i_s_wen,
    input                           i_s_ren,
    output reg  [`DATA_WIDTH-1:0]   o_s_rdata,
    output wire                     o_s_ready,

    // --- Internal Interface (to bf_accel_core) ---
    output reg                      o_cpu_cmd_valid,
    output reg  [`REQ_PKT_WIDTH-1:0]  o_cpu_cmd_packet,
    input                           i_fifo_full,
    output reg                      o_key_we,
    output reg  [63:0]              o_key_wdata,
    input                           i_accel_busy,
    input                           i_check_result // NEW: Input for the check result bit
);

//-----------------------------------------------------------------------------
// Local Parameters for Address Decoding
//-----------------------------------------------------------------------------
parameter ADDR_CMD_FIFO_LOW   = 32'h00;
parameter ADDR_CMD_FIFO_HIGH  = 32'h04;
parameter ADDR_STATUS_REG     = 32'h08;
parameter ADDR_KEY_LOW        = 32'h0C;
parameter ADDR_KEY_HIGH       = 32'h10;

//-----------------------------------------------------------------------------
// Internal Registers for Pipelining
//-----------------------------------------------------------------------------
reg [`DATA_WIDTH-1:0]   r_temp_low_word;
reg                     r_trigger_cmd_valid; // Internal signal for pipelining

//-----------------------------------------------------------------------------
// Bus Ready Signal
//-----------------------------------------------------------------------------
assign o_s_ready = !i_fifo_full;

//-----------------------------------------------------------------------------
// Read Logic (Combinational) - FIXED
//-----------------------------------------------------------------------------
always @(*) begin
    o_s_rdata = 32'h0;
    if (i_s_ren) begin
        case (i_s_addr)
            ADDR_STATUS_REG: begin
                // **FIXED**: Correctly assign all status bits to the read data bus.
                // Format: {check_result, ..., busy_bit, fifo_full_bit}
                o_s_rdata = {i_check_result, 29'b0, i_accel_busy, i_fifo_full};
            end
            default: begin
                o_s_rdata = 32'h0;
            end
        endcase
    end
end

//-----------------------------------------------------------------------------
// Write Logic (Pipelined for Data/Valid Robustness)
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_temp_low_word     <= 32'h0;
        o_cpu_cmd_packet    <= 64'h0;
        o_key_we            <= 1'b0;
        o_key_wdata         <= 64'h0;
        r_trigger_cmd_valid <= 1'b0;
        o_cpu_cmd_valid     <= 1'b0;
    end else begin
        // Pipeline Stage 2: The output valid signal is a registered version
        // of the internal trigger from the previous cycle.
        o_cpu_cmd_valid <= r_trigger_cmd_valid;
        
        // Default to de-asserting the trigger and other one-shot signals
        r_trigger_cmd_valid <= 1'b0;
        o_key_we            <= 1'b0;

        if (i_s_wen) begin
            case (i_s_addr)
                ADDR_CMD_FIFO_LOW: begin
                    // Latch the low word of the command
                    r_temp_low_word <= i_s_wdata;
                end
                ADDR_CMD_FIFO_HIGH: begin
                    // Pipeline Stage 1: Assemble the data packet and set the internal trigger.
                    // The data will be stable on the output register in this cycle.
                    // The valid signal will be asserted in the *next* cycle.
                    o_cpu_cmd_packet    <= {i_s_wdata, r_temp_low_word};
                    r_trigger_cmd_valid <= 1'b1;
                end
                ADDR_KEY_LOW: begin
                    // Latch the low word of the key
                    r_temp_low_word <= i_s_wdata;
                end
                ADDR_KEY_HIGH: begin
                    // For the key, a single-cycle assertion is generally safe
                    // as it's a configuration register, not a FIFO trigger.
                    o_key_we    <= 1'b1;
                    o_key_wdata <= {i_s_wdata, r_temp_low_word};
                end
            endcase
        end
    end
end

endmodule
