#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>

#include <SDL2/SDL_scancode.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include "TUM_Ball.h"
#include "TUM_Draw.h"
#include "TUM_Event.h"
#include "TUM_Sound.h"
#include "TUM_Utils.h"
#include "TUM_Font.h"

#include "AsyncIO.h"

#include "shapes.h"

#define mainGENERIC_PRIORITY (tskIDLE_PRIORITY)
#define mainGENERIC_STACK_SIZE ((unsigned short)2560)
#define STACK_SIZE 1000

#define STATE_QUEUE_LENGTH 1
#define STATE_COUNT 3

#define STATE_ONE 1
#define STATE_TWO 2
#define STATE_THREE 3

#define NEXT_TASK 1
#define PREV_TASK 2

#define STARTING_STATE STATE_ONE

#define STATE_DEBOUNCE_DELAY 300


#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

static uint8_t Circle1Hz;
static uint8_t Circle2Hz;
static uint8_t A_Count = 0;
static uint8_t B_Count = 0;
static uint8_t Timer_Count = 0;


const unsigned char next_state_signal = NEXT_TASK; //0
const unsigned char prev_state_signak = PREV_TASK; //1

static TaskHandle_t StateMachineTaskHandle = NULL;
static TaskHandle_t vDrawShapesTask1Handle = NULL;
static TaskHandle_t vDrawShapesTask2Handle = NULL;
static TaskHandle_t vSwapBuffersTaskHandle = NULL;
static TaskHandle_t vBlinkingCircle1HzTaskHandle = NULL;
static TaskHandle_t vBlinkingCircle2HzTaskHandle = NULL;
static TaskHandle_t vCountingTask1Handle = NULL;
static TaskHandle_t vCountingTask2Handle = NULL;
static TaskHandle_t vT1n2ResetTaskHandle = NULL;
static TaskHandle_t TimerTaskHandle = NULL;

static QueueHandle_t StateQueue = NULL;
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;
static SemaphoreHandle_t vCountingTask2_Semaphore = NULL;


typedef struct buttons_buffer {
	unsigned char buttons[SDL_NUM_SCANCODES];
	SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

StaticTask_t xTaskBuffer;
StackType_t xStack[ STACK_SIZE ]; 


void changeState(volatile unsigned char *state, unsigned char forwards)
{
    switch (forwards) {
        case 0:
            if (*state == 0) {
                *state = STATE_COUNT;
            }
            else {
                (*state)--;
            }
            break;
        case 1:
            if (*state == STATE_COUNT) {
                *state = 0;
            }
            else {
                (*state)++;
            }
            break;
        default:
            break;
    }
}

void basicSequentialStateMachine(void *pvParameters)
{
    unsigned char current_state = STARTING_STATE; // Default state
    unsigned char state_changed = 1; // Only re-evaluate state if it has changed
    unsigned char input = 0;

    //const int state_change_period = STATE_DEBOUNCE_DELAY;

    while (1) {
        if (state_changed) {
            goto initial_state;
        }

        // Handle state machine input
        if (StateQueue)
            if (xQueueReceive(StateQueue, &input, portMAX_DELAY) == pdTRUE) {
                if (input == NEXT_TASK) {
                    changeState(&current_state, 1);
                    state_changed = 1;   
                } else if (input == PREV_TASK) {
					changeState(&current_state, 0);
					state_changed = 1;
				}
			}

initial_state:
        if (state_changed) {
            switch (current_state) {
                case STATE_ONE:
					vTaskSuspend(vBlinkingCircle1HzTaskHandle);
					vTaskSuspend(vBlinkingCircle2HzTaskHandle);
					vTaskSuspend(vDrawShapesTask2Handle);
					vTaskSuspend(TimerTaskHandle);
					vTaskResume(vDrawShapesTask1Handle);
					state_changed = 0;
                    break;
                case STATE_TWO:
					vTaskSuspend(vDrawShapesTask1Handle);
					vTaskSuspend(vCountingTask1Handle);
					vTaskSuspend(vCountingTask2Handle);
					vTaskSuspend(vT1n2ResetTaskHandle);
					vTaskResume(vBlinkingCircle2HzTaskHandle);
					vTaskResume(vBlinkingCircle1HzTaskHandle);
					vTaskResume(TimerTaskHandle);
					vTaskResume(vDrawShapesTask2Handle);
                    state_changed = 0;
					break;
				case STATE_THREE:
					state_changed = 0;
					break;
                default:
                    break;
            }
        }
    }
}

void vSwapBuffers(void *pvParameters)
{
    TickType_t xLastWakeTime;
    xLastWakeTime = xTaskGetTickCount();
    const TickType_t frameratePeriod = 20;

    tumDrawBindThread(); // Setup Rendering handle with correct GL context

    while (1) {
        if (xSemaphoreTake(ScreenLock, portMAX_DELAY) == pdTRUE) {
            tumDrawUpdateScreen();
            tumEventFetchEvents();
            xSemaphoreGive(ScreenLock);
            xSemaphoreGive(DrawSignal);
            vTaskDelayUntil(&xLastWakeTime,pdMS_TO_TICKS(frameratePeriod));
        }
    }
}


void xGetButtonInput(void)
{
	if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
		xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
		xSemaphoreGive(buttons.lock);
	}
}


static int vCheckStateInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
        if (buttons.buttons[KEYCODE(E)]) {
        	buttons.buttons[KEYCODE(E)] = 0;
            if (StateQueue) {
                xSemaphoreGive(buttons.lock);
                xQueueSend(StateQueue, &next_state_signal, 0);
                return -1;
            }
        }
        xSemaphoreGive(buttons.lock);
    }

    return 0;
}


void vDrawShapesTask1(void *pvParameters)
{
	const unsigned char next_state_signal = NEXT_TASK;
	//Variables for drawing Strings and Shapes
	static char Qstring[100];
	static char KeyBoardCountString[100];
	static char text_below[100];
	static char text_above[100];
	static char Axis_String[100];
	static int Qstring_width = 0;
	static int KeyBoardCountString_width = 0;
	static int text_below_width = 0;
	static int text_above_width = 0;
	static int Axis_String_width = 0;
	static my_triangle_t tri;
	coord_t p_1;
	coord_t p_2;
	coord_t p_3;

	//Variables used for moving shapes
	static int wheel = 0;
	static float angle = 0; //Rotation angle

	//Variables for Counting Button Presses
	static int ButtonA = 0;
	static int ButtonB = 0;
	static int ButtonC = 0;
	static int ButtonD = 0;

	while (1) {
		if (DrawSignal) {	
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE) {

				xGetButtonInput();
				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				
				if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
				
					if (buttons.buttons[KEYCODE(A)]) {
						ButtonA++;
						buttons.buttons[KEYCODE(A)] = 0;
					}
					else if (buttons.buttons[KEYCODE(B)]) {
						ButtonB++;
						buttons.buttons[KEYCODE(B)] = 0;
					}
					else if (buttons.buttons[KEYCODE(C)]) {
						ButtonC++;
						buttons.buttons[KEYCODE(C)] = 0;
					} 
					else if (buttons.buttons[KEYCODE(D)]) {
						ButtonD++;
						buttons.buttons[KEYCODE(D)] = 0;
					} 
					else if (tumEventGetMouseRight()) {
						ButtonA = 0;
						ButtonB = 0;
						ButtonC = 0;
						ButtonD = 0;
					}	

				xSemaphoreGive(buttons.lock);
				}

				tumDrawClear(White); // Clear screen

				p_1.x = tumEventGetMouseX() / 4+(SCREEN_WIDTH / 2) - 15;
				p_1.y = tumEventGetMouseY() / 4+(SCREEN_HEIGHT / 2) + 15;
				p_2.x = tumEventGetMouseX() / 4+(SCREEN_WIDTH / 2) + 15;
				p_2.y = tumEventGetMouseY() / 4+(SCREEN_HEIGHT / 2) + 15;
				p_3.x = tumEventGetMouseX() / 4+(SCREEN_WIDTH / 2);
				p_3.y = tumEventGetMouseY() / 4+(SCREEN_HEIGHT / 2) - 15;
				coord_t points[3] = { p_1, p_2, p_3 };
				tri.points = points;
				tri.color = Green;
				if (!tumDrawTriangle(tri.points, tri.color)) {} //Draw Triangle.

				if (!tumDrawFilledBox(tumEventGetMouseX() / 4+(SCREEN_WIDTH / 2)+ 60*cos(angle),
				      	tumEventGetMouseY() / 4+(SCREEN_HEIGHT / 2) + 60*sin(angle),
				      	25,25,TUMBlue)){} //Draw rotating square.	

				if (!tumDrawCircle(tumEventGetMouseX() / 4+(SCREEN_WIDTH/2) - 40*cos(angle-180),
				   	  	tumEventGetMouseY() / 4+(SCREEN_HEIGHT/2) - 40*sin(angle-180),
				   	  	12.5, Red))  {} //Draw rotating Circle.	

				angle = angle + 0.1;
				if (angle == 360.1) {
					angle = 0;
				}
		
				sprintf(Qstring,"Press Q to quit"); // Formatting string into char array.
				sprintf(text_below, "Press E to switch states");
				sprintf(text_above, "This text is moving");
				sprintf(KeyBoardCountString, "A: %d | B: %d | C: %d | D: %d",ButtonA, ButtonB, ButtonC, ButtonD);
				sprintf(Axis_String, "Axis 1: %5d | Axis 2: %5d", tumEventGetMouseX(), tumEventGetMouseY());
				
				if (!tumGetTextSize((char *)Qstring, &Qstring_width, NULL))
					tumDrawText(Qstring,
				    	tumEventGetMouseX() / 4+(SCREEN_WIDTH / 2) - (Qstring_width / 2),
				    	tumEventGetMouseY() / 4+(SCREEN_HEIGHT * 7 / 8) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Navy);

				if (!tumGetTextSize((char *)text_below, &text_below_width,NULL))
						tumDrawText(text_below,
				    	tumEventGetMouseX() / 4+(SCREEN_WIDTH / 2) - (text_below_width / 2),
				    	tumEventGetMouseY() / 4+(SCREEN_HEIGHT * 6 / 8) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Olive);

				if (!tumGetTextSize((char *)text_above, &text_above_width,NULL))
						tumDrawText(text_above, 
						tumEventGetMouseX() / 4 + wheel,
				    	tumEventGetMouseY() / 4 + (SCREEN_HEIGHT / 8) - (DEFAULT_FONT_SIZE / 2),
				    	Gray);
		
				wheel ++;

				if ((wheel + text_above_width) >= SCREEN_WIDTH) {
				wheel = 0;
				}

				if (!tumGetTextSize((char *)KeyBoardCountString,&KeyBoardCountString_width, NULL))
						tumDrawText(KeyBoardCountString,
				    	tumEventGetMouseX() / 4+(SCREEN_WIDTH / 6) - (KeyBoardCountString_width / 2),
				    	tumEventGetMouseY() / 4+(SCREEN_HEIGHT / 20) - (DEFAULT_FONT_SIZE / 2),
				    	Black);

				if (!tumGetTextSize((char *)Axis_String,&Axis_String_width, NULL))
						tumDrawText(Axis_String,
						tumEventGetMouseX() / 4+ (SCREEN_WIDTH / 6) - (Axis_String_width / 2),
						tumEventGetMouseY() / 4+ (SCREEN_HEIGHT / 20) + 15 -(DEFAULT_FONT_SIZE / 2),
						Black);
						
				xSemaphoreGive(ScreenLock); 
				vCheckStateInput();
			}		
		}
	}
}


void vBlinkingCircle2HzTask(void *pvParameters) {
	while (1) {
		Circle1Hz = 1;
		vTaskDelay(250 / portTICK_PERIOD_MS);
		Circle1Hz = 0;
		// Olive Circle
		vTaskDelay(250 / portTICK_PERIOD_MS);
	}

}
void vBlinkingCircle1HzTask(void *pvParameters) {
	while (1) {
		Circle2Hz = 1;
		vTaskDelay(500 / portTICK_PERIOD_MS);
		Circle2Hz = 0;
		// Black Circle
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}

}
void vCountingTask1() {
	static char string[100];
	static int string_width = 0;
	while (1) {
		uint32_t NotifiedValue;
		if (xTaskNotifyWait(0x00,0xffffffff, &NotifiedValue, portMAX_DELAY) == pdTRUE) {
			A_Count++ ;
		}	
		sprintf(string, "A: %d ", A_Count);
		if (!tumGetTextSize((char *)string, &string_width, NULL))
			tumDrawText(string,
				    	(SCREEN_WIDTH / 6) - (string_width / 2),
				    	(SCREEN_HEIGHT * 8 / 10) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Blue);

	}
}

void vCountingTask2() {
	while (1) {
		if ( xSemaphoreTake(vCountingTask2_Semaphore,portMAX_DELAY) == pdTRUE) {
			B_Count++ ;
		}

	}
}

void vT1n2ResetTask() {
	while (1) {
		A_Count = 0;
		B_Count = 0;
		vTaskDelay(15000 / portTICK_PERIOD_MS);
	}

}

void TimerTask() {

	while (1) {
		Timer_Count++;
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

void vDrawShapesTask2(void *pvParameters) {

	const unsigned char next_state_signal = NEXT_TASK;
	unsigned char go_task1=0, go_task2=0;
	static uint8_t Task_Count = 0;
	static char FPS_String[100];
	static char String1[100];
	static char String2[100];
	static int FPS_String_width = 0;
	static int String1_width = 0;
	static int String2_width = 0;

	while (1) {
		if (DrawSignal) {	
			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE) {

				xGetButtonInput();
				xSemaphoreTake(ScreenLock, portMAX_DELAY);
				
				/*if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) {
				
					if (buttons.buttons[KEYCODE(E)]) { 
						xQueueSend(StateQueue,&next_state_signal,100);
						buttons.buttons[KEYCODE(E)] = 0;
					}	

				xSemaphoreGive(buttons.lock);
				}*/

				tumDrawClear(White); // Clear screen

		    	//FPS Display
		        sprintf(FPS_String, "Frame rate: %d Hz", 1000 / 20);

		 		if (!tumGetTextSize((char *)FPS_String, &FPS_String_width, NULL))
					tumDrawText(FPS_String,
				    	(SCREEN_WIDTH * 5 / 6) - (FPS_String_width / 2),
				    	(SCREEN_HEIGHT * 9 / 10) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Black);

		 		//Manipulating Blinking Circles
				if (Circle1Hz == 1) {
				if (!tumDrawCircle((SCREEN_WIDTH / 2) - 40,(SCREEN_HEIGHT / 2),20, Olive))  {}
				}

				if (Circle2Hz == 1) {
				if (!tumDrawCircle((SCREEN_WIDTH / 2) + 40,(SCREEN_HEIGHT / 2),20,Black))  {}
				}

				

				//Task that appears when pressing A
				if (buttons.buttons[KEYCODE(A)]) {
					go_task1 = 1 ;
					vTaskResume(vCountingTask1Handle);
					vTaskResume(vT1n2ResetTaskHandle);
					//Set the notification value of the task referenced by task1_countingHandle to 0x01
					xTaskNotify(vCountingTask1Handle, 0x01, eSetValueWithOverwrite);
					buttons.buttons[KEYCODE(A)] = 0;
				}

				if (go_task1 == 1) {
					vTaskResume(vCountingTask1Handle);
					vTaskResume(vT1n2ResetTaskHandle);
				}

				//Task that appears when pressing B
				if (buttons.buttons[KEYCODE(B)]) {
					go_task2 = 1;
					vTaskResume(vCountingTask2Handle);
					vTaskResume(vT1n2ResetTaskHandle);
					xSemaphoreGive(vCountingTask2_Semaphore);
					buttons.buttons[KEYCODE(B)] = 0;
				}
				if (go_task2 == 1) {
					vTaskResume(vCountingTask2Handle);
					vTaskResume(vT1n2ResetTaskHandle);
					sprintf(String1, "B: %d ", B_Count);
					if (!tumGetTextSize((char *)String1, &String1_width, NULL))
					tumDrawText(String1,
				    	(SCREEN_WIDTH / 6) - (String1_width / 2),
				    	(SCREEN_HEIGHT * 9 / 10) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Black);
				}


				//Timer is controlled using C Button
				sprintf(String2, "Timer: %d", Timer_Count);
				if (!tumGetTextSize((char *)String2, &String2_width, NULL))
					tumDrawText(String2,
				    	(SCREEN_WIDTH * 5/ 6) - (String2_width / 2),
				    	(SCREEN_HEIGHT * 8 / 10) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Black);

				if (buttons.buttons[KEYCODE(C)]) {
					Task_Count++;
					if (Task_Count % 2 != 0 ) {
						vTaskSuspend(TimerTaskHandle);
					}
					else {
						vTaskResume(TimerTaskHandle);
					}
					buttons.buttons[KEYCODE(C)] = 0;
				}

				xSemaphoreGive(ScreenLock); 
				// tumDrawUpdateScreen(); // Refresh the screen to draw string
				vCheckStateInput();
			}
		}
	}
}

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
StackType_t **ppxIdleTaskStackBuffer,
uint32_t *pulIdleTaskStackSize )
{

static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

*ppxIdleTaskStackBuffer = uxIdleTaskStack;

*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
StackType_t **ppxTimerTaskStackBuffer,
uint32_t *pulTimerTaskStackSize )
{

static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];


*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

*ppxTimerTaskStackBuffer = uxTimerTaskStack;

*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

int main(int argc, char *argv[]) 
{
	
	char *bin_folder_path = tumUtilGetBinFolderPath(argv[0]);

	printf("Initializing: ");

	if (tumDrawInit(bin_folder_path)) {
		PRINT_ERROR("Failed to initialize drawing");
		goto err_init_drawing;
	}

	if (tumEventInit()) {
		PRINT_ERROR("Failed to initialize events");
		goto err_init_events;
	}

	if (tumSoundInit(bin_folder_path)) {
		PRINT_ERROR("Failed to initialize audio");
		goto err_init_audio;
	}

	buttons.lock = xSemaphoreCreateMutex(); // Locking mechanism
	if (!buttons.lock) {
		PRINT_ERROR("Failed to create buttons lock");
		goto err_buttons_lock;
	}

	DrawSignal = xSemaphoreCreateBinary(); // Screen buffer locking
    if (!DrawSignal) {
        PRINT_ERROR("Failed to create draw signal");
        goto err_draw_signal;
    }

    ScreenLock = xSemaphoreCreateMutex();
    if (!ScreenLock) {
        PRINT_ERROR("Failed to create screen lock");
        goto err_screen_lock;
    } 

	// Message sending
    StateQueue = xQueueCreate(STATE_QUEUE_LENGTH, sizeof(unsigned char));
    if (!StateQueue) {
        PRINT_ERROR("Could not open state queue");
        goto err_state_queue;
    }
	
	if (xTaskCreate(vSwapBuffers, "SwapBuffers",mainGENERIC_STACK_SIZE * 2, NULL,
					configMAX_PRIORITIES,vSwapBuffersTaskHandle) != pdPASS) {
        goto err_bufferswap;
	
	}
	if (xTaskCreate(basicSequentialStateMachine, "StateMachine",mainGENERIC_STACK_SIZE * 2, NULL,
                    configMAX_PRIORITIES - 1, StateMachineTaskHandle) != pdPASS) {
        goto err_statemachine;
	}

	if (xTaskCreate(vDrawShapesTask1, "DrawShapesTask1", mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 3, &vDrawShapesTask1Handle) != pdPASS) {
		goto err_drawshapestask1;
	}
	if (xTaskCreate(vDrawShapesTask2, "DrawShapesTask2", mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 3, &vDrawShapesTask2Handle) != pdPASS) {
		goto err_drawshapestask2;
	}

	xTaskCreate(vBlinkingCircle2HzTask, "BlinkingCircle2HzTask", mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES -3, &vBlinkingCircle2HzTaskHandle) ;
	xTaskCreate(vBlinkingCircle1HzTask, "BlinkingCircle1HzTask", mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES -5, &vBlinkingCircle1HzTaskHandle) ;
    		//vBlinkingCircle1HzTaskHandle= xTaskCreateStatic(vBlinkingCircle1HzTask, "BlinkingCircle1HzTask",
			//STACK_SIZE,NULL, configMAX_PRIORITIES - 5,xStack, &xTaskBuffer);
	vCountingTask2_Semaphore = xSemaphoreCreateBinary(); // binary Semaphore for counting task
    xTaskCreate(vCountingTask1, "CountingTask1", 1000, NULL, 4, &vCountingTask1Handle);
    xTaskCreate(vCountingTask2, "CountingTask2", 1000, NULL, 5,&vCountingTask2Handle);
    xTaskCreate(vT1n2ResetTask, "T1n2ResetTask", 1000, NULL, 5,&vT1n2ResetTaskHandle);

    xTaskCreate(TimerTask, "TimerTask", 1000, NULL, configMAX_PRIORITIES-3, &TimerTaskHandle);

	vTaskSuspend(vDrawShapesTask1Handle);
	vTaskSuspend(vDrawShapesTask2Handle);
    vTaskSuspend(TimerTaskHandle);	

	vTaskStartScheduler();

	return EXIT_SUCCESS;

err_drawshapestask1:
	vSemaphoreDelete(buttons.lock);
err_drawshapestask2:
	vSemaphoreDelete(buttons.lock);
err_bufferswap:
    vTaskDelete(StateMachineTaskHandle);
err_statemachine:
    vQueueDelete(StateQueue);
err_buttons_lock:
	tumSoundExit();
err_state_queue:
	vSemaphoreDelete(StateQueue);
err_screen_lock:
	vSemaphoreDelete(DrawSignal);
err_draw_signal:
	vSemaphoreDelete(buttons.lock);
err_init_audio:
	tumEventExit();
err_init_events:
	tumDrawExit();
err_init_drawing:
	return EXIT_FAILURE;
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vMainQueueSendPassed(void)
{
	/* This is just an example implementation of the "queue send" trace hook. */
}

// cppcheck-suppress unusedFunction
__attribute__((unused)) void vApplicationIdleHook(void)
{
#ifdef __GCC_POSIX__
	struct timespec xTimeToSleep, xTimeSlept;
	/* Makes the process more agreeable when using the Posix simulator. */
	xTimeToSleep.tv_sec = 1;
	xTimeToSleep.tv_nsec = 0;
	nanosleep(&xTimeToSleep, &xTimeSlept);
#endif
}
