// #include "Adafruit_VL6180X.h"

class AbcsRod

{
private:
  int tcaIndex;
  int photoCellPin;
  Adafruit_VL6180X vl = Adafruit_VL6180X();

  const byte PulsesPerRevolution = 2;
  const unsigned long ZeroTimeout = 500000;
  const byte numReadings = 4;

  AnalogControl photoCellCtrl;

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

  float lightThreshold = 0.4;

  int pulse = 0;
  int PERIOD = 1;

  float k = 0.0;
  float oldk = 0.0;

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
  AbcsRod(uint8_t index, int _photoCellPin)
  {
    tcaIndex = index;
    photoCellPin = _photoCellPin;
  };
  ~AbcsRod(){};
  void Init(float callback_rate)
  {
    photoCellCtrl.Init(photoCellPin, callback_rate);

    // pinMode(photoCellPin, INPUT);
    // Wire.begin();
    vl.begin();
  };
  float GetDistance();
  float GetRotationSpeed()
  {
    return average / 60.0;
  };
  int GetPulse()
  {
    return pulse;
  };

  void Process()
  {
    photoCellCtrl.Process();
    k = photoCellCtrl.Value();
    updateVelocityAverage();

    // Serial.println(photoCellVal);
    if (k > 0)
    {
      if (k > lightThreshold)
      {
        if (pulse == 0)
        {
          pulse = 1;
          Pulse_Event();
        }
      }
      else
      {
        pulse = 0;
      }
    }
  }

  void Loop()
  {
    updateVelocityAverage();
    // int photoCellVal = analogRead(photoCellPin);
    // photoCellVal = 10;
    // Serial.println(photoCellVal);
    if (k > 0)
    {
      if (k > lightThreshold)
      {
        if (pulse == 0)
        {
          pulse = 1;
          Pulse_Event();
        }
      }
      else
      {
        pulse = 0;
      }
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
