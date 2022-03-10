#include "DaisyDuino.h"

class AbcsSynthVoice2
{
private:
  Oscillator osc;
  Oscillator lfo;
  MoogLadder flt;

  float modsig;
  float oscFreq;
  float lfoFreq;
  float lfoDepth;
  float dc_os_;
  uint8_t waveform;
  uint8_t lfoTarget;

public:
  AbcsSynthVoice2(){};
  ~AbcsSynthVoice2(){};

  void Init(float sample_rate)
  {
    osc.Init(sample_rate);
    lfo.Init(sample_rate);
    flt.Init(sample_rate);

    modsig = 0.0f;
    oscFreq = 0.0f;
    lfoFreq = 2.0f;
    lfoDepth = 1.0f;
    dc_os_ = 0.0f;

    osc.SetAmp(1.0f);

    lfo.SetWaveform(lfo.WAVE_SIN);
    lfo.SetFreq(2.0f);

    flt.SetFreq(10000);
    flt.SetRes(0.2);

    setLfoTarget(1);
  }

  float Process()
  {
    float lfoVal = lfo.Process();

    /* Vibrato */
    if (lfoTarget == 0)
    {
      modsig = dc_os_ - 1.0 + lfoVal * (oscFreq * 0.1);
      osc.SetFreq(oscFreq + modsig);
    }
    else
    {
      osc.SetFreq(oscFreq);
    }

    float sig = osc.Process();

    /* Tremolo */
    if (lfoTarget == 1)
    {
      modsig = dc_os_ + lfoVal;
      sig *= modsig;
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
    lfo.SetFreq(lfoFreq);
  }

  void SetLfoDepth(float depth)
  {
    depth = fclamp(depth, 0.f, 1.f);
    lfoDepth = depth;
    depth *= .5f;
    lfo.SetAmp(depth);
    dc_os_ = 1.f - depth;
  }

  void SetFilterCutoff(float freq) { flt.SetFreq(freq); }

  void SetAmp(float amp) { osc.SetAmp(amp); }
};

// Oscillator AbcsSynthVoice2::osc;
// Oscillator AbcsSynthVoice2::lfo;
// MoogLadder AbcsSynthVoice2::flt;