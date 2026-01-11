// ============================================================================
//  Module: crypto_scrambler (Simplified for Address-Only Obfuscation)
//  Description: A simple cryptographic scrambler that performs a bitwise
//               XOR operation on the input address with a secret key.
//               This version does not process data.
//  Standard: Verilog-2001
// ============================================================================
`timescale 1ns / 1ps

`include "bf_defines.vh"

module crypto_scrambler (
    // --- Configuration Input ---
    input       [63:0]              i_crypto_key,

    // --- Address Scrambling ---
    input       [`ADDR_WIDTH-1:0]   i_addr_plain,
    output      [`ADDR_WIDTH-1:0]   o_addr_scrambled
);

//-----------------------------------------------------------------------------
// Scrambling Logic
//-----------------------------------------------------------------------------
// Scramble the address for private memory access.
// We use the upper 32 bits of the 64-bit key for address scrambling.
assign o_addr_scrambled = i_addr_plain ^ i_crypto_key[63:32];

endmodule
