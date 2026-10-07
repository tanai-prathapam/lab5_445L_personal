// File **********music.c***********
// Programs to play pre-programmed music and respond to switch
// inputs. MSPM0
// EE445L Fall 2026
//    Jonathan W. Valvano 6/28/26

// the 64 comes from the length of the sine wave table
// Bus cycle runs at 80MHz
// freq =80,000,000/64/Period = 1,250,000/Period

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Switch.h"
#include "../inc/MCP4921.h"
#include "../inc/Timer.h"
#include "music.h"
#include "mailbox.h"
#include "../lib/DurationTimer.h"

void SysTick_InitArm(void (*task)(void), uint32_t period, uint32_t priority);
 
#define NOTE_A4  2841U  // Approximately 440 Hz.
#define NOTE_C5  2390U  // Approximately 523 Hz.
#define REST        0U

//Song Definition
static const Note_t TestNotes[] = {
  {NOTE_A4, 3000}, //Format: {period, duration in ms}
  {REST,   2000},
  {NOTE_C5, 3000}
};

static const Song_t TestSong = {
  TestNotes,
  sizeof(TestNotes) / sizeof(TestNotes[0])
};

//Wave Definition
static const uint16_t SineWave[64] = {
  2048,2244,2438,2629,2813,2991,3159,3317,
  3462,3594,3711,3812,3896,3962,4010,4038,
  4048,4038,4010,3962,3896,3812,3711,3594,
  3462,3317,3159,2991,2813,2629,2438,2244,
  2048,1852,1658,1467,1283,1105,937,779,
  634,502,385,284,200,134,86,58,
  48,58,86,134,200,284,385,502,
  634,779,937,1105,1283,1467,1658,1852
};


static const uint16_t balls[//putlengthhere] = {
  //copy paste here
}

static const Instrument_t SineInstrument = {
  SineWave,
  sizeof(SineWave) / sizeof(SineWave[0])
};

//State Variables
typedef enum {
  STOPPED,
  PLAYING,
  PAUSED
} PlaybackState_t;

static volatile uint32_t PlaybackSpeed = 1;
static volatile PlaybackState_t PlaybackState = STOPPED; //start with STOPPED state
static const Song_t *CurrentSong = &TestSong;
static volatile uint32_t NoteIndex = 0;
static volatile uint32_t WaveIndex = 0;
static volatile uint32_t RemainingMs = 0;

//Sets one sample value for the DAC
static void Music_OutputSample(void){
  if(PlaybackState != PLAYING){
    return; //hold same voltage value (do not advance through wave or notes)
  }

  // A period of zero marks a rest.
  if(CurrentSong->notes[NoteIndex].period == REST){
    return; //hold same voltage value (do not advance through wave or notes)
  }

  MCP4921_OutNonBlocking( SineInstrument.samples[WaveIndex]); //set sample value
  WaveIndex++;

  //wrap around to repeat wave
  if(WaveIndex >= SineInstrument.length){
    WaveIndex = 0;
  }
}

static void Music_LoadNote(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  // Stop waveform interrupts while changing notes.
  SysTick->CTRL = 0;
  WaveIndex = 0;
  MCP4921_OutNonBlocking(2048); //midpoint start

  if(NoteIndex >= CurrentSong->length){ //end of song
    PlaybackState = STOPPED;
    RemainingMs = 0;
  }
  else {
    const Note_t *note = &CurrentSong->notes[NoteIndex];
    RemainingMs = note->duration_ms;
    if(note->period != REST){
      SysTick_InitArm(&Music_OutputSample, note->period, 0);
    }
  }

  __set_PRIMASK(previousMask);
}

// Call once every 1 millisecond.
// static void Music_Tick1ms(void){
//   uint32_t previousMask = __get_PRIMASK();
//   __disable_irq();

//   if(PlaybackState == PLAYING){
//     if(RemainingMs > 0){
//       RemainingMs--;

//       if(RemainingMs == 0){
//         NoteIndex++;
//         Music_LoadNote();
//       }
//     }
//   }

//   __set_PRIMASK(previousMask);
// }


static void Music_CheckButtons(void){
  uint32_t play = Get_Button_Press(BUTTON_PLAY);
  uint32_t rewind = Get_Button_Press(BUTTON_REWIND);
  uint32_t speed = Get_Button_Press(BUTTON_SPEED);

  if(rewind){
    Rewind();
  }
  else if(play){
      Play(CurrentSong);
  }

  if(speed){
    ToggleSpeed();
  }
}

static void Music_Tick1ms(void){
  Music_CheckButtons();
  
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  if(PlaybackState == PLAYING && RemainingMs > 0){
    if(RemainingMs <= PlaybackSpeed){ //prevent overflow
      RemainingMs = 0;
      NoteIndex++;
      Music_LoadNote();
    }else{
      RemainingMs -= PlaybackSpeed; //subtract either 1 or 2 to move through the note at normal speed or 2x the speed
    }
  }

  __set_PRIMASK(previousMask);
}

void Song_Init(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  SysTick->CTRL = 0; //prevent waveform gen

  PlaybackState = STOPPED;
  PlaybackSpeed = 1;
  CurrentSong = &TestSong;
  NoteIndex = 0;
  WaveIndex = 0;
  RemainingMs = 0;

  Switch_Init();
  MCP4921_Init(2048); //SPI init
  MCP4921_Out(2048); //Start at midpoint voltage
  DurationTimer_Init(&Music_Tick1ms); //1ms timer
  
  __set_PRIMASK(previousMask);
}

void Pause(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  if(PlaybackState == PLAYING){
    PlaybackState = PAUSED;
    SysTick->CTRL = 0; //prevent waveform generation
    MCP4921_OutNonBlocking(2048);
  }

  __set_PRIMASK(previousMask);
}

void Play(const Song_t *song){
  if(song == 0 || song->notes == 0 || song->length == 0){
    return;
  }

  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  //If first time playing song or after a rewind
  if(PlaybackState == STOPPED || CurrentSong != song){
    SysTick->CTRL = 0;
    CurrentSong = song;
    NoteIndex = 0;
    PlaybackState = PLAYING;
    Music_LoadNote();
  }
  
  //Resume at same position (do not load a new note); wave index should stay the same, note index should stay the same
  else if(PlaybackState == PAUSED) {
    PlaybackState = PLAYING;
    uint32_t period = CurrentSong->notes[NoteIndex].period;
    if(period != REST){
      SysTick_InitArm(&Music_OutputSample, period, 0);
    }
  }

  __set_PRIMASK(previousMask);
}

void Rewind (void) {
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();

  PlaybackState = STOPPED;
  SysTick->CTRL = 0;
  MCP4921_OutNonBlocking(2048);

  NoteIndex = 0;
  WaveIndex = 0;
  RemainingMs = 0;

  __set_PRIMASK(previousMask);
}

void ToggleSpeed(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackSpeed = (PlaybackSpeed == 1) ? 2 : 1; //toggle
  __set_PRIMASK(previousMask);
}

