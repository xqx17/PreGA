// ============================================================================
//  Module: dual_port_sram
//  Description: A behavioral model of a generic dual-port Synchronous RAM.
//               This model is intended to be inferred as Block RAM (BRAM)
//               by FPGA synthesis tools.
//               - Port A is configured for writing.
//               - Port B is configured for reading.
//  Standard: Verilog-2001
// ============================================================================
`timescale 1ns / 1ps

module dual_port_sram #(
    parameter DATA_WIDTH = 32,   // Width of the data bus
    parameter ADDR_WIDTH = 8,    // Width of the address bus
    parameter DEPTH      = 256   // Depth of the memory (2^ADDR_WIDTH)
) (
    // --- Global Clock ---
    input                               clk,

    // --- Port A (Write Port) ---
    input                               wea,         // Write Enable for Port A
    input      [ADDR_WIDTH-1:0]         addra,       // Address for Port A
    input      [DATA_WIDTH-1:0]         dina,        // Data Input for Port A

    // --- Port B (Read Port) ---
    input      [ADDR_WIDTH-1:0]         addrb,       // Address for Port B
    output reg [DATA_WIDTH-1:0]         doutb        // Data Output for Port B
);

//-----------------------------------------------------------------------------
// Memory Storage Array
//-----------------------------------------------------------------------------
// This register array represents the physical storage of the SRAM.
reg [DATA_WIDTH-1:0] mem [0:DEPTH-1];

//-----------------------------------------------------------------------------
// Port A Write Logic
//-----------------------------------------------------------------------------
// This block handles write operations on Port A.
// Data is written on the positive edge of the clock when write enable is high.
always @(posedge clk) begin
    if (wea) begin
        mem[addra] <= dina;
    end
end

//-----------------------------------------------------------------------------
// Port B Read Logic
//-----------------------------------------------------------------------------
// This block handles read operations on Port B.
// The read is synchronous: data addressed by 'addrb' on one clock edge
// will appear on 'doutb' on the next clock edge.
always @(posedge clk) begin
    // The data output is registered, which is a common and robust
    // BRAM read mode (Read-First or Write-First depending on setup).
    doutb <= mem[addrb];
end

// Note on Read-During-Write behavior:
// The behavior when Port A writes to the same address that Port B is reading
// from in the same clock cycle is dependent on the specific BRAM architecture
// of the target FPGA. This model represents a "Read-First" or "Transparent"
// mode, where the old data is read out before the new data is written.
// For designs where this is critical, consult the FPGA documentation.
// In our BFAccel design, DMA writes and Datapath reads are to different
// addresses or happen at different times, so this conflict does not occur.

endmodule
