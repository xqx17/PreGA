// ============================================================================
//  Module: single_port_sram
//  Description: A behavioral model of a generic single-port Synchronous RAM.
//               This model is intended to be inferred as Block RAM (BRAM)
//               by FPGA synthesis tools. It has one port for both reading
//               and writing.
//  Standard: Verilog-2001
// ============================================================================
//bloom过滤器存储载体

`timescale 1ns / 1ps

module single_port_sram #(
    parameter DATA_WIDTH = 8,    // Width of the data bus
    parameter ADDR_WIDTH = 11,   // Width of the address bus
    parameter DEPTH      = 2048  // Depth of the memory (2^ADDR_WIDTH)
) (
    // --- Global Clock ---
    input                               clk,

    // --- Single Port Interface ---
    input                               we,          // Write Enable. If 0, it's a read operation.
    input      [ADDR_WIDTH-1:0]         addr,        // Address for read or write
    input      [DATA_WIDTH-1:0]         din,         // Data Input for writing
    output reg [DATA_WIDTH-1:0]         dout         // Data Output for reading
);

//-----------------------------------------------------------------------------
// Memory Storage Array
//-----------------------------------------------------------------------------
// This register array represents the physical storage of the SRAM.
reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];

//-----------------------------------------------------------------------------
// Read/Write Logic
//-----------------------------------------------------------------------------
// This block handles both read and write operations through the single port.
always @(posedge clk) begin
    // --- Write Operation ---
    // If write enable is asserted, write the input data to the specified address.
    if (we) begin
        mem[addr] <= din;
    end

    // --- Read Operation ---
    // The read is synchronous. The data at the specified address is always
    // read and latched into the output register on every clock cycle.
    // If a write occurs to the same address, the old data is read out
    // before the new data is written (Read-First behavior).
    dout <= mem[addr];
end

// Note on Read-During-Write behavior:
// When 'we' is high, a write to 'addr' occurs. In the same clock cycle,
// the 'dout' will output the *old* value that was stored at 'addr' before
// the write operation completed. This is known as a "Read-First" or
// "Transparent Write" memory architecture, which is a common BRAM mode.
// This behavior is ideal for read-modify-write cycles:
// Cycle 1: Present address, 'we' is low.
// Cycle 2: 'dout' has the read data. Logic can modify it. Present address again,
//          present modified data on 'din', and set 'we' to high.
// Cycle 3: The new data is written into the memory.

endmodule
