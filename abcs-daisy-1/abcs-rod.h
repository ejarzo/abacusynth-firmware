// #include "Adafruit_VL6180X.h"

class AbcsRod
{
private:
  int tcaIndex;
  int pinBreakBeam_;
  int pinEnc1_;
  int pinEnc2_;
  int pinEncBtn_;
  // Adafruit_VL6180X vl = Adafruit_VL6180X();

  const byte PulsesPerRevolution = 2;
  const unsigned long ZeroTimeout = 500000;
  const byte numReadings = 4;

  Encoder rodEncoder;
  Switch breakBeamSwitch;

  unsigned long LastTimeWeMeasured;
  unsigned long PeriodBetweenPulses = ZeroTimeout + 10;
  unsigned long PeriodAverage = ZeroTimeout + 10;
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
  unsigned int photoCellVal;

  int encoderVal = 0;
  int waveformIndex = 0;

  int prevLongPress = 0;
  int longPress = 0;
  int longPressRisingEdge = 0;

  float lightThreshold = 0.14;

  int pulse = 0;
  int PERIOD = 1;

  float k = 0.0;
  float oldk = 0.0;

  bool canUpdateWaveform = false;
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

public:
  AbcsRod(uint8_t index, int pinBreakBeam, int pinEnc1, int pinEnc2, int pinEncBtn)
  {
    tcaIndex = index;
    encoderVal = index + 1;
    pinBreakBeam_ = pinBreakBeam;
    pinEnc1_ = pinEnc1;
    pinEnc2_ = pinEnc2;
    pinEncBtn_ = pinEncBtn;
  };
  ~AbcsRod(){};
  void Init(float callback_rate)
  {
    rodEncoder.Init(callback_rate, pinEnc1_, pinEnc2_, pinEncBtn_, INPUT_PULLUP, INPUT_PULLUP, INPUT_PULLUP);
    breakBeamSwitch.Init(callback_rate, false, pinBreakBeam_, INPUT_PULLUP);
    // Wire.begin();
    // vl.begin();
  };
  float GetDistance();
  float GetRotationSpeed()
  {
    return average / 60.0;
  };

  int GetPulse()
  {
    return breakBeamSwitch.Pressed();
  };

  int GetEncoderVal()
  {
    return encoderVal;
  };

  int GetWaveformIndex()
  {
    return waveformIndex;
  };

  void SetVal(float val)
  {
    k = val;
  }

  int GetLongPress()
  {
    return longPressRisingEdge;
  }

  float GetPressTime()
  {
    return rodEncoder.TimeHeldMs();
  }

  void Process()
  {
    rodEncoder.Debounce();
    breakBeamSwitch.Debounce();

    if (rodEncoder.RisingEdge())
    {
      canUpdateWaveform = true;
    }

    longPress = rodEncoder.TimeHeldMs() > LONG_PRESS_THRESHOLD;

    if (!prevLongPress && longPress)
    {
      longPressRisingEdge = true;
    }
    else
    {
      longPressRisingEdge = false;
    }

    if (longPress)
    {
      canUpdateWaveform = false;
    }
    // Serial.println(prevLongPress);
    if (canUpdateWaveform)
    {
      waveformIndex += rodEncoder.FallingEdge();
      waveformIndex = (waveformIndex % NUM_WAVEFORMS + NUM_WAVEFORMS) % NUM_WAVEFORMS;
    }

    prevLongPress = longPress;

    encoderVal -= rodEncoder.Increment();
    encoderVal = (encoderVal % 10 + 10) % 10;

    updateVelocityAverage();

    if (breakBeamSwitch.RisingEdge())
    {
      Pulse_Event();
    }
  }
};

float AbcsRod::GetDistance()
{
  // tcaselect(tcaIndex);
  /* Todo: readStatus? */
  // Wire.beginTransmission(TCAADDR);
  // Wire.write(1 << 0);
  // Wire.endTransmission();
  // uint8_t range = vl.readRange();
  // float normal = range / 255.0;
  // if (!vl.readRangeStatus())
  // {
  //   uint8_t range = vl.readRange();
  //   float normal = range / 255.0;
  //   return normal;
  // }
  return 1.0;
}
