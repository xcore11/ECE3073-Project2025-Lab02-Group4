/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'image_proc' in SOPC Builder design 'NIOSII_WEEK3'
 * SOPC Builder design path: ../../NIOSII_WEEK3.sopcinfo
 *
 * Generated: Fri May 01 04:08:32 SGT 2026
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
#define ALT_CPU_BREAK_ADDR 0x04008820
#define ALT_CPU_CPU_ARCH_NIOS2_R1
#define ALT_CPU_CPU_FREQ 50000000u
#define ALT_CPU_CPU_ID_SIZE 1
#define ALT_CPU_CPU_ID_VALUE 0x00000000
#define ALT_CPU_CPU_IMPLEMENTATION "fast"
#define ALT_CPU_DATA_ADDR_WIDTH 0x1b
#define ALT_CPU_DCACHE_BYPASS_MASK 0x80000000
#define ALT_CPU_DCACHE_LINE_SIZE 32
#define ALT_CPU_DCACHE_LINE_SIZE_LOG2 5
#define ALT_CPU_DCACHE_SIZE 2048
#define ALT_CPU_EXCEPTION_ADDR 0x04004020
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
#define ALT_CPU_NAME "image_proc"
#define ALT_CPU_NUM_OF_SHADOW_REG_SETS 0
#define ALT_CPU_OCI_VERSION 1
#define ALT_CPU_RESET_ADDR 0x04004000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x04008820
#define NIOS2_CPU_ARCH_NIOS2_R1
#define NIOS2_CPU_FREQ 50000000u
#define NIOS2_CPU_ID_SIZE 1
#define NIOS2_CPU_ID_VALUE 0x00000000
#define NIOS2_CPU_IMPLEMENTATION "fast"
#define NIOS2_DATA_ADDR_WIDTH 0x1b
#define NIOS2_DCACHE_BYPASS_MASK 0x80000000
#define NIOS2_DCACHE_LINE_SIZE 32
#define NIOS2_DCACHE_LINE_SIZE_LOG2 5
#define NIOS2_DCACHE_SIZE 2048
#define NIOS2_EXCEPTION_ADDR 0x04004020
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
#define NIOS2_RESET_ADDR 0x04004000


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_MUTEX
#define __ALTERA_AVALON_NEW_SDRAM_CONTROLLER
#define __ALTERA_AVALON_ONCHIP_MEMORY2
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_SPI
#define __ALTERA_NIOS2_GEN2


/*
 * System configuration
 *
 */

#define ALT_DEVICE_FAMILY "MAX 10"
#define ALT_ENHANCED_INTERRUPT_API_PRESENT
#define ALT_IRQ_BASE NULL
#define ALT_LOG_PORT "/dev/null"
#define ALT_LOG_PORT_BASE 0x0
#define ALT_LOG_PORT_DEV null
#define ALT_LOG_PORT_TYPE ""
#define ALT_NUM_EXTERNAL_INTERRUPT_CONTROLLERS 0
#define ALT_NUM_INTERNAL_INTERRUPT_CONTROLLERS 1
#define ALT_NUM_INTERRUPT_CONTROLLERS 1
#define ALT_STDERR "/dev/jtag_image"
#define ALT_STDERR_BASE 0x40090a8
#define ALT_STDERR_DEV jtag_image
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtag_image"
#define ALT_STDIN_BASE 0x40090a8
#define ALT_STDIN_DEV jtag_image
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtag_image"
#define ALT_STDOUT_BASE 0x40090a8
#define ALT_STDOUT_DEV jtag_image
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "NIOSII_WEEK3"


/*
 * hal configuration
 *
 */

#define ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API
#define ALT_MAX_FD 32
#define ALT_SYS_CLK none
#define ALT_TIMESTAMP_CLK none


/*
 * image_mem configuration
 *
 */

#define ALT_MODULE_CLASS_image_mem altera_avalon_onchip_memory2
#define IMAGE_MEM_ALLOW_IN_SYSTEM_MEMORY_CONTENT_EDITOR 0
#define IMAGE_MEM_ALLOW_MRAM_SIM_CONTENTS_ONLY_FILE 0
#define IMAGE_MEM_BASE 0x4004000
#define IMAGE_MEM_CONTENTS_INFO ""
#define IMAGE_MEM_DUAL_PORT 0
#define IMAGE_MEM_GUI_RAM_BLOCK_TYPE "AUTO"
#define IMAGE_MEM_INIT_CONTENTS_FILE "NIOSII_WEEK3_image_mem"
#define IMAGE_MEM_INIT_MEM_CONTENT 0
#define IMAGE_MEM_INSTANCE_ID "NONE"
#define IMAGE_MEM_IRQ -1
#define IMAGE_MEM_IRQ_INTERRUPT_CONTROLLER_ID -1
#define IMAGE_MEM_NAME "/dev/image_mem"
#define IMAGE_MEM_NON_DEFAULT_INIT_FILE_ENABLED 0
#define IMAGE_MEM_RAM_BLOCK_TYPE "AUTO"
#define IMAGE_MEM_READ_DURING_WRITE_MODE "DONT_CARE"
#define IMAGE_MEM_SINGLE_CLOCK_OP 0
#define IMAGE_MEM_SIZE_MULTIPLE 1
#define IMAGE_MEM_SIZE_VALUE 16384
#define IMAGE_MEM_SPAN 16384
#define IMAGE_MEM_TYPE "altera_avalon_onchip_memory2"
#define IMAGE_MEM_WRITABLE 1


/*
 * img_IRQ_rx configuration
 *
 */

#define ALT_MODULE_CLASS_img_IRQ_rx altera_avalon_pio
#define IMG_IRQ_RX_BASE 0x4009070
#define IMG_IRQ_RX_BIT_CLEARING_EDGE_REGISTER 0
#define IMG_IRQ_RX_BIT_MODIFYING_OUTPUT_REGISTER 0
#define IMG_IRQ_RX_CAPTURE 0
#define IMG_IRQ_RX_DATA_WIDTH 1
#define IMG_IRQ_RX_DO_TEST_BENCH_WIRING 0
#define IMG_IRQ_RX_DRIVEN_SIM_VALUE 0
#define IMG_IRQ_RX_EDGE_TYPE "NONE"
#define IMG_IRQ_RX_FREQ 50000000
#define IMG_IRQ_RX_HAS_IN 1
#define IMG_IRQ_RX_HAS_OUT 0
#define IMG_IRQ_RX_HAS_TRI 0
#define IMG_IRQ_RX_IRQ 2
#define IMG_IRQ_RX_IRQ_INTERRUPT_CONTROLLER_ID 0
#define IMG_IRQ_RX_IRQ_TYPE "LEVEL"
#define IMG_IRQ_RX_NAME "/dev/img_IRQ_rx"
#define IMG_IRQ_RX_RESET_VALUE 0
#define IMG_IRQ_RX_SPAN 16
#define IMG_IRQ_RX_TYPE "altera_avalon_pio"


/*
 * img_IRQ_tx configuration
 *
 */

#define ALT_MODULE_CLASS_img_IRQ_tx altera_avalon_pio
#define IMG_IRQ_TX_BASE 0x4009080
#define IMG_IRQ_TX_BIT_CLEARING_EDGE_REGISTER 0
#define IMG_IRQ_TX_BIT_MODIFYING_OUTPUT_REGISTER 0
#define IMG_IRQ_TX_CAPTURE 0
#define IMG_IRQ_TX_DATA_WIDTH 1
#define IMG_IRQ_TX_DO_TEST_BENCH_WIRING 0
#define IMG_IRQ_TX_DRIVEN_SIM_VALUE 0
#define IMG_IRQ_TX_EDGE_TYPE "NONE"
#define IMG_IRQ_TX_FREQ 50000000
#define IMG_IRQ_TX_HAS_IN 0
#define IMG_IRQ_TX_HAS_OUT 1
#define IMG_IRQ_TX_HAS_TRI 0
#define IMG_IRQ_TX_IRQ -1
#define IMG_IRQ_TX_IRQ_INTERRUPT_CONTROLLER_ID -1
#define IMG_IRQ_TX_IRQ_TYPE "NONE"
#define IMG_IRQ_TX_NAME "/dev/img_IRQ_tx"
#define IMG_IRQ_TX_RESET_VALUE 0
#define IMG_IRQ_TX_SPAN 16
#define IMG_IRQ_TX_TYPE "altera_avalon_pio"


/*
 * jtag_image configuration
 *
 */

#define ALT_MODULE_CLASS_jtag_image altera_avalon_jtag_uart
#define JTAG_IMAGE_BASE 0x40090a8
#define JTAG_IMAGE_IRQ 1
#define JTAG_IMAGE_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAG_IMAGE_NAME "/dev/jtag_image"
#define JTAG_IMAGE_READ_DEPTH 64
#define JTAG_IMAGE_READ_THRESHOLD 8
#define JTAG_IMAGE_SPAN 8
#define JTAG_IMAGE_TYPE "altera_avalon_jtag_uart"
#define JTAG_IMAGE_WRITE_DEPTH 64
#define JTAG_IMAGE_WRITE_THRESHOLD 8


/*
 * mutex_0 configuration
 *
 */

#define ALT_MODULE_CLASS_mutex_0 altera_avalon_mutex
#define MUTEX_0_BASE 0x4011210
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
#define NEW_SDRAM_CONTROLLER_0_BASE 0x0
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
 * pio_spi_select configuration
 *
 */

#define ALT_MODULE_CLASS_pio_spi_select altera_avalon_pio
#define PIO_SPI_SELECT_BASE 0x4009090
#define PIO_SPI_SELECT_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_SPI_SELECT_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_SPI_SELECT_CAPTURE 0
#define PIO_SPI_SELECT_DATA_WIDTH 1
#define PIO_SPI_SELECT_DO_TEST_BENCH_WIRING 0
#define PIO_SPI_SELECT_DRIVEN_SIM_VALUE 0
#define PIO_SPI_SELECT_EDGE_TYPE "NONE"
#define PIO_SPI_SELECT_FREQ 50000000
#define PIO_SPI_SELECT_HAS_IN 1
#define PIO_SPI_SELECT_HAS_OUT 0
#define PIO_SPI_SELECT_HAS_TRI 0
#define PIO_SPI_SELECT_IRQ -1
#define PIO_SPI_SELECT_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_SPI_SELECT_IRQ_TYPE "NONE"
#define PIO_SPI_SELECT_NAME "/dev/pio_spi_select"
#define PIO_SPI_SELECT_RESET_VALUE 0
#define PIO_SPI_SELECT_SPAN 16
#define PIO_SPI_SELECT_TYPE "altera_avalon_pio"


/*
 * spi_0 configuration
 *
 */

#define ALT_MODULE_CLASS_spi_0 altera_avalon_spi
#define SPI_0_BASE 0x4009020
#define SPI_0_CLOCKMULT 1
#define SPI_0_CLOCKPHASE 0
#define SPI_0_CLOCKPOLARITY 0
#define SPI_0_CLOCKUNITS "Hz"
#define SPI_0_DATABITS 8
#define SPI_0_DATAWIDTH 16
#define SPI_0_DELAYMULT "1.0E-9"
#define SPI_0_DELAYUNITS "ns"
#define SPI_0_EXTRADELAY 0
#define SPI_0_INSERT_SYNC 0
#define SPI_0_IRQ 0
#define SPI_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define SPI_0_ISMASTER 1
#define SPI_0_LSBFIRST 0
#define SPI_0_NAME "/dev/spi_0"
#define SPI_0_NUMSLAVES 1
#define SPI_0_PREFIX "spi_"
#define SPI_0_SPAN 32
#define SPI_0_SYNC_REG_DEPTH 2
#define SPI_0_TARGETCLOCK 128000u
#define SPI_0_TARGETSSDELAY "0.0"
#define SPI_0_TYPE "altera_avalon_spi"

#endif /* __SYSTEM_H_ */
