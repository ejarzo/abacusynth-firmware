
#include <Arduino.h>
#include <MIDI.h>
#include <Wire.h>
#include "DaisyDuino.h"
#include "Adafruit_VL6180X.h"

#define TCAADDR 0x70
#define NUM_WAVEFORMS 4
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

static AdEnv ad;
static int waveform;

int noteVal;
float attack, release;
int loopCount = 0;
uint8_t range1, range2, range3, range4;

float gain = 1.f;

float pressTime = 0.0;

float f1 = 0;
float f2 = 0;
float f3 = 0;
float f4 = 0;
int vlIndex = 0;

float prevGain = gain;
float newGain = 0.0;

Line gainLine;
uint8_t gainLineFinished;

/* ======================================================== */

void Controls();

void tcaselect(uint8_t i)
{
  if (i > 7)
    return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

int harmonicTest = 1;

void handleNoteOn(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity)
{
  // harmonicTest++;
  // synthVoices[0].SetHarmonic(harmonicTest);
  // return;
  Serial.println("NOTE ON");
  noteVal = inNote;
  float fq = mtof(noteVal);

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].SetFundamentalFreq(fq);
  }
}

void handleNoteOff(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity)
{
  Serial.print("MIDI OFF");
  Serial.print(" ");
  Serial.print(inChannel);
  Serial.print(" ");
  Serial.print(inNote);
  Serial.print(" ");
  Serial.print(inVelocity);
  Serial.println();
  // if (inVelocity > 0)
  // {
  //   noteVal = inNote;
  // }
  // voice_handler.OnNoteOff(inNote, inVelocity);
}

void NextSamples(float &sig)
{
  float result = 0.0;
  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    float oscSig = synthVoices[i].Process() / float(NUM_SYNTHVOICES);
    result += oscSig;
  }

  /* Envelope */
  // sig *= ad_out;

  sig = result;
}

void MyCallback(float **in, float **out, size_t size)
{
  MIDI.read();

  Controls();

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
    // Initialize distance sensors
    tcaselect(0);
    if (!vl1.begin())
    {
      Serial.println("Could not find vl1 distance sensor");
    }
    delay(10);
    tcaselect(1);
    if (!vl1.begin())
    {
      Serial.println("Could not find vl1 distance sensor");
    }
    delay(10);
    tcaselect(2);
    if (!vl1.begin())
    {
      Serial.println("Could not find vl1 distance sensor");
    }
    delay(10);
    tcaselect(3);
    if (!vl1.begin())
    {
      Serial.println("Could not find vl1 distance sensor");
    }
    delay(10);
  }

  /* Init Daisy */
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();
  callback_rate = DAISY.get_callbackrate();

  /* For debugging */
  // pinMode(PIN_LED_OUT_1, OUTPUT);
  // pinMode(PIN_LED_OUT_2, OUTPUT);

  /* Global Gain */
  gainLine.Init(sample_rate);
  gainPot.Init(PIN_POT_GAIN, callback_rate, true);

  /* Init Rods */
  for (size_t i = 0; i < NUM_RODS; i++)
  {
    abcsRods[i].Init(callback_rate);
  }

  noteVal = 40;
  attack = .01f;
  release = .2f;

  waveform = waveforms[0];

  float fq = mtof(noteVal);

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].Init(sample_rate);
    synthVoices[i].SetOscWaveform(waveforms[4 - i]);
    synthVoices[i].SetFundamentalFreq(fq);
    synthVoices[i].SetHarmonic(i + 1);
  }

  // Init everything
  ad.Init(sample_rate);

  // Set envelope parameters
  ad.SetTime(ADENV_SEG_ATTACK, 0.01);
  ad.SetTime(ADENV_SEG_DECAY, .2);
  ad.SetMax(1);
  ad.SetMin(0);
  ad.SetCurve(0.5);

  // start the audio callback

  MIDI.setHandleNoteOn(handleNoteOn);
  MIDI.setHandleNoteOff(handleNoteOff);
  MIDI.turnThruOff();
  MIDI.begin(MIDI_CHANNEL_OMNI); // Listen to all incoming messages

  DAISY.begin(MyCallback);
}

void loop()
{
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

void Controls()
{
  for (size_t i = 0; i < NUM_RODS; i++)
  {
    /* Get spin speed */
    abcsRods[i].Process();

    /* TODO: move to loop? */
    float rotationSpeed = abcsRods[i].GetRotationSpeed();
    int harmonic = abcsRods[i].GetEncoderVal();
    int waveform = abcsRods[i].GetWaveformIndex();

    synthVoices[i].SetHarmonic(harmonic);
    synthVoices[i].SetLfoFreq(rotationSpeed);
    synthVoices[i].SetOscWaveform(waveforms[waveform]);
    // synthVoices[i].SetOscWaveform(2 - i);
    if (abcsRods[i].GetLongPress())
    {
      synthVoices[i].IncrementLfoTarget();
    }
  }

  /* LEDs */
  // digitalWrite(PIN_LED_OUT_1, abcsRods[0].GetPulse());
  // digitalWrite(PIN_LED_OUT_2, abcsRods[1].GetPulse());
}
