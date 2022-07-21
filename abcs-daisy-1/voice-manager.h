#include "DaisyDuino.h"
#include <string>
#include "./adsr2/adsr2.h"

using namespace daisysp;

/* ========================= Single Voice ========================= */

class Voice
{
public:
  Voice() {}
  ~Voice() {}
  void Init(float samplerate, int blocksize)
  {
    active_ = false;
    setActiveOnNext = false;
    env_.Init(samplerate, 1);
    interruptGain = 0.f;
    velLine.Init(samplerate);
    isFadeFinished = false;
    fading = false;

    setADSR(0.005f, 0.1f, 0.5f, 0.2f);
  }

  void setADSR(float a, float d, float s, float r)
  {
    env_.SetTime(ADSR_SEG_ATTACK, a);
    env_.SetTime(ADSR_SEG_DECAY, d);
    env_.SetSustainLevel(s);
    env_.SetTime(ADSR_SEG_RELEASE, r);
  }

  float Process()
  {
    if (active_)
    {
      velGain = velLine.Process(&isFadeFinished);
      float amp;
      amp = env_.Process(env_gate_);
      if (!env_.IsRunning())
      {
        active_ = false;
      }
      return amp * velGain;
    }
    return 0.f;
  }

  void OnNoteOn(int note, int newVelocity)
  {
    if (active_)
    {
      env_.Retrigger(false);
    }

    /* Ramp to new velocity */
    velLine.Start(velGain, newVelocity / 127.f, 0.02f);

    note_ = note;
    velocity_ = newVelocity;
    active_ = true;
    env_gate_ = true;
  }

  void OnNoteOff()
  {
    Serial.println("Voice OnNoteOff Called");
    env_gate_ = false;
  }

  inline bool IsActive() const { return active_; }
  inline bool IsEnvGate() const { return env_gate_; }
  inline int GetNote() const { return note_; }

private:
  Adsr2 env_;
  Line velLine;
  uint8_t isFadeFinished;
  float interruptGain;
  float velGain;
  int note_, velocity_, prevVelocity;
  bool active_;
  bool fading;
  bool env_gate_;
  bool setActiveOnNext;
};

/* ========================= Polyphony Voice Manager ========================= */

template <size_t max_voices>
class VoiceManager
{
public:
  VoiceManager() {}
  ~VoiceManager() {}

  void Init(float samplerate, int blocksize)
  {
    for (size_t i = 0; i < max_voices; i++)
    {
      voices[i].Init(samplerate, blocksize);
    }
  }

  float Process()
  {
    float sum;
    sum = 0.f;
    for (size_t i = 0; i < max_voices; i++)
    {
      sum += voices[i].Process();
    }
    return sum;
  }

  Voice *GetVoices()
  {
    return voices;
  }

  void setADSR(float a, float d, float s, float r)
  {
    for (size_t i = 0; i < max_voices; i++)
    {
      Voice *v = &voices[i];
      v->setADSR(a, d, s, r);
    }
  }

  void OnNoteOn(int noteNumber, int velocity)
  {
    Voice *v = FindFreeVoice(noteNumber);
    if (v == NULL)
      return;
    v->OnNoteOn(noteNumber, velocity);
  }

  void OnNoteOff(int noteNumber, int velocity)
  {
    for (size_t i = 0; i < max_voices; i++)
    {
      Voice *v = &voices[i];
      if (v->IsActive() && v->GetNote() == noteNumber)
      {
        v->OnNoteOff();
      }
    }
  }

  void FreeAllVoices()
  {
    for (size_t i = 0; i < max_voices; i++)
    {
      voices[i].OnNoteOff();
    }
  }

private:
  Voice voices[max_voices];
  Voice *FindFreeVoice(int noteNumber)
  {
    Voice *v = NULL;

    /* Re-trigger if same note */
    for (size_t i = 0; i < max_voices; i++)
    {
      if (voices[i].GetNote() == noteNumber)
      {
        v = &voices[i];
        break;
      }
    }

    if (v)
      return v;

    /* Find inactive voices */
    for (size_t i = 0; i < max_voices; i++)
    {
      if (!voices[i].IsActive())
      {
        v = &voices[i];
        break;
      }
    }
    if (v)
      return v;

    /* Find voices in the release phase */
    for (size_t i = 0; i < max_voices; i++)
    {
      if (!voices[i].IsEnvGate())
      {
        v = &voices[i];
        break;
      }
    }
    if (v)
      return v;
  }
};
