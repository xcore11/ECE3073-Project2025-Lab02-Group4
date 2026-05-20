// *********************************
//
// VGA Controller Module
// Mixed mode version:
// - Image rectangle is always shown as grayscale.
// - Everything outside the image rectangle uses a 4-bit RGB palette for colorful UI.
//
// *********************************

module vga_controller (
    // 25MHz clock derived from CLOCK_50
    input VGA_CLK,

    // Controller interface with the pixel buffer
    input [3:0] VGA_DATA,
    output [18:0] VGA_ADDR,

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

    // These must match main.c
    parameter IMAGE_BOX_X = 114;
    parameter IMAGE_BOX_Y = 24;
    parameter IMAGE_BOX_W = 92;
    parameter IMAGE_BOX_H = 92;

    reg [18:0] H_ADDR = 19'd0;
    reg [18:0] V_ADDR = 19'd0;

    wire pixelValid;
    wire [18:0] H_MEM_ADDR;
    wire [18:0] V_MEM_ADDR;

    // 320x240 logical pixel coordinate after 640x480 -> 320x240 scaling
    wire [9:0] pixel_x;
    wire [8:0] pixel_y;
    wire inside_image_area;

    always @(posedge VGA_CLK) begin
        if (H_ADDR < H_CYCLE - 1)
            H_ADDR <= H_ADDR + 1'b1;
        else
            H_ADDR <= 19'd0;
    end

    always @(posedge VGA_CLK) begin
        if (H_ADDR == H_CYCLE - 1) begin
            if (V_ADDR < V_CYCLE - 1)
                V_ADDR <= V_ADDR + 1'b1;
            else
                V_ADDR <= 19'd0;
        end
    end

    assign VGA_HS = ~(H_ADDR < H_SYNC);
    assign VGA_VS = ~(V_ADDR < V_SYNC);

    assign pixelValid =
        (H_ADDR > (H_SYNC + H_BACK) && H_ADDR < (H_CYCLE - H_FRONT)) &&
        (V_ADDR > (V_SYNC + V_BACK) && V_ADDR < (V_CYCLE - V_FRONT));

    assign H_MEM_ADDR = H_ADDR - (H_SYNC + H_BACK) + 1;
    assign V_MEM_ADDR = V_ADDR - (V_SYNC + V_BACK);

    assign pixel_x = H_MEM_ADDR[18:1]; // same as H_MEM_ADDR >> 1
    assign pixel_y = V_MEM_ADDR[18:1]; // same as V_MEM_ADDR >> 1

    assign inside_image_area =
        (pixel_x >= IMAGE_BOX_X) && (pixel_x < (IMAGE_BOX_X + IMAGE_BOX_W)) &&
        (pixel_y >= IMAGE_BOX_Y) && (pixel_y < (IMAGE_BOX_Y + IMAGE_BOX_H));

    /*
       Final color decision:

       1. Outside active video: black.
       2. Inside image rectangle: treat VGA_DATA as grayscale.
          This keeps your SPI/captured image grayscale.
       3. Outside image rectangle: treat VGA_DATA as a color index.
          This makes your C-drawn UI colorful.
    */
    always @(*) begin
        if (!pixelValid) begin
            VGA_R = 4'h0;
            VGA_G = 4'h0;
            VGA_B = 4'h0;
        end
        else if (inside_image_area) begin
            VGA_R = VGA_DATA;
            VGA_G = VGA_DATA;
            VGA_B = VGA_DATA;
        end
        else begin
            case (VGA_DATA)
                4'h0: begin VGA_R = 4'h0; VGA_G = 4'h0; VGA_B = 4'h0; end // black
                4'h1: begin VGA_R = 4'h0; VGA_G = 4'h0; VGA_B = 4'h5; end // navy
                4'h2: begin VGA_R = 4'h0; VGA_G = 4'h2; VGA_B = 4'h9; end // panel blue
                4'h3: begin VGA_R = 4'h0; VGA_G = 4'h0; VGA_B = 4'hF; end // blue
                4'h4: begin VGA_R = 4'h0; VGA_G = 4'hF; VGA_B = 4'h0; end // green
                4'h5: begin VGA_R = 4'h0; VGA_G = 4'hF; VGA_B = 4'hF; end // cyan
                4'h6: begin VGA_R = 4'hF; VGA_G = 4'h0; VGA_B = 4'h0; end // red
                4'h7: begin VGA_R = 4'hF; VGA_G = 4'h0; VGA_B = 4'hF; end // magenta
                4'h8: begin VGA_R = 4'h4; VGA_G = 4'h4; VGA_B = 4'h4; end // dark gray
                4'h9: begin VGA_R = 4'h6; VGA_G = 4'h6; VGA_B = 4'h6; end // gray
                4'hA: begin VGA_R = 4'h8; VGA_G = 4'h8; VGA_B = 4'h8; end // gray
                4'hB: begin VGA_R = 4'hA; VGA_G = 4'hA; VGA_B = 4'hA; end // light gray
                4'hC: begin VGA_R = 4'hC; VGA_G = 4'hC; VGA_B = 4'hC; end // light gray
                4'hD: begin VGA_R = 4'hE; VGA_G = 4'hE; VGA_B = 4'hE; end // almost white
                4'hE: begin VGA_R = 4'hF; VGA_G = 4'hF; VGA_B = 4'h0; end // yellow
                4'hF: begin VGA_R = 4'hF; VGA_G = 4'hF; VGA_B = 4'hF; end // white
                default: begin VGA_R = 4'h0; VGA_G = 4'h0; VGA_B = 4'h0; end
            endcase
        end
    end

    // Scale the addresses to 320x240 instead of 640x480.
    assign VGA_ADDR = ((H_MEM_ADDR >> 1) + ((V_MEM_ADDR >> 1) * 320));

endmodule
