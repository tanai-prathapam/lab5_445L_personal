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
  // ****ECE445L write this ****
 



//---------------------Switch_Init---------------------
// initialize switch interface
// Input: none
// Output: none 
void Switch_Init(void){ 

}

//---------------------Switch_Play---------------------
// read the switches
// Input: none
// Output: false the Play switch not pressed
//         true the Play switch is pressed 
int Switch_Play(void){ 
   return 1;
}
//---------------------Switch_Voice---------------------
// read the switches
// Input: none
// Output: false the Voice switch not pressed
//         true the Voice switch is pressed 
int Switch_Voice(void){ 
   return 0;
}
