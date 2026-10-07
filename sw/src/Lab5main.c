/* Lab5main.c
 * This is a C language project that implements ECE445L Lab5.
 * Jonathan Valvano
 * June 29, 2026
 */
// Possible hardware interface
// 2-bit input, positive logic switches, positive logic software
//   bit1 PA28 Voice switch 
//   bit0 PA27 Play switch 

// MCP4921
// pin signal MSPM0   mode
//  1   Vdd   +3.3V
//  2   /CS   PA8    SPI0_CS0
//  3   SCLK  PB18   SPII_SCK
//  4   SDI   PB17   SPI_PICO
//  5   /LDAC ground
//  6   REF          1.255 V refernce
//  7   AGND         ground
//  8   OUT          DAC output


#include <ti/devices/msp/msp.h>
#include "../inc/LaunchPad.h"
#include "../inc/Clock.h"
#include "../inc/MCP4921.h"
#include "../inc/ST7735_SDC.h"
#include "Switch.h"
#include "music.h"
#include "mailbox.h"

void (*PeriodicTask)(void);   // user function to be called periodically

// ------------------    SysTick_Init     ------------------------------------
// Initialize Systick periodic interrupts
// Input: user task to execute, void-void function
//        interrupt period
//            Units of period are 12.5ns
//            Maximum is 2^24-1
//            Minimum is determined by length of ISR
// Output: none
void SysTick_InitArm(void(*task)(void), uint32_t period, uint32_t priority){
  // ****ECE445L write this ****
  SysTick->CTRL = 0x00;      // disable SysTick during setup
  PeriodicTask = task; 
  SysTick->LOAD = period-1;  // reload value
  SCB->SHP[1] = (SCB->SHP[1]&(~0xC0000000))|(priority<<30); // supplied priority
  SysTick->VAL = 0;          // any write to VAL clears COUNT and sets VAL equal to LOAD
  SysTick->CTRL = 0x07;      // enable SysTick with 80 MHz bus clock and interrupts
}

int main0(void){ // main0 is used to test the DAC interface
  uint32_t n=0;
  __disable_irq(); 
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator

  ST7735_InitR(INITR_REDTAB); //INITR_REDTAB for AdaFruit, INITR_BLACKTAB for SPI HiLetgo ST7735R
  ST7735_FillScreen(ST7735_BLACK);
  ST7735_SetCursor(0, 0);  
  ST7735_OutString("ECE445L main0\n");
  ST7735_OutString("Ramp output to DAC\n");
  TIMG0->COUNTERREGS.CTRCTL &= ~0x01; // disarm if not using SDC
  MCP4921_Init(0);
  while(1){
    MCP4921_OutNonBlocking(n);
    n = (n+1)&0x0FFF; // 0 to 4095
    Clock_Delay(1000);
  }
}
// *************main1 creates one continuous sin wave
// 12-bit 64-element sine wave
const uint16_t Wave[64] = {
  2048,2244,2438,2629,2813,2991,3159,3317,3462,3594,3711,
  3812,3896,3962,4010,4038,4048,4038,4010,3962,3896,3812,
  3711,3594,3462,3317,3159,2991,2813,2629,2438,2244,2048,
  1852,1658,1467,1283,1105,937,779,634,502,385,284,
  200,134,86,58,48,58,86,134,200,284,385,
  502,634,779,937,1105,1283,1467,1658,1852
}; 
uint32_t Index64;
void OutputSineWave(void){
  MCP4921_OutNonBlocking(Wave[Index64]);
  Index64 = (Index64+1)&0x3F; // 0 to 63
}
int main1(void){ // main1 output sine wave to DAC
  __disable_irq(); 

  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);  // 0.005% accurate running off external crystal oscillator

  // ST7735_InitR(INITR_REDTAB); //INITR_REDTAB for AdaFruit, INITR_BLACKTAB for SPI HiLetgo ST7735R
  // ST7735_FillScreen(ST7735_BLACK);
  // ST7735_SetCursor(0, 0);  
  // ST7735_OutString("ECE445L main1\n");
  // ST7735_OutString("Sine wave out to DAC\n");
  TIMG0->COUNTERREGS.CTRCTL &= ~0x01; // disarm G0 if not using SDC
  MCP4921_Init(2048);
  Index64 = 0;
  SysTick_InitArm(&OutputSineWave,80000/64,0);
  // 64 kHz interrupt, generating 1kHz sine wave
  __enable_irq();
  while(1){
    __WFI();
  }
}



//--------------------------------------------BUTTON TEST--------------------------------
#include "../lib/DurationTimer.h"
volatile uint32_t PlayRaw = 0;
volatile uint32_t RewindRaw = 0;
volatile uint32_t SpeedRaw = 0;
volatile uint32_t PlayEvents = 0;
volatile uint32_t RewindEvents = 0;
volatile uint32_t SpeedEvents = 0;

static void ButtonTest_Tick(void){
  PlayRaw = Switch_Play();
  RewindRaw = Switch_Rewind();
  SpeedRaw = Switch_Voice();

  if(Get_Button_Press(BUTTON_PLAY)){
    PlayEvents++;
  }

  if(Get_Button_Press(BUTTON_REWIND)){
    RewindEvents++;
  }

  if(Get_Button_Press(BUTTON_SPEED)){
    SpeedEvents++;
  }
}

int main2(void){
  __disable_irq();

  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);
  Switch_Init();

  DurationTimer_Init(&ButtonTest_Tick);

  __enable_irq();

  while(1){
    __WFI();
  }
}
int main(void){ 
  __disable_irq(); 

  //comment out for button test
  LaunchPad_Init();
  Clock_Init_HFXT_40_80MHz(0);
  Song_Init(); //Initializes SPI, switches, and 1 ms timer

 __enable_irq();

  while(1){
    //return main2(); //button test
    // return main0();
    // return main1();
    __WFI();
  } 
} 

// -------------------      SysTick_Handler    -------------------------------
// Interrupt service routine
// Executed every 12.5ns*(period)

void SysTick_Handler(void){
// ****ECE445L write this ****
  PeriodicTask();
}



// Executed every 1 ms, armed by ST7735_InitR
// this should not trigger because Lab 5 should disarm G0 timer
// remove this ISR if using the SDC
void TIMG0_IRQHandler(void){
  if((TIMG0->CPU_INT.IIDX) == 1){ // this will acknowledge
// the SDC interface will use this
  }
}