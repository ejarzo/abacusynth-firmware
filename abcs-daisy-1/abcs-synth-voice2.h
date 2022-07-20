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
  Oscillator oscillators[NUM_POLY_VOICES];

  float oscFreqs[NUM_POLY_VOICES];
  float realFreqs[NUM_POLY_VOICES];
  float modSigs[NUM_POLY_VOICES];

  // Svf flt;
  Tone flt;
  Line gainLine;

  float oscFreq;
  float oscFreq2;
  float realFreq;
  float realFreq2;

  float lfoFreq;
  float lfoDepth;
  float prevDepth;

  uint8_t waveform;
  uint8_t lfoTarget;
  uint8_t harmonicMultiplier;

  float filterCutoff;
  float prevFilterCutoff;

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
    for (size_t i = 0; i < NUM_POLY_VOICES; i++)
    {
      float fq = oscFreqs[i] * harmonicMultiplier;
      realFreqs[i] = fq;
      modSigs[i] = (fq * 0.05) / 2;
      // modSigs[i] = lfoDepth * (fq * 0.05) / 2;
      oscillators[i].SetFreq(fq);
    }
  }

public:
  AbcsSynthVoice2(){};
  ~AbcsSynthVoice2(){};

  void Init(float sample_rate)
  {
    for (size_t i = 0; i < NUM_POLY_VOICES; i++)
    {
      oscFreqs[i] = 0.0f;
      realFreqs[i] = 0.0f;
      oscillators[i].Init(sample_rate);
      oscillators[i].SetAmp(1.0f);
    }

    flt.Init(sample_rate);
    gainLine.Init(sample_rate);

    gain = 1.0f;
    lfoFreq = 0.0f;
    lfoDepth = 0.0f;
    prevDepth = 0.0f;

    filterCutoff = 15000;
    prevFilterCutoff = 15000;

    harmonicMultiplier = 1;

    // vibratoDepth = lfoDepth * (realFreq * 0.05);

    flt.SetFreq(filterCutoff);
    SetLfoTarget(1);
  }

  void Loop()
  {
    /* TODO: confirm working */
    filterCutoff = filterCutoff * 0.08 + prevFilterCutoff * 0.92;
    flt.SetFreq(filterCutoff);
    prevFilterCutoff = filterCutoff;
  }

  float Process(float amps[NUM_POLY_VOICES])
  {
    // iterate oscillator
    sinZ = sinZ + lfoFreq * cosZ;
    cosZ = cosZ - lfoFreq * sinZ;

    // if (!gainLineFinished)
    // {
    gain = gainLine.Process(&gainLineFinished);
    // }

    float sum = 0.0f;

    for (size_t i = 0; i < NUM_POLY_VOICES; i++)
    {
      /* Vibrato */
      if (lfoTarget == 0)
      {
        float fq = realFreqs[i];
        float modSig = lfoDepth * modSigs[i] * sinZ;
        oscillators[i].SetFreq(fq + modSig);
        /* Todo reset freq (once) if not vibrato */
      }
      sum += oscillators[i].Process() * amps[i];
    }

    float sig = sum / NUM_POLY_VOICES;

    /* Tremolo */
    if (lfoTarget == 1)
    {
      float modSig = sinZ / 2 + 1.0;
      sig = sig * (1 - lfoDepth) + (sig * modSig) * lfoDepth;
    }

    float sigOut = sig;

    /* Only filter saw and square */
    if (isSaw(waveform) || isSquare(waveform))
    {
      sigOut = flt.Process(sig);
    }

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
    for (size_t i = 0; i < NUM_POLY_VOICES; i++)
    {
      oscillators[i].SetWaveform(waveform);

      // adjust volumes
      if (isSaw(waveform))
      {
        oscillators[i].SetAmp(0.7F);
      }

      if (isSquare(waveform))
      {
        oscillators[i].SetAmp(0.8F);
      }
    }
  }

  void SetFundamentalFreq(float freq, int target)
  {
    oscFreqs[target] = freq;
    // if (target == 0)
    // {
    //   oscFreq = freq;
    // }
    // if (target == 1)
    // {
    //   oscFreq2 = freq;
    // }
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
    // vibratoDepth = lfoDepth * (realFreq * 0.05);
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
    for (size_t i = 0; i < NUM_POLY_VOICES; i++)
    {
      oscillators[i].SetAmp(amp);
    }
    // osc.SetAmp(amp);
    // osc2.SetAmp(amp);
  }
  void SetGain(float targetGain) { gainLine.Start(gain, fclamp(targetGain, 0, 1), 0.2); }
};
