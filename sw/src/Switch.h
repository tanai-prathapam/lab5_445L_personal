// File **********Switch.h***********
// Lab 5
// Programs to interface with Switch buttons   
// EE445L Fall 2026
//    Jonathan W. Valvano 6/29/26
// 2-bit input, positive logic switches, positive logic software

#include <stdint.h>

    // write this
    // ***solution***
// PA28 Voice switch 
// PA27 Play switch 

//---------------------Switch_Init---------------------
// initialize switch interface
// Input: none
// Output: none 
void Switch_Init(void);



//---------------------Switch_Play---------------------
// read the switches
// Input: none
// Output: false the Play switch not pressed
//         true the Play switch is pressed 
int Switch_Play(void);

//---------------------Switch_Voice---------------------
// read the switches
// Input: none
// Output: false the Voice switch not pressed
//         true the Voice switch is pressed 
int Switch_Voice(void);

int Switch_Rewind(void);

typedef enum {
  BUTTON_PLAY,
  BUTTON_REWIND,
  BUTTON_SPEED,
  BUTTON_COUNT
} Button_t;

uint32_t Get_Button_Press(Button_t button);

