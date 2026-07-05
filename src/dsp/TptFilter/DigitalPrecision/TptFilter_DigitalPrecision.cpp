// ==========================================
// TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.cpp
// Model 17,18,19,20
// ==========================================
#include "TptFilter_DigitalPrecision.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_DigitalPrecision
{

    void updateCoeffs(TptFilterState& st)
    {
        const float fc  = st.smoothedDigitalCutoff;
        const float res = st.currentResVal;
        const float maxSafeFreq = (float)st.sampleRate * 0.45f;
        const float pi  = juce::MathConstants<float>::pi;

        // Chebyshev / Elliptic 共通リップル計算
        float rippleDb = juce::jmap(res, 0.1f, 10.0f, 0.1f, 3.0f);
        float eps      = std::sqrt(std::pow(10.0f, rippleDb / 10.0f) - 1.0f);

        // ── Model 20 (Elliptic): 全セクション共通の伝達零点係数を事前計算 ──
        // Reso → selectivity ξ: 低=ノッチが遠い(Wide), 高=ノッチが近い(Steep)
        // fz = fc / ξ  (fc より上、カットオフ直後〜遠く)
        // gz = tan(π * fz / fs)  → alpha = gp_k / gz (各セクション独立)
        float gz_elliptic = 1.0f;
        if (st.filterModel == 20)
        {
            float xi = juce::jmap(res, 0.1f, 10.0f, 0.3f, 0.9f);
            float fz = std::clamp(fc / xi, 20.0f, maxSafeFreq);
            gz_elliptic = std::tan(pi * fz / (float)st.sampleRate);
        }

        for (int k = 0; k < st.currentStages; ++k)
        {
            float q          = 0.707f;
            float freqScale  = 1.0f;
            const int m      = st.filterModel;

            if (m == 17)
            {
                // ── Butterworth: Reso → Q 倍率（Peak）──
                // 純粋 Butterworth の Q に Reso で乗数をかける。
                // Res=min → 1.0倍（フラット）, Res=max → 3.0倍（共振ピーク）
                float theta    = pi * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                float qButter  = 1.0f / (2.0f * std::sin(theta));
                float qBoost   = juce::jmap(res, 0.1f, 10.0f, 1.0f, 3.0f);
                q = std::max(0.5f, qButter * qBoost);
            }
            else if (m == 18)
            {
                // ── Chebyshev Type I: Reso → リップル量（Ripple）── 変更なし
                float theta    = pi * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                float a        = (1.0f / st.filterOrder) * std::asinh(1.0f / eps);
                float real_p   = -std::sinh(a) * std::sin(theta);
                float imag_p   =  std::cosh(a) * std::cos(theta);
                float wn2      = real_p * real_p + imag_p * imag_p;
                freqScale      = std::sqrt(wn2);
                q              = std::max(0.5f, std::sqrt(wn2) / (-2.0f * real_p));
            }
            else if (m == 19)
            {
                // ── Bessel: Reso → Bessel 近似係数（Phase）──
                // 0.577(純粋 Bessel: 最大位相線形)→ 1.0(通常 SVF: より急峻)
                // freqScale も位相線形度に応じて補正
                float theta       = pi * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                float besselFactor = juce::jmap(res, 0.1f, 10.0f, 0.577f, 1.0f);
                q = std::max(0.5f, (1.0f / (2.0f * std::sin(theta))) * besselFactor);
                // besselNorm=0(純粋Bessel) → freqScale=1+order*0.1 (補正フル)
                // besselNorm=1(SVF的)     → freqScale=1.0 (補正なし)
                float besselNorm  = (besselFactor - 0.577f) / (1.0f - 0.577f);
                freqScale         = 1.0f + (float)st.filterOrder * 0.1f * (1.0f - besselNorm);
            }
            else if (m == 20)
            {
                // ── Elliptic: Chebyshev 極 + 伝達零点（Stopband）──
                // 極: Chebyshev Type I と同一（eps係数は0.5倍でより控えめなリップル）
                float theta    = pi * (2.0f * k + 1.0f) / (2.0f * st.filterOrder);
                float a        = (1.0f / st.filterOrder) * std::asinh(1.0f / (eps * 0.5f));
                float real_p   = -std::sinh(a) * std::sin(theta);
                float imag_p   =  std::cosh(a) * std::cos(theta);
                float wn2      = real_p * real_p + imag_p * imag_p;
                freqScale      = std::sqrt(wn2);
                q              = std::max(0.5f, std::sqrt(wn2) / (-2.0f * real_p) * 1.2f);
            }

            float sectionFreq = std::clamp(fc * freqScale, 20.0f, maxSafeFreq);
            float sectionWd   = pi * sectionFreq / (float)st.sampleRate;

            if ((int)st.dpCoeffs.size() <= k)
                st.dpCoeffs.resize(k + 1);

            st.dpCoeffs[k].g = std::tan(sectionWd);
            st.dpCoeffs[k].R = 1.0f / (2.0f * q);
            st.dpCoeffs[k].h = 1.0f / (1.0f + 2.0f * st.dpCoeffs[k].R * st.dpCoeffs[k].g
                              + st.dpCoeffs[k].g * st.dpCoeffs[k].g);

            // Elliptic: alpha = gp/gz → 零点 fz で完全ノッチ
            // 他モデル: alpha=1.0（効果なし）
            if (st.filterModel == 20)
                st.dpCoeffs[k].alpha = st.dpCoeffs[k].g / gz_elliptic;
            else
                st.dpCoeffs[k].alpha = 1.0f;
        }
    }

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int ch = channel;

        for (int stage = 0; stage < st.currentStages; ++stage)
        {
            if (stage >= (int)st.dpCoeffs.size()) break;
            const float cur_g = st.dpCoeffs[stage].g;
            const float cur_R = st.dpCoeffs[stage].R;
            const float cur_h = st.dpCoeffs[stage].h;

            float hp = (out - (2.0f * cur_R + cur_g) * st.dp_s1[stage][ch]
                - st.dp_s2[stage][ch]) * cur_h;
            float bp = cur_g * hp + st.dp_s1[stage][ch];
            float lp = cur_g * bp + st.dp_s2[stage][ch];
            st.dp_s1[stage][ch] = cur_g * hp + bp;
            st.dp_s2[stage][ch] = cur_g * bp + lp;

            if (st.filterModel == 20)
            {
                // ── Elliptic 出力: 伝達零点 alpha を使用 ──
                // alpha = gp/gz (updateCoeffs で計算済み)
                // LP: lp + α²·hp → ストップバンドに零点 fz=fp/α
                // HP: hp + α²·lp → ストップバンドに零点 fz=fp·α (< fp)
                // BP: 標準 BP
                // Notch: lp + hp（極周波数でノッチ）
                const float a  = st.dpCoeffs[stage].alpha;
                const float a2 = a * a;
                if      (st.filterType == 0) out = lp + a2 * hp;   // LP Elliptic
                else if (st.filterType == 1) out = bp;              // BP
                else if (st.filterType == 2) out = hp + a2 * lp;   // HP Elliptic
                else                         out = lp + hp;         // Notch
            }
            else
            {
                if      (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }

        return out;
    }

} // namespace TptFilter_DigitalPrecision