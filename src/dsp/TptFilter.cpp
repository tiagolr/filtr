// ==========================================
// TptFilter.cpp
// Dispatcher + 共通処理
// ==========================================
#include "TptFilter.h"
#include <cmath>
#include <algorithm>
// TptFilter.cpp の先頭（他の #include の後）に一時的に追加して確認
// #include <xsimd/xsimd.hpp>

// ==========================================
// Constructor
// ==========================================
TptFilter::TptFilter()
{
    cutoff.reset(state.sampleRate, 0.01);
    resonance.reset(state.sampleRate, 0.01);
    cutoff.setCurrentAndTargetValue(1000.0f);
    resonance.setCurrentAndTargetValue(0.707f);
    state.dpCoeffs.resize(8);
    state.zplaneCoeffs.resize(7);
}

void TptFilter::setLerp(int samples)
{
    cutoff.reset(state.sampleRate, samples / state.sampleRate);
    resonance.reset(state.sampleRate, samples / state.sampleRate);
}

// ==========================================
// prepare
// ==========================================
void TptFilter::prepare(double newSampleRate, int samplesPerBlock, int numChannels)
{
    state.sampleRate = newSampleRate;
    maxChannels = juce::jmin(numChannels, 2);
    preparedBlockSize = samplesPerBlock;

    cutoff.reset(state.sampleRate, 0.01);
    resonance.reset(state.sampleRate, 0.01);
    state.smoothedDigitalCutoff = cutoff.getCurrentValue();

    if (oversampler != nullptr)
    {
        oversampler->initProcessing(static_cast<size_t>(samplesPerBlock));
        oversampler->reset();
    }

    reset();
}

// ==========================================
// reset
// ==========================================
void TptFilter::reset()
{
    // SVF
    for (int stage = 0; stage < 8; ++stage)
        for (int ch = 0; ch < 2; ++ch) {
            state.s1[stage][ch] = 0.0f; state.s2[stage][ch] = 0.0f;
            state.sk_s1[stage][ch] = 0.0f; state.sk_s2[stage][ch] = 0.0f;
            state.ms20_dc_x1[stage][ch] = 0.0f; state.ms20_dc_y1[stage][ch] = 0.0f;
            state.dp_s1[stage][ch] = 0.0f; state.dp_s2[stage][ch] = 0.0f;
            for (int pole = 0; pole < 4; ++pole) state.zdfState[stage][ch][pole] = 0.0f;
            state.combWriteIdx[stage][ch] = 0; state.comb_ap_state[stage][ch] = 0.0f;
            for (int i = 0; i < 16384; ++i) state.combBuffer[stage][ch][i] = 0.0f;
            state.pa_s[stage][ch] = 0.0f;
        }

    // Phased Array 拡張分 (stage 8〜15) のクリア
    // pa_s を [16][2] に拡張したが上のループは stage<8 まで。
    // 残り 8 スロットを明示的にゼロ初期化してゴミデータを防ぐ。
    for (int stage = 8; stage < 16; ++stage)
        for (int ch = 0; ch < 2; ++ch)
            state.pa_s[stage][ch] = 0.0f;

    // Phaser / Kilo
    for (int stage = 0; stage < 16; ++stage)
        for (int ch = 0; ch < 2; ++ch) {
            state.ap_s[stage][ch] = 0.0f;
            state.kilo_s1[stage][ch] = 0.0f; state.kilo_s2[stage][ch] = 0.0f;
        }

    // Z-Plane
    for (int stage = 0; stage < 7; ++stage) {
        for (int ch = 0; ch < 2; ++ch) {
            state.zplane_s1[stage][ch] = 0.0f; state.zplane_s2[stage][ch] = 0.0f;
        }
        state.zp_fc[stage] = 1000.0f; state.zp_q[stage] = 0.707f;
    }

    // FDN Reverb
    for (int s = 0; s < 4; ++s)
        for (int ch = 0; ch < 2; ++ch) {
            state.fdnWriteIdx[s][ch] = 0; state.fdn_ap_state[s][ch] = 0.0f;
            state.fdnLpState[s][ch] = 0.0f;
            for (int i = 0; i < 16384; ++i) state.fdnBuffer[s][ch][i] = 0.0f;
        }

    // Per-channel
    for (int ch = 0; ch < 2; ++ch) {
        state.rmsIn[ch] = 0.0f; state.rmsOut[ch] = 0.0f; state.agcGain[ch] = 1.0f;
        state.srrPhase[ch] = 0.0f; state.srrHeld[ch] = 0.0f;
        state.ap_out_prev[ch] = 0.0f;
        state.lpgEnv[ch] = 0.0f; state.bodePhase[ch] = 0.0f;
        state.bodeOutPrev[ch] = 0.0f;
        state.wgWriteIdx[ch] = 0; state.wg_ap_state[ch] = 0.0f;
        for (int i = 0; i < 16384; ++i) state.wgBuffer[ch][i] = 0.0f;
        for (int f = 0; f < 3; ++f) { state.form_s1[f][ch] = 0.0f; state.form_s2[f][ch] = 0.0f; }
        for (int b = 0; b < TptFilterState::numModalBands; ++b) {
            state.modal_s1[b][ch] = 0.0f; state.modal_s2[b][ch] = 0.0f;
        }
        for (int h = 0; h < 4; ++h) {
            state.hilbertStateA[h][ch] = 0.0f; state.hilbertStateB[h][ch] = 0.0f;
        }
        // ===== Nyquist Anti-alias: 最大8段分クリア =====
        for (int a = 0; a < 8; ++a) {
            state.aa_s1[a][ch] = 0.0f;
            state.aa_s2[a][ch] = 0.0f;
        }

        // ===== 既存コード =====
        state.diodeHpfS[ch] = 0.0f;

        // ===== 【新規追加】=====
        state.diodePrevY4[ch] = 0.0f;

        // ===== 既存コード（続き）=====
    }  // for (int ch...)

    // ===== Wavefolder ADAA (Model 9) =====
    // wf_F_z1 の正しい初期値は -cos(0) = -1.0。
    // ゼロ初期化（= {}）では誤りで、最初のサンプルで差分商が誤った値になりクリックが発生する。
    for (int s = 0; s < 8; ++s)
        for (int c = 0; c < 2; ++c) {
            state.wf_x_z1[s][c] = 0.0;
            state.wf_F_z1[s][c] = -1.0;  // -cos(0) = -1.0
        }

    // lastCutoff = -1.0f は「強制再計算」のセンチネルとして安全に使える。
    // しかし lastRes = -1.0f はスムージング計算に直接使われるため危険:
    //   currentRes = lastRes + coef*(target - lastRes) → 初回に負値になる
    //   → R = 1/(2 * negative) < 0 → SVF が正帰還発散 → state が inf になり
    //   PluginDoctor 等の線形解析ツールに「カーブなし」として見える。
    // 修正: lastRes は resonance の現在値（常に正）で初期化する。
    lastCutoff = -1.0f;
    lastRes = resonance.getCurrentValue();
    state.smoothedDigitalCutoff = cutoff.getCurrentValue();

    if (oversampler != nullptr)
        oversampler->reset();
}

// ==========================================
// Parameter Setters
// ==========================================
void TptFilter::setModel(int newModel)
{
    if (state.filterModel == newModel) return;
    state.filterModel = newModel;
    lastCutoff = -1.0f;
    if (osMode == 1)
        rebuildOversampler(getAutoOsFactor(newModel));
}

void TptFilter::setCutoff(float newCutoff)
{
    cutoff.setTargetValue(juce::jlimit(20.0f, 20000.0f, newCutoff));
}

void TptFilter::setResonance(float newResonance)
{
    resonance.setTargetValue(juce::jmax(0.1f, newResonance));
}

void TptFilter::setType(int newType)
{
    state.filterType = newType;
    // Model 11 (Phase Shift): filterType がスプレッドパターンを決定するため、
    // Type 変更時に updateCoefficients の早期 return を無効化して
    // updateCoeffs (kilo_g/R/h の再計算) を強制する。
    if (state.filterModel == 11)
        lastCutoff = -1.0f;
}

void TptFilter::setSlope(int index)
{
    // slopeIdx が変わった場合（例: Accent Off→Low→High の切り替え）は
    // lastCutoff を無効化し、次の updateCoefficients で係数再計算を強制する。
    // ※ updateCoefficients は cutoff/res 変化がなければ早期 return するため、
    //   slopeIdx 変更だけでは TptFilter_Ladder::updateCoeffs が呼ばれない。
    if (state.slopeIdx != index)
        lastCutoff = -1.0f;

    // ===== TB-303 は Accent なので、変換は不要（index は常に 0,1,2）=====
    state.slopeIdx = index;  // 全モデル共通で、変換なし
    const int m = state.filterModel;

    if (m == 5 || m == 10 || m == 21 || m == 22 || m == 24 || m == 25)
    {
        // これらのモデルはStageをDSP内部で独自管理（slopeIdxは別の意味に転用）
        state.currentStages = 1; state.filterOrder = 2;
    }
    else if (m == 27)
    {
        // Nyquist Anti-alias: Slope = Stage数（急峻さ）
        //   0→2段(24dB/oct), 1→4段(48dB/oct), 2→6段(72dB/oct), 3→8段(96dB/oct)
        const int newStages = (index == 0) ? 2 : (index == 1) ? 4 : (index == 2) ? 6 : 8;

        // Stage数が実際に変わった場合のみ内部ステートをクリア。
        // ※ setSlope() は processBlock から毎ブロック呼ばれるため、
        //   無条件クリアにすると毎ブロックフィルターステートがリセットされ
        //   Model 27 がまともに機能しなくなる。必ず変化検出を挟むこと。
        if (newStages != state.currentStages)
        {
            state.currentStages = newStages;
            for (int s = 0; s < 8; ++s)
                for (int ch = 0; ch < 2; ++ch) {
                    state.aa_s1[s][ch] = 0.0f;
                    state.aa_s2[s][ch] = 0.0f;
                }
        }
        state.filterOrder = state.currentStages * 2;
    }
    else if (m == 8 || m == 11 || m == 26)
    {
        state.currentStages = (index == 0) ? 2 : (index == 1) ? 4 : (index == 2) ? 8 : 16;
        state.filterOrder = state.currentStages * 2;
    }
    else if (m >= 17 && m <= 20)
    {
        state.filterOrder = (index == 0) ? 2 : (index == 1) ? 4 : (index == 2) ? 8 : 16;
        state.currentStages = state.filterOrder / 2;
    }
    else if (m == 23)
    {
        // Waveguide: Slope → 反射段数 (1x/2x/4x/8x)
        // 段数変化時は combBuffer (遅延ライン) をクリアして過去の残響をフラッシュ。
        // Model 6 (Comb) と combBuffer を共用するため、同様のクリア処理が必要。
        // ※ setSlope() は毎ブロック呼ばれるが newStages != state.currentStages は
        //   ユーザー操作時のみ真となるため、高頻度クリアは発生しない。
        const int newStages = (index == 0) ? 1 : (index == 1) ? 2 : (index == 2) ? 4 : 8;
        if (newStages != state.currentStages)
        {
            state.currentStages = newStages;
            for (int s = 0; s < 8; ++s)
                for (int ch = 0; ch < 2; ++ch) {
                    state.combWriteIdx[s][ch] = 0;
                    state.comb_ap_state[s][ch] = 0.0f;
                    std::fill(std::begin(state.combBuffer[s][ch]),
                              std::end  (state.combBuffer[s][ch]), 0.0f);
                    state.s1[s][ch] = 0.0f;
                    state.s2[s][ch] = 0.0f;
                }
        }
        state.filterOrder = state.currentStages * 2;
    }
    else if (m == 0 || m == 3 || m == 4 || m == 6 || m == 7 || m == 9 ||
        m == 14 || m == 16)
    {
        state.currentStages = (index == 0) ? 1 : (index == 1) ? 2 : (index == 2) ? 4 : 8;
        state.filterOrder = state.currentStages * 2;
    }
    else if (m == 2)
    {
        state.currentStages = 1;
        state.filterOrder = 4;
    }
    else
    {
        state.currentStages = (index == 0) ? 1 : (index == 1) ? 1 : (index == 2) ? 2 : 4;
        state.filterOrder = state.currentStages * 4;
    }
    state.scalerSVF = (state.currentStages > 1)
        ? std::pow(0.6f, std::log2((float)state.currentStages)) : 1.0f;
    state.scalerMoog = (state.currentStages > 1)
        ? std::pow(0.7f, std::log2((float)state.currentStages)) : 1.0f;
    state.scalerDiode = (state.currentStages > 1)
        ? std::pow(0.7f, std::log2((float)state.currentStages)) : 1.0f;
    lastCutoff = -1.0f;
}

// ==========================================
// Oversampling
// ==========================================
void TptFilter::setOsMode(int mode)
{
    mode = juce::jlimit(0, 3, mode);
    if (mode == osMode) return;
    osMode = mode;
    int newFactor = 0;
    switch (osMode) {
    case 0: newFactor = 0;                                       break;
    case 1: newFactor = getAutoOsFactor(state.filterModel);      break;
    case 2: newFactor = 1;                                       break;
    case 3: newFactor = 2;                                       break;
    }
    rebuildOversampler(newFactor);
}

int TptFilter::getOsLatencySamples() const
{
    return (oversampler != nullptr)
        ? static_cast<int>(oversampler->getLatencyInSamples()) : 0;
}

int TptFilter::getAutoOsFactor(int m) const
{
    if (m == 9) return 2;
    if (m == 1 || m == 2 || m == 3 || m == 4 || m == 6 || m == 7 ||
        m == 10 || m == 12 || m == 13 || m == 14 || m == 15 || m == 16) return 1;
    // Model 10 (FDN Reverb): SVF プリフィルターの共鳴エイリアシング低減のため 2× OS
    return 0;
}

void TptFilter::rebuildOversampler(int newFactor)
{
    if (newFactor == currentOsFactor && oversampler != nullptr) return;
    currentOsFactor = newFactor;
    if (newFactor == 0) { oversampler.reset(); lastCutoff = -1.0f; return; }

    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        maxChannels, newFactor,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
    oversampler->initProcessing(static_cast<size_t>(preparedBlockSize));
    oversampler->reset();
    lastCutoff = -1.0f;
}

void TptFilter::updateCoefficients()
{
    float targetCutoff = cutoff.getTargetValue();
    float targetRes = resonance.getTargetValue();

    float smoothCoef = 1.0f - std::exp(-1.0f / (0.01f * (float)state.sampleRate));
    state.smoothedDigitalCutoff += smoothCoef * (targetCutoff - state.smoothedDigitalCutoff);
    float currentRes = lastRes + smoothCoef * (targetRes - lastRes);

    if (std::abs(state.smoothedDigitalCutoff - lastCutoff) < 0.01f &&
        std::abs(currentRes - lastRes) < 0.001f)
        return;

    state.currentResVal = currentRes;  // ← この行を追加
    state.currentCutoffVal = cutoff.getCurrentValue();
    state.currentResVal = currentRes;

    const int m = state.filterModel;

    if (m == 5 || m == 22)
        TptFilter_AnalogEmulation::updateCoeffs(state);
    else if (m >= 17 && m <= 20)
        TptFilter_DigitalPrecision::updateCoeffs(state);
    else if (m == 11 || m == 25)
        TptFilter_Spectral::updateCoeffs(state);
    else if (m == 2)
        TptFilter_Ladder::updateCoeffs(state);
    else
    {
        float maxSafeFreq = (float)state.sampleRate * 0.45f;
        float safeCutoff = std::clamp(state.smoothedDigitalCutoff, 20.0f, maxSafeFreq);
        float wd = juce::MathConstants<float>::pi * safeCutoff / (float)state.sampleRate;
        state.g = std::tan(wd);
        state.ladderG = state.g / (1.0f + state.g);

        if (m == 0 || m == 3 || m == 4 || m == 6 || m == 7 || m == 9 ||
            m == 14 || m == 16 || m == 21 || m == 23 || m == 26 || m == 27)
        {
            float adjRes = currentRes * ((m != 7 && m != 14 && m != 21) ? state.scalerSVF : 1.0f);
            state.R = 1.0f / (2.0f * adjRes);
            state.h = 1.0f / (1.0f + 2.0f * state.R * state.g + state.g * state.g);
        }
        else if (m == 10)
        {
            // FDN Reverb: SVF プリフィルター用係数（固定 Q=1.2、Cutoff が filter 周波数）
            // Res は Decay（フィードバック量）として使用するため Q には反映しない
            state.R = 1.0f / (2.0f * 1.2f);
            state.h = 1.0f / (1.0f + 2.0f * state.R * state.g + state.g * state.g);
        }

        // Model 4 Bitcrush: ビット深度から量子化ステップ数を事前計算してキャッシュ。
        // processSample() 内で std::pow を呼ぶとリアルタイム安全性が損なわれるため、
        // updateCoefficients()（係数変化時のみ呼ばれる）でここで一度だけ計算する。
        if (m == 4)
        {
            float bits = juce::jmap(currentRes, 0.1f, 10.0f, 16.0f, 2.0f);
            state.srrBitSteps = std::pow(2.0f, bits);
        }
        else if (m == 1 || m == 12 || m == 13 || m == 15)
        {
            // 【修正】r_scale: Moog=4.5 に引き上げ（発振閾値確保）
            float r_scale = (m == 13) ? 5.0f : 4.5f;
            state.ladderRes = juce::jmap(currentRes, 0.1f, 10.0f, 0.0f, r_scale)
                * state.scalerMoog;
        }
    }

    lastCutoff = state.smoothedDigitalCutoff;
    lastRes = currentRes;
}

float TptFilter::processSample(int ch, float x)
{
    const int m = state.filterModel;

    state.currentCutoffVal = cutoff.getCurrentValue();


    // ===== 共通前処理: comp =====
    float comp = 1.0f;
    if (m == 0 || m == 4 || m == 5 || m == 6 || m == 9 || m == 11 || m == 16 || m == 26)
        comp = 1.0f + state.currentResVal * 0.1f;
    else if (m == 1 || m == 12 || m == 13 || m == 15)
        comp = 1.0f + 0.5f * state.ladderRes;
    // 修正後
    else if (m == 2) {
        float k_max = 4.2f;
        if (state.slopeIdx == 1) k_max = 4.6f;
        else if (state.slopeIdx == 2) k_max = 5.0f;
        float k_val = juce::jmap(state.currentResVal, 0.707f, 10.0f, 0.0f, k_max);
        comp = 1.0f + 0.2f * k_val;
    }

    else if (m == 7)
        comp = 1.0f + state.currentResVal * 0.15f;

    // FIX - for some reason comp is broken, makes the output louder
    //x *= comp;

    // ===== Bitcrush / SRR: 前処理 =====
    if (m == 4)
    {
        float phaseInc = state.currentCutoffVal / (float)state.sampleRate;
        state.srrPhase[ch] += phaseInc;
        if (state.srrPhase[ch] >= 1.0f)
        {
            state.srrPhase[ch] -= std::floor(state.srrPhase[ch]);
            // srrBitSteps は updateCoefficients() で事前計算済み（std::pow をここで呼ばない）
            state.srrHeld[ch] = std::round(x * state.srrBitSteps) / state.srrBitSteps;
        }
        x = state.srrHeld[ch];
    }

    // ===== RMS 入力 =====
    state.rmsIn[ch] = (1.0f - 0.005f) * state.rmsIn[ch] + 0.005f * (x * x);

    // ===== カテゴリ dispatch =====
    float out = 0.0f;
    if (m == 0 || m == 3 || m == 4 || m == 6 || m == 7 || m == 9 || m == 14 || m == 16 || m == 23)
        out = TptFilter_SVF::processSample(ch, x, state);
    else if (m == 1 || m == 2 || m == 12 || m == 13 || m == 15)
        out = TptFilter_Ladder::processSample(ch, x, state);
    else if (m == 5 || m == 21 || m == 22 || m == 27)
        out = TptFilter_AnalogEmulation::processSample(ch, x, state);
    else if (m >= 17 && m <= 20)
        out = TptFilter_DigitalPrecision::processSample(ch, x, state);
    else if (m == 8 || m == 11 || m == 24 || m == 25 || m == 26)
        out = TptFilter_Spectral::processSample(ch, x, state);
    else if (m == 10)
        out = TptFilter_Experimental::processSample(ch, x, state);
    else
        out = x;

    // ===== RMS 出力 + AGC =====
// ===== ここから processSample の末尾全体を置き換え =====
    state.rmsOut[ch] = (1.0f - 0.005f) * state.rmsOut[ch] + 0.005f * (out * out);

    float targetGain = 1.0f;
    if (state.rmsOut[ch] > 1e-8f) {
        targetGain = std::sqrt((state.rmsIn[ch] + 1e-8f) / (state.rmsOut[ch] + 1e-8f));
        targetGain = juce::jlimit(0.1f, 15.0f, targetGain);
    }
    state.agcGain[ch] = (1.0f - 0.0005f) * state.agcGain[ch] + 0.0005f * targetGain;

    float finalGain = state.agcGain[ch];

    // ===== model 0 (CleanSVF): AGC バイパス =====
    // CleanSVF は線形フィルターなので AGC は不要。
    // AGC がゲインを時変にすると PluginDoctor などの LTI 解析ツールが
    // 逆畳み込みに失敗し「カーブなし」になる。
    // state の更新（rmsOut / agcGain）は継続することで、
    // 他モデルへ切り替えた際の不連続を防ぐ。
    if (m == 0)
    {
        finalGain = 1.0f;
    }
    else if (m == 1 || m == 2 || m == 12 || m == 13 || m == 15)
    {
        float k_norm;
        if (m == 2) {
            k_norm = juce::jmap(state.currentResVal, 0.707f, 10.0f, 0.0f, 1.0f);
        }
        else {
            float r_scale = (m == 13) ? 5.0f : 4.5f;
            k_norm = (r_scale > 0.0f) ? (state.ladderRes / r_scale) : 0.0f;
        }
        k_norm = juce::jlimit(0.0f, 1.0f, k_norm);

        if (k_norm > 0.85f) {
            float bypassFactor = juce::jlimit(0.0f, 1.0f, (k_norm - 0.85f) * 10.0f);
            state.agcGain[ch] = state.agcGain[ch] * (1.0f - bypassFactor * 0.01f)
                + 1.0f * (bypassFactor * 0.01f);
            finalGain = state.agcGain[ch] * (1.0f - bypassFactor)
                + 1.0f * bypassFactor;
        }
    }
    return out * finalGain;
}  // processSample の閉じ括弧


float TptFilter::processMono(float x)
{
    return processSample(0, x);
}

   // ==========================================
// process（OS 対応）
// ==========================================
void TptFilter::process(juce::AudioBuffer<float>& buffer)
{
    int numSamples = buffer.getNumSamples();
    int numChannels = juce::jmin(buffer.getNumChannels(), maxChannels);

    if (oversampler == nullptr || currentOsFactor == 0)
    {
        float* wp[2] = { nullptr, nullptr };
        for (int ch = 0; ch < numChannels; ++ch) wp[ch] = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i) {
            if (i % 16 == 0) updateCoefficients();
            for (int ch = 0; ch < numChannels; ++ch)
                wp[ch][i] = processSample(ch, wp[ch][i]);
        }
        return;
    }

    juce::dsp::AudioBlock<float> block(buffer);
    auto oversampledBlock = oversampler->processSamplesUp(block);

    int osN = static_cast<int>(oversampledBlock.getNumSamples());
    int osCh = juce::jmin(static_cast<int>(oversampledBlock.getNumChannels()), maxChannels);

    double savedRate = state.sampleRate;
    state.sampleRate = savedRate * std::pow(2.0, static_cast<double>(currentOsFactor));
    lastCutoff = -1.0f;

    for (int i = 0; i < osN; ++i) {
        if (i % 16 == 0) updateCoefficients();
        for (int ch = 0; ch < osCh; ++ch) {
            float* p = oversampledBlock.getChannelPointer(ch);
            p[i] = processSample(ch, p[i]);
        }
    }

    state.sampleRate = savedRate;
    lastCutoff = -1.0f;
    oversampler->processSamplesDown(block);
}

// ==========================================
// getMagnitudeForFrequency（変更なし・state経由に更新）
// ==========================================
float TptFilter::getMagnitudeForFrequency(float frequency) const
{
    float fc = cutoff.getCurrentValue();
    float res = resonance.getCurrentValue();
    float w = frequency / juce::jlimit(20.0f, 20000.0f, fc);
    float w2 = w * w;
    float mag = 1.0f;
    const int m = state.filterModel;

    if (m == 0 || m == 3 || m == 4 || m == 9 || m == 14 || m == 16) {
        float adjustedRes = res * ((m != 14) ? state.scalerSVF : 1.0f);
        float d = 1.0f / juce::jlimit(0.1f, 10.0f, adjustedRes);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (state.filterType == 1) mag *= w; else if (state.filterType == 2) mag *= w2;
        else if (state.filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, state.currentStages);
    }
    else if (m >= 17 && m <= 20) {
        float mag_total = 1.0f;
        for (int k = 0; k < state.currentStages; ++k) {
            if (k >= (int)state.dpCoeffs.size()) break;
            float cur_g = state.dpCoeffs[k].g; float cur_R = state.dpCoeffs[k].R;
            float stage_fc = std::atan(cur_g) * (float)state.sampleRate / juce::MathConstants<float>::pi;
            float sw = frequency / juce::jlimit(20.0f, 20000.0f, stage_fc);
            float sw2 = sw * sw;
            float den = std::sqrt(std::pow(1.0f - sw2, 2.0f) + std::pow(sw * 2.0f * cur_R, 2.0f));
            float mm = 1.0f / den;
            if (m == 20) { mm = std::abs(1.0f - sw2 * 0.5f) / den; }
            else {
                if (state.filterType == 1) mm *= sw; else if (state.filterType == 2) mm *= sw2;
                else if (state.filterType == 3) mm *= std::abs(1.0f - sw2);
            }
            mag_total *= mm;
        }
        return mag_total;
    }
    else if (m == 11) {
        // Phase Shift: updateCoeffs と同一の4パターン (Type 0-3)
        static constexpr float randOffsets[16] = {
             0.000f,  0.618f, -0.382f,  0.854f,
            -0.146f,  0.472f, -0.764f,  0.236f,
             0.944f, -0.528f,  0.090f, -0.910f,
             0.708f, -0.292f,  0.562f, -0.438f
        };
        const float spread  = juce::jmap(res, 0.1f, 10.0f, 0.0f, 2.5f);
        const float q       = 0.5f + (spread * 2.0f);
        const float d       = 1.0f / q;
        const float maxSafe = (float)state.sampleRate * 0.45f;
        float mag_total = 1.0f;
        for (int k = 0; k < state.currentStages; ++k) {
            float st_fc = fc;
            switch (state.filterType) {
                case 0: { const float off = (state.currentStages > 1) ? ((float)k / (state.currentStages - 1)) * 2.0f - 1.0f : 0.0f;
                          st_fc = fc + off * fc * spread * 0.5f; break; }
                case 1: { const float off = (state.currentStages > 1) ? ((float)k / (state.currentStages - 1)) * 2.0f - 1.0f : 0.0f;
                          st_fc = fc * std::pow(2.0f, off * spread); break; }
                case 2: { const float sign = (k % 2 == 0) ? 1.0f : -1.0f;
                          const float magK = (state.currentStages > 1) ? (float)((k/2)+1)/(float)((state.currentStages+1)/2) : 0.0f;
                          st_fc = fc * std::pow(2.0f, sign * magK * spread); break; }
                default:{ st_fc = fc * std::pow(2.0f, randOffsets[k % 16] * spread); break; }
            }
            st_fc = std::clamp(st_fc, 20.0f, maxSafe);
            float sw = frequency / st_fc; float sw2 = sw * sw;
            float den = std::sqrt(std::pow(1.0f - sw2, 2.0f) + std::pow(sw * d, 2.0f));
            float bp = (1.0f / den) * sw;
            mag_total *= (1.0f + std::pow(bp, 1.2f) * res * 0.1f);
        }
        return mag_total;
    }
    else if (m == 10) {
        // FDN Reverb: SVF プリフィルター × FDN コム応答 (Visualizer と同一ロジック)
        static constexpr float base_ms_tab[4] = { 20.0f, 60.0f, 120.0f, 30.0f };
        const float base_ms_g = base_ms_tab[juce::jlimit(0, 3, state.slopeIdx)];

        // SVF プリフィルター
        const float d_g   = 1.0f / 1.2f;
        const float den_g = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d_g, 2.0f));
        float svf_g;
        if      (state.filterType == 0) svf_g = 1.0f / den_g;
        else if (state.filterType == 1) svf_g = (w * d_g) / den_g;
        else if (state.filterType == 2) svf_g = w2 / den_g;
        else                            svf_g = 1.0f;   // Open: flat

        // FDN コム応答
        const float fb_g   = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.98f);
        const float baseD_g = base_ms_g / 1000.0f;
        float fdn_total = 0.0f;
        for (int i = 0; i < 4; ++i) {
            float wD_g = 2.0f * juce::MathConstants<float>::pi * frequency
                              * (baseD_g * state.fdnDelayTimes[i]);
            fdn_total += 1.0f / std::sqrt(juce::jmax(0.001f,
                1.0f + fb_g*fb_g - 2.0f*fb_g*std::cos(wD_g)));
        }
        const float fdn_g = fdn_total * 0.25f;

        return svf_g * (0.6f + fdn_g * 0.4f);
    }

    // ===== 以下を前回の末尾 "else if (m==5) {" に続けて追加 =====

    else if (m == 5) {
        float v = juce::jlimit(0.0f, 4.0f,
            juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
        int idx = (int)v; float frac = v - idx; if (idx >= 4) { idx = 3; frac = 1.0f; }
        float f1[5] = { 730.f,270.f,300.f,530.f,400.f };
        float f2[5] = { 1090.f,2290.f,870.f,1840.f,840.f };
        float f3[5] = { 2440.f,3010.f,2240.f,2480.f,2800.f };
        float f_arr[3] = {
            f1[idx] + (f1[idx + 1] - f1[idx]) * frac,
            f2[idx] + (f2[idx + 1] - f2[idx]) * frac,
            f3[idx] + (f3[idx + 1] - f3[idx]) * frac
        };
        float mag_sum = 0.0f; float gains[3] = { 1.0f,0.5f,0.2f };
        for (int f = 0; f < 3; ++f) {
            float w_f = frequency / juce::jlimit(20.0f, 20000.0f, f_arr[f]);
            float d_f = 1.0f / juce::jlimit(0.1f, 10.0f, res);
            float m_f = 1.0f / std::sqrt(std::pow(1.0f - w_f * w_f, 2.0f) + std::pow(w_f * d_f, 2.0f));
            if (state.filterType == 1) m_f *= w_f;
            else if (state.filterType == 2) m_f *= w_f * w_f;
            else if (state.filterType == 3) m_f *= std::abs(1.0f - w_f * w_f);
            mag_sum += m_f * gains[f];
        }
        return mag_sum;
    }
    else if (m == 6) {
        float delaySamples = (float)state.sampleRate / juce::jlimit(20.0f, 20000.0f, fc);
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        if (state.filterType == 1 || state.filterType == 3) fb = -fb;
        float wD = 2.0f * juce::MathConstants<float>::pi * frequency * (delaySamples / (float)state.sampleRate);
        float mm = (state.filterType == 0 || state.filterType == 1)
            ? (1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD)))
            : std::sqrt(1.0f + fb * fb + 2.0f * fb * std::cos(wD));
        return std::pow(mm, state.currentStages);
    }
    else if (m == 7) {
        float ms_res = juce::jmap(res, 0.1f, 10.0f, 0.0f, 2.5f);
        float d = 1.0f / (ms_res + 0.1f);
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        mag = 1.0f / den;
        if (state.filterType == 1) mag *= w;
        else if (state.filterType == 2) mag *= w2;
        else if (state.filterType == 3) mag *= std::abs(1.0f - w2);
        return std::pow(mag, state.currentStages);
    }
    else if (m == 8) {
        float phi = -2.0f * std::atan(frequency / juce::jlimit(20.0f, 20000.0f, fc));
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.95f);
        if (state.filterType == 1 || state.filterType == 3) fb = -fb;
        float c_ap = std::cos(state.currentStages * phi);
        float s_ap = std::sin(state.currentStages * phi);
        float den2 = (1.0f - fb * c_ap) * (1.0f - fb * c_ap) + (fb * s_ap) * (fb * s_ap);
        float rya = (c_ap - fb) / den2;
        float iya = s_ap / den2;
        float ro = 0.5f * (1.0f + rya);
        float io = 0.5f * iya;
        return std::sqrt(ro * ro + io * io);
    }
    else if (m == 21) {
        float lpgFc = juce::jlimit(20.0f, 15000.0f, 20.0f * std::pow(1000.0f, state.lpgEnv[0]));
        float d = 1.0f / 0.707f;
        float w_lpg = frequency / lpgFc;
        mag = (1.0f / std::sqrt(std::pow(1.0f - w_lpg * w_lpg, 2.0f) + std::pow(w_lpg * d, 2.0f)))
            * state.lpgEnv[0];
        return mag;
    }
    else if (m == 22) {
        float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);
        float mag_sum = 0.0f;
        for (int b = 0; b < TptFilterState::numModalBands; ++b) {
            float bFreq = std::clamp(fc * std::pow((float)(b + 1), inharmonicity),
                20.0f, (float)state.sampleRate * 0.45f);
            float sw = frequency / bFreq;
            float q = 50.0f / std::sqrt((float)(b + 1));
            float d = 1.0f / q;
            mag_sum += (1.0f / std::sqrt(std::pow(1.0f - sw * sw, 2.0f) + std::pow(sw * d, 2.0f)))
                * sw * (1.0f / std::sqrt((float)(b + 1)));
        }
        return mag_sum;
    }
    else if (m == 24) {
        // 対数中心対称 Shift マッピング (DSP と一致)
        const float logMin_g = std::log10(20.0f);
        const float logMax_g = std::log10(20000.0f);
        const float logCtr_g = (logMin_g + logMax_g) * 0.5f;
        const float logFc_g  = std::log10(juce::jlimit(20.0f, 20000.0f, fc));
        const float norm_g   = (logFc_g < logCtr_g)
            ? (logFc_g - logCtr_g) / (logCtr_g - logMin_g)
            : (logFc_g - logCtr_g) / (logMax_g - logCtr_g);
        const float shiftHz  = norm_g * 1000.0f;
        return (std::abs(frequency - (1000.0f + shiftHz)) < 80.0f) ? 6.0f : 1.0f;
    }
    else if (m == 25) {
        // 2D Morph: Type 別 (LP/HP=cascade、BP/Notch=parallel sum)
        float mag_accum = (state.filterType == 0 || state.filterType == 2) ? 1.0f : 0.0f;
        for (int k = 0; k < 7; ++k) {
            if (k >= (int)state.zplaneCoeffs.size()) break;
            const float cur_g = state.zplaneCoeffs[k].g;
            const float cur_R = state.zplaneCoeffs[k].R;
            const float sfc   = std::atan(cur_g) * (float)state.sampleRate
                                / juce::MathConstants<float>::pi;
            const float sw    = frequency / juce::jlimit(20.0f, 20000.0f, sfc);
            const float sw2   = sw * sw;
            const float d     = 2.0f * cur_R;  // d = 1/Q in this form
            const float den   = std::sqrt(std::pow(1.0f - sw2, 2.0f) + std::pow(sw * d, 2.0f));
            if      (state.filterType == 0) mag_accum *= (1.0f / den);
            else if (state.filterType == 1) mag_accum += (sw * d) / den;   // BP parallel
            else if (state.filterType == 2) mag_accum *= (sw2 / den);
            else                            mag_accum += std::abs(1.0f - sw2) / den; // Notch parallel
        }
        if (state.filterType == 1 || state.filterType == 3) mag_accum /= 7.0f;
        return mag_accum;
    }
    else if (m == 26) {
        // Phased Array: Blend/Wet の位相キャンセル応答近似
        const float phi_g       = -2.0f * std::atan(w);
        const float totalPhase  = (float)state.currentStages * phi_g;
        const float depth       = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.8f);
        if (state.filterType == 0)  // Blend
            return (1.0f + depth) * std::abs(std::cos(totalPhase * 0.5f));
        else                        // Wet (subtractive)
            return 2.0f * std::abs(std::sin(totalPhase * 0.5f)) * (1.0f + depth);
    }
    else if (m == 23) {
        // Waveguide: コム状共鳴スペクトル（遅延 = SR/fc サンプル）
        // fb = jmap(res, 0.1→0.99) で DSP と一致
        float delaySamples = (float)state.sampleRate / juce::jlimit(20.0f, 20000.0f, fc);
        float fb = juce::jmap(res, 0.1f, 10.0f, 0.0f, 0.99f);
        float wD = 2.0f * juce::MathConstants<float>::pi * frequency
                   * (delaySamples / (float)state.sampleRate);
        float comb_mag = 1.0f / std::sqrt(1.0f + fb * fb - 2.0f * fb * std::cos(wD));
        // Type 0 (Wet) = コムのみ、Type 1 (Mix) = ドライ50%+コム50%
        float mm = (state.filterType == 0) ? comb_mag : (0.5f + 0.5f * comb_mag);
        return std::pow(mm, state.currentStages);
    }
    else if (m == 27) {
        float d = 1.0f / 1.5f;
        float den = std::sqrt(std::pow(1.0f - w2, 2.0f) + std::pow(w * d, 2.0f));
        return std::pow(1.0f / den, 4.0f);
    }


    // ===== 【置き換え後】=====
    else {
        // 8Hz HPF補正（TB-303用）
        auto hpfCorr = [](float f) -> float {
            return (f / 8.0f) / std::sqrt(1.0f + std::pow(f / 8.0f, 2.0f));
            };
        if (m == 2) {
            // TB-303: Phi function でアクセントモード別カットオフシフト
            // Off=1.00x, Low=1.21x (+3.2st), High=1.56x (+7.7st)
            static constexpr float accentPhi[4] = { 1.0f, 1.21f, 1.56f, 1.56f };
            const float phi    = accentPhi[juce::jlimit(0, 3, state.slopeIdx)];
            const float fc_eff = juce::jlimit(20.0f, 20000.0f, fc * phi);
            // k_max per accent mode（PluginDoctor/解析用 — DSP とは別値）
            // 4.0 未満に揃えることで Stinchcombe ピーク高 = 1/|k-4| が
            // Off < Low < High の順で正しく増大する。
            static constexpr float kMaxTab[4] = { 3.5f, 3.7f, 3.85f, 3.85f };
            const float k_max = kMaxTab[juce::jlimit(0, 3, state.slopeIdx)];
            float k = juce::jmap(juce::jlimit(0.1f, 10.0f, res), 0.1f, 10.0f, 0.0f, k_max);
            // Soft clamp (tanh近似)
            if (k > 3.8f) { const float excess = k - 3.8f; k = 3.8f + excess / (1.0f + excess * 0.6f); }
            const float w_tb  = frequency / fc_eff;
            const float w2_tb = w_tb * w_tb;
            // Stinchcombe formula
            const float real_p = std::pow(1.0f - w2_tb, 2.0f) - 4.0f * w2_tb + k;
            const float imag_p = 4.0f * w_tb * (1.0f - w2_tb);
            const float denom  = std::max(std::sqrt(real_p * real_p + imag_p * imag_p), 0.003f);
            const float g_loss = 1.0f / (1.0f + k * 0.07f);
            float mag_tb = (1.0f / denom) * hpfCorr(frequency) * g_loss;
            return std::min(mag_tb, 1000.0f);
        }
        // Moog 系 (1,12,13,15)
        float r_scale = (m == 13) ? 5.0f : 4.5f;
        float r_val = juce::jmap(res, 0.1f, 10.0f, 0.0f, r_scale) * state.scalerMoog;
        if (state.slopeIdx == 0) {
            float Q_eff = juce::jlimit(0.5f, 15.0f, 0.5f + r_val * 2.5f);
            float den = std::sqrt(std::pow(1.0f - w2, 2.0f)
                + std::pow(w / Q_eff, 2.0f));
            mag = 1.0f / den;
            if (state.filterType == 1) mag *= w;
            else if (state.filterType == 2) mag *= w2;
            else if (state.filterType == 3) mag *= std::abs(1.0f - w2);
            float peak = (r_val > 2.0f) ? (1.0f + (r_val - 2.0f) * 0.8f) : 1.0f;
            return std::min(mag * peak, 1000.0f);
        }
        else {
            int cascade = (state.slopeIdx == 1) ? 1 : (state.slopeIdx == 2) ? 2 : 4;
            float r_sc = r_val * std::pow(0.7f, std::log2((float)cascade));
            float real_p = std::pow(1.0f - w2, 2.0f) - 4.0f * w2 + r_sc;
            float imag_p = 4.0f * w * (1.0f - w2);
            // 【修正】分母をclampして跳ね上がり防止
            float denom = std::max(std::sqrt(real_p * real_p + imag_p * imag_p),
                0.005f);
            mag = std::pow(1.0f / denom, cascade);
            if (state.filterType == 1) mag *= w2;
            else if (state.filterType == 2) mag *= w2 * w2;
            else if (state.filterType == 3) mag *= std::abs(1.0f - w2 * w2);
            return std::min(mag * (1.0f + 0.5f * r_sc), 1000.0f);
        }
        }
} // getMagnitudeForFrequency の閉じ括弧