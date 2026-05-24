#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "control.h"
#include "includes.h"
#include "soundeffects.h"

/*
   CONTROL PROCESSOR MAIN - RTOS integrated version

   Dominant baseline kept:
   - SHARED_FLAGS_BASE = 0x05212000 for existing IMG/VGA key/switch/SFX flags.
   - DEBUG_CONTROL_BASE = 0x06000000 for IMG-decoded DEBUG commands.
   - Game SFX flags from snake/battleship are still consumed from 0x05212000.

   Friend RTOS behavior integrated:
   - Input handled by ISR -> semaphore -> input_task.
   - HEX display handled by HEX_task.
   - LED/debug mailbox handled by leds_update_task.
   - Speaker switch handled by speaker_switch_task.

   Required switch behavior:
   - SW1: HEX master ON/OFF.
   - SW2: show captured/decoded debug sentence as scrolling HEX message.
   - SW3: show CPU utilization percentage on HEX.
   - SW4: speaker ON/OFF. If a debug speaker frequency was decoded, SW4 plays
          that frequency; otherwise it plays the default 1000 Hz test tone.
*/

#define CONTROL_STACK_LIGHT       768
#define CONTROL_STACK_MEDIUM      1024
#define CONTROL_STACK_HEAVY       2048

#define INPUT_TASK_PRIO           3
#define LEDS_TASK_PRIO            4
#define HEX_TASK_PRIO             5
#define SPEAKER_TASK_PRIO         6
#define SFX_TASK_PRIO             7
#define INITIAL_TASK_PRIO         10

OS_STK input_task_stk[CONTROL_STACK_MEDIUM];
OS_STK leds_update_task_stk[CONTROL_STACK_MEDIUM];
OS_STK hex_task_stk[CONTROL_STACK_HEAVY];
OS_STK speaker_switch_task_stk[CONTROL_STACK_MEDIUM];
OS_STK sfx_task_stk[CONTROL_STACK_HEAVY];
OS_STK initial_task_stk[CONTROL_STACK_LIGHT];

OS_EVENT *input_update_sem = NULL;
OS_EVENT *leds_update_sem = NULL;

volatile int control_sfx_busy = 0;

static void shared_write32(uint32_t offset, uint32_t value)
{
    IOWR_32DIRECT(SHARED_FLAGS_BASE, offset, value);
}

static uint32_t shared_read32(uint32_t offset)
{
    return IORD_32DIRECT(SHARED_FLAGS_BASE, offset);
}

static int consume_shared_flag(uint32_t offset)
{
    if (shared_read32(offset) != 0) {
        shared_write32(offset, 0);
        return 1;
    }

    return 0;
}

static void clear_all_game_sfx_flags(void)
{
    shared_write32(FLAG_SFX_EAT_APPLE, 0);
    shared_write32(FLAG_SFX_GAME_OVER, 0);
    shared_write32(FLAG_SFX_PORTAL, 0);
    shared_write32(FLAG_SFX_BATTLE_HIT, 0);
    shared_write32(FLAG_SFX_BATTLE_MISS, 0);
    shared_write32(FLAG_SFX_CHANGE_ARSENAL, 0);
    shared_write32(FLAG_SFX_MENU_BLIP, 0);
}

static void game_sfx_task(void *pdata)
{
    (void)pdata;

    while (1) {
        if (consume_shared_flag(FLAG_SFX_EAT_APPLE)) {
            control_sfx_busy = 1;
            eat_apple(1);
            control_sfx_busy = 0;
        }

        if (consume_shared_flag(FLAG_SFX_GAME_OVER)) {
            control_sfx_busy = 1;
            game_over(1);
            control_sfx_busy = 0;
        }

        if (consume_shared_flag(FLAG_SFX_PORTAL)) {
            control_sfx_busy = 1;
            portal(1);
            control_sfx_busy = 0;
        }

        if (consume_shared_flag(FLAG_SFX_BATTLE_HIT)) {
            control_sfx_busy = 1;
            explosion(1);
            control_sfx_busy = 0;
        }

        if (consume_shared_flag(FLAG_SFX_BATTLE_MISS)) {
            control_sfx_busy = 1;
            miss(1);
            control_sfx_busy = 0;
        }

        if (consume_shared_flag(FLAG_SFX_CHANGE_ARSENAL)) {
            control_sfx_busy = 1;
            sfx_change();
            control_sfx_busy = 0;
        }

        if (consume_shared_flag(FLAG_SFX_MENU_BLIP)) {
            control_sfx_busy = 1;
            sfx_menu_blip();
            control_sfx_busy = 0;
        }

        OSTimeDlyHMSM(0, 0, 0, 20);
    }
}

static void Initial_Task(void *pdata)
{
    (void)pdata;

    OSStatInit();

    OSTaskCreateExt(input_task, NULL, &input_task_stk[CONTROL_STACK_MEDIUM - 1],
                    INPUT_TASK_PRIO, INPUT_TASK_PRIO,
                    input_task_stk, CONTROL_STACK_MEDIUM, NULL, 0);

    OSTaskCreateExt(leds_update_task, NULL, &leds_update_task_stk[CONTROL_STACK_MEDIUM - 1],
                    LEDS_TASK_PRIO, LEDS_TASK_PRIO,
                    leds_update_task_stk, CONTROL_STACK_MEDIUM, NULL, 0);

    OSTaskCreateExt(HEX_task, NULL, &hex_task_stk[CONTROL_STACK_HEAVY - 1],
                    HEX_TASK_PRIO, HEX_TASK_PRIO,
                    hex_task_stk, CONTROL_STACK_HEAVY, NULL, 0);

    OSTaskCreateExt(speaker_switch_task, NULL, &speaker_switch_task_stk[CONTROL_STACK_MEDIUM - 1],
                    SPEAKER_TASK_PRIO, SPEAKER_TASK_PRIO,
                    speaker_switch_task_stk, CONTROL_STACK_MEDIUM, NULL, 0);

    OSTaskCreateExt(game_sfx_task, NULL, &sfx_task_stk[CONTROL_STACK_HEAVY - 1],
                    SFX_TASK_PRIO, SFX_TASK_PRIO,
                    sfx_task_stk, CONTROL_STACK_HEAVY, NULL, 0);

    OSTaskDel(OS_PRIO_SELF);
}

int main(void)
{
    OSInit();

    input_update_sem = OSSemCreate(0);
    leds_update_sem = OSSemCreate(0);

    control_shared_flags_init();
    debug_control_init();
    sound_init();
    clear_all_game_sfx_flags();

    /* Idle output state: LED module OFF, LEDR OFF, HEX OFF, speaker OFF. */
    debug_control_all_outputs_off();

    switch_setup();
    key_setup();
    img_rx_setup();
    vga_rx_setup();

    printf("CONTROL RTOS ready: shared=0x%08X debug=0x%08X\n",
           (unsigned int)SHARED_FLAGS_BASE,
           (unsigned int)DEBUG_CONTROL_BASE);
    fflush(stdout);

    OSTaskCreateExt(Initial_Task, NULL, &initial_task_stk[CONTROL_STACK_LIGHT - 1],
                    INITIAL_TASK_PRIO, INITIAL_TASK_PRIO,
                    initial_task_stk, CONTROL_STACK_LIGHT, NULL, 0);

    OSStart();

    return 0;
}
