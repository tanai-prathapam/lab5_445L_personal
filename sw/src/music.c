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

// SysTick reload values, in 12.5ns bus cycles per waveform sample.
// period = 1,250,000 / frequency. Every value below lands within 0.03%
// of equal temperament with A4 = 440 Hz.
#define NOTE_FS4 3378U  //  370.0 Hz
#define NOTE_G4  3189U  //  392.0 Hz
#define NOTE_A4  2841U  //  440.0 Hz
#define NOTE_B4  2531U  //  493.9 Hz
#define NOTE_C5  2389U  //  523.2 Hz
#define NOTE_D5  2128U  //  587.3 Hz
#define NOTE_E5  1896U  //  659.3 Hz
#define NOTE_FS5 1689U  //  740.0 Hz
#define NOTE_G5  1594U  //  784.0 Hz
#define REST        0U

// Note durations in milliseconds at the written tempo of 120 quarter
// notes per minute. The piece is in 3/4, so one bar is three quarters.
#define EIGHTH    250U
#define QUARTER   500U
#define HALF     1000U
#define DOTHALF  1500U

// Envelope times are musical milliseconds: 2x tempo shortens both fades
// These times do not depend on the pitch or waveform interrupt frequency
#define ENVELOPE_ATTACK_MS  40U
#define ENVELOPE_RELEASE_MS 80U
#define GAIN_FULL          256U
#define DAC_MIDPOINT      2048

// Song Definition
// Minuet in G major
// Melody repeated twice
// 9 distinct pitches, 48 seconds 
#define MINUET_SECTION_A                                                   \
  /* bar  1 */ {NOTE_D5, QUARTER}, {NOTE_G4, EIGHTH}, {NOTE_A4, EIGHTH},   \
               {NOTE_B4, EIGHTH}, {NOTE_C5, EIGHTH},                       \
  /* bar  2 */ {NOTE_D5, QUARTER}, {NOTE_G4, QUARTER}, {NOTE_G4, QUARTER}, \
  /* bar  3 */ {NOTE_E5, QUARTER}, {NOTE_C5, EIGHTH}, {NOTE_D5, EIGHTH},   \
               {NOTE_E5, EIGHTH}, {NOTE_FS5, EIGHTH},                      \
  /* bar  4 */ {NOTE_G5, QUARTER}, {NOTE_G4, QUARTER}, {NOTE_G4, QUARTER}, \
  /* bar  5 */ {NOTE_C5, QUARTER}, {NOTE_D5, EIGHTH}, {NOTE_C5, EIGHTH},   \
               {NOTE_B4, EIGHTH}, {NOTE_A4, EIGHTH},                       \
  /* bar  6 */ {NOTE_B4, QUARTER}, {NOTE_C5, EIGHTH}, {NOTE_B4, EIGHTH},   \
               {NOTE_A4, EIGHTH}, {NOTE_G4, EIGHTH},                       \
  /* bar  7 */ {NOTE_FS4, QUARTER}, {NOTE_G4, EIGHTH}, {NOTE_A4, EIGHTH},  \
               {NOTE_B4, EIGHTH}, {NOTE_G4, EIGHTH},                       \
  /* bar  8 */ {NOTE_A4, DOTHALF},                                         \
  /* bar  9 */ {NOTE_D5, QUARTER}, {NOTE_G4, EIGHTH}, {NOTE_A4, EIGHTH},   \
               {NOTE_B4, EIGHTH}, {NOTE_C5, EIGHTH},                       \
  /* bar 10 */ {NOTE_D5, QUARTER}, {NOTE_G4, QUARTER}, {NOTE_G4, QUARTER}, \
  /* bar 11 */ {NOTE_E5, QUARTER}, {NOTE_C5, EIGHTH}, {NOTE_D5, EIGHTH},   \
               {NOTE_E5, EIGHTH}, {NOTE_FS5, EIGHTH},                      \
  /* bar 12 */ {NOTE_G5, QUARTER}, {NOTE_G4, QUARTER}, {NOTE_G4, QUARTER}, \
  /* bar 13 */ {NOTE_C5, QUARTER}, {NOTE_D5, EIGHTH}, {NOTE_C5, EIGHTH},   \
               {NOTE_B4, EIGHTH}, {NOTE_A4, EIGHTH},                       \
  /* bar 14 */ {NOTE_B4, QUARTER}, {NOTE_C5, EIGHTH}, {NOTE_B4, EIGHTH},   \
               {NOTE_A4, EIGHTH}, {NOTE_G4, EIGHTH},                       \
  /* bar 15 */ {NOTE_A4, QUARTER}, {NOTE_B4, EIGHTH}, {NOTE_A4, EIGHTH},   \
               {NOTE_G4, EIGHTH}, {NOTE_FS4, EIGHTH},                      \
  /* bar 16 */ {NOTE_G4, DOTHALF}

static const Note_t MinuetNotes[] = {
  MINUET_SECTION_A,     // Format: {period, duration in ms}
  {REST, QUARTER},      // breath before the written repeat
  MINUET_SECTION_A
};

static const Song_t MinuetSong = {
  MinuetNotes,
  sizeof(MinuetNotes) / sizeof(MinuetNotes[0])
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

static const Instrument_t SineInstrument = {
  SineWave,
  sizeof(SineWave) / sizeof(SineWave[0])
};

// Each voice owns its score position, waveform position and envelope.
typedef struct {
  uint32_t attack_ms;
  uint32_t release_ms;
} Envelope_t;

typedef struct {
  const Song_t *score;
  const Instrument_t *instrument;
  const Envelope_t *envelope;
  void (*arm)(uint32_t period);
  void (*stop)(void);
  uint32_t noteIndex;
  uint32_t waveIndex;
  uint32_t remainingMs;
  uint32_t elapsedMs;
  uint32_t attackMs;
  uint32_t releaseMs;
  uint32_t gain;             // 0 = silence, 256 = full amplitude.
  int32_t latestSample;      // Waveform sample centered around zero.
  uint32_t sounding;         // False during rests and after the score ends.
} Voice_t;

typedef enum { STOPPED, PLAYING, PAUSED } PlaybackState_t;

static const Envelope_t DefaultEnvelope = {
  ENVELOPE_ATTACK_MS, ENVELOPE_RELEASE_MS
};

// Smoothstep curve, 3*x*x - 2*x*x*x, sampled from x=0 to x=1.
// Integer lookup gives gentle starts/ends without floating point in an ISR.
static const uint16_t EnvelopeCurve[33] = {
  0,1,3,6,11,17,24,31,40,49,59,70,81,92,104,116,128,
  140,152,164,175,186,197,207,216,225,232,239,245,250,253,255,256
};

static void Music_OutputSample(void);
static void Music_ArmMelody(uint32_t period){
  SysTick_InitArm(&Music_OutputSample, period, 0);
}
static void Music_StopMelody(void){
  SysTick->CTRL = 0;
}

static volatile Voice_t Melody = {
  .score = &MinuetSong,
  .instrument = &SineInstrument,
  .envelope = &DefaultEnvelope,
  .arm = Music_ArmMelody,
  .stop = Music_StopMelody
};
static volatile uint32_t PlaybackSpeed = 1;
static volatile PlaybackState_t PlaybackState = STOPPED;
// Song_Init uses this selection too; change the default song here.
static const Song_t *CurrentSong = &MinuetSong;

// Called by the 1 ms score clock (independent from pitch)
//choose current gain
static void Music_UpdateEnvelope(volatile Voice_t *voice){
  uint32_t gain = GAIN_FULL;
  if(!voice->sounding || voice->remainingMs == 0){
    gain = 0;
  }
  else if(voice->attackMs != 0 && voice->elapsedMs < voice->attackMs){
    uint32_t index = (voice->elapsedMs * 32U) / voice->attackMs;
    gain = EnvelopeCurve[index];
  }
  else if(voice->releaseMs != 0 && voice->remainingMs <= voice->releaseMs){
    uint32_t index = (voice->remainingMs * 32U) / voice->releaseMs;
    gain = EnvelopeCurve[index];
  }
  voice->gain = gain;
}

// SCALE -  gain=0 always means the DAC midpoint.
static int32_t Music_VoiceContribution(const volatile Voice_t *voice){
  return (voice->latestSample * (int32_t)voice->gain) / (int32_t)GAIN_FULL;
}

// All waveform callbacks use this output point. When harmony is added,
// add its contribution and divide the sum by two to reserve DAC headroom.\
//ADD MIDPOINT BACK
static void Music_OutputMix(void){
  int32_t sample = DAC_MIDPOINT;
  if(PlaybackState == PLAYING){
    sample += Music_VoiceContribution(&Melody);
  }
  // Defensive bounds also protect the DAC configuration bits.
  if(sample < 0){ sample = 0; }
  else if(sample > 4095){ sample = 4095; }
  MCP4921_OutNonBlocking((uint32_t)sample);
}

//UNCENTER - sine sample
static void Music_StepVoiceSample(volatile Voice_t *voice){
  if(!voice->sounding){ return; }
  voice->latestSample =
      (int32_t)voice->instrument->samples[voice->waveIndex] - DAC_MIDPOINT;
  voice->waveIndex++;
  if(voice->waveIndex >= voice->instrument->length){
    voice->waveIndex = 0;
  }
}

static void Music_OutputSample(void){
  if(PlaybackState != PLAYING){ return; }
  Music_StepVoiceSample(&Melody);
  Music_OutputMix();
}

// Preserve position and envelope for pause. Rewind resets them separately.
static void Music_Silence(void){
  Melody.stop();
  MCP4921_OutNonBlocking(DAC_MIDPOINT);
}

static void Music_ResetVoice(volatile Voice_t *voice, const Song_t *score){
  voice->stop();
  voice->score = score;
  voice->noteIndex = 0;
  voice->waveIndex = 0;
  voice->remainingMs = 0;
  voice->elapsedMs = 0;
  voice->attackMs = 0;
  voice->releaseMs = 0;
  voice->gain = 0;
  voice->latestSample = 0;
  voice->sounding = 0;
}

static void Music_LoadVoiceNote(volatile Voice_t *voice){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  voice->stop();
  voice->waveIndex = 0;
  voice->elapsedMs = 0;
  voice->remainingMs = 0;
  voice->latestSample = 0;
  voice->gain = 0;
  voice->sounding = 0;

  if(voice->noteIndex < voice->score->length){
    const Note_t *note = &voice->score->notes[voice->noteIndex];
    voice->remainingMs = note->duration_ms;
    voice->sounding = (note->period != REST && note->duration_ms != 0);

    //set original range
    // Short notes still have non-overlapping attack and release windows.
    voice->attackMs = voice->envelope->attack_ms;
    if(voice->attackMs > note->duration_ms / 2U){
      voice->attackMs = note->duration_ms / 2U;
    }
    voice->releaseMs = voice->envelope->release_ms;
    if(voice->releaseMs > note->duration_ms - voice->attackMs){
      voice->releaseMs = note->duration_ms - voice->attackMs;
    }
    Music_UpdateEnvelope(voice);
    if(voice->sounding){ voice->arm(note->period); }
  }
  // Clearing one voice must not silence the other when harmony is added.
  Music_OutputMix();
  __set_PRIMASK(previousMask);
}

static void Music_TickVoice(volatile Voice_t *voice){
  if(voice->noteIndex >= voice->score->length){ return; }
  uint32_t step = PlaybackSpeed;
  if(voice->remainingMs <= step){
    voice->gain = 0;
    voice->remainingMs = 0;
    voice->noteIndex++;
    Music_LoadVoiceNote(voice);
  }
  else{
    voice->remainingMs -= step;
    voice->elapsedMs += step;
    Music_UpdateEnvelope(voice);
  }
}

static void Music_CheckButtons(void){
  uint32_t play = Get_Button_Press(BUTTON_PLAY);
  uint32_t rewind = Get_Button_Press(BUTTON_REWIND);
  uint32_t speed = Get_Button_Press(BUTTON_SPEED);
  if(rewind){ Rewind(); }
  else if(play){
    if(PlaybackState == PLAYING){ Pause(); }
    else{ Play(CurrentSong); }
  }
  if(speed){ ToggleSpeed(); }
}

static void Music_Tick1ms(void){
  Music_CheckButtons();
  if(PlaybackState != PLAYING){ return; }
  // Waveform interrupts may preempt envelope calculations. They read only
  // the published gain and sample, not the in-progress note bookkeeping.
  Music_TickVoice(&Melody);
  // Add Music_TickVoice(&Harmony) here later; stop after BOTH scores finish.
  if(Melody.noteIndex >= Melody.score->length){
    PlaybackState = STOPPED;
    Music_Silence();
  }
}

void Song_Init(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackState = STOPPED;
  PlaybackSpeed = 1;
  Music_ResetVoice(&Melody, CurrentSong);
  Switch_Init();
  MCP4921_Init(DAC_MIDPOINT);
  MCP4921_Out(DAC_MIDPOINT);
  DurationTimer_Init(&Music_Tick1ms);
  __set_PRIMASK(previousMask);
}

void Pause(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  if(PlaybackState == PLAYING){
    PlaybackState = PAUSED;
    Music_Silence();
  }
  __set_PRIMASK(previousMask);
}

void Play(const Song_t *song){
  if(song == 0 || song->notes == 0 || song->length == 0){ return; }
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  if(PlaybackState == STOPPED || CurrentSong != song){
    CurrentSong = song;
    Music_ResetVoice(&Melody, song);
    PlaybackState = PLAYING;
    Music_LoadVoiceNote(&Melody);
  }
  else if(PlaybackState == PAUSED){
    PlaybackState = PLAYING;
    if(Melody.sounding){
      Melody.arm(Melody.score->notes[Melody.noteIndex].period);
    }
  }
  __set_PRIMASK(previousMask);
}

void Rewind(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackState = STOPPED;
  Music_ResetVoice(&Melody, CurrentSong);
  Music_Silence();
  __set_PRIMASK(previousMask);
}

void ToggleSpeed(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackSpeed = (PlaybackSpeed == 1) ? 2 : 1;
  __set_PRIMASK(previousMask);
}
