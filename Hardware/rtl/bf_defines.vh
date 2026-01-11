// ============================================================================
//  File: bf_defines.vh (Corrected and Refined)
//  Description: Global definitions, parameters, and constants for the
//               Bloom Filter Accelerator (BFAccel) project.
//  Standard: Verilog-2001
// ============================================================================

`timescale 1ns / 1ps

//-----------------------------------------------------------------------------
// Custom Instruction OpCodes (for funct3 field)
//-----------------------------------------------------------------------------
`define OP_CFG_KEY      3'b000
`define OP_ADD          3'b001
`define OP_CHECK        3'b010
`define OP_REMOVE       3'b011
`define OP_STATUS       3'b100


//-----------------------------------------------------------------------------
// Main Control Unit FSM States
//-----------------------------------------------------------------------------
`define FSM_IDLE        3'b000
`define FSM_LATCH_REQ   3'b001
`define FSM_START_DMA   3'b010
`define FSM_WAIT_DMA    3'b011
`define FSM_START_DP    3'b100
`define FSM_WAIT_DP     3'b101


//-----------------------------------------------------------------------------
// Datapath Sub-FSM States (Expanded for full Read-Modify-Write cycle)
//-----------------------------------------------------------------------------
`define DP_FSM_IDLE         4'b0000
`define DP_FSM_HASH_START   4'b0001
`define DP_FSM_HASH_WAIT    4'b0010
`define DP_FSM_MEM_READ_1   4'b0011
`define DP_FSM_MEM_WRITE_1  4'b0100
`define DP_FSM_MEM_READ_2   4'b0101
`define DP_FSM_MEM_WRITE_2  4'b0110
`define DP_FSM_MEM_READ_3   4'b0111
`define DP_FSM_MEM_WRITE_3  4'b1000
`define DP_FSM_CHECK_NEXT   4'b1001
`define DP_FSM_FINISH       4'b1010


//-----------------------------------------------------------------------------
// Bus and Address Widths
//-----------------------------------------------------------------------------
`define ADDR_WIDTH      32
`define DATA_WIDTH      32


//-----------------------------------------------------------------------------
// Request Packet Bit Packing (Corrected)
//-----------------------------------------------------------------------------
// Defines how a request from the CPU is packed into a single wide word
// for the internal FIFO. The packet is formed by concatenating two 32-bit
// CPU writes: {High_Word, Low_Word}.
// Total width: 32 (High Word: Op+Len) + 32 (Low Word: Addr) = 64 bits.
`define REQ_PKT_WIDTH   64

// These ranges are for PARSING the 64-bit packet after it's dequeued.
// The BFM/CPU is responsible for packing the high word correctly.
// Let's assume the packing is: High Word = {2'b0, op_type[1:0], len[29:0]}
// For simplicity in Verilog-2001, we will just use ranges on the 64-bit vector.
`define REQ_OP_RANGE    63:61   // Operation type is in the upper bits of the high word
`define REQ_LEN_RANGE   60:32   // Length is in the lower bits of the high word
`define REQ_ADDR_RANGE  31:0    // Address is the entire low word


//-----------------------------------------------------------------------------
// Hash Engine Initial Values
//-----------------------------------------------------------------------------
`define HASH_DJB2_INIT    32'h00001505  // 5381
`define HASH_SDBM_INIT    32'h00000000  // 0
`define HASH_FNV1A_INIT   32'h811c9dc5  // 2166136261


//-----------------------------------------------------------------------------
// FNV-1a Prime Constant
//-----------------------------------------------------------------------------
`define FNV_PRIME         32'h01000193  // 16777619

