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
  uint32_t period;       // Legacy pitch unit: 80 MHz / (64 * period); 0 = rest.
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
// Play starts fixed 32 kHz SysTick (melody/DAC) and TimerA1 (harmony).
// Pitches use independent fractional waveform positions; TimerG8 is 1 ms.
// Inputs: none
// Outputs: none
// called once
void Song_Init(void);

// Play a melody and its optional harmony; both share transport and tempo.
void Play(const Song_t *song);
void Pause(void);
void Rewind(void);
void ToggleSpeed(void);
// Hold/release Speed to toggle harmony with a smooth 40 ms mixer fade.
void ToggleHarmony(void);
// CCS watch: 1 = harmony requested, 0 = muted; preserved through rewind.
extern volatile uint32_t Music_HarmonyEnabled;

// Nonzero means the SPI transmit FIFO was full when an audio write was due.
// Inspect in the CCS watch window; reset by Song_Init().
extern volatile uint32_t Music_DACDroppedSamples;

// Number of samples queued by the audio path; reset by Song_Init().
// A rising count shows the writer is active, but does not prove DAC output.
extern volatile uint32_t Music_DACSamplesWritten;

// Listening-test label: 1 melody, 2 harmony, 3 unison, 4 octave;
// 0 during gaps, pause, stop, or regular song playback.
extern volatile uint32_t Music_DiagnosticStage;

#endif // MUSIC_H
