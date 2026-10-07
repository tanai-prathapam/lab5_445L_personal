// File **********music.c***********
// Programs to play pre-programmed music and respond to switch
// inputs. MSPM0
// EE445L Fall 2026
//    Jonathan W. Valvano 6/28/26

// Legacy note periods encode pitch: frequency = 80 MHz / (64 * period).
// Both waveform generators now run at a fixed 32 kHz. A fractional
// position in the sine table sets each pitch independently of that rate.

#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "Switch.h"
#include "../inc/MCP4921.h"
#include "../inc/Timer.h"
#include "music.h"
#include "mailbox.h"
#include "../lib/DurationTimer.h"

void SysTick_InitArm(void (*task)(void), uint32_t period, uint32_t priority);

// Pitch constants in the original 12.5ns bus-cycle/sample units.
// These are converted to phase increments, not used as timer reloads.
// period = 1,250,000 / frequency. Every value below lands within 0.03%
// of equal temperament with A4 = 440 Hz.
// Bass pitches use the same 80 MHz / 64-sample period units.
#define NOTE_G2 12755U  //  98.0 Hz
#define NOTE_D3  8513U  // 146.8 Hz
#define NOTE_FS3 6757U  // 185.0 Hz
#define NOTE_G3  6378U  // 196.0 Hz
#define NOTE_A3  5682U  // 220.0 Hz
#define NOTE_B3  5062U  // 246.9 Hz
#define NOTE_C4  4778U  // 261.6 Hz
#define NOTE_D4  4257U  // 293.7 Hz
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
// Fixed waveform/DAC cadence for every note, including rests.
#define MUSIC_SAMPLE_PERIOD 2500U // 80 MHz / 2500 = 32 kHz.
// Q16 table position: upper bits are the index, lower 16 are fractional.
#define PHASE_FRACTION_BITS 16U
#define PHASE_ONE (1U << PHASE_FRACTION_BITS)
// Startup choice: 0 = harmony muted, 1 = harmony audible.
// Hold Speed for at least 800 ms, then release, to toggle while playing.
#define MUSIC_ENABLE_HARMONY 1
// 1 = simpler octave below melody; 0 = the original score's bass line.
#define MUSIC_HARMONY_STYLE 1
// Bass level relative to melody: 128 = half; 64 = quarter; 256 = equal.
#define MUSIC_HARMONY_GAIN 128U
#define HARMONY_HOLD_MS 800U
#define HARMONY_TOGGLE_FADE_MS 40U
// 256 = current level. Try 128 to check amplifier/speaker headroom.
#define MUSIC_OUTPUT_GAIN 256U
// Temporary listening test: 1 selects four isolated tones; 0 restores Minuet.
// The test uses the normal voice/envelope/mixer code at quarter volume.
#define MUSIC_DIAGNOSTIC_MODE 0
#define MUSIC_DIAGNOSTIC_GAIN 64U

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

// Bass line from the first section of BWV Anh.114. The opening G-B-D
// chord is reduced to its bass G, keeping exactly two simultaneous voices.
// Source (public-domain typesetting):
// https://www.mutopiaproject.org/cgibin/piece-info.cgi?id=75
#define MINUET_HARMONY_A                                                   \
  /* bar  1 */ {NOTE_G3, HALF}, {NOTE_A3, QUARTER},                       \
  /* bar  2 */ {NOTE_B3, DOTHALF},                                       \
  /* bar  3 */ {NOTE_C4, DOTHALF},                                       \
  /* bar  4 */ {NOTE_B3, DOTHALF},                                       \
  /* bar  5 */ {NOTE_A3, DOTHALF},                                       \
  /* bar  6 */ {NOTE_G3, DOTHALF},                                       \
  /* bar  7 */ {NOTE_D4, QUARTER}, {NOTE_B3, QUARTER}, {NOTE_G3, QUARTER}, \
  /* bar  8 */ {NOTE_D4, QUARTER}, {NOTE_D3, EIGHTH}, {NOTE_C4, EIGHTH},   \
               {NOTE_B3, EIGHTH}, {NOTE_A3, EIGHTH},                     \
  /* bar  9 */ {NOTE_B3, HALF}, {NOTE_A3, QUARTER},                       \
  /* bar 10 */ {NOTE_G3, QUARTER}, {NOTE_B3, QUARTER}, {NOTE_G3, QUARTER}, \
  /* bar 11 */ {NOTE_C4, DOTHALF},                                       \
  /* bar 12 */ {NOTE_B3, QUARTER}, {NOTE_C4, EIGHTH}, {NOTE_B3, EIGHTH},   \
               {NOTE_A3, EIGHTH}, {NOTE_G3, EIGHTH},                     \
  /* bar 13 */ {NOTE_A3, HALF}, {NOTE_FS3, QUARTER},                      \
  /* bar 14 */ {NOTE_G3, HALF}, {NOTE_B3, QUARTER},                       \
  /* bar 15 */ {NOTE_C4, QUARTER}, {NOTE_D4, QUARTER}, {NOTE_D3, QUARTER}, \
  /* bar 16 */ {NOTE_G3, HALF}, {NOTE_G2, QUARTER}

static const Note_t MinuetHarmonyNotes[] = {
  MINUET_HARMONY_A,
  {REST, QUARTER},
  MINUET_HARMONY_A
};
static const Song_t MinuetHarmony = {
  MinuetHarmonyNotes,
  sizeof(MinuetHarmonyNotes) / sizeof(MinuetHarmonyNotes[0]),
  0 // No further voice attached to this bass track.
};
// Reuse the melody's timing. Music_LoadVoiceNote doubles its pitch period
// for this score, producing an octave below without duplicating the array.
static const Song_t MinuetOctaveHarmony = {
  MinuetNotes,
  sizeof(MinuetNotes) / sizeof(MinuetNotes[0]),
  0
};
static const Song_t MinuetSong = {
  MinuetNotes,
  sizeof(MinuetNotes) / sizeof(MinuetNotes[0]),
  MUSIC_HARMONY_STYLE ? &MinuetOctaveHarmony : &MinuetHarmony
};

// Four 4-second tones, each followed by 1 second of silence:
// 1 melody G4, 2 harmony G4, 3 both G4, 4 melody G4 + harmony G3.
// Keep the harmony attached in every stage for consistent mixer scaling.
static const Note_t DiagnosticMelodyNotes[] = {
  {NOTE_G4, 4000U}, {REST, 1000U},
  {REST,    4000U}, {REST, 1000U},
  {NOTE_G4, 4000U}, {REST, 1000U},
  {NOTE_G4, 4000U}, {REST, 1000U}
};
static const Note_t DiagnosticHarmonyNotes[] = {
  {REST,    4000U}, {REST, 1000U},
  {NOTE_G4, 4000U}, {REST, 1000U},
  {NOTE_G4, 4000U}, {REST, 1000U},
  {NOTE_G3, 4000U}, {REST, 1000U}
};
static const Song_t DiagnosticHarmony = {
  DiagnosticHarmonyNotes,
  sizeof(DiagnosticHarmonyNotes) / sizeof(DiagnosticHarmonyNotes[0]),
  0
};
static const Song_t DiagnosticSong = {
  DiagnosticMelodyNotes,
  sizeof(DiagnosticMelodyNotes) / sizeof(DiagnosticMelodyNotes[0]),
  &DiagnosticHarmony
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
  uint32_t noteIndex;
  uint32_t phase;            // Q16 position within one waveform cycle.
  uint32_t phaseStep;        // Pitch: table positions advanced per audio tick.
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
static void Music_ArmMelody(void){
  // Priority 1: the priority-0 harmony update finishes before the DAC mix.
  SysTick_InitArm(&Music_OutputSample, MUSIC_SAMPLE_PERIOD, 1);
}
static void Music_StopMelody(void){
  SysTick->CTRL = 0;
}

// TimerA1 is configured once by Song_Init. No timer output pins are used.
static uint32_t HarmonyTimerReady = 0;
static void Music_StopHarmony(void){
  if(!HarmonyTimerReady){ return; }
  TIMA1->COUNTERREGS.CTRCTL &= ~1U;
  TIMA1->CPU_INT.ICLR = 1U; // Clear peripheral zero event.
  NVIC->ICPR[0] = 1U << 19; // Clear the corresponding pending NVIC interrupt.
}
static void Music_ArmHarmony(void){
  Music_StopHarmony();
  TIMA1->COUNTERREGS.LOAD = MUSIC_SAMPLE_PERIOD - 1U;
  // CTRCTL.CVAE=0 from TimerA1_IntArm reloads the counter on enable.
  TIMA1->COUNTERREGS.CTRCTL |= 1U;
}
static void Music_InitHarmonyTimer(void){
  // TimerA1 clock = 80 MHz, prescale=1: same fixed rate as SysTick.
  // Priority 0 lets the bass sample finish before priority-1 DAC output.
  TimerA1_IntArm(65535U, 1U, 0U);
  HarmonyTimerReady = 1;
  Music_StopHarmony();
}

static volatile Voice_t Melody = {
  .score = &MinuetSong,
  .instrument = &SineInstrument,
  .envelope = &DefaultEnvelope
};
static volatile Voice_t Harmony = {
  .score = &MinuetHarmony,
  .instrument = &SineInstrument,
  .envelope = &DefaultEnvelope
};
static volatile uint32_t PlaybackSpeed = 1;
static volatile PlaybackState_t PlaybackState = STOPPED;
// Song_Init uses this selection too; change the default song here.
static const Song_t *CurrentSong =
    MUSIC_DIAGNOSTIC_MODE ? &DiagnosticSong : &MinuetSong;
// Set outside the sample ISR when selecting a song.
static volatile uint32_t Music_OutputGain = MUSIC_OUTPUT_GAIN;
// Mute only the contribution, keeping the harmony score/clock in time.
// This choice survives pause, rewind and replay; Song_Init resets it.
volatile uint32_t Music_HarmonyEnabled = MUSIC_ENABLE_HARMONY;
static uint32_t HarmonyFadePosition = HARMONY_TOGGLE_FADE_MS;
static uint32_t Music_HarmonyLevel = MUSIC_HARMONY_GAIN;
static volatile uint32_t Music_HarmonyMixGain = MUSIC_HARMONY_GAIN;

static void Music_UpdateHarmonyMixGain(void){
  Music_HarmonyMixGain =
      (Music_HarmonyLevel * HarmonyFadePosition) / HARMONY_TOGGLE_FADE_MS;
}
static void Music_SelectOutputLevels(void){
  Music_OutputGain = (CurrentSong == &DiagnosticSong) ?
      MUSIC_DIAGNOSTIC_GAIN : MUSIC_OUTPUT_GAIN;
  // Keep the diagnostic melody/harmony comparison at equal levels.
  Music_HarmonyLevel = (CurrentSong == &DiagnosticSong) ?
      GAIN_FULL : MUSIC_HARMONY_GAIN;
  Music_UpdateHarmonyMixGain();
}
static void Music_TickHarmonyFade(void){
  uint32_t target = Music_HarmonyEnabled ? HARMONY_TOGGLE_FADE_MS : 0U;
  if(HarmonyFadePosition < target){ HarmonyFadePosition++; }
  else if(HarmonyFadePosition > target){ HarmonyFadePosition--; }
  else{ return; }
  // Fade in real milliseconds, even at 2x tempo or while paused.
  Music_UpdateHarmonyMixGain();
}
// CCS watch label: 1..4 during the test tones; 0 in silence or when stopped/paused.
volatile uint32_t Music_DiagnosticStage = 0;
static void Music_UpdateDiagnosticStage(void){
  if(CurrentSong == &DiagnosticSong && PlaybackState == PLAYING &&
     Melody.noteIndex < DiagnosticSong.length && (Melody.noteIndex & 1U) == 0U){
    Music_DiagnosticStage = Melody.noteIndex / 2U + 1U;
  }
  else{
    Music_DiagnosticStage = 0;
  }
}

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

// A full SPI queue indicates lost audio samples. Inspect this in CCS.
// It is reset at Song_Init; the ISR never waits for FIFO space.
volatile uint32_t Music_DACDroppedSamples = 0;
// Counts samples actually queued to SPI, not measured analog output.
volatile uint32_t Music_DACSamplesWritten = 0;
static void Music_WriteDAC(uint32_t sample){
  if((SPI0->STAT & 0x02U) == 0U){
    Music_DACDroppedSamples++;
    return;
  }
  MCP4921_OutNonBlocking(sample);
  Music_DACSamplesWritten++;
}

// Only SysTick streams audio to the DAC. Both waveform clocks have the
// same fixed rate; priority-0 TimerA1 publishes bass before priority-1
// SysTick combines it with the current melody sample. Neither clock is
// restarted on note changes. Reserve half the amplitude per attached voice,
// including through rests, to avoid clipping or sudden volume changes.
static void Music_OutputMix(void){
  int32_t sample = DAC_MIDPOINT;
  if(PlaybackState == PLAYING){
    int32_t melody = Music_VoiceContribution(&Melody);
    if(Harmony.score != 0){
      int32_t harmony = (Music_VoiceContribution(&Harmony) *
          (int32_t)Music_HarmonyMixGain) / (int32_t)GAIN_FULL;
      sample += (melody + harmony) / 2;
    }
    else{
      sample += melody; // A song without harmony retains full amplitude.
    }
  }
  sample = DAC_MIDPOINT +
      ((sample - DAC_MIDPOINT) * (int32_t)Music_OutputGain) / (int32_t)GAIN_FULL;
  // Defensive bounds also protect the DAC configuration bits.
  if(sample < 0){ sample = 0; }
  else if(sample > 4095){ sample = 4095; }
  Music_WriteDAC((uint32_t)sample);
}

// Generate a sample at the voice's fractional table position. Linear
// interpolation avoids rounding every output sample to one of 64 entries.
// The audio ISR uses integer multiply/shifts; pitch division happens only
// when loading a note, at the 1 ms score rate.
static void Music_StepVoiceSample(volatile Voice_t *voice){
  if(!voice->sounding){ return; }
  const uint16_t *wave = voice->instrument->samples;
  uint32_t length = voice->instrument->length;
  uint32_t phase = voice->phase;
  uint32_t index = phase >> PHASE_FRACTION_BITS;
  uint32_t next = index + 1U;
  if(next == length){ next = 0; }
  int32_t fraction = (int32_t)(phase & (PHASE_ONE - 1U));
  int32_t first = (int32_t)wave[index];
  int32_t delta = (int32_t)wave[next] - first;
  voice->latestSample = first + (delta * fraction) / (int32_t)PHASE_ONE
      - DAC_MIDPOINT;
  phase += voice->phaseStep;
  uint32_t cycle = length << PHASE_FRACTION_BITS;
  if(phase >= cycle){ phase -= cycle; }
  voice->phase = phase;
}

static void Music_OutputSample(void){
  if(PlaybackState != PLAYING){ return; }
  Music_StepVoiceSample(&Melody);
  Music_OutputMix();
}

// TimerA1's zero-event acknowledgement is the same pattern as TimerG8.
void TIMA1_IRQHandler(void){
  if(TIMA1->CPU_INT.IIDX == 1U){
    if(PlaybackState == PLAYING && Harmony.sounding){
      Music_StepVoiceSample(&Harmony);
      // Both clocks are 32 kHz; SysTick outputs this sample after this ISR.
    }
  }
}

// Preserve position and envelope for pause. Rewind resets them separately.
static void Music_Silence(void){
  Music_StopMelody();
  Music_StopHarmony();
  Music_WriteDAC(DAC_MIDPOINT);
}

static void Music_ResetVoice(volatile Voice_t *voice, const Song_t *score){
  voice->score = score;
  voice->noteIndex = 0;
  voice->phase = 0;
  voice->phaseStep = 0;
  voice->remainingMs = 0;
  voice->elapsedMs = 0;
  voice->attackMs = 0;
  voice->releaseMs = 0;
  voice->gain = 0;
  voice->latestSample = 0;
  voice->sounding = 0;
}

static void Music_LoadVoiceNote(volatile Voice_t *voice){
  const Note_t *note = 0;
  uint32_t phaseStep = 0;
  if(voice->score != 0 && voice->noteIndex < voice->score->length){
    note = &voice->score->notes[voice->noteIndex];
    if(note->period != REST && note->duration_ms != 0){
      uint32_t period = note->period;
      if(voice->score == &MinuetOctaveHarmony){ period *= 2U; }
      // Increment = sample-clock period / legacy note period, in Q16.
      // All supported pitches are far below the 16 kHz Nyquist limit.
      phaseStep = ((MUSIC_SAMPLE_PERIOD << PHASE_FRACTION_BITS) +
          period / 2U) / period;
    }
  }
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  voice->phase = 0;
  voice->phaseStep = 0;
  voice->elapsedMs = 0;
  voice->remainingMs = 0;
  voice->latestSample = 0;
  voice->gain = 0;
  voice->sounding = 0;

  if(note != 0){
    voice->remainingMs = note->duration_ms;
    voice->phaseStep = phaseStep;
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
  }
  // Publish only voice state. Both sample timers keep their fixed cadence
  // through note changes, rests and an early end of either score.
  __set_PRIMASK(previousMask);
}

static void Music_TickVoice(volatile Voice_t *voice){
  if(voice->score == 0 || voice->noteIndex >= voice->score->length){ return; }
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
  if(speed){
    if(Get_Button_HoldMs(BUTTON_SPEED) >= HARMONY_HOLD_MS){ ToggleHarmony(); }
    else{ ToggleSpeed(); }
  }
}

static void Music_Tick1ms(void){
  Music_CheckButtons();
  Music_TickHarmonyFade();
  if(PlaybackState != PLAYING){ return; }
  // Waveform interrupts may preempt envelope calculations. They read only
  // the published gain and sample, not the in-progress note bookkeeping.
  Music_TickVoice(&Melody);
  Music_TickVoice(&Harmony);
  if(Melody.noteIndex >= Melody.score->length &&
     (Harmony.score == 0 || Harmony.noteIndex >= Harmony.score->length)){
    PlaybackState = STOPPED;
    Music_Silence();
  }
  Music_UpdateDiagnosticStage();
}

void Song_Init(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackState = STOPPED;
  PlaybackSpeed = 1;
  Music_DACDroppedSamples = 0;
  Music_DACSamplesWritten = 0;
  Music_DiagnosticStage = 0;
  // Diagnostics start with both paths audible, regardless of song default.
  Music_HarmonyEnabled = (CurrentSong == &DiagnosticSong) ?
      1U : MUSIC_ENABLE_HARMONY;
  HarmonyFadePosition = Music_HarmonyEnabled ? HARMONY_TOGGLE_FADE_MS : 0U;
  Music_SelectOutputLevels();
  Music_InitHarmonyTimer();
  Music_ResetVoice(&Melody, CurrentSong);
  Music_ResetVoice(&Harmony, CurrentSong->harmony);
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
    Music_UpdateDiagnosticStage();
  }
  __set_PRIMASK(previousMask);
}

void Play(const Song_t *song){
  if(song == 0 || song->notes == 0 || song->length == 0){ return; }
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  if(PlaybackState == STOPPED || CurrentSong != song){
    CurrentSong = song;
    Music_SelectOutputLevels();
    Music_StopMelody();
    Music_StopHarmony();
    Music_ResetVoice(&Melody, song);
    Music_ResetVoice(&Harmony, song->harmony);
    PlaybackState = PLAYING;
    Music_LoadVoiceNote(&Melody);
    Music_LoadVoiceNote(&Harmony);
    // Start bass first, then melody/DAC with the same period. This order,
    // plus the higher bass priority, gives one new bass sample per mix.
    if(Harmony.score != 0){ Music_ArmHarmony(); }
    Music_ArmMelody();
  }
  else if(PlaybackState == PAUSED){
    PlaybackState = PLAYING;
    if(Harmony.score != 0){ Music_ArmHarmony(); }
    Music_ArmMelody();
  }
  Music_UpdateDiagnosticStage();
  __set_PRIMASK(previousMask);
}

void Rewind(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackState = STOPPED;
  Music_ResetVoice(&Melody, CurrentSong);
  Music_ResetVoice(&Harmony, CurrentSong->harmony);
  Music_Silence();
  Music_UpdateDiagnosticStage();
  __set_PRIMASK(previousMask);
}

void ToggleSpeed(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  PlaybackSpeed = (PlaybackSpeed == 1) ? 2 : 1;
  __set_PRIMASK(previousMask);
}

void ToggleHarmony(void){
  uint32_t previousMask = __get_PRIMASK();
  __disable_irq();
  Music_HarmonyEnabled = !Music_HarmonyEnabled;
  // TimerG8 ramps the mixer gain over 40 ms; neither score is restarted.
  __set_PRIMASK(previousMask);
}
