#ifndef DURATION_TIMER_H
#define DURATION_TIMER_H

// Assumes an 80 MHz system clock.
// Timer G8 must be available.
// task must be a valid function pointer.
void DurationTimer_Init(void (*task)(void));

#endif