// File **********Switch.c***********
// Lab5
// Programs to interface with Switch buttons   
// ECE445L Fall 2026
//    Jonathan W. Valvano 6/29/26
// 2-bit input, positive logic switches, positive logic software
// define your hardware interface
// bit1 PA28 Voice switch 
// bit0 PA27 Play switch 


#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "Switch.h"

//PIN CONFIGURATION   
#define PLAY_MASK  (1U << 27)
#define VOICE_MASK (1U << 28)
#define REWIND_MASK (1U << 6)
 

//---------------------Switch_Init---------------------
// initialize switch interface
// Input: none
// Output: none 
void Switch_Init(void){ 
   // 1. Configure IOMUX for pure inputs (Input Enable bit set, no internal resistors)
   IOMUX->SECCFG.PINCM[PA27INDEX] = 0x00040081; 
   IOMUX->SECCFG.PINCM[PA28INDEX] = 0x00040081;
   IOMUX->SECCFG.PINCM[PB6INDEX] = 0x00040081;

   // 2. Clear DOE bits to 0 to ensure they are configured as inputs
   GPIOA->DOE31_0 &= ~(1 << 27);  
   GPIOA->DOE31_0 &= ~(1 << 28);
   GPIOB->DOE31_0 &= ~(1 << 6);
}

//---------------------Switch_Play---------------------
// read the switches
// Input: none
// Output: false the Play switch not pressed
//         true the Play switch is pressed 
int Switch_Play(void){ 
   return (GPIOA->DIN31_0 & PLAY_MASK) != 0;
}
//---------------------Switch_Voice---------------------
// read the switches
// Input: none
// Output: false the Voice switch not pressed
//         true the Voice switch is pressed 
//Playback speed!
int Switch_Voice(void){ 
   return (GPIOA->DIN31_0 & VOICE_MASK) != 0;
}

int Switch_Rewind(void) {
   return (GPIOB->DIN31_0 & REWIND_MASK) != 0;
}

typedef struct {
  uint32_t stablePressed;
  uint32_t changeCount;
} ButtonState_t;

static ButtonState_t ButtonStates[BUTTON_COUNT] = {0};

static uint32_t ReadButton(Button_t button){
  switch(button){
    case BUTTON_PLAY:
      return Switch_Play();

    case BUTTON_REWIND:
      return Switch_Rewind();

    case BUTTON_SPEED:
      return Switch_Voice();

    default:
      return 0;
  }
}

// Call once per millisecond for each button using TimerG8
// Returns 1 once after a stable press followed by a stable release.
uint32_t Get_Button_Press(Button_t button){
  ButtonState_t *state = &ButtonStates[button];
  uint32_t cur_state = ReadButton(button);

  if(cur_state == state->stablePressed){ //still on same state
    state->changeCount = 0;
  }
  
  else{ //change in state has been detected
    state->changeCount++;

    if(state->changeCount >= 10){ //if state is still the same for 10ms...
      // Accept the new state: either pressed OR released.
      state->stablePressed = cur_state;
      state->changeCount = 0;

      if(cur_state == 0){ //a press and release has been detected
        return 1;
      }
    }
  }

  return 0;
}

