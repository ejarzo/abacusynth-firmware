#include "DaisyDuino.h"
#include <string>

class Voice
{
public:
  Voice() {}
  ~Voice() {}
  void Init(float samplerate)
  {
    active_ = false;
    // osc_.Init(samplerate);
    // osc_.SetAmp(0.75f);
    // osc_.SetWaveform(Oscillator::WAVE_POLYBLEP_SAW);
    env_.Init(samplerate);
    env_.SetSustainLevel(0.5f);
    env_.SetTime(ADSR_SEG_ATTACK, 0.005f);
    env_.SetTime(ADSR_SEG_DECAY, 0.005f);
    env_.SetTime(ADSR_SEG_RELEASE, 0.2f);
    // filt_.Init(samplerate);
    // filt_.SetFreq(6000.f);
    // filt_.SetRes(0.6f);
    // filt_.SetDrive(0.8f);
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
      float amp;
      amp = env_.Process(env_gate_);
      if (!env_.IsRunning())
      {
        active_ = false;
      }
      // sig = osc_.Process();
      // filt_.Process(sig);
      // return filt_.Low() * (velocity_ / 127.f) * amp;
      return amp * velocity_ / 127.f;
    }
    return 0.f;
  }

  void OnNoteOn(int note, int velocity)
  {
    Serial.println("Voice OnNoteOn Called");
    note_ = note;
    velocity_ = velocity;
    // osc_.SetFreq(mtof(note_));
    active_ = true;
    env_gate_ = true;
  }

  void OnNoteOff()
  {
    Serial.println("Voice OnNoteOff Called");
    env_gate_ = false;
  }

  // void SetCutoff(float val) { filt_.SetFreq(val); }

  inline bool IsActive() const { return active_; }
  inline int GetNote() const { return note_; }

private:
  // Oscillator osc_;
  // Svf filt_;
  Adsr env_;
  int note_, velocity_;
  bool active_;
  bool env_gate_;
};

template <size_t max_voices>
class VoiceManager
{
public:
  VoiceManager() {}
  ~VoiceManager() {}

  void Init(float samplerate)
  {
    for (size_t i = 0; i < max_voices; i++)
    {
      voices[i].Init(samplerate);
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

  void OnNoteOn(int notenumber, int velocity)
  {
    Voice *v = FindFreeVoice();
    if (v == NULL)
      return;
    v->OnNoteOn(notenumber, velocity);
  }

  void OnNoteOff(int notenumber, int velocity)
  {
    for (size_t i = 0; i < max_voices; i++)
    {
      Voice *v = &voices[i];
      if (v->IsActive() && v->GetNote() == notenumber)
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
  Voice *FindFreeVoice()
  {
    Voice *v = NULL;
    for (size_t i = 0; i < max_voices; i++)
    {
      if (!voices[i].IsActive())
      {
        v = &voices[i];
        break;
      }
    }
    return v;
  }
};

// void AudioCallback(float *in, float *out, size_t size)
// {
//   float sum = 0.f;
//   hw.ProcessDigitalControls();
//   hw.ProcessAnalogControls();
//   if (hw.buttons[0].FallingEdge())
//   {
//     voice_handler.FreeAllVoices();
//   }
//   voice_handler.SetCutoff(250.f + hw.GetKnobValue(0) * 8000.f);

//   for (size_t i = 0; i < size; i += 2)
//   {
//     sum = 0.f;
//     sum = voice_handler.Process() * 0.5f;
//     out[i] = sum;
//     out[i + 1] = sum;
//   }
// }
