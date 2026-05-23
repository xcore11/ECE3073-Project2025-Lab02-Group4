// *********************************
//
// VGA Controller Module
// Direct RGB332 framebuffer version.
//
// This version is matched to the altsyncram-based pxl.v.
// The RAM has registered read address and registered output, so the
// active-video signal is delayed by 2 VGA clocks before RGB output.
//
// Pixel RAM stores one 8-bit RGB332 pixel per logical 320x240 pixel:
//
//     VGA_DATA[7:5] = Red   intensity, 0 to 7
//     VGA_DATA[4:2] = Green intensity, 0 to 7
//     VGA_DATA[1:0] = Blue  intensity, 0 to 3
//
// The controller expands RGB332 to physical RGB444 VGA pins.
// Logical framebuffer size: 320x240
// Physical VGA timing:       640x480 @ 60 Hz, 25 MHz pixel clock
// Scaling:                   each framebuffer pixel is doubled 2x2
//
// *********************************

module vga_controller (
    // 25MHz clock derived from CLOCK_50
    input VGA_CLK,

    // Controller interface with the pixel buffer
    input [7:0] VGA_DATA,
    output [16:0] VGA_ADDR,

    // Pixel data output
    output reg [3:0] VGA_R,
    output reg [3:0] VGA_G,
    output reg [3:0] VGA_B,

    // VGA timing output
    output VGA_HS,
    output VGA_VS
);

    parameter H_SYNC  = 96;
    parameter H_BACK  = 48;
    parameter H_FRONT = 16;
    parameter H_CYCLE = 800;

    parameter V_SYNC  = 2;
    parameter V_BACK  = 33;
    parameter V_FRONT = 10;
    parameter V_CYCLE = 525;

    parameter FB_WIDTH  = 320;
    parameter FB_HEIGHT = 240;

    reg [9:0] h_count = 10'd0;
    reg [9:0] v_count = 10'd0;

    wire active_video;
    wire [9:0] active_x_640;
    wire [8:0] active_y_480;
    wire [8:0] fb_x;
    wire [7:0] fb_y;
    wire [16:0] fb_addr_calc;

    // The altsyncram pxl.v has address_reg_b=CLOCK0 and outdata_reg_b=CLOCK0.
    // That means the pixel returned on VGA_DATA belongs to the address from
    // about two VGA clocks earlier. Delay active_video by the same amount so
    // blanking stays aligned with valid RGB data.
    reg active_video_d1 = 1'b0;
    reg active_video_d2 = 1'b0;

    always @(posedge VGA_CLK) begin
        if (h_count == H_CYCLE - 1) begin
            h_count <= 10'd0;

            if (v_count == V_CYCLE - 1)
                v_count <= 10'd0;
            else
                v_count <= v_count + 10'd1;
        end
        else begin
            h_count <= h_count + 10'd1;
        end
    end

    assign VGA_HS = ~(h_count < H_SYNC);
    assign VGA_VS = ~(v_count < V_SYNC);

    assign active_video =
        (h_count >= (H_SYNC + H_BACK)) &&
        (h_count <  (H_SYNC + H_BACK + 640)) &&
        (v_count >= (V_SYNC + V_BACK)) &&
        (v_count <  (V_SYNC + V_BACK + 480));

    assign active_x_640 = h_count - (H_SYNC + H_BACK);
    assign active_y_480 = v_count - (V_SYNC + V_BACK);

    // 640x480 physical pixel coordinate -> 320x240 framebuffer coordinate.
    assign fb_x = active_x_640[9:1];
    assign fb_y = active_y_480[8:1];

    assign fb_addr_calc = fb_x + (fb_y * FB_WIDTH);
    assign VGA_ADDR = active_video ? fb_addr_calc : 17'd0;

    always @(posedge VGA_CLK) begin
        active_video_d1 <= active_video;
        active_video_d2 <= active_video_d1;

        if (active_video_d2) begin
            // RGB332 -> RGB444 expansion.
            // Repeating the most-significant source bit gives better brightness scaling
            // than just padding zeros.
            VGA_R <= {VGA_DATA[7:5], VGA_DATA[7]};
            VGA_G <= {VGA_DATA[4:2], VGA_DATA[4]};
            VGA_B <= {VGA_DATA[1:0], VGA_DATA[1:0]};
        end
        else begin
            VGA_R <= 4'h0;
            VGA_G <= 4'h0;
            VGA_B <= 4'h0;
        end
    end

endmodule
