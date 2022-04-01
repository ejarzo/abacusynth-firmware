
/*
  "forcedInclude": [
      "/Users/eliasjarzombek/Library/Arduino15/packages/STMicroelectronics/hardware/stm32/2.2.0/cores/arduino/Arduino.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/Adafruit_VL6180X/Adafruit_VL6180X.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/DaisyDuino/src/utility/DaisySP/daisysp.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/DaisyDuino/src/utility/DaisySP/modules/moogladder.h"
  ],
 */

#include <Arduino.h>
#include <MIDI.h>
#include <Wire.h>
#include "DaisyDuino.h"
#include "Adafruit_VL6180X.h"

#define TCAADDR 0x70
#define NUM_WAVEFORMS 4
#define NUM_SYNTHVOICES 2
#define NUM_RODS 2
#define NUM_LFO_TARGETS 2

#define USING_DISTANCE_SENSORS true

#define LONG_PRESS_THRESHOLD 800

#define PIN_BREAKBEAM_IN_1 15
#define PIN_BREAKBEAM_IN_2 16

#define PIN_LED_OUT_1 17
#define PIN_LED_OUT_2 18

#include "./abcs-synth-voice2.h"
#include "./abcs-rod.h"

using namespace daisysp;

MIDI_CREATE_DEFAULT_INSTANCE();

DaisyHardware hw;

uint8_t waveforms[NUM_WAVEFORMS] = {
    Oscillator::WAVE_SIN,
    Oscillator::WAVE_TRI,
    Oscillator::WAVE_POLYBLEP_SAW,
    Oscillator::WAVE_POLYBLEP_SQUARE,
};

static AdEnv ad;
static int waveform;

int noteVal, wave, oscLfoTarget;
float oscFreq, cutoff, lfoFreqCtrl, lfoDepthCtrl;
float attack, release;
float oldk1, oldk2, oldk3, k1, k2, k3;

int activeVoice = 0;

Adafruit_VL6180X vl1 = Adafruit_VL6180X();

AbcsSynthVoice2 synthVoices[NUM_SYNTHVOICES];

AbcsRod abcsRods[NUM_RODS] = {
    AbcsRod(0, PIN_BREAKBEAM_IN_1, 0, 1, 2),
    AbcsRod(1, PIN_BREAKBEAM_IN_2, 3, 4, 5)};

float rotationSpeed;
int pulse;

int count = 0;

float filterVal1 = 15000.0;
float filterVal2 = 15000.0;

float filterValOld = filterVal1;
float filterValOld2 = filterVal2;

float filterVal = filterValOld;
uint8_t range1;
uint8_t range2;

int harmonicVal1 = 1;

void Controls();

void tcaselect(uint8_t i)
{
  if (i > 7)
    return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

void handleNoteOn(uint8_t inChannel, uint8_t inNote, uint8_t inVelocity)
{
  Serial.println("NOTE ON");
  noteVal = inNote;
  float fq = mtof(noteVal);

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].SetOscFreq(fq);
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

  sig = result;

  /* Envelope */
  // sig *= ad_out;
}

void MyCallback(float **in, float **out, size_t size)
{
  Controls();

  for (size_t i = 0; i < size; i++)
  {
    float sig;
    NextSamples(sig);

    out[0][i] = sig;
    out[1][i] = sig;
  }
}

void setup()
{
  float sample_rate, callback_rate;
  delay(300);

  Wire.begin();

  if (USING_DISTANCE_SENSORS)
  {
    // Initialize distance sensors
    tcaselect(0);
    if (!vl1.begin())
    {
      Serial.println("Could not find vl1 distance sensor");
    }
    tcaselect(1);
    if (!vl1.begin())
    {
      Serial.println("Could not find vl1 distance sensor");
    }
  }

  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();
  callback_rate = DAISY.get_callbackrate();

  hw.numControls = 0;

  pinMode(PIN_LED_OUT_1, OUTPUT);
  pinMode(PIN_LED_OUT_2, OUTPUT);

  // hw.controls[0].Init(A0, callback_rate);
  // hw.controls[1].Init(A1, callback_rate);
  // hw.controls[2].Init(A2, callback_rate);
  // hw.controls[3].Init(A3, callback_rate);

  for (size_t i = 0; i < NUM_RODS; i++)
  {
    abcsRods[i].Init(callback_rate);
  }

  noteVal = 40;
  attack = .01f;
  release = .2f;
  lfoFreqCtrl = 0.0f;
  lfoDepthCtrl = 0.0f;
  oscLfoTarget = 0;

  waveform = waveforms[0];

  float fq = mtof(noteVal);

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].Init(sample_rate);
    synthVoices[i].SetOscWaveform(waveform);
    synthVoices[i].SetOscFreq(fq);
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

float rangeToFilterFreq(int range)
{
  float exp = range * 11 / 128.0 + 5;
  exp = constrain(exp, 5, 14);
  return pow(2, exp);
}

float pressTime = 0.0;

void loop()
{
  // Serial.println("loop");
  MIDI.read();

  // for (size_t i = 0; i < 4; i++)
  // {
  //   Serial.print(hw.controls[i].Value());
  //   Serial.print(" ");
  // }
  // // for (size_t i = 0; i < 4; i++)
  // // {
  // //   Serial.print(hw.AnalogReadToVolts(A0 + i));
  // //   Serial.print(" ");
  // // }
  // Serial.println();

  /* range is pretty slow to read, only check every 100 cycles */
  if (USING_DISTANCE_SENSORS && count >= 80)
  {
    tcaselect(1);
    range1 = vl1.readRange();
    filterVal1 = rangeToFilterFreq(range1);
    tcaselect(0);
    range2 = vl1.readRange();
    filterVal2 = rangeToFilterFreq(range2);

    synthVoices[0].SetFilterCutoff(filterVal1);
    synthVoices[1].SetFilterCutoff(filterVal2);

    count = 0;
  }

  count++;
}

void Controls()
{
  // hw.ProcessAllControls();

  for (size_t i = 0; i < NUM_RODS; i++)
  {
    /* Get spin speed */
    abcsRods[i].Process();

    rotationSpeed = abcsRods[i].GetRotationSpeed();
    harmonicVal1 = abcsRods[i].GetEncoderVal();
    waveform = abcsRods[i].GetWaveformIndex();

    synthVoices[i].SetHarmonic(harmonicVal1);
    synthVoices[i].SetLfoFreq(rotationSpeed);
    synthVoices[i].SetOscWaveform(waveforms[waveform]);
    if (abcsRods[i].GetLongPress())
    {
      synthVoices[i].IncrementLfoTarget();
    }
  }

  /* Encoder */

  /* Set voice frequencies */
  // noteVal += hw.encoder.Increment();
  // float fq = mtof(noteVal);

  // for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  // {
  //   synthVoices[i].SetOscFreq(fq);
  // }

  /* LEDs */

  digitalWrite(PIN_LED_OUT_1, abcsRods[0].GetPulse());
  digitalWrite(PIN_LED_OUT_2, abcsRods[1].GetPulse());
}
