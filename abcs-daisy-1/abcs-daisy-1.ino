/*
  "forcedInclude": [
      "/Users/eliasjarzombek/Library/Arduino15/packages/STMicroelectronics/hardware/stm32/2.2.0/cores/arduino/Arduino.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/Adafruit_VL6180X/Adafruit_VL6180X.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/DaisyDuino/src/utility/DaisySP/daisysp.h",
      "/Users/eliasjarzombek/Documents/Arduino/libraries/DaisyDuino/src/utility/DaisySP/modules/moogladder.h"
  ],
 */

#include "DaisyDuino.h"
#include "./abcs-synth-voice2.h"

#include <Wire.h>
#include "Adafruit_VL6180X.h"

#define TCAADDR 0x70
#define NUM_WAVEFORMS 4

uint8_t waveforms[NUM_WAVEFORMS] = {
    Oscillator::WAVE_SIN,
    Oscillator::WAVE_TRI,
    Oscillator::WAVE_POLYBLEP_SAW,
    Oscillator::WAVE_POLYBLEP_SQUARE,
};

static AdEnv ad;
static Parameter cutoffParam, lfoFreqParam, lfoDepthCtrlParam;
static int waveform;

int noteVal, wave, oscLfoTarget;
float oscFreq, cutoff, lfoFreqCtrl, lfoDepthCtrl;
float attack, release;
float oldk1, oldk2, k1, k2;

Adafruit_VL6180X vl1 = Adafruit_VL6180X();
DaisyHardware pod;
AbcsSynthVoice2 synthVoice1;
AbcsSynthVoice2 synthVoice2;
AbcsSynthVoice2 synthVoice3;

/* =============================== */

class AbcsRod
{
private:
  int tcaIndex;
  Adafruit_VL6180X vl = Adafruit_VL6180X();

  const byte PulsesPerRevolution = 2;
  const unsigned long ZeroTimeout = 500000;
  const byte numReadings = 4;

  volatile unsigned long LastTimeWeMeasured;
  volatile unsigned long PeriodBetweenPulses = ZeroTimeout + 10;
  volatile unsigned long PeriodAverage = ZeroTimeout + 10;
  unsigned long FrequencyRaw;
  unsigned long FrequencyReal;
  unsigned long RPM;
  unsigned int PulseCounter = 1;
  unsigned long PeriodSum;

  unsigned long LastTimeCycleMeasure = LastTimeWeMeasured;
  unsigned long CurrentMicros = micros();
  unsigned int AmountOfReadings = 1;
  unsigned int ZeroDebouncingExtra;
  unsigned long readings[4];
  unsigned long readIndex;
  unsigned long total;
  unsigned int average;

  int currTime = 0;
  int prevTime = 0;
  int pulse = 0;
  int timeBetweenPulse = 0;
  int count = 0;
  int pulse2 = 0;

  int prevCount = 0;

  unsigned long previousTime = 0;

  int PERIOD = 1;

public:
  AbcsRod(uint8_t index);
  ~AbcsRod();
  void Init();
  float GetDistance();
  float GetRotationSpeed()
  {
    return average / 60.0;
  };
  void updateVelocityAverage()
  {
    LastTimeCycleMeasure = LastTimeWeMeasured;
    CurrentMicros = micros();
    if (CurrentMicros < LastTimeCycleMeasure)
    {
      LastTimeCycleMeasure = CurrentMicros;
    }
    FrequencyRaw = 10000000000 / PeriodAverage;
    if (PeriodBetweenPulses > ZeroTimeout - ZeroDebouncingExtra || CurrentMicros - LastTimeCycleMeasure > ZeroTimeout - ZeroDebouncingExtra)
    {
      FrequencyRaw = 0; // Set frequency as 0.
      ZeroDebouncingExtra = 2000;
    }
    else
    {
      ZeroDebouncingExtra = 0;
    }
    FrequencyReal = FrequencyRaw / 10000;

    RPM = FrequencyRaw / PulsesPerRevolution * 60;
    RPM = RPM / 10000;
    total = total - readings[readIndex];
    readings[readIndex] = RPM;
    total = total + readings[readIndex];
    readIndex = readIndex + 1;

    if (readIndex >= numReadings)
    {
      readIndex = 0;
    }
    average = total / numReadings;
  }
  void Pulse_Event()
  {
    PeriodBetweenPulses = micros() - LastTimeWeMeasured;
    LastTimeWeMeasured = micros();

    if (PulseCounter >= AmountOfReadings)
    {
      PeriodAverage = PeriodSum / AmountOfReadings;
      PulseCounter = 1;
      PeriodSum = PeriodBetweenPulses;

      int RemapedAmountOfReadings = map(PeriodBetweenPulses, 40000, 5000, 1, 10);
      RemapedAmountOfReadings = constrain(RemapedAmountOfReadings, 1, 10);
      AmountOfReadings = RemapedAmountOfReadings;
    }
    else
    {
      PulseCounter++;
      PeriodSum = PeriodSum + PeriodBetweenPulses;
    }
  }
  void Loop()
  {
    updateVelocityAverage();
    int photoCell = analogRead(PIN_A1);
    Serial.println(photoCell);
    int lightThreshold = 120;
    if (photoCell > lightThreshold)
    {
      pod.leds[1].Set(1, 1, 1);
      if (pulse == 0)
      {
        // phaseTime1 = micros();
        // rotationDirCount++;
        pulse = 1;
        Pulse_Event();
      }
    }
    else
    {
      pod.leds[1].Set(0, 0, 0);
      pulse = 0;
    }
  }
};

AbcsRod::AbcsRod(uint8_t index)
{
  tcaIndex = index;
}

void AbcsRod::Init()
{
  pinMode(16, INPUT);
  Wire.begin();
  vl1.begin();
}

AbcsRod::~AbcsRod() {}

float AbcsRod::GetDistance()
{
  // tcaselect(tcaIndex);
  /* Todo: readStatus? */
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << tcaIndex);
  Wire.endTransmission();
  if (!vl.readRangeStatus())
  {
    uint8_t range = vl.readRange();
  }
  // float normal = range / 255.0;
  return 1.0;
}

/* =============================== */

AbcsRod rod0 = AbcsRod(0);

void ConditionalParameter(float oldVal, float newVal, float &param, float update);

void Controls();

void NextSamples(float &sig)
{
  // float ad_out = ad.Process();
  float sig1 = synthVoice1.Process();
  float sig2 = synthVoice2.Process();
  float sig3 = synthVoice3.Process();

  // sig = sig1 / 1.f;
  sig = sig1 / 3.f + sig2 / 3.f + sig3 / 3.f;
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

  Serial.begin(115200);
  // while (!Serial)
  // {
  //   delay(1);
  // }

  Wire.begin();

  // Initialize distance sensors
  tcaselect(0);

  if (!vl1.begin())
  {
    // Serial.print("Could not find vl1 distance sensor");
  }

  rod0.Init();

  pod = DAISY.init(DAISY_POD, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();
  callback_rate = DAISY.get_callbackrate();

  // Set global variables
  oldk1 = oldk2 = 0;
  k1 = k2 = 0;

  noteVal = 40;
  attack = .01f;
  release = .2f;
  lfoFreqCtrl = 0.0f;
  lfoDepthCtrl = 0.0f;
  oscLfoTarget = 1;

  float fq = mtof(noteVal);

  synthVoice1.Init(sample_rate);
  synthVoice1.SetOscWaveform(waveforms[2]);
  synthVoice1.SetOscFreq(fq);
  synthVoice1.SetLfoFreq(2.0);

  synthVoice2.Init(sample_rate);
  synthVoice2.SetOscWaveform(waveforms[3]);
  synthVoice2.SetOscFreq(fq * 2.0);
  synthVoice2.SetLfoFreq(2.0);

  synthVoice3.Init(sample_rate);
  synthVoice3.SetOscWaveform(waveforms[1]);
  synthVoice3.SetOscFreq(fq * 3.0);
  synthVoice3.SetLfoFreq(2.0);

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

void loop()
{
  rod0.Loop();
  tcaselect(0);

  // float dist = rod0.GetDistance();
  float rotationSpeed = rod0.GetRotationSpeed();

  Serial.println(rotationSpeed);
  uint8_t range1 = vl1.readRange();
  float filterVal = map(range1, 0, 255, 0.0, 10000.0);
  synthVoice1.SetLfoFreq(rotationSpeed);
  float depth = rotationSpeed * 0.5;
  synthVoice1.SetLfoDepth(fclamp(depth, 0.f, 1.f));
  synthVoice1.SetFilterCutoff(filterVal);
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

  /* Encoder */
  noteVal += pod.encoder.Increment();
  float fq = mtof(noteVal);

  synthVoice1.SetOscFreq(fq);
  synthVoice2.SetOscFreq(fq * 2.f);
  synthVoice3.SetOscFreq(fq * 3.f);

  /* Knobs */
  k1 = pod.controls[0].Value();
  k2 = pod.controls[1].Value();

  ConditionalParameter(oldk1, k1, cutoff, cutoffParam.Process());
  ConditionalParameter(oldk2, k2, lfoFreqCtrl, lfoFreqParam.Process());
  ConditionalParameter(oldk2, k2, lfoDepthCtrl, lfoDepthCtrlParam.Process());

  // synthVoice1.SetFilterCutoff(cutoff);
  // synthVoice1.SetLfoFreq(lfoFreqCtrl);
  // synthVoice1.SetLfoDepth(lfoDepthCtrl);

  /* LEDs */
  pod.leds[0].Set((waveform == 2 || waveform == 3), (waveform == 1 || waveform == 3), (waveform == 0));
  // pod.leds[1].Set((oscLfoTarget == 0), (oscLfoTarget == 1), false);

  /* Buttons */
  waveform += pod.buttons[0].RisingEdge();
  waveform = (waveform % NUM_WAVEFORMS + NUM_WAVEFORMS) % NUM_WAVEFORMS;
  synthVoice1.SetOscWaveform(waveforms[waveform]);

  oscLfoTarget += pod.buttons[1].RisingEdge();
  oscLfoTarget = (oscLfoTarget % 2 + 2) % 2;
  synthVoice1.setLfoTarget(oscLfoTarget);

  oldk1 = k1;
  oldk2 = k2;
}
