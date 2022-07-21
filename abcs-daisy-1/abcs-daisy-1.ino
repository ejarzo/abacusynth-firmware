
#include <Arduino.h>
#include <MIDI.h>
#include <Wire.h>
#include "DaisyDuino.h"
#include "Adafruit_VL6180X.h"

#define TCAADDR 0x70
#define NUM_WAVEFORMS 4

#define BLOCK_SIZE 4

#define NUM_POLY_VOICES 3
#define NUM_SYNTHVOICES 4
#define NUM_RODS 4

#define NUM_LFO_TARGETS 2

#define USING_GAIN_POT true
#define USING_DISTANCE_SENSORS true

#define LONG_PRESS_THRESHOLD 700

#define PIN_BREAKBEAM_IN_1 15
#define PIN_BREAKBEAM_IN_2 16
#define PIN_BREAKBEAM_IN_3 17
#define PIN_BREAKBEAM_IN_4 18

#define PIN_POT_GAIN A4

#define TCA_IDX_1 0
#define TCA_IDX_2 1
#define TCA_IDX_3 3
#define TCA_IDX_4 2

#define PIN_ENC_1_A 3
#define PIN_ENC_1_B 4
#define PIN_ENC_1_BTN 5

#define PIN_ENC_2_A 6
#define PIN_ENC_2_B 7
#define PIN_ENC_2_BTN 8

#define PIN_ENC_3_A 25
#define PIN_ENC_3_B 26
#define PIN_ENC_3_BTN 27

#define PIN_ENC_4_A 0
#define PIN_ENC_4_B 1
#define PIN_ENC_4_BTN 2

#define OUTPUT_GAIN 0.4

#define PIN_LED_OUT_1 19
#define PIN_LED_OUT_2 20

#include "./voice-manager.h"
#include "./abcs-synth-voice2.h"
#include "./abcs-rod.h"

/* ======================================================== */

using namespace daisysp;

MIDI_CREATE_DEFAULT_INSTANCE();

uint8_t waveforms[NUM_WAVEFORMS] = {
    Oscillator::WAVE_SIN,
    Oscillator::WAVE_TRI,
    Oscillator::WAVE_POLYBLEP_SAW,
    Oscillator::WAVE_POLYBLEP_SQUARE,
    // Oscillator::WAVE_SQUARE,
};

DaisyHardware hw;
AnalogControl gainPot;
Adafruit_VL6180X vl1 = Adafruit_VL6180X();

AbcsRod abcsRods[NUM_RODS] = {
    AbcsRod(0, PIN_BREAKBEAM_IN_1, PIN_ENC_1_A, PIN_ENC_1_B, PIN_ENC_1_BTN),
    AbcsRod(1, PIN_BREAKBEAM_IN_2, PIN_ENC_2_A, PIN_ENC_2_B, PIN_ENC_2_BTN),
    AbcsRod(2, PIN_BREAKBEAM_IN_3, PIN_ENC_3_A, PIN_ENC_3_B, PIN_ENC_3_BTN),
    AbcsRod(3, PIN_BREAKBEAM_IN_4, PIN_ENC_4_A, PIN_ENC_4_B, PIN_ENC_4_BTN)};

AbcsSynthVoice2 synthVoices[NUM_SYNTHVOICES];

static VoiceManager<NUM_POLY_VOICES> voice_handler;

Voice *voices = voice_handler.GetVoices();

int loopCount = 0;
uint8_t range1, range2, range3, range4;

float gain = 1.f;
float prevGain = gain;
float newGain = 0.0;
Line gainLine;
uint8_t gainLineFinished;

float amps[NUM_POLY_VOICES];

/* ======================================================== */

void tcaselect(uint8_t i)
{
  if (i > 7)
    return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

/* ======================================================== */

void handleNoteOn(byte inChannel, byte inNote, byte inVelocity)
{
  Serial.print("NOTE ON: ");
  Serial.print(inNote);
  Serial.print(" ");
  Serial.println(inVelocity);

  /* Overflow due to MIDI not being read fast enough */
  if (inVelocity > 127)
  {
    return;
  }

  /* "Hack" to set ADSR based on MIDI channel */
  if (inChannel == 2)
  {
    voice_handler.setADSR(3.f, 2.f, 0.3f, 3.f);
  }
  else
  {
    voice_handler.setADSR(0.005f, 0.005f, 0.5f, 0.2f);
  }

  // Note Off can come in as Note On w/ 0 Velocity
  if (inVelocity > 0)
  {
    voice_handler.OnNoteOn(inNote, inVelocity);
  }
  else
  {
    voice_handler.OnNoteOff(inNote, inVelocity);
  }

  Serial.println("voices");
  for (size_t i = 0; i < NUM_POLY_VOICES; i++)
  {
    Voice *v = &voices[i];

    Serial.print(v->GetNote());
    Serial.print(" ");
    Serial.println(v->IsActive());

    for (size_t j = 0; j < NUM_SYNTHVOICES; j++)
    {
      synthVoices[j].SetFundamentalFreq(mtof(v->GetNote()), i);
    }
  }
}

void handleNoteOff(byte inChannel, byte inNote, byte inVelocity)
{
  Serial.println("note off");
  Serial.println(inNote);
  Serial.println(inVelocity);
  voice_handler.OnNoteOff(inNote, inVelocity);
}

/* ======================================================== */

void NextSamples(float &sig)
{
  float result = 0.0;

  /* Get amplitude envelopes from voices */
  for (size_t i = 0; i < NUM_POLY_VOICES; i++)
  {
    Voice *v = &voices[i];
    amps[i] = v->Process();
  }

  /* Pass amps to each rod */
  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    result += synthVoices[i].Process(amps);
  }

  sig = result / NUM_SYNTHVOICES;
}

void MyCallback(float **in, float **out, size_t size)
{
  MIDI.read();

  for (size_t i = 0; i < size; i++)
  {
    float sig;
    NextSamples(sig);
    sig = sig * gain;

    out[0][i] = sig;
    out[1][i] = sig;
  }
}

void setup()
{
  float sample_rate, callback_rate;
  // Serial.begin();
  delay(500);
  Serial.println("SETUP");

  Wire.begin();

  if (USING_DISTANCE_SENSORS)
  {
    for (size_t i = 0; i < 4; i++)
    {
      tcaselect(i);
      /* TODO: error if can't connect */
      vl1.begin();
    }
  }

  /* Init Daisy */
  // DAISY.SetAudioBlockSize(96);
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);

  DAISY.SetAudioBlockSize(BLOCK_SIZE);

  sample_rate = DAISY.get_samplerate();
  callback_rate = DAISY.get_callbackrate();

  /* Voices */
  voice_handler.Init(sample_rate, BLOCK_SIZE);

  /* For debugging */
  // pinMode(PIN_LED_OUT_1, OUTPUT);
  // pinMode(PIN_LED_OUT_2, OUTPUT);

  /* Global Gain */
  gainLine.Init(sample_rate);
  gainPot.Init(PIN_POT_GAIN, callback_rate, true);

  /* Init Rods */
  for (size_t i = 0; i < NUM_RODS; i++)
  {
    abcsRods[i].Init(callback_rate / BLOCK_SIZE);
  }

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].Init(sample_rate);
    synthVoices[i].SetOscWaveform(waveforms[4 - i]);
    synthVoices[i].SetHarmonic(i + 1);
  }

  /* MIDI Setup */
  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.turnThruOff();
  MIDI.begin(MIDI_CHANNEL_OMNI);

  DAISY.begin(MyCallback);
}

void loop()
{
  // return;
  for (size_t i = 0; i < NUM_RODS; i++)
  {
    /* Get spin speed */
    abcsRods[i].Process();

    float rotationSpeed = abcsRods[i].GetRotationSpeed();
    int harmonic = abcsRods[i].GetEncoderVal();
    int waveform = abcsRods[i].GetWaveformIndex();

    synthVoices[i].SetHarmonic(harmonic);
    synthVoices[i].SetLfoFreq(rotationSpeed);
    synthVoices[i].SetOscWaveform(waveforms[waveform]);

    /* Long press */
    if (abcsRods[i].GetLongPress())
    {
      synthVoices[i].IncrementLfoTarget();
    }
  }

  if (USING_GAIN_POT)
  {
    gainPot.Process();
    newGain = gainPot.Value();
    if (newGain < 0.06)
    {
      newGain = 0;
    }

    /* Smooth gain */
    if (newGain == 0 || abs(newGain - gain) > 0.05)
    {
      gain = prevGain * 0.8 + newGain * 0.2;
    }

    prevGain = gain;
  }

  /* range is pretty slow to read, only check every 100 cycles */
  if (USING_DISTANCE_SENSORS && loopCount >= 100)
  {
    tcaselect(TCA_IDX_1);
    range1 = vl1.readRange();
    tcaselect(TCA_IDX_2);
    range2 = vl1.readRange();
    tcaselect(TCA_IDX_3);
    range3 = vl1.readRange();
    tcaselect(TCA_IDX_4);
    range4 = vl1.readRange();

    synthVoices[0].SetRange(range1);
    synthVoices[1].SetRange(range2);
    synthVoices[2].SetRange(range3);
    synthVoices[3].SetRange(range4);

    loopCount = 0;
  }

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].Loop();
  }

  loopCount++;
}
