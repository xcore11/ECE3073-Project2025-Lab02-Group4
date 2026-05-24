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
#define   HEAVY_STACKSIZE			2056 // For heavy-duty task loads (way more RAM)
OS_STK input_task_stk[EFFICIENT_STACKSIZE];
OS_STK hex_task_stk[HEAVY_STACKSIZE];

// Priorities
#define   INPUT_TASK_PRIO	3
#define   HEX_TASK_PRIO 	4
// List of semaphores
OS_EVENT *input_update_sem;

int main(void)
{
	// create semaphore first to avoid boot race condition
	input_update_sem = OSSemCreate(0);

	// then initialize the rest
	control_shared_flags_init();
	switch_setup();
	key_setup();
	img_rx_setup();
	vga_rx_setup();

    // For catching input
    OSTaskCreateExt(input_task, NULL, &input_task_stk[EFFICIENT_STACKSIZE - 1],
        INPUT_TASK_PRIO, INPUT_TASK_PRIO, input_task_stk, EFFICIENT_STACKSIZE, NULL, 0);

    // For handling the HEX displays
    OSTaskCreateExt(HEX_task, NULL, &hex_task_stk[HEAVY_STACKSIZE - 1],
        HEX_TASK_PRIO, HEX_TASK_PRIO, hex_task_stk, HEAVY_STACKSIZE, NULL, 0);

    OSStart();

    return 0;
}
