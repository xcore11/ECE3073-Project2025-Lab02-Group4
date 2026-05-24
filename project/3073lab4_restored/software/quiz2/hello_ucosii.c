#include <stdio.h>
#include "includes.h"

#define TASK_STACKSIZE 512

#define START_TASK_PRIO 5
#define TASK1_PRIO 11
#define TASK2_PRIO 12
#define TASK3_PRIO 13
#define TASK4_PRIO 14
#define TASK5_PRIO 15

// Task Stacks
OS_STK StartTaskStk[TASK_STACKSIZE];
OS_STK Task1Stk[TASK_STACKSIZE];
OS_STK Task2Stk[TASK_STACKSIZE];
OS_STK Task3Stk[TASK_STACKSIZE];
OS_STK Task4Stk[TASK_STACKSIZE];
OS_STK Task5Stk[TASK_STACKSIZE];

// Task Prototypes
void Task1(void* pdata);
void Task2(void* pdata);
void Task3(void* pdata);
void Task4(void* pdata);
void Task5(void* pdata);

// StartTask
void StartTask(void* pdata);

// Declare Globalvalue
int Globalvalue = 0;

// Declare Semaphore
OS_EVENT* GlobalSemaphore;

// For Deadlock
OS_EVENT* SemA;
OS_EVENT* SemB;

// Basic structural baseline for individual tasks
void Task1(void* pdata) {
	INT8U err;
    while (1) {
    	OSSemPend(GlobalSemaphore, 0, &err);
        printf("Task 1 Here. GlobalValue before mod: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue++; // Increment
        printf("Task 1 Incremented GlobalValue to: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue--; // Decrement
        printf("Task 1 Decremented GlobalValue back to: %d\n", Globalvalue);
        fflush(stdout);

        // semaphore
        OSSemPost(GlobalSemaphore);

        OSTimeDlyHMSM(0, 0, 2, 0);
    }
}

void Task2(void* pdata) {
	INT8U err;
    while (1) {
    	OSSemPend(GlobalSemaphore, 0, &err);
        printf("Task 2 Here. GlobalValue before mod: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue++; // Increment
        printf("Task 2 Incremented GlobalValue to: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue--; // Decrement
        printf("Task 2 Decremented GlobalValue back to: %d\n", Globalvalue);
        fflush(stdout);

        // semaphore
        OSSemPost(GlobalSemaphore);

        OSTimeDlyHMSM(0, 0, 2, 0);
    }
}

void Task3(void* pdata) {
	INT8U err;
    while (1) {
    	OSSemPend(GlobalSemaphore, 0, &err);
        printf("Task 3 Here. GlobalValue before mod: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue++; // Increment
        printf("Task 3 Incremented GlobalValue to: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue--; // Decrement
        printf("Task 3 Decremented GlobalValue back to: %d\n", Globalvalue);
        fflush(stdout);

        // semaphore
        OSSemPost(GlobalSemaphore);

        // Demo deadlock
        printf("[Deadlock Demo] Task 3 attempting to acquire Resource A.\n");
        fflush(stdout);
        OSSemPend(SemA, 0, &err);
        printf("[Deadlock Demo] Task 3 SUCCESSFUL holding Resource A.\n");
        fflush(stdout);

        // Give up the CPU temporarily so Task 4 can run and grab Resource B
        OSTimeDly(5);

        printf("[Deadlock Demo] Task 3 now attempting to acquire Resource B...\n");
        fflush(stdout);
        OSSemPend(SemB, 0, &err);

        OSSemPost(SemB);
        OSSemPost(SemA);

        OSTimeDlyHMSM(0, 0, 2, 0);
    }
}


void Task4(void* pdata) {
	INT8U err;
    while (1) {
    	OSSemPend(GlobalSemaphore, 0, &err);
        printf("Task 4 Here. GlobalValue before mod: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue++; // Increment
        printf("Task 4 Incremented GlobalValue to: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue--; // Decrement
        printf("Task 4 Decremented GlobalValue back to: %d\n", Globalvalue);
        fflush(stdout);

        // semaphore
        OSSemPost(GlobalSemaphore);

        // Demo deadlock
        printf("[Deadlock Demo] Task 4 attempting to acquire Resource B.\n");
        fflush(stdout);
        OSSemPend(SemB, 0, &err);
        printf("[Deadlock Demo] Task 4 SUCCESSFUL holding Resource B.\n");
        fflush(stdout);

        // Give up the CPU temporarily so Task 3 can proceed and grab Resource A
        OSTimeDly(5);

        printf("[Deadlock Demo] Task 4 now attempting to acquire Resource A...\n");
        fflush(stdout);
        OSSemPend(SemA, 0, &err);

        // These will never execute
        OSSemPost(SemA);
        OSSemPost(SemB);

        OSTimeDlyHMSM(0, 0, 2, 0);
    }
}

void Task5(void* pdata) {
	INT8U err;
    while (1) {
    	OSSemPend(GlobalSemaphore, 0, &err);
        printf("Task 5 Here. GlobalValue before mod: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue++; // Increment
        printf("Task 5 Incremented GlobalValue to: %d\n", Globalvalue);
        fflush(stdout);
        Globalvalue--; // Decrement
        printf("Task 5 Decremented GlobalValue back to: %d\n", Globalvalue);
        fflush(stdout);

        // semaphore
        OSSemPost(GlobalSemaphore);

        OSTimeDlyHMSM(0, 0, 2, 0);
    }
}

void StartTask(void* pdata)
{
	INT8U err;

	// Create a global semaphore for part c
	GlobalSemaphore = OSSemCreate(1);

	// Deadlock scenario part D
	SemA = OSSemCreate(1);
	SemB = OSSemCreate(1);
    printf("Start Task Running\n");
    fflush(stdout);

    // Create Tasks
    OSTaskCreateExt(Task1, NULL, &Task1Stk[TASK_STACKSIZE - 1], TASK1_PRIO, TASK1_PRIO, Task1Stk, TASK_STACKSIZE, NULL, OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    OSTaskCreateExt(Task2, NULL, &Task2Stk[TASK_STACKSIZE - 1], TASK2_PRIO, TASK2_PRIO, Task2Stk, TASK_STACKSIZE, NULL, OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    OSTaskCreateExt(Task3, NULL, &Task3Stk[TASK_STACKSIZE - 1], TASK3_PRIO, TASK3_PRIO, Task3Stk, TASK_STACKSIZE, NULL, OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    OSTaskCreateExt(Task4, NULL, &Task4Stk[TASK_STACKSIZE - 1], TASK4_PRIO, TASK4_PRIO, Task4Stk, TASK_STACKSIZE, NULL, OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);
    OSTaskCreateExt(Task5, NULL, &Task5Stk[TASK_STACKSIZE - 1], TASK5_PRIO, TASK5_PRIO, Task5Stk, TASK_STACKSIZE, NULL, OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

    printf("Five tasks created successfully\n\n");
    fflush(stdout);

    while (1)
    {
        OSTimeDlyHMSM(0, 0, 2, 0);
    }
}

int main(void)
{
	INT8U err;

    OSInit(); // Initialize uC/OS-II

    // Create the initial Start Task
    OSTaskCreateExt(StartTask,
                    NULL,
                    &StartTaskStk[TASK_STACKSIZE - 1],
                    START_TASK_PRIO,
                    START_TASK_PRIO,
                    StartTaskStk,
                    TASK_STACKSIZE,
                    NULL,
                    OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR);

    OSStart();
    return 0;
}
