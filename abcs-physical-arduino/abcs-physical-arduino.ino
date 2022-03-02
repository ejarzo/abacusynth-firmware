// RPM calculations from https://srituhobby.com/ir-infrared-speed-sensor-with-arduino-how-does-work-ir-speed-sensor/

#include <Wire.h>
#include "Adafruit_VL6180X.h"
#define TCAADDR 0x70

Adafruit_VL6180X vl1 = Adafruit_VL6180X();
Adafruit_VL6180X vl2 = Adafruit_VL6180X();

const int PHOTO_CELL_PIN = 14;
const int PHOTO_CELL_PIN_2 = 15;
const int PHOTO_CELL_PIN_3 = 15;

const int IR_PIN = 16;

int lightThreshold = 120;

void tcaselect(uint8_t i) {
  if (i > 7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << i);
  Wire.endTransmission();
}


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
unsigned long readings[numReadings];
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

static unsigned long previousTime = 0;

int PERIOD = 1;

void Pulse_Event() {
  PeriodBetweenPulses = micros() - LastTimeWeMeasured;
  LastTimeWeMeasured = micros();

  if (PulseCounter >= AmountOfReadings)  {
    PeriodAverage = PeriodSum / AmountOfReadings;
    PulseCounter = 1;
    PeriodSum = PeriodBetweenPulses;

    int RemapedAmountOfReadings = map(PeriodBetweenPulses, 40000, 5000, 1, 10);
    RemapedAmountOfReadings = constrain(RemapedAmountOfReadings, 1, 10);
    AmountOfReadings = RemapedAmountOfReadings;
  } else {
    PulseCounter++;
    PeriodSum = PeriodSum + PeriodBetweenPulses;
  }
}



void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(1);
  }

  pinMode(PHOTO_CELL_PIN, INPUT);
  Wire.begin();

  // Initialize distance sensors
  Serial.println("Init VL6180x 1");
  tcaselect(0);
  if (! vl1.begin()) {
    Serial.println("Failed to find sensor");
    while (1);
  }

  Serial.println("Init VL6180x 2");
  tcaselect(1);
  if (! vl2.begin()) {
    Serial.println("Failed to find sensor");
    while (1);
  }
}

void updateVelocityAverage() {
  LastTimeCycleMeasure = LastTimeWeMeasured;
  CurrentMicros = micros();
  if (CurrentMicros < LastTimeCycleMeasure) {
    LastTimeCycleMeasure = CurrentMicros;
  }
  FrequencyRaw = 10000000000 / PeriodAverage;
  if (PeriodBetweenPulses > ZeroTimeout - ZeroDebouncingExtra || CurrentMicros - LastTimeCycleMeasure > ZeroTimeout - ZeroDebouncingExtra) {
    FrequencyRaw = 0;  // Set frequency as 0.
    ZeroDebouncingExtra = 2000;
  } else {
    ZeroDebouncingExtra = 0;
  }
  FrequencyReal = FrequencyRaw / 10000;

  RPM = FrequencyRaw / PulsesPerRevolution * 60;
  RPM = RPM / 10000;
  total = total - readings[readIndex];
  readings[readIndex] = RPM;
  total = total + readings[readIndex];
  readIndex = readIndex + 1;

  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  average = total / numReadings;
}

int rotationDirCount = 0;

int rotationPhase = 0;
int phaseTime1 = micros();
int phaseTime2 = micros();

void loop() {
  updateVelocityAverage();

  int photoCell = analogRead(PHOTO_CELL_PIN);
  int photoCell2 = analogRead(PHOTO_CELL_PIN_2);
  // int irReading = analogRead(PHOTO_CELL_PIN_3);

  if (photoCell > lightThreshold) {
    if (pulse == 0) {
      phaseTime1 = micros();
      rotationDirCount++;
      pulse = 1;
      Pulse_Event();
    }
  } else {
    pulse = 0;
  }

  if (photoCell2 > lightThreshold) {
    if (pulse2 == 0) {
      phaseTime2 = micros() - phaseTime1;
      pulse2 = 1;
      rotationDirCount = 0;
    }
  } else {
    pulse2 = 0;
  }

  //
  //  if(rotationDirCount >=2) {
  //    rotationDir = 0;
  //  }

  tcaselect(0);
  uint8_t range1 = vl1.readRange();

  tcaselect(1);
  uint8_t range2 = vl2.readRange();




  Serial.print(range1);
  Serial.print(" ");
  Serial.print(pulse);
  Serial.print(" ");
  Serial.print(pulse2);
  Serial.print(" ");
  Serial.print(average);
  Serial.print(" ");
  Serial.print(phaseTime1 / 1000);
  Serial.print(" ");
  Serial.print(phaseTime2 / 1000);
  Serial.print(" ");
  Serial.print(range2);




  Serial.println();



  //  Serial.print("Lux: "); Serial.println(lux);
  //  uint8_t status = vl.readRangeStatus();

  //  delay(10);
  //  return;

  //  if (status == VL6180X_ERROR_NONE) {
  //    Serial.print("Range: "); Serial.println(range);
  //  }
  //
  //  // Some error occurred, print it out!
  //
  //  if  ((status >= VL6180X_ERROR_SYSERR_1) && (status <= VL6180X_ERROR_SYSERR_5)) {
  //    Serial.println("System error");
  //  }
  //  else if (status == VL6180X_ERROR_ECEFAIL) {
  //    Serial.println("ECE failure");
  //  }
  //  else if (status == VL6180X_ERROR_NOCONVERGE) {
  //    Serial.println("No convergence");
  //  }
  //  else if (status == VL6180X_ERROR_RANGEIGNORE) {
  //    Serial.println("Ignoring range");
  //  }
  //  else if (status == VL6180X_ERROR_SNR) {
  //    Serial.println("Signal/Noise error");
  //  }
  //  else if (status == VL6180X_ERROR_RAWUFLOW) {
  //    Serial.println("Raw reading underflow");
  //  }
  //  else if (status == VL6180X_ERROR_RAWOFLOW) {
  //    Serial.println("Raw reading overflow");
  //  }
  //  else if (status == VL6180X_ERROR_RANGEUFLOW) {
  //    Serial.println("Range reading underflow");
  //  }
  //  else if (status == VL6180X_ERROR_RANGEOFLOW) {
  //    Serial.println("Range reading overflow");
  //  }
  //  delay(10);
}
