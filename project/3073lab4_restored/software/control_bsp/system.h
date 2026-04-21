/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'control_proc' in SOPC Builder design 'NIOSII_WEEK3'
 * SOPC Builder design path: ../../NIOSII_WEEK3.sopcinfo
 *
 * Generated: Mon Apr 20 20:51:00 SGT 2026
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
#define ALT_CPU_BREAK_ADDR 0x08010820
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
#define ALT_CPU_EXCEPTION_ADDR 0x08008020
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
#define ALT_CPU_INST_ADDR_WIDTH 0x1c
#define ALT_CPU_NAME "control_proc"
#define ALT_CPU_NUM_OF_SHADOW_REG_SETS 0
#define ALT_CPU_OCI_VERSION 1
#define ALT_CPU_RESET_ADDR 0x08008000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x08010820
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
#define NIOS2_EXCEPTION_ADDR 0x08008020
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
#define NIOS2_INST_ADDR_WIDTH 0x1c
#define NIOS2_NUM_OF_SHADOW_REG_SETS 0
#define NIOS2_OCI_VERSION 1
#define NIOS2_RESET_ADDR 0x08008000


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_NEW_SDRAM_CONTROLLER
#define __ALTERA_AVALON_ONCHIP_MEMORY2
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_SPI
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
#define ALT_STDERR "/dev/jtag_uart_0"
#define ALT_STDERR_BASE 0x80111e8
#define ALT_STDERR_DEV jtag_uart_0
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtag_uart_0"
#define ALT_STDIN_BASE 0x80111e8
#define ALT_STDIN_DEV jtag_uart_0
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtag_uart_0"
#define ALT_STDOUT_BASE 0x80111e8
#define ALT_STDOUT_DEV jtag_uart_0
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "NIOSII_WEEK3"


/*
 * accelerometer_spi_0 configuration
 *
 */

#define ACCELEROMETER_SPI_0_BASE 0x80111f0
#define ACCELEROMETER_SPI_0_IRQ 0
#define ACCELEROMETER_SPI_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define ACCELEROMETER_SPI_0_NAME "/dev/accelerometer_spi_0"
#define ACCELEROMETER_SPI_0_SPAN 2
#define ACCELEROMETER_SPI_0_TYPE "altera_up_avalon_accelerometer_spi"
#define ALT_MODULE_CLASS_accelerometer_spi_0 altera_up_avalon_accelerometer_spi


/*
 * control_mem configuration
 *
 */

#define ALT_MODULE_CLASS_control_mem altera_avalon_onchip_memory2
#define CONTROL_MEM_ALLOW_IN_SYSTEM_MEMORY_CONTENT_EDITOR 0
#define CONTROL_MEM_ALLOW_MRAM_SIM_CONTENTS_ONLY_FILE 0
#define CONTROL_MEM_BASE 0x8008000
#define CONTROL_MEM_CONTENTS_INFO ""
#define CONTROL_MEM_DUAL_PORT 0
#define CONTROL_MEM_GUI_RAM_BLOCK_TYPE "AUTO"
#define CONTROL_MEM_INIT_CONTENTS_FILE "NIOSII_WEEK3_control_mem"
#define CONTROL_MEM_INIT_MEM_CONTENT 0
#define CONTROL_MEM_INSTANCE_ID "NONE"
#define CONTROL_MEM_IRQ -1
#define CONTROL_MEM_IRQ_INTERRUPT_CONTROLLER_ID -1
#define CONTROL_MEM_NAME "/dev/control_mem"
#define CONTROL_MEM_NON_DEFAULT_INIT_FILE_ENABLED 0
#define CONTROL_MEM_RAM_BLOCK_TYPE "AUTO"
#define CONTROL_MEM_READ_DURING_WRITE_MODE "DONT_CARE"
#define CONTROL_MEM_SINGLE_CLOCK_OP 0
#define CONTROL_MEM_SIZE_MULTIPLE 1
#define CONTROL_MEM_SIZE_VALUE 32768
#define CONTROL_MEM_SPAN 32768
#define CONTROL_MEM_TYPE "altera_avalon_onchip_memory2"
#define CONTROL_MEM_WRITABLE 1


/*
 * hal configuration
 *
 */

#define ALT_INCLUDE_INSTRUCTION_RELATED_EXCEPTION_API
#define ALT_MAX_FD 32
#define ALT_SYS_CLK none
#define ALT_TIMESTAMP_CLK none


/*
 * jtag_uart_0 configuration
 *
 */

#define ALT_MODULE_CLASS_jtag_uart_0 altera_avalon_jtag_uart
#define JTAG_UART_0_BASE 0x80111e8
#define JTAG_UART_0_IRQ 1
#define JTAG_UART_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAG_UART_0_NAME "/dev/jtag_uart_0"
#define JTAG_UART_0_READ_DEPTH 64
#define JTAG_UART_0_READ_THRESHOLD 8
#define JTAG_UART_0_SPAN 8
#define JTAG_UART_0_TYPE "altera_avalon_jtag_uart"
#define JTAG_UART_0_WRITE_DEPTH 64
#define JTAG_UART_0_WRITE_THRESHOLD 8


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
 * pio_gpio configuration
 *
 */

#define ALT_MODULE_CLASS_pio_gpio altera_avalon_pio
#define PIO_GPIO_BASE 0x80111a0
#define PIO_GPIO_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_GPIO_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_GPIO_CAPTURE 0
#define PIO_GPIO_DATA_WIDTH 2
#define PIO_GPIO_DO_TEST_BENCH_WIRING 0
#define PIO_GPIO_DRIVEN_SIM_VALUE 0
#define PIO_GPIO_EDGE_TYPE "NONE"
#define PIO_GPIO_FREQ 50000000
#define PIO_GPIO_HAS_IN 0
#define PIO_GPIO_HAS_OUT 1
#define PIO_GPIO_HAS_TRI 0
#define PIO_GPIO_IRQ -1
#define PIO_GPIO_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_GPIO_IRQ_TYPE "NONE"
#define PIO_GPIO_NAME "/dev/pio_gpio"
#define PIO_GPIO_RESET_VALUE 0
#define PIO_GPIO_SPAN 16
#define PIO_GPIO_TYPE "altera_avalon_pio"


/*
 * pio_hex0 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_hex0 altera_avalon_pio
#define PIO_HEX0_BASE 0x8011190
#define PIO_HEX0_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_HEX0_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_HEX0_CAPTURE 0
#define PIO_HEX0_DATA_WIDTH 8
#define PIO_HEX0_DO_TEST_BENCH_WIRING 0
#define PIO_HEX0_DRIVEN_SIM_VALUE 0
#define PIO_HEX0_EDGE_TYPE "NONE"
#define PIO_HEX0_FREQ 50000000
#define PIO_HEX0_HAS_IN 0
#define PIO_HEX0_HAS_OUT 1
#define PIO_HEX0_HAS_TRI 0
#define PIO_HEX0_IRQ -1
#define PIO_HEX0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_HEX0_IRQ_TYPE "NONE"
#define PIO_HEX0_NAME "/dev/pio_hex0"
#define PIO_HEX0_RESET_VALUE 0
#define PIO_HEX0_SPAN 16
#define PIO_HEX0_TYPE "altera_avalon_pio"


/*
 * pio_hex1 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_hex1 altera_avalon_pio
#define PIO_HEX1_BASE 0x8011180
#define PIO_HEX1_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_HEX1_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_HEX1_CAPTURE 0
#define PIO_HEX1_DATA_WIDTH 8
#define PIO_HEX1_DO_TEST_BENCH_WIRING 0
#define PIO_HEX1_DRIVEN_SIM_VALUE 0
#define PIO_HEX1_EDGE_TYPE "NONE"
#define PIO_HEX1_FREQ 50000000
#define PIO_HEX1_HAS_IN 0
#define PIO_HEX1_HAS_OUT 1
#define PIO_HEX1_HAS_TRI 0
#define PIO_HEX1_IRQ -1
#define PIO_HEX1_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_HEX1_IRQ_TYPE "NONE"
#define PIO_HEX1_NAME "/dev/pio_hex1"
#define PIO_HEX1_RESET_VALUE 0
#define PIO_HEX1_SPAN 16
#define PIO_HEX1_TYPE "altera_avalon_pio"


/*
 * pio_hex2 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_hex2 altera_avalon_pio
#define PIO_HEX2_BASE 0x8011170
#define PIO_HEX2_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_HEX2_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_HEX2_CAPTURE 0
#define PIO_HEX2_DATA_WIDTH 8
#define PIO_HEX2_DO_TEST_BENCH_WIRING 0
#define PIO_HEX2_DRIVEN_SIM_VALUE 0
#define PIO_HEX2_EDGE_TYPE "NONE"
#define PIO_HEX2_FREQ 50000000
#define PIO_HEX2_HAS_IN 0
#define PIO_HEX2_HAS_OUT 1
#define PIO_HEX2_HAS_TRI 0
#define PIO_HEX2_IRQ -1
#define PIO_HEX2_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_HEX2_IRQ_TYPE "NONE"
#define PIO_HEX2_NAME "/dev/pio_hex2"
#define PIO_HEX2_RESET_VALUE 0
#define PIO_HEX2_SPAN 16
#define PIO_HEX2_TYPE "altera_avalon_pio"


/*
 * pio_hex3 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_hex3 altera_avalon_pio
#define PIO_HEX3_BASE 0x8011160
#define PIO_HEX3_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_HEX3_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_HEX3_CAPTURE 0
#define PIO_HEX3_DATA_WIDTH 8
#define PIO_HEX3_DO_TEST_BENCH_WIRING 0
#define PIO_HEX3_DRIVEN_SIM_VALUE 0
#define PIO_HEX3_EDGE_TYPE "NONE"
#define PIO_HEX3_FREQ 50000000
#define PIO_HEX3_HAS_IN 0
#define PIO_HEX3_HAS_OUT 1
#define PIO_HEX3_HAS_TRI 0
#define PIO_HEX3_IRQ -1
#define PIO_HEX3_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_HEX3_IRQ_TYPE "NONE"
#define PIO_HEX3_NAME "/dev/pio_hex3"
#define PIO_HEX3_RESET_VALUE 0
#define PIO_HEX3_SPAN 16
#define PIO_HEX3_TYPE "altera_avalon_pio"


/*
 * pio_hex4 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_hex4 altera_avalon_pio
#define PIO_HEX4_BASE 0x8011150
#define PIO_HEX4_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_HEX4_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_HEX4_CAPTURE 0
#define PIO_HEX4_DATA_WIDTH 8
#define PIO_HEX4_DO_TEST_BENCH_WIRING 0
#define PIO_HEX4_DRIVEN_SIM_VALUE 0
#define PIO_HEX4_EDGE_TYPE "NONE"
#define PIO_HEX4_FREQ 50000000
#define PIO_HEX4_HAS_IN 0
#define PIO_HEX4_HAS_OUT 1
#define PIO_HEX4_HAS_TRI 0
#define PIO_HEX4_IRQ -1
#define PIO_HEX4_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_HEX4_IRQ_TYPE "NONE"
#define PIO_HEX4_NAME "/dev/pio_hex4"
#define PIO_HEX4_RESET_VALUE 0
#define PIO_HEX4_SPAN 16
#define PIO_HEX4_TYPE "altera_avalon_pio"


/*
 * pio_hex5 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_hex5 altera_avalon_pio
#define PIO_HEX5_BASE 0x8011140
#define PIO_HEX5_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_HEX5_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_HEX5_CAPTURE 0
#define PIO_HEX5_DATA_WIDTH 8
#define PIO_HEX5_DO_TEST_BENCH_WIRING 0
#define PIO_HEX5_DRIVEN_SIM_VALUE 0
#define PIO_HEX5_EDGE_TYPE "NONE"
#define PIO_HEX5_FREQ 50000000
#define PIO_HEX5_HAS_IN 0
#define PIO_HEX5_HAS_OUT 1
#define PIO_HEX5_HAS_TRI 0
#define PIO_HEX5_IRQ -1
#define PIO_HEX5_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_HEX5_IRQ_TYPE "NONE"
#define PIO_HEX5_NAME "/dev/pio_hex5"
#define PIO_HEX5_RESET_VALUE 0
#define PIO_HEX5_SPAN 16
#define PIO_HEX5_TYPE "altera_avalon_pio"


/*
 * pio_led configuration
 *
 */

#define ALT_MODULE_CLASS_pio_led altera_avalon_pio
#define PIO_LED_BASE 0x80111d0
#define PIO_LED_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_LED_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_LED_CAPTURE 0
#define PIO_LED_DATA_WIDTH 10
#define PIO_LED_DO_TEST_BENCH_WIRING 0
#define PIO_LED_DRIVEN_SIM_VALUE 0
#define PIO_LED_EDGE_TYPE "NONE"
#define PIO_LED_FREQ 50000000
#define PIO_LED_HAS_IN 0
#define PIO_LED_HAS_OUT 1
#define PIO_LED_HAS_TRI 0
#define PIO_LED_IRQ -1
#define PIO_LED_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_LED_IRQ_TYPE "NONE"
#define PIO_LED_NAME "/dev/pio_led"
#define PIO_LED_RESET_VALUE 0
#define PIO_LED_SPAN 16
#define PIO_LED_TYPE "altera_avalon_pio"


/*
 * pio_led_module configuration
 *
 */

#define ALT_MODULE_CLASS_pio_led_module altera_avalon_pio
#define PIO_LED_MODULE_BASE 0x8011120
#define PIO_LED_MODULE_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_LED_MODULE_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_LED_MODULE_CAPTURE 0
#define PIO_LED_MODULE_DATA_WIDTH 3
#define PIO_LED_MODULE_DO_TEST_BENCH_WIRING 0
#define PIO_LED_MODULE_DRIVEN_SIM_VALUE 0
#define PIO_LED_MODULE_EDGE_TYPE "NONE"
#define PIO_LED_MODULE_FREQ 50000000
#define PIO_LED_MODULE_HAS_IN 0
#define PIO_LED_MODULE_HAS_OUT 1
#define PIO_LED_MODULE_HAS_TRI 0
#define PIO_LED_MODULE_IRQ -1
#define PIO_LED_MODULE_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_LED_MODULE_IRQ_TYPE "NONE"
#define PIO_LED_MODULE_NAME "/dev/pio_led_module"
#define PIO_LED_MODULE_RESET_VALUE 0
#define PIO_LED_MODULE_SPAN 16
#define PIO_LED_MODULE_TYPE "altera_avalon_pio"


/*
 * pio_pb configuration
 *
 */

#define ALT_MODULE_CLASS_pio_pb altera_avalon_pio
#define PIO_PB_BASE 0x80111b0
#define PIO_PB_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_PB_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_PB_CAPTURE 0
#define PIO_PB_DATA_WIDTH 2
#define PIO_PB_DO_TEST_BENCH_WIRING 0
#define PIO_PB_DRIVEN_SIM_VALUE 0
#define PIO_PB_EDGE_TYPE "NONE"
#define PIO_PB_FREQ 50000000
#define PIO_PB_HAS_IN 1
#define PIO_PB_HAS_OUT 0
#define PIO_PB_HAS_TRI 0
#define PIO_PB_IRQ -1
#define PIO_PB_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_PB_IRQ_TYPE "NONE"
#define PIO_PB_NAME "/dev/pio_pb"
#define PIO_PB_RESET_VALUE 0
#define PIO_PB_SPAN 16
#define PIO_PB_TYPE "altera_avalon_pio"


/*
 * pio_speaker configuration
 *
 */

#define ALT_MODULE_CLASS_pio_speaker altera_avalon_pio
#define PIO_SPEAKER_BASE 0x8011110
#define PIO_SPEAKER_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_SPEAKER_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_SPEAKER_CAPTURE 0
#define PIO_SPEAKER_DATA_WIDTH 1
#define PIO_SPEAKER_DO_TEST_BENCH_WIRING 0
#define PIO_SPEAKER_DRIVEN_SIM_VALUE 0
#define PIO_SPEAKER_EDGE_TYPE "NONE"
#define PIO_SPEAKER_FREQ 50000000
#define PIO_SPEAKER_HAS_IN 0
#define PIO_SPEAKER_HAS_OUT 1
#define PIO_SPEAKER_HAS_TRI 0
#define PIO_SPEAKER_IRQ -1
#define PIO_SPEAKER_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_SPEAKER_IRQ_TYPE "NONE"
#define PIO_SPEAKER_NAME "/dev/pio_speaker"
#define PIO_SPEAKER_RESET_VALUE 0
#define PIO_SPEAKER_SPAN 16
#define PIO_SPEAKER_TYPE "altera_avalon_pio"


/*
 * pio_spi_select configuration
 *
 */

#define ALT_MODULE_CLASS_pio_spi_select altera_avalon_pio
#define PIO_SPI_SELECT_BASE 0x8011130
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
 * pio_sw configuration
 *
 */

#define ALT_MODULE_CLASS_pio_sw altera_avalon_pio
#define PIO_SW_BASE 0x80111c0
#define PIO_SW_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_SW_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_SW_CAPTURE 0
#define PIO_SW_DATA_WIDTH 4
#define PIO_SW_DO_TEST_BENCH_WIRING 0
#define PIO_SW_DRIVEN_SIM_VALUE 0
#define PIO_SW_EDGE_TYPE "NONE"
#define PIO_SW_FREQ 50000000
#define PIO_SW_HAS_IN 1
#define PIO_SW_HAS_OUT 0
#define PIO_SW_HAS_TRI 0
#define PIO_SW_IRQ -1
#define PIO_SW_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_SW_IRQ_TYPE "NONE"
#define PIO_SW_NAME "/dev/pio_sw"
#define PIO_SW_RESET_VALUE 0
#define PIO_SW_SPAN 16
#define PIO_SW_TYPE "altera_avalon_pio"


/*
 * spi_0 configuration
 *
 */

#define ALT_MODULE_CLASS_spi_0 altera_avalon_spi
#define SPI_0_BASE 0x8011020
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
#define SPI_0_IRQ 2
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
