// File **********music.h***********
// Lab 5
// Programs to play pre-programmed music and respond to switch
// inputs.
// EE445L Fall 2026
//    Jonathan W. Valvano 6/29/26

#ifndef MUSIC_H
#define MUSIC_H

#include <stdint.h>

typedef struct {
  uint32_t period;       // SysTick clock cycles per sample; 0 = rest.
  uint32_t duration_ms;  // How long this note or rest lasts.
} Note_t;

typedef struct Song {
  const Note_t *notes;   // Address of this voice's note array.
  uint32_t length;      // Number of entries in that array.
  const struct Song *harmony; // Optional bass score; 0 for melody only.
} Song_t;

typedef struct {
  const uint16_t *samples;  // Points to one cycle of waveform data.
  uint32_t length;         // Number of samples in that cycle.
} Instrument_t;

//-------------- Song_Init ----------------
// Initialize both voices, DAC, switches, TimerA1 and TimerG8 score clock.
// Play starts SysTick (melody) and TimerA1 (harmony) for pitched notes.
// Inputs: none
// Outputs: none
// called once
void Song_Init(void);

// Play a melody and its optional harmony; both share transport and tempo.
void Play(const Song_t *song);
void Pause(void);
void Rewind(void);
void ToggleSpeed(void);

// Nonzero means the SPI transmit FIFO was full when an audio write was due.
// Inspect in the CCS watch window; reset by Song_Init().
extern volatile uint32_t Music_DACDroppedSamples;

#endif // MUSIC_H
