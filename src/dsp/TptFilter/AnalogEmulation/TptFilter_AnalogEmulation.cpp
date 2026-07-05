// ==========================================
// TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.cpp
// Model 5,21,22,27
// ==========================================
#include "TptFilter_AnalogEmulation.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_AnalogEmulation
{

    void updateCoeffs(TptFilterState& st)
    {
        const float fc = st.smoothedDigitalCutoff;
        const float res = st.currentResVal;

        // ----- Model 5: Formant -----
        // フォルマント周波数テーブル（英語5母音、Peterson & Barney 1952ベース）
        // Cutoff: 20Hz〜20kHz を 0〜4 にマップして5母音間を連続補間
        //   idx=0:a(father), 1:i(see), 2:u(too), 3:e(met), 4:o(go)
        // Res: フォルマントのQ（バンド幅）。小→ブロード(声っぽい), 大→シャープ(鋭い)
        //   Q = 2.3 + res*3.0  →  range Q≈2.3(res=0.1) 〜 32(res=10)
        //   Q < 2 だと母音特徴が消えるため下限を設ける
        if (st.filterModel == 5)
        {
            float v = juce::jlimit(0.0f, 4.0f,
                juce::jmap(std::log10(fc), std::log10(20.0f), std::log10(20000.0f), 0.0f, 4.0f));
            int   idx = (int)v;
            float frac = v - idx;
            if (idx >= 4) { idx = 3; frac = 1.0f; }

            float f1_map[5] = { 730.f, 270.f, 300.f, 530.f, 400.f };
            float f2_map[5] = { 1090.f, 2290.f, 870.f, 1840.f, 840.f };
            float f3_map[5] = { 2440.f, 3010.f, 2240.f, 2480.f, 2800.f };
            float f_arr[3] = {
                f1_map[idx] + (f1_map[idx + 1] - f1_map[idx]) * frac,
                f2_map[idx] + (f2_map[idx + 1] - f2_map[idx]) * frac,
                f3_map[idx] + (f3_map[idx + 1] - f3_map[idx]) * frac
            };

            // Q = 2.3 〜 32（下限2.3でフォルマント特徴を保持）
            const float formQ = 2.0f + res * 3.0f;
            const float formR = 1.0f / (2.0f * formQ);

            for (int f = 0; f < 3; ++f)
            {
                float wd = juce::MathConstants<float>::pi * f_arr[f] / (float)st.sampleRate;
                st.form_g[f] = std::tan(wd);
                st.form_R[f] = formR;
                st.form_h[f] = 1.0f / (1.0f + 2.0f * st.form_R[f] * st.form_g[f]
                    + st.form_g[f] * st.form_g[f]);
            }
        }
        // ----- Model 22: Modal Resonator -----
        // Cutoff: 基本周波数（基音モードの共振周波数）
        // Res: 非調波性（Inharmonicity）。1.0=倍音列, 2.5=金属打楽器的な非調和
        // Slope: モードのQ（減衰の長さ）
        //   Slope 0 = Low-Q  (q_base=5,  木材・弦・マリンバ的な素早い減衰)
        //   Slope 1 = Mid-Q  (q_base=15, 中間的なサウンド)
        //   Slope 2 = High-Q (q_base=50, 金属・鐘・シンバル的な長い残響)
        //   各バンドは高次になるほど自然減衰: q = qBase / sqrt(b+1)
        else if (st.filterModel == 22)
        {
            float baseFreq = std::clamp(fc, 20.0f, (float)st.sampleRate * 0.45f);
            float inharmonicity = juce::jmap(res, 0.1f, 10.0f, 1.0f, 2.5f);

            // Slope→Q変換テーブル
            constexpr float qBaseTable[3] = { 5.0f, 15.0f, 50.0f };
            const float qBase = qBaseTable[juce::jlimit(0, 2, st.slopeIdx)];

            for (int b = 0; b < TptFilterState::numModalBands; ++b)
            {
                float bFreq = std::clamp(baseFreq * std::pow((float)(b + 1), inharmonicity),
                    20.0f, (float)st.sampleRate * 0.45f);
                float wd = juce::MathConstants<float>::pi * bFreq / (float)st.sampleRate;
                st.modal_g[b] = std::tan(wd);
                // 高次バンドほど減衰が速い（物理的に自然な特性）
                float q = qBase / std::sqrt((float)(b + 1));
                st.modal_R[b] = 1.0f / (2.0f * q);
                st.modal_h[b] = 1.0f / (1.0f + 2.0f * st.modal_R[b] * st.modal_g[b]
                    + st.modal_g[b] * st.modal_g[b]);
            }
        }
    }

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 5: Formant (Vowel) -----
        if (m == 5)
        {
            float out_formant = 0.0f;
            float gains[3] = { 1.0f, 0.5f, 0.2f };
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float stage_in = out;
                out_formant = 0.0f;
                for (int f = 0; f < 3; ++f)
                {
                    float hp = (stage_in
                        - (2.0f * st.form_R[f] + st.form_g[f]) * st.form_s1[f][ch]
                        - st.form_s2[f][ch]) * st.form_h[f];
                    float bp = st.form_g[f] * hp + st.form_s1[f][ch];
                    float lp = st.form_g[f] * bp + st.form_s2[f][ch];
                    st.form_s1[f][ch] = st.form_g[f] * hp + bp;
                    st.form_s2[f][ch] = st.form_g[f] * bp + lp;

                    // Formant filter: BP出力固定。LP/HP/Notchはフォルマント特徴を損なう。
                    // Type コンボは ModelCapabilities で BP のみ有効に設定済み。
                    out_formant += bp * gains[f];
                }
                out = out_formant;
            }
        }
        // ----- Model 21: Vactrol LPG -----
        // Cutoff: LPGの開口量（Cvとして機能）。上=開く=明るく大きい音、下=閉じる=暗く小さい音
        // Res:    Vacトロールのリリース時間（Decay）。小=素早く閉じる, 大=ゆっくり余韻
        // Slope:  Vacトロールのアタック速度（LEDの応答特性を模倣）
        //   Slope 0 = Fast   (1ms:  LED素子が高速なタイプ、クリスプなゲート)
        //   Slope 1 = Medium (5ms:  標準的なVacトロール挙動)
        //   Slope 2 = Slow   (20ms: 鈍いLED、なめらかなスウェル感)
        else if (m == 21)
        {
            float targetEnv = std::log10(st.smoothedDigitalCutoff / 20.0f) / 3.0f;
            float currentEnv = st.lpgEnv[ch];

            // Slope → アタック時間テーブル（ms）
            constexpr float attackTimeTable[3] = { 0.001f, 0.005f, 0.020f };
            const float attackTime = attackTimeTable[juce::jlimit(0, 2, st.slopeIdx)];
            float attackCoef = 1.0f - std::exp(-1.0f / (attackTime * (float)st.sampleRate));

            float releaseTime = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.05f, 2.0f);
            float releaseCoef = 1.0f - std::exp(-1.0f / (releaseTime * (float)st.sampleRate));
            float coef = (targetEnv > currentEnv) ? attackCoef : releaseCoef;
            st.lpgEnv[ch] = currentEnv + coef * (targetEnv - currentEnv);

            float lpgFc = juce::jlimit(20.0f, 15000.0f,
                20.0f * std::pow(1000.0f, st.lpgEnv[ch]));
            float lpgWd = juce::MathConstants<float>::pi * lpgFc / (float)st.sampleRate;
            float lpgG = std::tan(lpgWd);
            float lpgR = 1.0f / (2.0f * 0.707f);
            float lpgH = 1.0f / (1.0f + 2.0f * lpgR * lpgG + lpgG * lpgG);

            float hp = (out - (2.0f * lpgR + lpgG) * st.s1[0][ch] - st.s2[0][ch]) * lpgH;
            float bp = lpgG * hp + st.s1[0][ch];
            float lp = lpgG * bp + st.s2[0][ch];
            st.s1[0][ch] = lpgG * hp + bp;
            st.s2[0][ch] = lpgG * bp + lp;
            out = lp * (st.lpgEnv[ch] * st.lpgEnv[ch] + 0.001f);
        }
        // ----- Model 22: Modal Resonator -----
        else if (m == 22)
        {
            float modalMix = 0.0f;
            for (int b = 0; b < TptFilterState::numModalBands; ++b)
            {
                float hp = (out - (2.0f * st.modal_R[b] + st.modal_g[b]) * st.modal_s1[b][ch]
                    - st.modal_s2[b][ch]) * st.modal_h[b];
                float bp = st.modal_g[b] * hp + st.modal_s1[b][ch];
                float lp = st.modal_g[b] * bp + st.modal_s2[b][ch];
                st.modal_s1[b][ch] = st.modal_g[b] * hp + bp;
                st.modal_s2[b][ch] = st.modal_g[b] * bp + lp;
                modalMix += bp * (1.0f / std::sqrt((float)(b + 1)));
            }
            if (st.filterType == 0) out = modalMix;
            else if (st.filterType == 1) out = out * 0.5f + modalMix;
            else if (st.filterType == 2) out = out * 0.5f - modalMix;
            else                         out = std::tanh(modalMix * 2.0f);
        }
        // ----- Model 27: Nyquist Anti-alias -----
        // 超急峻なLPFをカスケードSVFで構成する。電子音楽的な「デジタルらしい切り捨て」サウンド。
        // Cutoff: カットオフ周波数（ナイキスト付近まで設定可能）
        // Res:    各段のQ（フィルター特性）。小=Butterworth的, 大=ピーキー（カットオフ付近が盛り上がる）
        //   Q = 0.5(res=0.1) 〜 3.0(res=10) にマップ
        //   Q=0.707=Butterworthが各段最もフラットな通過域を持つ
        // Slope:  段数（急峻さ）
        //   Slope 0 = 2段 (24 dB/oct)
        //   Slope 1 = 4段 (48 dB/oct, 旧デフォルト)
        //   Slope 2 = 6段 (72 dB/oct)
        //   Slope 3 = 8段 (96 dB/oct)
        else if (m == 27)
        {
            float aa_g = st.g;
            // Reso → Q: 各段のキャラクターを制御（0.5=Bessel的, 0.707=Butterworth, 3.0=ピーキー）
            float aa_Q = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.5f, 3.0f);
            float aa_R = 1.0f / (2.0f * aa_Q);
            float aa_h = 1.0f / (1.0f + 2.0f * aa_R * aa_g + aa_g * aa_g);
            // currentStages はsetSlope()で Slope 0→2, 1→4, 2→6, 3→8 に設定済み
            const int aa_stages = juce::jlimit(2, 8, st.currentStages);
            for (int stage = 0; stage < aa_stages; ++stage)
            {
                float hp = (out - (2.0f * aa_R + aa_g) * st.aa_s1[stage][ch]
                    - st.aa_s2[stage][ch]) * aa_h;
                float bp = aa_g * hp + st.aa_s1[stage][ch];
                float lp = aa_g * bp + st.aa_s2[stage][ch];
                st.aa_s1[stage][ch] = aa_g * hp + bp;
                st.aa_s2[stage][ch] = aa_g * bp + lp;
                out = lp;
            }
        }

        return out;
    }

} // namespace TptFilter_AnalogEmulation