// ============================================================================
//  Module: hash_engine
//  Description: A parallel hash engine that computes three different hash
//               values (djb2, sdbm, fnv1a) simultaneously for an input
//               data stream. It is designed to process data in a pipelined
//               fashion.
//  Standard: Verilog-2001
// ============================================================================
`timescale 1ns / 1ps

`include "bf_defines.vh"

module hash_engine (
    // --- Global Signals ---
    input                               i_clk,
    input                               i_rst_n,

    // --- Input Data Stream ---
    input                               i_data_valid,        // Indicates that i_data is valid
    input      [`DATA_WIDTH-1:0]        i_data,              // Input data word (32-bit)
    input                               i_data_is_first,     // Flag: this is the first word of a new element
    input                               i_data_is_last,      // Flag: this is the last word of an element

    // --- Output ---
    output reg                          o_hash_valid,        // Asserted for one cycle when all hashes are computed
    output reg [`DATA_WIDTH-1:0]        o_hash_0,            // Result for hash function 0 (djb2)
    output reg [`DATA_WIDTH-1:0]        o_hash_1,            // Result for hash function 1 (sdbm)
    output reg [`DATA_WIDTH-1:0]        o_hash_2             // Result for hash function 2 (fnv1a)
);

//-----------------------------------------------------------------------------
// Internal Registers for Hash Accumulators
//-----------------------------------------------------------------------------
reg [`DATA_WIDTH-1:0]       r_hash_djb2;
reg [`DATA_WIDTH-1:0]       r_hash_sdbm;
reg [`DATA_WIDTH-1:0]       r_hash_fnv1a;

//-----------------------------------------------------------------------------
// Combinational Logic for Next Hash Value Calculation
//-----------------------------------------------------------------------------
wire [`DATA_WIDTH-1:0]      w_next_hash_djb2;
wire [`DATA_WIDTH-1:0]      w_next_hash_sdbm;
wire [`DATA_WIDTH-1:0]      w_next_hash_fnv1a;

// --- DJB2 Hash Logic ---
// hash = ((hash << 5) + hash) + char;
// We process byte by byte for a 32-bit word.
// This is a multi-stage combinational logic block.
reg [`DATA_WIDTH-1:0] djb2_stage0, djb2_stage1, djb2_stage2, djb2_stage3;
always @(*) begin
    // Stage 0: Process byte 0
    djb2_stage0 = ((r_hash_djb2 << 5) + r_hash_djb2) + i_data[7:0];
    // Stage 1: Process byte 1
    djb2_stage1 = ((djb2_stage0 << 5) + djb2_stage0) + i_data[15:8];
    // Stage 2: Process byte 2
    djb2_stage2 = ((djb2_stage1 << 5) + djb2_stage1) + i_data[23:16];
    // Stage 3: Process byte 3
    djb2_stage3 = ((djb2_stage2 << 5) + djb2_stage2) + i_data[31:24];
end
assign w_next_hash_djb2 = djb2_stage3;

// --- SDBM Hash Logic ---
// hash = char + (hash << 6) + (hash << 16) - hash;
reg [`DATA_WIDTH-1:0] sdbm_stage0, sdbm_stage1, sdbm_stage2, sdbm_stage3;
always @(*) begin
    sdbm_stage0 = i_data[7:0]   + (r_hash_sdbm << 6) + (r_hash_sdbm << 16) - r_hash_sdbm;
    sdbm_stage1 = i_data[15:8]  + (sdbm_stage0 << 6) + (sdbm_stage0 << 16) - sdbm_stage0;
    sdbm_stage2 = i_data[23:16] + (sdbm_stage1 << 6) + (sdbm_stage1 << 16) - sdbm_stage1;
    sdbm_stage3 = i_data[31:24] + (sdbm_stage2 << 6) + (sdbm_stage2 << 16) - sdbm_stage2;
end
assign w_next_hash_sdbm = sdbm_stage3;

// --- FNV-1a Hash Logic ---
// hash = hash ^ char; hash = hash * 16777619;
parameter FNV_PRIME = 32'h01000193; // 16777619
reg [`DATA_WIDTH-1:0] fnv1a_stage0_xor, fnv1a_stage0_mult;
reg [`DATA_WIDTH-1:0] fnv1a_stage1_xor, fnv1a_stage1_mult;
reg [`DATA_WIDTH-1:0] fnv1a_stage2_xor, fnv1a_stage2_mult;
reg [`DATA_WIDTH-1:0] fnv1a_stage3_xor, fnv1a_stage3_mult;
always @(*) begin
    fnv1a_stage0_xor = r_hash_fnv1a ^ i_data[7:0];
    fnv1a_stage0_mult = fnv1a_stage0_xor * FNV_PRIME;
    fnv1a_stage1_xor = fnv1a_stage0_mult ^ i_data[15:8];
    fnv1a_stage1_mult = fnv1a_stage1_xor * FNV_PRIME;
    fnv1a_stage2_xor = fnv1a_stage1_mult ^ i_data[23:16];
    fnv1a_stage2_mult = fnv1a_stage2_xor * FNV_PRIME;
    fnv1a_stage3_xor = fnv1a_stage2_mult ^ i_data[31:24];
    fnv1a_stage3_mult = fnv1a_stage3_xor * FNV_PRIME;
end
assign w_next_hash_fnv1a = fnv1a_stage3_mult;

//-----------------------------------------------------------------------------
// Hash Accumulator Register Logic (Sequential)
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        r_hash_djb2  <= 32'h1505;      // Initial value for djb2 (5381)
        r_hash_sdbm  <= 32'h0;         // Initial value for sdbm
        r_hash_fnv1a <= 32'h811c9dc5;  // Initial value for FNV-1a (2166136261)
    end else begin
        if (i_data_is_first && i_data_valid) begin
            // If this is the first chunk of a new element, reset accumulators
            r_hash_djb2  <= 32'h1505;
            r_hash_sdbm  <= 32'h0;
            r_hash_fnv1a <= 32'h811c9dc5;
        end else if (i_data_valid) begin
            // For subsequent chunks, update with the next hash value
            r_hash_djb2  <= w_next_hash_djb2;
            r_hash_sdbm  <= w_next_hash_sdbm;
            r_hash_fnv1a <= w_next_hash_fnv1a;
        end
    end
end

//-----------------------------------------------------------------------------
// Output Logic (Sequential)
//-----------------------------------------------------------------------------
always @(posedge i_clk or negedge i_rst_n) begin
    if (!i_rst_n) begin
        o_hash_valid <= 1'b0;
        o_hash_0     <= 32'h0;
        o_hash_1     <= 32'h0;
        o_hash_2     <= 32'h0;
    end else begin
        // The output is valid for one cycle after the last data chunk is processed.
        if (i_data_is_last && i_data_valid) begin
            o_hash_valid <= 1'b1;
            o_hash_0     <= w_next_hash_djb2;
            o_hash_1     <= w_next_hash_sdbm;
            o_hash_2     <= w_next_hash_fnv1a;
        end else begin
            o_hash_valid <= 1'b0;
        end
    end
end

endmodule
