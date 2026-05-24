#include "io.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "control.h"
#include "includes.h"

#define USER_MESSAGE "I Love NIOS"
#define USER_MESSAGE_LENGTH 16

#define TRAFFIC_THRESHOLD 20000
#define ACCEL_THRESHOLD 5000

/* Definition of Task Stacks */
#define   EFFICIENT_STACKSIZE       512 // For lighter task loads (less RAM)
#define   HEAVY_STACKSIZE			2048 // For heavy-duty task loads (way more RAM)
OS_STK input_task_stk[EFFICIENT_STACKSIZE];
OS_STK hex_task_stk[HEAVY_STACKSIZE];
OS_STK leds_update_task_stk[EFFICIENT_STACKSIZE];
OS_STK sfx_task_stk[HEAVY_STACKSIZE];
OS_STK initial_task_stk[EFFICIENT_STACKSIZE];

// Priorities
#define   INPUT_TASK_PRIO            3
#define   LEDS_UPDATE_TASK_PRIO      4
#define   HEX_TASK_PRIO              5
#define   SFX_TASK_PRIO              6
#define   INITIAL_TASK_PRIO          7
// List of semaphores
OS_EVENT *input_update_sem;
OS_EVENT *leds_update_sem;

void Initial_Task(void* pdata) {
	// For the CPU load + utilisation measurement
    OSStatInit();

    // Create the other tasks

    // For catching input
	OSTaskCreateExt(input_task, NULL, &input_task_stk[EFFICIENT_STACKSIZE - 1],
		INPUT_TASK_PRIO, INPUT_TASK_PRIO, input_task_stk, EFFICIENT_STACKSIZE, NULL, 0);

	// For handling the HEX displays
	OSTaskCreateExt(HEX_task, NULL, &hex_task_stk[HEAVY_STACKSIZE - 1],
		HEX_TASK_PRIO, HEX_TASK_PRIO, hex_task_stk, HEAVY_STACKSIZE, NULL, 0);

	// For updating the LEDS
	OSTaskCreateExt(leds_update_task, NULL, &leds_update_task_stk[EFFICIENT_STACKSIZE - 1],
			LEDS_UPDATE_TASK_PRIO, LEDS_UPDATE_TASK_PRIO, leds_update_task_stk, EFFICIENT_STACKSIZE, NULL, 0);

	// For speaker / gameplay SFX / DEBUG speaker command
	OSTaskCreateExt(sfx_task, NULL, &sfx_task_stk[HEAVY_STACKSIZE - 1],
			SFX_TASK_PRIO, SFX_TASK_PRIO, sfx_task_stk, HEAVY_STACKSIZE, NULL, 0);

	// Delete task
	OSTaskDel(OS_PRIO_SELF);
}

int main(void)
{
	// create semaphore first to avoid boot race condition
	input_update_sem = OSSemCreate(0);
	leds_update_sem = OSSemCreate(0);

	// then initialize the rest
	sound_init();
	debug_control_init();
	control_shared_flags_init();
	switch_setup();
	key_setup();
	img_rx_setup();
	vga_rx_setup();

	OSTaskCreateExt(Initial_Task, NULL, &initial_task_stk[EFFICIENT_STACKSIZE - 1],
			INITIAL_TASK_PRIO, INITIAL_TASK_PRIO, initial_task_stk, EFFICIENT_STACKSIZE, NULL, 0);

    OSStart();

    return 0;
}
