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

#define STATE_QUEUE_LENGTH 1


#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

static TaskHandle_t DrawShapesTaskHandle = NULL;
static TaskHandle_t vSwapBuffersTaskHandle = NULL;

static QueueHandle_t StateQueue = NULL;
static SemaphoreHandle_t DrawSignal = NULL;
static SemaphoreHandle_t ScreenLock = NULL;


typedef struct buttons_buffer {
	unsigned char buttons[SDL_NUM_SCANCODES];
	SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };

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
            vTaskDelayUntil(&xLastWakeTime,
                            pdMS_TO_TICKS(frameratePeriod));
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


void vDrawShapesTask(void *pvParameters)
{
	//Variables for drawing Strings qnd Shapes
	static char Qstring[100];
	static char KeyBoardCountString[100];
	static char text_below[100];
	static char text_above[100];
	static int Qstring_width = 0;
	static int KeyBoardCountString_width = 0;
	static int text_below_width = 0;
	static int text_above_width = 0;
	static my_triangle_t tri;
	coord_t p_1;
	coord_t p_2;
	coord_t p_3;

	/* const unsigned char next_state_control = NEXT_TASK; //Allowing transition to next state */

	//Variables used for moving shapes
	static int wheel = 0;
	static float angle = 0; //Rotation angle

	//Variables for Counting Button Presses
	static int ButtonA = 0;
	static int ButtonB = 0;
	static int ButtonC = 0;
	static int ButtonD = 0;
	

	tumDrawBindThread();

	while (1) {
		/*if (xSemaphoreTake*/ if (DrawSignal) /*, portMAX_DELAY) == pdTRUE)*/ {
			// tumEventFetchEvents();
			//int MouseX = tumEventGetMouseX();
			//int MouseY = tumEventGetMouseY();

			if (xSemaphoreTake(DrawSignal, portMAX_DELAY) == pdTRUE) {

				xGetButtonInput();
				xSemaphoreTake(ScreenLock, portMAX_DELAY);

				if (buttons.buttons[KEYCODE(Q)]) {
					exit(EXIT_SUCCESS);
				}
				//else if (buttons.buttons[KEYCODE(E)]) { //Equiv to SDL_SCANCODE_E
					/* xQueueSend (StateQueue,&next_state_control,100); */
			
				if (buttons.buttons[KEYCODE(A)]) {
					ButtonA++;
				}
				else if (buttons.buttons[KEYCODE(B)]) {
					ButtonB++;
				}
				else if (buttons.buttons[KEYCODE(C)]) {
					ButtonC++;
				} 
				else if (buttons.buttons[KEYCODE(D)]) {
					ButtonD++;
				} 
				else if (tumEventGetMouseRight()) {
					ButtonA = 0;
					ButtonB = 0;
					ButtonC = 0;
					ButtonD = 0;
				}	

					

				tumDrawClear(White); // Clear screen

				p_1.x = (SCREEN_WIDTH / 2) - 15;
				p_1.y = (SCREEN_HEIGHT / 2) + 15;
				p_2.x = (SCREEN_WIDTH / 2) + 15;
				p_2.y = (SCREEN_HEIGHT / 2) + 15;
				p_3.x = (SCREEN_WIDTH / 2);
				p_3.y = (SCREEN_HEIGHT / 2) - 15;
				coord_t points[3] = { p_1, p_2, p_3 };
				tri.points = points;
				tri.color = Green;
				if (!tumDrawTriangle(tri.points, tri.color)) {} //Draw Triangle.

				if (!tumDrawFilledBox((SCREEN_WIDTH / 2)+ 60*cos(angle),
				      	(SCREEN_HEIGHT / 2) + 60*sin(angle),
				      	25,25,TUMBlue)){} //Draw rotating square.	

				if (!tumDrawCircle( (SCREEN_WIDTH/2) - 40*cos(angle-180),
				   	  	(SCREEN_HEIGHT/2) - 40*sin(angle-180),
				   	  12.5, Red))  {} //Draw rotating Circle.	

				angle = angle + 0.1;
				if (angle == 360.1) {
					angle = 0;
				}
		
				sprintf(Qstring,"Press Q to quit"); // Formatting string into char array.
				sprintf(text_below, "Press E to switch states");
				sprintf(text_above, "This text is moving");
				sprintf(KeyBoardCountString, "A: %d | B: %d | C: %d | D: %d",ButtonA, ButtonB, ButtonC, ButtonD);

				if (!tumGetTextSize((char *)Qstring, &Qstring_width, NULL))
					tumDrawText(Qstring,
				    	(SCREEN_WIDTH / 2) - (Qstring_width / 2),
				    	(SCREEN_HEIGHT * 7 / 8) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Navy);

				if (!tumGetTextSize((char *)text_below, &text_below_width,NULL))
					tumDrawText(text_below,
				    	(SCREEN_WIDTH / 2) - (text_below_width / 2),
				    	(SCREEN_HEIGHT * 6 / 8) -
					    (DEFAULT_FONT_SIZE / 2),
				    	Olive);

				if (!tumGetTextSize((char *)text_above, &text_above_width,NULL))
					tumDrawText(text_above, 
						wheel,
				    	(SCREEN_HEIGHT / 8) - (DEFAULT_FONT_SIZE / 2),
				    	Gray);
		
				wheel ++;

				if ((wheel + text_above_width) >= SCREEN_WIDTH) {
				wheel = 0;
				}

				if (!tumGetTextSize((char *)KeyBoardCountString,&KeyBoardCountString_width, NULL))
						tumDrawText(KeyBoardCountString,
				    	(SCREEN_WIDTH / 4) - (KeyBoardCountString_width / 2),
				    	(SCREEN_HEIGHT / 15) - (DEFAULT_FONT_SIZE / 2),
				    	Black);
						
				xSemaphoreGive(ScreenLock); 
				tumDrawUpdateScreen(); // Refresh the screen to draw string

				//vTaskDelay(
					// (TickType_t)1000); // Basic sleep of 1000 milliseconds
			}		
		}
	}
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
	
	if (xTaskCreate(vSwapBuffers, "BufferSwapTask",mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES,vSwapBuffersTaskHandle) != pdPASS) {
        //PRINT_TASK_ERROR("BufferSwapTask");
        //goto err_bufferswap;
	}

	if (xTaskCreate(vDrawShapesTask, "DrawShapesTask", mainGENERIC_STACK_SIZE * 2, NULL,
			configMAX_PRIORITIES - 1, &DrawShapesTaskHandle) != pdPASS) {
		goto err_drawshapestask;
	}

	vTaskStartScheduler();

	return EXIT_SUCCESS;

err_drawshapestask:
	vSemaphoreDelete(buttons.lock);
//err_bufferswap:
    //vTaskDelete(StateMachine);
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
