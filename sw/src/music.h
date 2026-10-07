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

typedef struct {
  const Note_t *notes;   // Address of the song's note array.
  uint32_t length;      // Number of entries in that array.
} Song_t;

typedef struct {
  const uint16_t *samples;  // Points to one cycle of waveform data.
  uint32_t length;         // Number of samples in that cycle.
} Instrument_t;

//-------------- Song_Init ----------------
// Initialize the melody voice, DAC, switches and TimerG8 score clock.
// SysTick starts when Play() loads a pitched note.
// Inputs: none
// Outputs: none
// called once
void Song_Init(void);

void Play(const Song_t *song);
void Pause(void);
void Rewind(void);
void ToggleSpeed(void);

#endif // MUSIC_H
