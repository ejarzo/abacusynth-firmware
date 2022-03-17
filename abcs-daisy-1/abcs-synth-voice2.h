#include "DaisyDuino.h"

class AbcsSynthVoice2
{
private:
  Oscillator osc;
  MoogLadder flt;

  float modsig;
  float modsig2;
  float oscFreq;
  float lfoFreq;
  float lfoDepth;
  float prevDepth;
  float dc_os_;
  uint8_t waveform;
  uint8_t lfoTarget;

  /* efficient LFO: https://www.earlevel.com/main/2003/03/02/the-digital-state-variable-filter/ */
  float sinZ = 0.0;
  float cosZ = 1.0;

public:
  AbcsSynthVoice2(){};
  ~AbcsSynthVoice2(){};

  void Init(float sample_rate)
  {
    osc.Init(sample_rate);
    flt.Init(sample_rate);

    modsig = 0.0f;
    oscFreq = 0.0f;
    lfoFreq = 2.0f;
    lfoDepth = 1.0f;
    prevDepth = 1.0f;
    dc_os_ = 0.0f;

    osc.SetAmp(1.0f);

    flt.SetFreq(10000);
    flt.SetRes(0.2);

    setLfoTarget(1);
  }

  float Process()
  {
    float f = lfoFreq / 10000.0;

    // iterate oscillator
    sinZ = sinZ + f * cosZ;
    cosZ = cosZ - f * sinZ;
    // modsig2 = sinZ / 2 + 1.0;

    /* Vibrato */
    if (lfoTarget == 0)
    {
      modsig2 = lfoDepth * (oscFreq * 0.1) * sinZ / 2;
      osc.SetFreq(oscFreq + modsig2);
    }
    else
    {
      osc.SetFreq(oscFreq);
    }

    float sig = osc.Process();

    /* Tremolo */
    if (lfoTarget == 1)
    {
      modsig2 = sinZ / 2 + 1.0;
      sig = sig * (1 - lfoDepth) + (sig * modsig2) * lfoDepth;
    }

    if (waveform == Oscillator::WAVE_POLYBLEP_SAW)
    {
      sig *= 0.7;
    }
    if (waveform == Oscillator::WAVE_POLYBLEP_SQUARE)
    {
      sig *= 0.8;
    }

    sig = flt.Process(sig);

    return sig;
  }

  void setLfoTarget(int target)
  {
    lfoTarget = target;
  }

  void SetOscWaveform(uint8_t wf)
  {
    waveform = wf;
    osc.SetWaveform(waveform);
  }

  void SetOscFreq(float freq) { oscFreq = freq; }

  void SetLfoFreq(float freq)
  {
    lfoFreq = freq;
    /* Auto set depth based on freq */
    SetLfoDepth(fclamp(freq / 4, 0.f, 1.f));
  }

  void SetLfoDepth(float depth)
  {
    lfoDepth = depth * 0.05 + prevDepth * 0.95;
    prevDepth = lfoDepth;
  }

  void SetFilterCutoff(float freq) { flt.SetFreq(freq); }

  void SetAmp(float amp) { osc.SetAmp(amp); }
};
