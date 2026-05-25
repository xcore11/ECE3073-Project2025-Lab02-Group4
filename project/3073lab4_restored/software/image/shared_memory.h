#ifndef SHARED_MEMORY_H
#define SHARED_MEMORY_H

#include <stdint.h>

/* =========================
   SDRAM memory map

   Image processor writes here.
   VGA processor only reads these same addresses.
   ========================= */

#ifndef TEXT_BUFFER_BASE
#define TEXT_BUFFER_BASE       0x05200000
#endif

#ifndef TEXT_BUFFER_SIZE
#define TEXT_BUFFER_SIZE       4096
#endif

/*
   Internal 32-bit status registers for the image/SPI processor.
   These are NOT the 24-byte ESP packet header.
*/
#ifndef STATUS_REG_BASE
#define STATUS_REG_BASE        0x05201000
#endif

/*
   Full raw ESP packet storage:
   [24-byte header][text bytes][96x96 raw grayscale bytes]
*/
#ifndef SPI_PACKET_BASE
#define SPI_PACKET_BASE        0x05202000
#endif

/*
   Keep full packet below STATUS_BASE at 0x05209000.
   0x05209000 - 0x05202000 = 0x7000 = 28672 bytes.
   Your real packet is around 9.2 KB, so this is enough.
*/
#ifndef SPI_PACKET_MAX_SIZE
#define SPI_PACKET_MAX_SIZE    0x7000
#endif

/*
   24-byte packet header copy for VGA/status checking.
   VGA checks STATUS_BASE[0..23] and expects magic 'G''V'.
*/
#ifndef STATUS_BASE
#define STATUS_BASE            0x05209000
#endif

#ifndef STATUS_HEADER_SIZE
#define STATUS_HEADER_SIZE     24
#endif

/*
   Raw 96x96 grayscale image buffer for VGA display.
*/
#ifndef IMAGE_BUFFER_BASE
#define IMAGE_BUFFER_BASE      0x0520A000
#endif

#ifndef IMAGE_BUFFER_SIZE
#define IMAGE_BUFFER_SIZE      32768
#endif

#ifndef IMAGE_WIDTH_DEFAULT
#define IMAGE_WIDTH_DEFAULT    96
#endif

#ifndef IMAGE_HEIGHT_DEFAULT
#define IMAGE_HEIGHT_DEFAULT   96
#endif

#ifndef IMAGE_LENGTH_DEFAULT
#define IMAGE_LENGTH_DEFAULT   (IMAGE_WIDTH_DEFAULT * IMAGE_HEIGHT_DEFAULT)
#endif

/* =========================
   Internal 32-bit status register offsets
   Base = STATUS_REG_BASE
   ========================= */

#define STATUS_TEXT_READY       0x00
#define STATUS_CMD_READY        0x04
#define STATUS_CMD_ACK          0x08
#define STATUS_SPI_RX_COUNT     0x0C
#define STATUS_LAST_ERROR       0x10
#define STATUS_PACKET_READY     0x14
#define STATUS_PACKET_LENGTH    0x18
#define STATUS_IMAGE_LENGTH     0x1C
#define STATUS_TEXT_LENGTH      0x20
#define STATUS_LINE_COUNT       0x24
#define STATUS_IMAGE_WIDTH      0x28
#define STATUS_IMAGE_HEIGHT     0x2C

/* =========================
   ESP packet header byte offsets
   Base = STATUS_BASE when copied for VGA
   ========================= */

#define PKT_MAGIC0_OFS          0
#define PKT_MAGIC1_OFS          1
#define PKT_VERSION_OFS         2
#define PKT_STATUS_OFS          3
#define PKT_TOTAL_LEN_OFS       4
#define PKT_TEXT_LEN_OFS        8
#define PKT_IMAGE_W_OFS         10
#define PKT_IMAGE_H_OFS         12
#define PKT_LINE_COUNT_OFS      14
#define PKT_IMAGE_LEN_OFS       16
#define PKT_HEADER_LEN_OFS      20
#define PKT_FLAGS_OFS           22


/* =========================
   Shared VGA panel flags page
   This page is common to VGA and IMG processors.
   ========================= */

#ifndef SHARED_FLAGS_BASE
#define SHARED_FLAGS_BASE      0x05212000
#endif

#define FLAG_SYSTEM_MAGIC              0x00
#define FLAG_SESSION_STARTED           0x04
#define FLAG_IMAGE_PROCESSOR_READY     0x08
#define FLAG_VGA_PROCESSOR_READY       0x0C
#define FLAG_CONTROL_PROCESSOR_READY   0x10
#define FLAG_CURRENT_MENU              0x14
#define FLAG_CURRENT_GAME              0x18
#define FLAG_GAME_RUNNING              0x1C
#define FLAG_DEBUG_MODE                0x20
#define FLAG_IMAGE_READY               0x24
#define FLAG_TEXT_READY_SHARED         0x28
#define FLAG_VGA_DISPLAY_DONE          0x2C
#define FLAG_LAST_COMMAND              0x30
#define FLAG_LAST_ERROR_SHARED         0x34
#define FLAG_MENU_ENTER_EVENT          0x38
#define FLAG_MENU_EXIT_EVENT           0x3C
#define FLAG_PANEL_MODE_SEQ            0x8C

/* Realtime instruction activity monitor.
   IMG decoder bumps this every time it receives/decodes a snake/draw/battle row.
   VGA panels use it for RX status dots; Snake also pauses movement while active. */
#define FLAG_RT_ACTIVITY_SEQ           0x870
#define FLAG_RT_PANEL_MODE             0x874
#define FLAG_RT_LAST_RESULT            0x878

#define GAME_MODE_MENU                 0
#define GAME_MODE_SNAKE                1
#define GAME_MODE_DRAW                 2
#define GAME_MODE_DEBUG                3
#define GAME_MODE_BATTLE               4

/* =========================
   Control processor event flags
   Control writes these before pulsing IMG/VGA IRQ lines.
   IMG/VGA ISRs read these to know which key/switch caused the IRQ.
   ========================= */

#define FLAG_CONTROL_EVENT_SEQ          0x800
#define FLAG_CONTROL_KEY_STATE          0x804
#define FLAG_CONTROL_KEY_PRESSED_MASK   0x808
#define FLAG_CONTROL_SWITCH_STATE       0x80C
#define FLAG_CONTROL_SWITCH_EVENT_SEQ   0x810
#define FLAG_CONTROL_LAST_EVENT_TYPE    0x814
#define FLAG_CONTROL_LAST_EVENT_VALUE   0x818

#define CONTROL_EVENT_NONE              0
#define CONTROL_EVENT_KEY               1
#define CONTROL_EVENT_SWITCH            2

#define CONTROL_KEY0_MASK               0x00000001u
#define CONTROL_KEY1_MASK               0x00000002u
#define CONTROL_SW_MASK                 0x000003FFu


/* =========================
   DEBUG -> CONTROL mailbox
   Effective address = DEBUG_CONTROL_BASE + offset.
   This uses the previously-free 0x06000000 SDRAM region so it does not
   collide with the normal VGA/IMG shared flags page at 0x05212000.
   ========================= */

#ifndef DEBUG_CONTROL_BASE
#define DEBUG_CONTROL_BASE              0x06000000
#endif

#define FLAG_CONTROL_SPEAKER_OPTION     0x81C
#define FLAG_CONTROL_LED_MODULE         0x820
#define FLAG_CONTROL_LEDR               0x824
#define FLAG_CONTROL_MESSAGE            0x828
#define DEBUG_CONTROL_MESSAGE_BYTES     68

#define DEBUG_CONTROL_CMD_NONE          0
#define DEBUG_CONTROL_CMD_BATCH         100

#define DEBUG_CONTROL_MASK_HEX_MESSAGE  0x00000001u
#define DEBUG_CONTROL_MASK_LED_MODULE   0x00000002u
#define DEBUG_CONTROL_MASK_LEDR         0x00000004u
#define DEBUG_CONTROL_MASK_SPEAKER      0x00000008u

#define DEBUG_OPTION_VALID              0x80000000u
#define DEBUG_OPTION_BLINK              0x40000000u
#define DEBUG_OPTION_SECONDS_SHIFT      16
#define DEBUG_OPTION_SECONDS_MASK       0x00FF0000u

#define DEBUG_LED_MODULE_RED            0x00000001u
#define DEBUG_LED_MODULE_YELLOW         0x00000002u
#define DEBUG_LED_MODULE_GREEN          0x00000004u

#define DEBUG_LEDR_FROM_LEFT            0x00000100u

#define DEBUG_SPEAKER_FREQ_MASK         0x0000FFFFu
#define DEBUG_SPEAKER_HAS_DURATION      0x40000000u

/* Debug image capture / draw-background handoff flags. */
#define FLAG_DEBUG_IMAGE_CAPTURE_REQ    0x840
#define FLAG_DEBUG_IMAGE_CAPTURE_ACK    0x844
#define FLAG_DEBUG_BG_ACCEPT_SEQ        0x848
#define FLAG_DEBUG_STATUS               0x84C

#define DEBUG_STATUS_IDLE               0
#define DEBUG_STATUS_CAPTURE_REQUESTED  1
#define DEBUG_STATUS_IMAGE_READY        2
#define DEBUG_STATUS_BG_ACCEPTED        3
#define DEBUG_STATUS_NO_IMAGE           4

/* Draw background copy stored by VGA debug after KEY1. */
#define DRAW_BG_READY                   0x980
#define DRAW_BG_SEQ                     0x984
#define DRAW_BG_GRID                    0x1000
#define DRAW_BG_GRID_SIZE               (DRAW_GRID_W * DRAW_GRID_H)

/* Draw realtime mailbox: offsets 0x900 - 0x97F inside SHARED_FLAGS_BASE. */
#define DRAW_MB_READY                  0x900
#define DRAW_MB_ACK                    0x904
#define DRAW_MB_SEQ                    0x908
#define DRAW_MB_X                      0x90C
#define DRAW_MB_Y                      0x910
#define DRAW_MB_COLOR                  0x914
#define DRAW_MB_STATUS                 0x918

#define DRAW_STATUS_OK                 0
#define DRAW_STATUS_BAD_COORD          1
#define DRAW_STATUS_BAD_COLOR          2

#define DRAW_GRID_W                    96
#define DRAW_GRID_H                    96

/* Battleship realtime mailbox: offsets 0xA00 - 0xA3F inside SHARED_FLAGS_BASE. */
#define BATTLE_MB_READY                0xA00
#define BATTLE_MB_ACK                  0xA04
#define BATTLE_MB_SEQ                  0xA08
#define BATTLE_MB_TYPE                 0xA0C
#define BATTLE_MB_X                    0xA10
#define BATTLE_MB_Y                    0xA14
#define BATTLE_MB_FLAGS                0xA18
#define BATTLE_MB_STATUS               0xA1C

#define BATTLE_CMD_NONE                0
#define BATTLE_CMD_SET_SHIP            1
#define BATTLE_CMD_CLEAR_CELL          2
#define BATTLE_CMD_CLEAR_ALL           3

#define BATTLE_GRID_W                  10
#define BATTLE_GRID_H                  10

/* Battleship direct hidden-map mirror.
   IMG writes one 32-bit word per 10x10 cell so VGA can sync the hidden map
   without a single-command mailbox bottleneck. */
#define BATTLE_GRID_SEQ                0xA40
#define BATTLE_GRID_READY              0xA44
#define BATTLE_GRID_BASE               0xA80
#define BATTLE_GRID_WORD_SIZE          (BATTLE_GRID_W * BATTLE_GRID_H * 4)

#endif
