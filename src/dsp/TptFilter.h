// ==========================================
// TptFilter.h
// ==========================================
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// 状態変数（カテゴリファイルと共有）
#include "TptFilterState.h"

// カテゴリ実装
#include "TptFilter/SVF/TptFilter_SVF.h"
#include "TptFilter/Ladder/TptFilter_Ladder.h"
#include "TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.h"
#include "TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.h"
#include "TptFilter/Spectral/TptFilter_Spectral.h"
#include "TptFilter/Experimental/TptFilter_Experimental.h"

class TptFilter
{
public:
    TptFilter();
    ~TptFilter() = default;

    void prepare(double newSampleRate, int samplesPerBlock, int numChannels);
    void reset();
    void setLerp(int samples);

    void setModel(int newModel);
    void setCutoff(float newCutoff);
    void setResonance(float newResonance);
    void setType(int newType);
    void setSlope(int slopeIndex);

    float processMono(float in);
    void process(juce::AudioBuffer<float>& buffer);
    float getMagnitudeForFrequency(float frequency) const;

    // Oversampling API
    void setOsMode(int mode);
    int  getOsLatencySamples() const;

    void  updateCoefficients();

private:
    // ===== DSP 状態（カテゴリファイルが参照）=====
    TptFilterState state;

    // ===== TptFilter 固有（カテゴリファイルに非公開）=====
    int    maxChannels = 2;
    int    preparedBlockSize = 512;

    juce::SmoothedValue<float> cutoff;
    juce::SmoothedValue<float> resonance;
    float lastCutoff = -1.0f;
    float lastRes = -1.0f;

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    int currentOsFactor = 0;
    int osMode = 0;

    // ===== 内部メソッド =====
    float processSample(int channel, float x);

    int  getAutoOsFactor(int modelIdx) const;
    void rebuildOversampler(int newFactor);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TptFilter)
};