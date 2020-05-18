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


static TaskHandle_t DemoTask = NULL;

typedef struct buttons_buffer {
    unsigned char buttons[SDL_NUM_SCANCODES];
    SemaphoreHandle_t lock;
} buttons_buffer_t;

static buttons_buffer_t buttons = { 0 };


void xGetButtonInput(void)
{
    if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) 
    {
        xQueueReceive(buttonInputQueue, &buttons.buttons, 0);
        xSemaphoreGive(buttons.lock);
    }
}

#define KEYCODE(CHAR) SDL_SCANCODE_##CHAR

void vDemoTask(void *pvParameters)
{
    //String
    static char my_string[100];
    static char text_below[100];
    static char text_above[100];
    static char keyboard_count[100];
    static int my_strings_width = 0;
    static int text_below_width = 0;
    static int text_above_width = 0;
    static int keyboard_count_width = 0;

    //Variables used for moving
    static int wheel;
    static int angle;

    //Creating Circle.
    static signed short circle_x=(SCREEN_WIDTH / 2) - 75;
    static signed short circle_y=SCREEN_HEIGHT  / 2;
    static signed short radius=25;
    my_circle_t* circ=create_circ(circle_x,circle_y,radius,Red);


    //Creating Triangle.
    
    //static signed short tri_x=SCREEN_WIDTH/2;
    //static signed short tri_y=SCREEN_HEIGHT/2;
    //my_triangle_t* tri=create_tri(tri_x,tri_y,Green);
    static my_triangle_t tri;

    coord_t p_1;
    p_1.x=(SCREEN_WIDTH / 2) - 25 ;
    p_1.y=SCREEN_HEIGHT / 2;
    coord_t p_2;
    p_2.x=(SCREEN_WIDTH / 2) + 25 ;
    p_2.y=SCREEN_HEIGHT / 2;
    coord_t p_3;
    p_3.x=SCREEN_WIDTH / 2;
    p_3.y=(SCREEN_HEIGHT / 2) - 50;
    
    coord_t points[3] ={p_1,p_2,p_3};

    tri.points = points;
    tri.color = Green;

    //Creating Square.
    static signed short side=50;
    static signed short box_x=(SCREEN_WIDTH /2) + 50;
    static signed short box_y=SCREEN_HEIGHT / 2;
    my_square_t* box=create_box(box_x,box_y,side,TUMBlue);

    //Initializing Moving Variables
    wheel = 0;
    angle = 0;

    tumDrawBindThread();

    while (1) 
    {
        tumEventFetchEvents(); 
        xGetButtonInput(); 

        if (xSemaphoreTake(buttons.lock, 0) == pdTRUE) 
        {
            if (buttons.buttons[KEYCODE(Q)]) // Equiv to SDL_SCANCODE_Q
            { 
                exit(EXIT_SUCCESS);
            }
            xSemaphoreGive(buttons.lock);
        }

        tumDrawClear(White); // Clear screen

        sprintf(my_string,"Press Q to quit"); // Formatting string into char array.
	sprintf(text_below,"This is just some random text");
	sprintf(text_above,"This text is moving");
	sprintf(keyboard_count,"A : B : C : D : ");

        if (!tumGetTextSize((char *)my_string,&my_strings_width, NULL))
            tumDrawText(my_string,SCREEN_WIDTH / 2 -
                        my_strings_width / 2,
                        SCREEN_HEIGHT*7 / 8 - DEFAULT_FONT_SIZE / 2,
                        Navy);

	if (!tumGetTextSize((char *)text_below,&text_below_width, NULL))
            tumDrawText(text_below,SCREEN_WIDTH / 2 -
                        text_below_width / 2,
                        SCREEN_HEIGHT*6 / 8 - DEFAULT_FONT_SIZE / 2,
                        Olive);

       if (!tumGetTextSize((char *)text_above,&text_above_width, NULL))
            tumDrawText(text_above,wheel,
                        SCREEN_HEIGHT / 8 - DEFAULT_FONT_SIZE / 2,
                        Gray);

       wheel+=10;
       
       if ((wheel+text_above_width) >= SCREEN_WIDTH){
	    wheel=0;
       }  

       if (!tumGetTextSize((char *)keyboard_count,&keyboard_count_width, NULL))
            tumDrawText(keyboard_count,SCREEN_WIDTH / 6 -
                        keyboard_count_width / 2,
                        SCREEN_HEIGHT / 15 - DEFAULT_FONT_SIZE / 2,
                        Black);


        
        if (!tumDrawCircle((SCREEN_WIDTH/2) + 60*cos(angle-180),(SCREEN_HEIGHT/2) +10+ 60*sin(angle-180),circ->radius,
	    circ->color)){} //Draw rotating Circle.

        if (!tumDrawTriangle(tri.points,tri.color)){} //Draw Triangle.
        
        if(!tumDrawFilledBox((SCREEN_WIDTH/2) - 10 + 60*cos(angle+180) ,(SCREEN_HEIGHT/2) + 60*sin(angle+180),
           box->width,box->height,box->color)){} //Draw rotating Box.

	angle = angle + 0.1;
	if (angle == 360.1){
	    angle = 0;
	}	

        tumDrawUpdateScreen(); // Refresh the screen to draw string

        vTaskDelay((TickType_t)1000); // Basic sleep of 1000 milliseconds
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

    if (xTaskCreate(vDemoTask, "DemoTask", mainGENERIC_STACK_SIZE * 2, NULL,
                    mainGENERIC_PRIORITY, &DemoTask) != pdPASS) {
        goto err_demotask;
    }

    vTaskStartScheduler();

    return EXIT_SUCCESS;

err_demotask:
    vSemaphoreDelete(buttons.lock);
err_buttons_lock:
    tumSoundExit();
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
