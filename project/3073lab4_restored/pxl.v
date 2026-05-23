// megafunction wizard: %RAM: 2-PORT%
// GENERATION: STANDARD
// MODULE: altsyncram
//
// ============================================================
// File Name: pxl.v
// Megafunction Name(s):
//             altsyncram
//
// RGB444 version for 320x240 VGA framebuffer.
// This keeps the same Quartus/Intel altsyncram style as your
// original pxl.v, but changes pixel width from 4 bits to 12 bits.
//
// Pixel format:
//     data[11:8] = R[3:0]
//     data[7:4]  = G[3:0]
//     data[3:0]  = B[3:0]
//
// Addressing:
//     addr = x + y * 320
//     valid framebuffer addresses: 0 to 76799
//
// Notes:
// - Address ports stay 17-bit so the schematic stays simple.
// - Memory depth is 76800 words, not 131072 words, to avoid wasting
//   block RAM after widening each pixel to 12 bits.
// - The important old MegaWizard settings are preserved:
//     operation_mode = DUAL_PORT
//     intended_device_family = MAX 10
//     address_reg_b = CLOCK0
//     outdata_reg_b = CLOCK0
//     read_during_write_mode_mixed_ports = DONT_CARE
// ============================================================

// synopsys translate_off
`timescale 1 ps / 1 ps
// synopsys translate_on

module pxl (
    clock,
    data,
    rdaddress,
    wraddress,
    wren,
    q
);

    input         clock;
    input  [11:0] data;
    input  [16:0] rdaddress;
    input  [16:0] wraddress;
    input         wren;
    output [11:0] q;

`ifndef ALTERA_RESERVED_QIS
// synopsys translate_off
`endif
    tri1          clock;
    tri0          wren;
`ifndef ALTERA_RESERVED_QIS
// synopsys translate_on
`endif

    wire [11:0] sub_wire0;
    wire [11:0] q = sub_wire0[11:0];

    altsyncram altsyncram_component (
        .address_a      (wraddress),
        .address_b      (rdaddress),
        .clock0         (clock),
        .data_a         (data),
        .wren_a         (wren),
        .q_b            (sub_wire0),
        .aclr0          (1'b0),
        .aclr1          (1'b0),
        .addressstall_a (1'b0),
        .addressstall_b (1'b0),
        .byteena_a      (1'b1),
        .byteena_b      (1'b1),
        .clock1         (1'b1),
        .clocken0       (1'b1),
        .clocken1       (1'b1),
        .clocken2       (1'b1),
        .clocken3       (1'b1),
        .data_b         ({12{1'b1}}),
        .eccstatus      (),
        .q_a            (),
        .rden_a         (1'b1),
        .rden_b         (1'b1),
        .wren_b         (1'b0)
    );

    defparam
        altsyncram_component.address_aclr_b = "NONE",
        altsyncram_component.address_reg_b = "CLOCK0",
        altsyncram_component.clock_enable_input_a = "BYPASS",
        altsyncram_component.clock_enable_input_b = "BYPASS",
        altsyncram_component.clock_enable_output_b = "BYPASS",
        altsyncram_component.intended_device_family = "MAX 10",
        altsyncram_component.lpm_type = "altsyncram",
        altsyncram_component.numwords_a = 76800,
        altsyncram_component.numwords_b = 76800,
        altsyncram_component.operation_mode = "DUAL_PORT",
        altsyncram_component.outdata_aclr_b = "NONE",
        altsyncram_component.outdata_reg_b = "CLOCK0",
        altsyncram_component.power_up_uninitialized = "FALSE",
        altsyncram_component.read_during_write_mode_mixed_ports = "DONT_CARE",
        altsyncram_component.widthad_a = 17,
        altsyncram_component.widthad_b = 17,
        altsyncram_component.width_a = 12,
        altsyncram_component.width_b = 12,
        altsyncram_component.width_byteena_a = 1;

endmodule
