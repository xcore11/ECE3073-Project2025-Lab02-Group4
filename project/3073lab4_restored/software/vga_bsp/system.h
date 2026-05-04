/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'vga_proc' in SOPC Builder design 'NIOSII_WEEK3'
 * SOPC Builder design path: ../../NIOSII_WEEK3.sopcinfo
 *
 * Generated: Sun May 03 19:40:49 SGT 2026
 */

/*
 * DO NOT MODIFY THIS FILE
 *
 * Changing this file will have subtle consequences
 * which will almost certainly lead to a nonfunctioning
 * system. If you do modify this file, be aware that your
 * changes will be overwritten and lost when this file
 * is generated again.
 *
 * DO NOT MODIFY THIS FILE
 */

/*
 * License Agreement
 *
 * Copyright (c) 2008
 * Altera Corporation, San Jose, California, USA.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This agreement shall be governed in all respects by the laws of the State
 * of California and by the laws of the United States of America.
 */

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

/* Include definitions from linker script generator */
#include "linker.h"


/*
 * CPU configuration
 *
 */

#define ALT_CPU_ARCHITECTURE "altera_nios2_gen2"
#define ALT_CPU_BIG_ENDIAN 0
#define ALT_CPU_BREAK_ADDR 0x00008820
#define ALT_CPU_CPU_ARCH_NIOS2_R1
#define ALT_CPU_CPU_FREQ 50000000u
#define ALT_CPU_CPU_ID_SIZE 1
#define ALT_CPU_CPU_ID_VALUE 0x00000000
#define ALT_CPU_CPU_IMPLEMENTATION "fast"
#define ALT_CPU_DATA_ADDR_WIDTH 0x1c
#define ALT_CPU_DCACHE_BYPASS_MASK 0x80000000
#define ALT_CPU_DCACHE_LINE_SIZE 32
#define ALT_CPU_DCACHE_LINE_SIZE_LOG2 5
#define ALT_CPU_DCACHE_SIZE 2048
#define ALT_CPU_EXCEPTION_ADDR 0x00004020
#define ALT_CPU_FLASH_ACCELERATOR_LINES 0
#define ALT_CPU_FLASH_ACCELERATOR_LINE_SIZE 0
#define ALT_CPU_FLUSHDA_SUPPORTED
#define ALT_CPU_FREQ 50000000
#define ALT_CPU_HARDWARE_DIVIDE_PRESENT 0
#define ALT_CPU_HARDWARE_MULTIPLY_PRESENT 1
#define ALT_CPU_HARDWARE_MULX_PRESENT 0
#define ALT_CPU_HAS_DEBUG_CORE 1
#define ALT_CPU_HAS_DEBUG_STUB
#define ALT_CPU_HAS_EXTRA_EXCEPTION_INFO
#define ALT_CPU_HAS_ILLEGAL_INSTRUCTION_EXCEPTION
#define ALT_CPU_HAS_JMPI_INSTRUCTION
#define ALT_CPU_ICACHE_LINE_SIZE 32
#define ALT_CPU_ICACHE_LINE_SIZE_LOG2 5
#define ALT_CPU_ICACHE_SIZE 4096
#define ALT_CPU_INITDA_SUPPORTED
#define ALT_CPU_INST_ADDR_WIDTH 0x1b
#define ALT_CPU_NAME "vga_proc"
#define ALT_CPU_NUM_OF_SHADOW_REG_SETS 0
#define ALT_CPU_OCI_VERSION 1
#define ALT_CPU_RESET_ADDR 0x00004000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x00008820
#define NIOS2_CPU_ARCH_NIOS2_R1
#define NIOS2_CPU_FREQ 50000000u
#define NIOS2_CPU_ID_SIZE 1
#define NIOS2_CPU_ID_VALUE 0x00000000
#define NIOS2_CPU_IMPLEMENTATION "fast"
#define NIOS2_DATA_ADDR_WIDTH 0x1c
#define NIOS2_DCACHE_BYPASS_MASK 0x80000000
#define NIOS2_DCACHE_LINE_SIZE 32
#define NIOS2_DCACHE_LINE_SIZE_LOG2 5
#define NIOS2_DCACHE_SIZE 2048
#define NIOS2_EXCEPTION_ADDR 0x00004020
#define NIOS2_FLASH_ACCELERATOR_LINES 0
#define NIOS2_FLASH_ACCELERATOR_LINE_SIZE 0
#define NIOS2_FLUSHDA_SUPPORTED
#define NIOS2_HARDWARE_DIVIDE_PRESENT 0
#define NIOS2_HARDWARE_MULTIPLY_PRESENT 1
#define NIOS2_HARDWARE_MULX_PRESENT 0
#define NIOS2_HAS_DEBUG_CORE 1
#define NIOS2_HAS_DEBUG_STUB
#define NIOS2_HAS_EXTRA_EXCEPTION_INFO
#define NIOS2_HAS_ILLEGAL_INSTRUCTION_EXCEPTION
#define NIOS2_HAS_JMPI_INSTRUCTION
#define NIOS2_ICACHE_LINE_SIZE 32
#define NIOS2_ICACHE_LINE_SIZE_LOG2 5
#define NIOS2_ICACHE_SIZE 4096
#define NIOS2_INITDA_SUPPORTED
#define NIOS2_INST_ADDR_WIDTH 0x1b
#define NIOS2_NUM_OF_SHADOW_REG_SETS 0
#define NIOS2_OCI_VERSION 1
#define NIOS2_RESET_ADDR 0x00004000


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_MUTEX
#define __ALTERA_AVALON_NEW_SDRAM_CONTROLLER
#define __ALTERA_AVALON_ONCHIP_MEMORY2
#define __ALTERA_AVALON_PIO
#define __ALTERA_NIOS2_GEN2
#define __ALTERA_UP_AVALON_ACCELEROMETER_SPI


/*
 * System configuration
 *
 */

#define ALT_DEVICE_FAMILY "MAX 10"
#define ALT_IRQ_BASE NULL
#define ALT_LEGACY_INTERRUPT_API_PRESENT
#define ALT_LOG_PORT "/dev/null"
#define ALT_LOG_PORT_BASE 0x0
#define ALT_LOG_PORT_DEV null
#define ALT_LOG_PORT_TYPE ""
#define ALT_NUM_EXTERNAL_INTERRUPT_CONTROLLERS 0
#define ALT_NUM_INTERNAL_INTERRUPT_CONTROLLERS 1
#define ALT_NUM_INTERRUPT_CONTROLLERS 1
#define ALT_STDERR "/dev/jtag_vga"
#define ALT_STDERR_BASE 0x90c8
#define ALT_STDERR_DEV jtag_vga
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtag_vga"
#define ALT_STDIN_BASE 0x90c8
#define ALT_STDIN_DEV jtag_vga
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtag_vga"
#define ALT_STDOUT_BASE 0x90c8
#define ALT_STDOUT_DEV jtag_vga
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "NIOSII_WEEK3"


/*
 * accelerometer_spi_0 configuration
 *
 */

#define ACCELEROMETER_SPI_0_BASE 0x90d0
#define ACCELEROMETER_SPI_0_IRQ 2
#define ACCELEROMETER_SPI_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define ACCELEROMETER_SPI_0_NAME "/dev/accelerometer_spi_0"
#define ACCELEROMETER_SPI_0_SPAN 2
#define ACCELEROMETER_SPI_0_TYPE "altera_up_avalon_accelerometer_spi"
#define ALT_MODULE_CLASS_accelerometer_spi_0 altera_up_avalon_accelerometer_spi


/*
 * hal configuration
 *
 */

#define ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API
#define ALT_MAX_FD 32
#define ALT_SYS_CLK none
#define ALT_TIMESTAMP_CLK none


/*
 * jtag_vga configuration
 *
 */

#define ALT_MODULE_CLASS_jtag_vga altera_avalon_jtag_uart
#define JTAG_VGA_BASE 0x90c8
#define JTAG_VGA_IRQ 0
#define JTAG_VGA_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAG_VGA_NAME "/dev/jtag_vga"
#define JTAG_VGA_READ_DEPTH 64
#define JTAG_VGA_READ_THRESHOLD 8
#define JTAG_VGA_SPAN 8
#define JTAG_VGA_TYPE "altera_avalon_jtag_uart"
#define JTAG_VGA_WRITE_DEPTH 64
#define JTAG_VGA_WRITE_THRESHOLD 8


/*
 * mutex_0 configuration
 *
 */

#define ALT_MODULE_CLASS_mutex_0 altera_avalon_mutex
#define MUTEX_0_BASE 0x8011208
#define MUTEX_0_IRQ -1
#define MUTEX_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define MUTEX_0_NAME "/dev/mutex_0"
#define MUTEX_0_OWNER_INIT 0
#define MUTEX_0_OWNER_WIDTH 16
#define MUTEX_0_SPAN 8
#define MUTEX_0_TYPE "altera_avalon_mutex"
#define MUTEX_0_VALUE_INIT 0
#define MUTEX_0_VALUE_WIDTH 16


/*
 * new_sdram_controller_0 configuration
 *
 */

#define ALT_MODULE_CLASS_new_sdram_controller_0 altera_avalon_new_sdram_controller
#define NEW_SDRAM_CONTROLLER_0_BASE 0x4000000
#define NEW_SDRAM_CONTROLLER_0_CAS_LATENCY 3
#define NEW_SDRAM_CONTROLLER_0_CONTENTS_INFO
#define NEW_SDRAM_CONTROLLER_0_INIT_NOP_DELAY 0.0
#define NEW_SDRAM_CONTROLLER_0_INIT_REFRESH_COMMANDS 2
#define NEW_SDRAM_CONTROLLER_0_IRQ -1
#define NEW_SDRAM_CONTROLLER_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define NEW_SDRAM_CONTROLLER_0_IS_INITIALIZED 1
#define NEW_SDRAM_CONTROLLER_0_NAME "/dev/new_sdram_controller_0"
#define NEW_SDRAM_CONTROLLER_0_POWERUP_DELAY 100.0
#define NEW_SDRAM_CONTROLLER_0_REFRESH_PERIOD 15.625
#define NEW_SDRAM_CONTROLLER_0_REGISTER_DATA_IN 1
#define NEW_SDRAM_CONTROLLER_0_SDRAM_ADDR_WIDTH 0x19
#define NEW_SDRAM_CONTROLLER_0_SDRAM_BANK_WIDTH 2
#define NEW_SDRAM_CONTROLLER_0_SDRAM_COL_WIDTH 10
#define NEW_SDRAM_CONTROLLER_0_SDRAM_DATA_WIDTH 16
#define NEW_SDRAM_CONTROLLER_0_SDRAM_NUM_BANKS 4
#define NEW_SDRAM_CONTROLLER_0_SDRAM_NUM_CHIPSELECTS 1
#define NEW_SDRAM_CONTROLLER_0_SDRAM_ROW_WIDTH 13
#define NEW_SDRAM_CONTROLLER_0_SHARED_DATA 0
#define NEW_SDRAM_CONTROLLER_0_SIM_MODEL_BASE 0
#define NEW_SDRAM_CONTROLLER_0_SPAN 67108864
#define NEW_SDRAM_CONTROLLER_0_STARVATION_INDICATOR 0
#define NEW_SDRAM_CONTROLLER_0_TRISTATE_BRIDGE_SLAVE ""
#define NEW_SDRAM_CONTROLLER_0_TYPE "altera_avalon_new_sdram_controller"
#define NEW_SDRAM_CONTROLLER_0_T_AC 5.5
#define NEW_SDRAM_CONTROLLER_0_T_MRD 3
#define NEW_SDRAM_CONTROLLER_0_T_RCD 20.0
#define NEW_SDRAM_CONTROLLER_0_T_RFC 70.0
#define NEW_SDRAM_CONTROLLER_0_T_RP 20.0
#define NEW_SDRAM_CONTROLLER_0_T_WR 14.0


/*
 * pio_ImgAddr configuration
 *
 */

#define ALT_MODULE_CLASS_pio_ImgAddr altera_avalon_pio
#define PIO_IMGADDR_BASE 0x90a0
#define PIO_IMGADDR_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_IMGADDR_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_IMGADDR_CAPTURE 0
#define PIO_IMGADDR_DATA_WIDTH 17
#define PIO_IMGADDR_DO_TEST_BENCH_WIRING 0
#define PIO_IMGADDR_DRIVEN_SIM_VALUE 0
#define PIO_IMGADDR_EDGE_TYPE "NONE"
#define PIO_IMGADDR_FREQ 50000000
#define PIO_IMGADDR_HAS_IN 0
#define PIO_IMGADDR_HAS_OUT 1
#define PIO_IMGADDR_HAS_TRI 0
#define PIO_IMGADDR_IRQ -1
#define PIO_IMGADDR_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_IMGADDR_IRQ_TYPE "NONE"
#define PIO_IMGADDR_NAME "/dev/pio_ImgAddr"
#define PIO_IMGADDR_RESET_VALUE 0
#define PIO_IMGADDR_SPAN 16
#define PIO_IMGADDR_TYPE "altera_avalon_pio"


/*
 * pio_camready configuration
 *
 */

#define ALT_MODULE_CLASS_pio_camready altera_avalon_pio
#define PIO_CAMREADY_BASE 0x9090
#define PIO_CAMREADY_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_CAMREADY_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_CAMREADY_CAPTURE 0
#define PIO_CAMREADY_DATA_WIDTH 1
#define PIO_CAMREADY_DO_TEST_BENCH_WIRING 0
#define PIO_CAMREADY_DRIVEN_SIM_VALUE 0
#define PIO_CAMREADY_EDGE_TYPE "NONE"
#define PIO_CAMREADY_FREQ 50000000
#define PIO_CAMREADY_HAS_IN 1
#define PIO_CAMREADY_HAS_OUT 0
#define PIO_CAMREADY_HAS_TRI 0
#define PIO_CAMREADY_IRQ -1
#define PIO_CAMREADY_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_CAMREADY_IRQ_TYPE "NONE"
#define PIO_CAMREADY_NAME "/dev/pio_camready"
#define PIO_CAMREADY_RESET_VALUE 0
#define PIO_CAMREADY_SPAN 16
#define PIO_CAMREADY_TYPE "altera_avalon_pio"


/*
 * pio_pixeldata configuration
 *
 */

#define ALT_MODULE_CLASS_pio_pixeldata altera_avalon_pio
#define PIO_PIXELDATA_BASE 0x9080
#define PIO_PIXELDATA_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_PIXELDATA_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_PIXELDATA_CAPTURE 0
#define PIO_PIXELDATA_DATA_WIDTH 4
#define PIO_PIXELDATA_DO_TEST_BENCH_WIRING 0
#define PIO_PIXELDATA_DRIVEN_SIM_VALUE 0
#define PIO_PIXELDATA_EDGE_TYPE "NONE"
#define PIO_PIXELDATA_FREQ 50000000
#define PIO_PIXELDATA_HAS_IN 0
#define PIO_PIXELDATA_HAS_OUT 1
#define PIO_PIXELDATA_HAS_TRI 0
#define PIO_PIXELDATA_IRQ -1
#define PIO_PIXELDATA_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_PIXELDATA_IRQ_TYPE "NONE"
#define PIO_PIXELDATA_NAME "/dev/pio_pixeldata"
#define PIO_PIXELDATA_RESET_VALUE 0
#define PIO_PIXELDATA_SPAN 16
#define PIO_PIXELDATA_TYPE "altera_avalon_pio"


/*
 * pio_wren configuration
 *
 */

#define ALT_MODULE_CLASS_pio_wren altera_avalon_pio
#define PIO_WREN_BASE 0x90b0
#define PIO_WREN_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_WREN_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_WREN_CAPTURE 0
#define PIO_WREN_DATA_WIDTH 1
#define PIO_WREN_DO_TEST_BENCH_WIRING 0
#define PIO_WREN_DRIVEN_SIM_VALUE 0
#define PIO_WREN_EDGE_TYPE "NONE"
#define PIO_WREN_FREQ 50000000
#define PIO_WREN_HAS_IN 0
#define PIO_WREN_HAS_OUT 1
#define PIO_WREN_HAS_TRI 0
#define PIO_WREN_IRQ -1
#define PIO_WREN_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_WREN_IRQ_TYPE "NONE"
#define PIO_WREN_NAME "/dev/pio_wren"
#define PIO_WREN_RESET_VALUE 0
#define PIO_WREN_SPAN 16
#define PIO_WREN_TYPE "altera_avalon_pio"


/*
 * vga_IRQ_rx configuration
 *
 */

#define ALT_MODULE_CLASS_vga_IRQ_rx altera_avalon_pio
#define VGA_IRQ_RX_BASE 0x9060
#define VGA_IRQ_RX_BIT_CLEARING_EDGE_REGISTER 0
#define VGA_IRQ_RX_BIT_MODIFYING_OUTPUT_REGISTER 0
#define VGA_IRQ_RX_CAPTURE 0
#define VGA_IRQ_RX_DATA_WIDTH 1
#define VGA_IRQ_RX_DO_TEST_BENCH_WIRING 0
#define VGA_IRQ_RX_DRIVEN_SIM_VALUE 0
#define VGA_IRQ_RX_EDGE_TYPE "NONE"
#define VGA_IRQ_RX_FREQ 50000000
#define VGA_IRQ_RX_HAS_IN 1
#define VGA_IRQ_RX_HAS_OUT 0
#define VGA_IRQ_RX_HAS_TRI 0
#define VGA_IRQ_RX_IRQ 1
#define VGA_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID 0
#define VGA_IRQ_RX_IRQ_TYPE "LEVEL"
#define VGA_IRQ_RX_NAME "/dev/vga_IRQ_rx"
#define VGA_IRQ_RX_RESET_VALUE 0
#define VGA_IRQ_RX_SPAN 16
#define VGA_IRQ_RX_TYPE "altera_avalon_pio"


/*
 * vga_IRQ_tx configuration
 *
 */

#define ALT_MODULE_CLASS_vga_IRQ_tx altera_avalon_pio
#define VGA_IRQ_TX_BASE 0x9070
#define VGA_IRQ_TX_BIT_CLEARING_EDGE_REGISTER 0
#define VGA_IRQ_TX_BIT_MODIFYING_OUTPUT_REGISTER 0
#define VGA_IRQ_TX_CAPTURE 0
#define VGA_IRQ_TX_DATA_WIDTH 1
#define VGA_IRQ_TX_DO_TEST_BENCH_WIRING 0
#define VGA_IRQ_TX_DRIVEN_SIM_VALUE 0
#define VGA_IRQ_TX_EDGE_TYPE "NONE"
#define VGA_IRQ_TX_FREQ 50000000
#define VGA_IRQ_TX_HAS_IN 0
#define VGA_IRQ_TX_HAS_OUT 1
#define VGA_IRQ_TX_HAS_TRI 0
#define VGA_IRQ_TX_IRQ -1
#define VGA_IRQ_TX_IRQ_INTERRUPT_CONTROLLER_ID -1
#define VGA_IRQ_TX_IRQ_TYPE "NONE"
#define VGA_IRQ_TX_NAME "/dev/vga_IRQ_tx"
#define VGA_IRQ_TX_RESET_VALUE 0
#define VGA_IRQ_TX_SPAN 16
#define VGA_IRQ_TX_TYPE "altera_avalon_pio"


/*
 * vga_mem configuration
 *
 */

#define ALT_MODULE_CLASS_vga_mem altera_avalon_onchip_memory2
#define VGA_MEM_ALLOW_IN_SYSTEM_MEMORY_CONTENT_EDITOR 0
#define VGA_MEM_ALLOW_MRAM_SIM_CONTENTS_ONLY_FILE 0
#define VGA_MEM_BASE 0x4000
#define VGA_MEM_CONTENTS_INFO ""
#define VGA_MEM_DUAL_PORT 0
#define VGA_MEM_GUI_RAM_BLOCK_TYPE "AUTO"
#define VGA_MEM_INIT_CONTENTS_FILE "NIOSII_WEEK3_vga_mem"
#define VGA_MEM_INIT_MEM_CONTENT 0
#define VGA_MEM_INSTANCE_ID "NONE"
#define VGA_MEM_IRQ -1
#define VGA_MEM_IRQ_INTERRUPT_CONTROLLER_ID -1
#define VGA_MEM_NAME "/dev/vga_mem"
#define VGA_MEM_NON_DEFAULT_INIT_FILE_ENABLED 0
#define VGA_MEM_RAM_BLOCK_TYPE "AUTO"
#define VGA_MEM_READ_DURING_WRITE_MODE "DONT_CARE"
#define VGA_MEM_SINGLE_CLOCK_OP 0
#define VGA_MEM_SIZE_MULTIPLE 1
#define VGA_MEM_SIZE_VALUE 16384
#define VGA_MEM_SPAN 16384
#define VGA_MEM_TYPE "altera_avalon_onchip_memory2"
#define VGA_MEM_WRITABLE 1

#endif /* __SYSTEM_H_ */
