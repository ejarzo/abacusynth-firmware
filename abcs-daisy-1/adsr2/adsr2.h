// #pragma once
// #ifndef DSY_ADSR_H
// #define DSY_ADSR_H
// #include "adsr2.h"
#include <math.h>

#include <stdint.h>
// #ifdef __cplusplus

using namespace daisysp;

// {
/** Distinct stages that the phase of the envelope can be located in.
- IDLE   = located at phase location 0, and not currently running
- ATTACK  = First segment of envelope where phase moves from 0 to 1
- DECAY   = Second segment of envelope where phase moves from 1 to SUSTAIN value
- RELEASE =     Fourth segment of envelop where phase moves from SUSTAIN to 0
*/
// enum
// {
//   ADSR_SEG_IDLE = 0,
//   ADSR_SEG_ATTACK = 1,
//   ADSR_SEG_DECAY = 2,
//   ADSR_SEG_RELEASE = 4
// };

/** adsr envelope module

Original author(s) : Paul Batchelor

Ported from Soundpipe by Ben Sergentanis, May 2020

Remake by Steffan DIedrichsen, May 2021
*/
class Adsr2
{
public:
  Adsr2() {}
  ~Adsr2() {}
  /** Initializes the Adsr module.
      \param sample_rate - The sample rate of the audio engine being run.
  */
  void Init(float sample_rate, int blockSize = 1);
  /**
   \function Retrigger forces the envelope back to attack phase
   \param hard  resets the history to zero, results in a click.
   */
  void Retrigger(bool hard);
  /** Processes one sample through the filter and returns one sample.
      \param gate - trigger the envelope, hold it to sustain
  */
  float Process(bool gate);
  /** Sets time
      Set time per segment in seconds
  */
  void SetTime(int seg, float time);
  void SetAttackTime(float timeInS, float shape = 0.0f);
  void SetDecayTime(float timeInS);
  void SetReleaseTime(float timeInS);

private:
  void SetTimeConstant(float timeInS, float &time, float &coeff);

public:
  /** Sustain level
      \param sus_level - sets sustain level, 0...1.0
  */
  inline void SetSustainLevel(float sus_level)
  {
    sus_level = (sus_level <= 0.f)  ? -0.01f // forces envelope into idle
                : (sus_level > 1.f) ? 1.f
                                    : sus_level;
    sus_level_ = sus_level;
  }
  /** get the current envelope segment
      \return the segment of the envelope that the phase is currently located in.
  */
  inline uint8_t GetCurrentSegment() { return mode_; }
  /** Tells whether envelope is active
      \return true if the envelope is currently in any stage apart from idle.
  */
  inline bool IsRunning() const { return mode_ != ADSR_SEG_IDLE; }

private:
  float sus_level_{0.f};
  float x_{0.f};
  float attackShape_{-1.f};
  float attackTarget_{0.0f};
  float attackTime_{-1.0f};
  float decayTime_{-1.0f};
  float releaseTime_{-1.0f};
  float attackD0_{0.f};
  float decayD0_{0.f};
  float releaseD0_{0.f};
  int sample_rate_;
  uint8_t mode_{ADSR_SEG_IDLE};
  bool gate_{false};
};
// } // namespace daisysp
// #endif
// #endif

void Adsr2::Init(float sample_rate, int blockSize)
{
  sample_rate_ = sample_rate / blockSize;
  attackShape_ = -1.f;
  attackTarget_ = 0.0f;
  attackTime_ = -1.f;
  decayTime_ = -1.f;
  releaseTime_ = -1.f;
  sus_level_ = 0.7f;
  x_ = 0.0f;
  gate_ = false;
  mode_ = ADSR_SEG_IDLE;

  SetTime(ADSR_SEG_ATTACK, 0.1f);
  SetTime(ADSR_SEG_DECAY, 0.1f);
  SetTime(ADSR_SEG_RELEASE, 0.1f);
}

void Adsr2::Retrigger(bool hard)
{
  mode_ = ADSR_SEG_ATTACK;
  if (hard)
    x_ = 0.f;
}

void Adsr2::SetTime(int seg, float time)
{
  switch (seg)
  {
  case ADSR_SEG_ATTACK:
    SetAttackTime(time, 0.0f);
    break;
  case ADSR_SEG_DECAY:
  {
    SetTimeConstant(time, decayTime_, decayD0_);
  }
  break;
  case ADSR_SEG_RELEASE:
  {
    SetTimeConstant(time, releaseTime_, releaseD0_);
  }
  break;
  default:
    return;
  }
}

void Adsr2::SetAttackTime(float timeInS, float shape)
{
  if ((timeInS != attackTime_) || (shape != attackShape_))
  {
    attackTime_ = timeInS;
    attackShape_ = shape;
    if (timeInS > 0.f)
    {
      float x = shape;
      float target = 9.f * powf(x, 10.f) + 0.3f * x + 1.01f;
      attackTarget_ = target;
      float logTarget = logf(1.f - (1.f / target)); // -1 for decay
      attackD0_ = 1.f - expf(logTarget / (timeInS * sample_rate_));
    }
    else
      attackD0_ = 1.f; // instant change
  }
}
void Adsr2::SetDecayTime(float timeInS)
{
  SetTimeConstant(timeInS, decayTime_, decayD0_);
}
void Adsr2::SetReleaseTime(float timeInS)
{
  SetTimeConstant(timeInS, releaseTime_, releaseD0_);
}

void Adsr2::SetTimeConstant(float timeInS, float &time, float &coeff)
{
  if (timeInS != time)
  {
    time = timeInS;
    if (time > 0.f)
    {
      const float target = logf(1. / M_E);
      coeff = 1.f - expf(target / (time * sample_rate_));
    }
    else
      coeff = 1.f; // instant change
  }
}

float Adsr2::Process(bool gate)
{
  float out = 0.0f;

  if (gate && !gate_) // rising edge
    mode_ = ADSR_SEG_ATTACK;
  else if (!gate && gate_) // falling edge
    mode_ = ADSR_SEG_RELEASE;
  gate_ = gate;

  float D0(attackD0_);
  if (mode_ == ADSR_SEG_DECAY)
    D0 = decayD0_;
  else if (mode_ == ADSR_SEG_RELEASE)
    D0 = releaseD0_;

  float target = mode_ == ADSR_SEG_DECAY ? sus_level_ : -0.01f;
  switch (mode_)
  {
  case ADSR_SEG_IDLE:
    out = 0.0f;
    break;
  case ADSR_SEG_ATTACK:
    x_ += D0 * (attackTarget_ - x_);
    out = x_;
    if (out > 1.f)
    {
      x_ = out = 1.f;
      mode_ = ADSR_SEG_DECAY;
    }
    break;
  case ADSR_SEG_DECAY:
  case ADSR_SEG_RELEASE:
    x_ += D0 * (target - x_);
    out = x_;
    if (out < 0.0f)
    {
      x_ = out = 0.f;
      mode_ = ADSR_SEG_IDLE;
    }
  default:
    break;
  }
  return out;
}