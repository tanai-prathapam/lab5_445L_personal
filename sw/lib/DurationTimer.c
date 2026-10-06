#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/Timer.h"
#include "DurationTimer.h"

static void (*TickTask)(void);

void DurationTimer_Init(void (*task)(void)){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  TickTask = task;

  // Timer G8 runs at 40 MHz with an 80 MHz system clock
  TimerG8_IntArm(1000, 40, 2);

  __set_PRIMASK(previousMask);
}

void TIMG8_IRQHandler(void){
  // Read IIDX to acknowledge interrupt
  if(TIMG8->CPU_INT.IIDX == 1){
    TickTask();
  }
}