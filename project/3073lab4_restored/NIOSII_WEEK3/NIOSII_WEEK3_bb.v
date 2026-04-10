
module NIOSII_WEEK3 (
	accel_I2C_SDAT,
	accel_I2C_SCLK,
	accel_G_SENSOR_CS_N,
	accel_G_SENSOR_INT,
	clk_clk,
	dram_addr,
	dram_ba,
	dram_cas_n,
	dram_cke,
	dram_cs_n,
	dram_dq,
	dram_dqm,
	dram_ras_n,
	dram_we_n,
	gpio_export,
	led_export,
	pb_export,
	sw_export,
	hex0_export,
	hex1_export,
	hex2_export,
	hex3_export,
	hex4_export,
	hex5_export);	

	inout		accel_I2C_SDAT;
	output		accel_I2C_SCLK;
	output		accel_G_SENSOR_CS_N;
	input		accel_G_SENSOR_INT;
	input		clk_clk;
	output	[12:0]	dram_addr;
	output	[1:0]	dram_ba;
	output		dram_cas_n;
	output		dram_cke;
	output		dram_cs_n;
	inout	[15:0]	dram_dq;
	output	[1:0]	dram_dqm;
	output		dram_ras_n;
	output		dram_we_n;
	output	[1:0]	gpio_export;
	output	[9:0]	led_export;
	input	[1:0]	pb_export;
	input	[3:0]	sw_export;
	output	[7:0]	hex0_export;
	output	[7:0]	hex1_export;
	output	[7:0]	hex2_export;
	output	[7:0]	hex3_export;
	output	[7:0]	hex4_export;
	output	[7:0]	hex5_export;
endmodule
