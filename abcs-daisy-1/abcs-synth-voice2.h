#include "DaisyDuino.h"

float rangeToFilterFreq(int range)
{
  float exp = range * 11 / 128.0 + 5;
  exp = constrain(exp, 5, 14);
  return pow(2, exp);
}

bool isSaw(int wf)
{
  return wf == Oscillator::WAVE_POLYBLEP_SAW;
}

bool isSquare(int wf)
{
  return wf == Oscillator::WAVE_POLYBLEP_SQUARE;
}

class AbcsSynthVoice2
{
private:
  Oscillator osc;
  Oscillator osc2;
  // Oscillator osc3;
  // Svf flt;
  Tone flt;

  Line gainLine;

  float modsig2;
  float oscFreq;
  float oscFreq2;
  float realFreq;
  float realFreq2;
  float lfoFreq;
  float lfoDepth;
  float prevDepth;
  uint8_t waveform;
  uint8_t lfoTarget;
  float filterCutoff;
  float prevFilterCutoff;
  float harmonicMultiplier;

  float vibratoDepth;
  float gain;
  uint8_t gainLineFinished;

  /*
    efficient LFO: https://www.earlevel.com/main/2003/03/02/the-digital-state-variable-filter/
  */
  float sinZ = 0.0;
  float cosZ = 1.0;

  void SetOscFreq()
  {
    realFreq = oscFreq * harmonicMultiplier;
    realFreq2 = oscFreq2 * harmonicMultiplier;
    // float realFreq3 = mtof(57) * harmonicMultiplier;
    osc.SetFreq(realFreq);
    osc2.SetFreq(realFreq2);
    // osc3.SetFreq(realFreq3);
  }

public:
  AbcsSynthVoice2(){};
  ~AbcsSynthVoice2(){};

  void Init(float sample_rate)
  {
    osc.Init(sample_rate);
    osc2.Init(sample_rate);
    // osc3.Init(sample_rate);
    flt.Init(sample_rate);
    gainLine.Init(sample_rate);

    gain = 1.0f;
    modsig2 = 0.0f;
    oscFreq = 0.0f;
    oscFreq2 = 0.0f;
    realFreq = 0.0f;
    realFreq2 = 0.0f;
    lfoFreq = 0.0f;
    lfoDepth = 0.0f;
    prevDepth = 0.0f;
    filterCutoff = 15000;
    prevFilterCutoff = 15000;
    harmonicMultiplier = 1.f;

    vibratoDepth = lfoDepth * (realFreq * 0.05);

    osc.SetAmp(1.0f);
    osc2.SetAmp(1.0f);
    // osc3.SetAmp(1.0f);

    flt.SetFreq(filterCutoff);
    // flt.SetRes(0.3f);

    SetLfoTarget(1);
  }

  void Loop()
  {
    /* TODO: confirm working */
    filterCutoff = filterCutoff * 0.08 + prevFilterCutoff * 0.92;
    // filterCutoff = filterCutoff * 0.999999;
    flt.SetFreq(filterCutoff);
    prevFilterCutoff = filterCutoff;
  }

  float Process()
  {

    gain = gainLine.Process(&gainLineFinished);

    // float f = lfoFreq / 10000.0;

    /* TODO: maybe not needed in process? */
    // filterCutoff = filterCutoff * 0.08 + prevFilterCutoff * 0.92;
    // flt.SetFreq(filterCutoff);
    // prevFilterCutoff = filterCutoff;

    // iterate oscillator
    sinZ = sinZ + lfoFreq * cosZ;
    cosZ = cosZ - lfoFreq * sinZ;
    // float realFreq = oscFreq * harmonicMultiplier;

    /* Vibrato */
    if (lfoTarget == 0)
    {
      modsig2 = lfoDepth * (realFreq * 0.05) * sinZ / 2;
      osc.SetFreq(realFreq + modsig2);
      // modsig2 = 10 * sinZ;
      // osc.SetFreq(realFreq);
      osc2.SetFreq(realFreq2 + modsig2);
      // osc3.SetFreq(110 + modsig2);
    }

    float sig = osc.Process();
    float sig2 = oscFreq2 > 0 ? osc2.Process() : 0.f;
    // float sig3 = osc3.Process();

    float sum = (sig + sig2) / 2;

    sig = sum;

    /* Tremolo */
    if (lfoTarget == 1)
    {
      modsig2 = sinZ / 2 + 1.0;
      sig = sig * (1 - lfoDepth) + (sig * modsig2) * lfoDepth;
    }

    float sigOut = sig;

    /* Only filter saw and square */
    if (isSaw(waveform) || isSquare(waveform))
    {
      sigOut = flt.Process(sig);
    }

    // fonepole(currentDelay, delayTarget, .0002f);
    // fonepole(12000f, sig, 0.05f);
    return sigOut * gain;
  }

  void SetLfoTarget(int target)
  {
    lfoTarget = target;
  }

  void IncrementLfoTarget()
  {
    lfoTarget++;
    lfoTarget = (lfoTarget % NUM_LFO_TARGETS + NUM_LFO_TARGETS) % NUM_LFO_TARGETS;
    SetLfoTarget(lfoTarget);
  }

  void SetHarmonic(int harmonic)
  {
    harmonicMultiplier = float(constrain(harmonic, 1, 10));
    SetOscFreq();
  }

  void SetOscWaveform(uint8_t wf)
  {
    waveform = wf;
    osc.SetWaveform(waveform);
    osc2.SetWaveform(waveform);

    // adjust volumes
    if (isSaw(waveform))
    {
      osc.SetAmp(0.7F);
      osc2.SetAmp(0.7F);
    }

    if (isSquare(waveform))
    {
      osc.SetAmp(0.8F);
      osc2.SetAmp(0.8F);
    }
  }

  void SetFundamentalFreq(float freq, int target)
  {
    if (target == 0)
    {
      oscFreq = freq;
    }
    if (target == 1)
    {
      oscFreq2 = freq;
    }
    SetOscFreq();
  }

  void SetLfoFreq(float freq)
  {
    lfoFreq = freq / 10000.0;
    /* Auto set depth based on freq */
    SetLfoDepth(fclamp(freq / 4, 0.f, 1.f));
  }

  void SetLfoDepth(float depth)
  {
    lfoDepth = depth * 0.05 + prevDepth * 0.95;
    prevDepth = lfoDepth;
    vibratoDepth = lfoDepth * (realFreq * 0.05);
  }

  void SetFilterCutoff(float freq)
  {
    filterCutoff = freq;
    flt.SetFreq(freq);
  }

  void SetRange(int range)
  {
    float freq = rangeToFilterFreq(range);
    SetFilterCutoff(freq);

    /* Filter sawtooth and square waves */
    if (isSaw(waveform) || isSquare(waveform))
    {
      SetGain(1);
    }

    /* Set gain for sine and triangle */
    else
    {
      // SetFilterCutoff(15000);
      SetGain(range / 130.f);
    }

    if (range < 2)
    {
      SetGain(0);
    }
  }

  void SetAmp(float amp)
  {
    osc.SetAmp(amp);
    osc2.SetAmp(amp);
  }
  void SetGain(float targetGain) { gainLine.Start(gain, fclamp(targetGain, 0, 1), 0.2); }
};
