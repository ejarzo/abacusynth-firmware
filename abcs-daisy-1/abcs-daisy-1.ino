/*
  "forcedInclude": [
      "/Users/eliasjarzombek/Library/Arduino15/packages/STMicroelectronics/hardware/stm32/2.2.0/cores/arduino/Arduino.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/Adafruit_VL6180X/Adafruit_VL6180X.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/DaisyDuino/src/utility/DaisySP/daisysp.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/DaisyDuino/src/utility/DaisySP/modules/moogladder.h"
  ],
 */

#include <Wire.h>
#include "DaisyDuino.h"
#include "Adafruit_VL6180X.h"

#define TCAADDR 0x70
#define NUM_WAVEFORMS 4
#define NUM_SYNTHVOICES 2
#define NUM_RODS 2

#include "./abcs-synth-voice2.h"
#include "./abcs-rod.h"

using namespace daisysp;

uint8_t waveforms[NUM_WAVEFORMS] = {
    Oscillator::WAVE_SIN,
    Oscillator::WAVE_TRI,
    Oscillator::WAVE_POLYBLEP_SAW,
    Oscillator::WAVE_POLYBLEP_SQUARE,
};

static AdEnv ad;
static Parameter cutoffParam, lfoFreqParam, lfoDepthCtrlParam, photoCellParam;
static int waveform;

int noteVal, wave, oscLfoTarget;
float oscFreq, cutoff, lfoFreqCtrl, lfoDepthCtrl;
float attack, release;
float oldk1, oldk2, oldk3, k1, k2, k3;

int activeVoice = 0;

Adafruit_VL6180X vl1 = Adafruit_VL6180X();
DaisyHardware pod;

// AnalogControl photoCellCtrl;

AbcsSynthVoice2 synthVoices[NUM_SYNTHVOICES];

// AbcsRod rod0 = AbcsRod(0, PIN_A1);
// AbcsRod rod1 = AbcsRod(1, PIN_A2);

AbcsRod abcsRods[NUM_RODS] = {
    AbcsRod(0, PIN_A1),
    AbcsRod(1, PIN_A2)};

void ConditionalParameter(float oldVal, float newVal, float &param, float update);

void Controls();

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
  // photoCellCtrl.Process();
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
  Serial.begin(115200);

  Wire.begin();

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

  pod = DAISY.init(DAISY_POD, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();
  callback_rate = DAISY.get_callbackrate();

  // photoCellCtrl.Init(PIN_A1, callback_rate);

  for (size_t i = 0; i < NUM_RODS; i++)
  {
    abcsRods[i].Init(callback_rate);
  }

  // rod0.Init(callback_rate);
  // rod1.Init(callback_rate);

  // Set global variables
  oldk1 = oldk2 = oldk3 = 0;
  k1 = k2 = k3 = 0;

  noteVal = 40;
  attack = .01f;
  release = .2f;
  lfoFreqCtrl = 0.0f;
  lfoDepthCtrl = 0.0f;
  oscLfoTarget = 1;

  float fq = mtof(noteVal);

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].Init(sample_rate);
    synthVoices[i].SetOscWaveform(waveforms[0]);
    synthVoices[i].SetOscFreq(fq * (float(i) + 1.f));
    synthVoices[i].SetLfoFreq(0.0);
    synthVoices[i].SetLfoDepth(0.0);
  }

  // Init everything
  ad.Init(sample_rate);
  waveform = waveforms[0];

  // Set envelope parameters
  ad.SetTime(ADENV_SEG_ATTACK, 0.01);
  ad.SetTime(ADENV_SEG_DECAY, .2);
  ad.SetMax(1);
  ad.SetMin(0);
  ad.SetCurve(0.5);

  // set parameter parameters
  cutoffParam.Init(pod.controls[0], 100, 20000, cutoffParam.LOGARITHMIC);
  lfoFreqParam.Init(pod.controls[1], 0.25, 10, lfoFreqParam.LINEAR);
  lfoDepthCtrlParam.Init(pod.controls[1], 0., 1., lfoDepthCtrlParam.LINEAR);

  // photoCellParam.Init(photoCellCtrl, 0, 1024, photoCellParam.LINEAR);

  // start the audio callback
  DAISY.begin(MyCallback);
}

void tcaselect(uint8_t i)
{
  if (i > 7)
    return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}

float rotationSpeed;
float depth;
int pulse;

int count = 0;

float filterValOld = 10000.0;
float filterValOld2 = 10000.0;
float filterVal = filterValOld;
uint8_t range1;
uint8_t range2;

float rangeToFilterFreq(int range)
{
  float exp = range * 11 / 128.0 + 5;
  exp = constrain(exp, 5, 14);
  return pow(2, exp);
}

void loop()
{
  /* range is pretty slow to read, only check every 100 cycles */
  if (count % 100 == 0)
  {
    tcaselect(0);
    range1 = vl1.readRange();
    tcaselect(1);
    range2 = vl1.readRange();
  }
  /* filter 1 */
  filterVal = rangeToFilterFreq(range1) * 0.1 + filterValOld * 0.9;
  filterValOld = filterVal;
  synthVoices[0].SetFilterCutoff(filterVal);

  /* filter 2 */
  filterVal = rangeToFilterFreq(range2) * 0.1 + filterValOld2 * 0.9;
  filterValOld2 = filterVal;
  synthVoices[1].SetFilterCutoff(filterVal);

  count++;
}

// Updates values if knob had changed
void ConditionalParameter(float oldVal, float newVal, float &param, float update)
{
  if (abs(oldVal - newVal) > 0.0005)
  {
    param = update;
  }
}

void Controls()
{
  pod.ProcessAllControls();

  for (size_t i = 0; i < NUM_RODS; i++)
  {
    /* Get spin speed */
    abcsRods[i].Process();
    rotationSpeed = abcsRods[i].GetRotationSpeed();
    depth = fclamp(rotationSpeed * 0.5, 0.f, 1.f);

    /* Set LFOs */
    synthVoices[i].SetLfoFreq(rotationSpeed);
  }

  int pulse = abcsRods[activeVoice].GetPulse();
  pod.leds[1].Set(pulse, pulse, pulse);

  /* Encoder */
  activeVoice += pod.encoder.RisingEdge();
  activeVoice = (activeVoice % NUM_SYNTHVOICES + NUM_SYNTHVOICES) % NUM_SYNTHVOICES;

  /* Set voice frequencies */
  noteVal += pod.encoder.Increment();
  float fq = mtof(noteVal);

  for (size_t i = 0; i < NUM_SYNTHVOICES; i++)
  {
    synthVoices[i].SetOscFreq(fq * (float(i) + 1.f));
  }

  /* Knobs */
  k1 = pod.controls[0].Value();
  k2 = pod.controls[1].Value();

  ConditionalParameter(oldk1, k1, cutoff, cutoffParam.Process());
  ConditionalParameter(oldk2, k2, lfoFreqCtrl, lfoFreqParam.Process());
  ConditionalParameter(oldk2, k2, lfoDepthCtrl, lfoDepthCtrlParam.Process());

  /* LEDs */
  pod.leds[0].Set((waveform == 2 || waveform == 3), (waveform == 1 || waveform == 3), (waveform == 0));
  // pod.leds[1].Set((oscLfoTarget == 0), (oscLfoTarget == 1), false);

  /* Buttons */
  waveform += pod.buttons[0].RisingEdge();
  waveform = (waveform % NUM_WAVEFORMS + NUM_WAVEFORMS) % NUM_WAVEFORMS;
  synthVoices[activeVoice].SetOscWaveform(waveforms[waveform]);

  oscLfoTarget += pod.buttons[1].RisingEdge();
  oscLfoTarget = (oscLfoTarget % 2 + 2) % 2;
  synthVoices[activeVoice].setLfoTarget(oscLfoTarget);

  oldk1 = k1;
  oldk2 = k2;
  oldk3 = k3;
}
