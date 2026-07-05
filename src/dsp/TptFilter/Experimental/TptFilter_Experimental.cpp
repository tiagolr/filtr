// ==========================================
// TptFilter/Experimental/TptFilter_Experimental.cpp
// Model 10: Resonant FDN Reverb Filter (SERUM型リバーブフィルター)
// ==========================================
//
// アーキテクチャ:
//   [SVF Pre-Filter] → [2段 Pre-Diffusion] → [4×4 FDN Core + Per-delay LP Damp]
//
// 設計方針:
//   ・SERUM 型リバーブフィルター: Cutoff が「FDN に送る帯域」を決定
//   ・Type (Dark/Mid/Air/Open) で SVF プリフィルターのタイプを選択
//   ・Slope (Room/Hall/Cave/Plate) で空間キャラクター (遅延長・拡散・減衰) を選択
//   ・Res が Decay (フィードバック量 = テール長) を制御
//   ・Jot(1992) 正規化入力スケール: input × (1-fb) → DC ゲイン = 1、tanh 不要
//   ・Per-delay LP: y[n] = (1-a)×x[n] + a×y[n-1] で高周波吸収を模倣
//   ・Pre-diffusion 2段 APF でトランジェントを平滑化 → 密な残響テール
//   ・2× オーバーサンプリング対応 (getAutoOsFactor で factor=1 設定済み)
//
// State 変数:
//   s1[0][ch], s2[0][ch]  ← SVF プリフィルター
//   ap_s[0..1][ch]        ← Pre-diffusion 2段オールパス (Model 8 Phaser と排他使用)
//   fdnBuffer[4][ch][*]   ← 遅延ライン
//   fdn_ap_state[4][ch]   ← Allpass 補間状態
//   fdnLpState[4][ch]     ← Per-delay LP ダンピング状態
//
// ==========================================
#include "TptFilter_Experimental.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Experimental
{

    float processSample(int channel, float x, TptFilterState& st)
    {
        const int   ch  = channel;
        const float dry = x;  // ドライ信号を保存

        // ──────────────────────────────────────────────────────────
        // Room Character プリセット (slopeIdx → 空間キャラクター)
        // ──────────────────────────────────────────────────────────
        // base_ms: 基本遅延時間 [ms] — 空間サイズ感
        // diff_g:  Pre-diffusion オールパス係数 (0=拡散なし, 0.7=最大拡散)
        // damp_a:  Per-delay LP係数 (0=明るい, 0.9=暗い)
        struct RoomPreset { float base_ms, diff_g, damp_a; };
        static constexpr RoomPreset presets[4] = {
            {  20.0f, 0.35f, 0.30f },   // Room:  小部屋、明るい、軽い拡散
            {  60.0f, 0.50f, 0.55f },   // Hall:  ホール、中程度
            { 120.0f, 0.65f, 0.75f },   // Cave:  洞窟、暗い、重い拡散
            {  30.0f, 0.70f, 0.15f },   // Plate: プレート、非常に明るい、最大拡散
        };
        const int        pidx    = juce::jlimit(0, 3, st.slopeIdx);
        const float      base_ms = presets[pidx].base_ms;
        const float      diff_g  = presets[pidx].diff_g;
        const float      damp_a  = presets[pidx].damp_a;

        // ── Decay (フィードバック量) ──
        const float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.98f);

        // ──────────────────────────────────────────────────────────
        // 1. SVF プリフィルター
        //    Cutoff が「FDN に送る帯域」を決定 (SERUM 型)
        //    係数 g/R/h は updateCoefficients() で事前計算済み (Q=1.2固定)
        // ──────────────────────────────────────────────────────────
        float pre_in;
        {
            const float hp = (x - (2.0f * st.R + st.g) * st.s1[0][ch]
                           - st.s2[0][ch]) * st.h;
            const float bp = st.g * hp + st.s1[0][ch];
            const float lp = st.g * bp + st.s2[0][ch];
            st.s1[0][ch] = st.g * hp + bp;
            st.s2[0][ch] = st.g * bp + lp;

            switch (st.filterType)
            {
                case 0: pre_in = lp;       break;  // Dark:  低域のみ → 暖かい残響
                case 1: pre_in = bp;       break;  // Mid:   中域のみ → ボーカル残響
                case 2: pre_in = hp;       break;  // Air:   高域のみ → 金属的エアー
                default:pre_in = x;        break;  // Open:  全帯域 → 標準リバーブ
            }
        }

        // ──────────────────────────────────────────────────────────
        // 2. Pre-diffusion (2段 1次オールパスフィルター)
        //    トランジェントを平滑化 → 密な残響テール
        //    ap_s[0..1][ch] を使用 (Model 8 Phaser と排他使用のため安全)
        //
        //    1次 APF (TPT 型): y = 2*lp - x
        //      v  = (in - s) * g
        //      lp = v + s
        //      s  = lp + v    ← 状態更新
        //      y  = 2*lp - in ← APF 出力
        // ──────────────────────────────────────────────────────────
        {
            const float g1 = diff_g;
            const float g2 = juce::jlimit(0.0f, 0.85f, diff_g * 1.18f);

            // Stage 1
            float v1 = (pre_in - st.ap_s[0][ch]) * g1;
            float lp1 = v1 + st.ap_s[0][ch];
            st.ap_s[0][ch] = lp1 + v1;
            pre_in = 2.0f * lp1 - pre_in;

            // Stage 2 (わずかに異なる係数で空間密度を増す)
            float v2 = (pre_in - st.ap_s[1][ch]) * g2;
            float lp2 = v2 + st.ap_s[1][ch];
            st.ap_s[1][ch] = lp2 + v2;
            pre_in = 2.0f * lp2 - pre_in;
        }

        // ──────────────────────────────────────────────────────────
        // 3. 4×4 FDN コア (Feedback Delay Network)
        //
        //    遅延比: { 1.0, 1.313, 1.637, 1.911 }
        //      相互素に近い比率でモーダル密度を最大化
        //
        //    Allpass 補間: Thiran 1次 APF (位相線形)
        //      eta = (1 - frac) / (1 + frac)
        //
        //    Per-delay LP ダンピング:
        //      y[n] = (1-a)*x[n] + a*y[n-1]  (damp_a が大きいほど暗い)
        //
        //    Hadamard 4×4 正規化行列 (エネルギー保存):
        //      m0 = 0.5*(d0+d1+d2+d3)
        //      m1 = 0.5*(d0-d1+d2-d3)
        //      m2 = 0.5*(d0+d1-d2-d3)
        //      m3 = 0.5*(d0-d1-d2+d3)
        //
        //    Jot(1992) 正規化入力スケール:
        //      fdnBuffer = pre_in*(1-fb) + m_i*fb
        //      DC ゲイン = 1.0、tanh 不要、完全安定
        // ──────────────────────────────────────────────────────────
        static constexpr float delayRatios[4] = { 1.0f, 1.313f, 1.637f, 1.911f };
        const float baseSamples = (base_ms / 1000.0f) * (float)st.sampleRate;

        float d_out[4];
        for (int i = 0; i < 4; ++i)
        {
            // 遅延ライン読み出し (Thiran allpass 補間)
            const float delaySamples = juce::jlimit(1.0f, 16383.0f,
                                                    baseSamples * delayRatios[i]);
            const int   dInt  = (int)delaySamples;
            const float dFrac = delaySamples - (float)dInt;
            const int r1 = (st.fdnWriteIdx[i][ch] - dInt) & 16383;
            const int r2 = (r1 - 1) & 16383;
            const float eta = (1.0f - dFrac) / (1.0f + dFrac);
            float delayed = st.fdnBuffer[i][ch][r2]
                          + eta * (st.fdnBuffer[i][ch][r1] - st.fdn_ap_state[i][ch]);
            st.fdn_ap_state[i][ch] = delayed;

            // Per-delay LP ダンピング (高周波吸収)
            st.fdnLpState[i][ch] = (1.0f - damp_a) * delayed
                                 + damp_a * st.fdnLpState[i][ch];
            d_out[i] = st.fdnLpState[i][ch];
        }

        // Hadamard 4×4 正規化ミックス
        const float m0 = 0.5f * ( d_out[0] + d_out[1] + d_out[2] + d_out[3]);
        const float m1 = 0.5f * ( d_out[0] - d_out[1] + d_out[2] - d_out[3]);
        const float m2 = 0.5f * ( d_out[0] + d_out[1] - d_out[2] - d_out[3]);
        const float m3 = 0.5f * ( d_out[0] - d_out[1] - d_out[2] + d_out[3]);

        // フィードバック書き込み (Jot 正規化: input × (1-fb))
        const float input_gain = 1.0f - fb;
        st.fdnBuffer[0][ch][st.fdnWriteIdx[0][ch]] = pre_in * input_gain + m0 * fb;
        st.fdnBuffer[1][ch][st.fdnWriteIdx[1][ch]] = pre_in * input_gain + m1 * fb;
        st.fdnBuffer[2][ch][st.fdnWriteIdx[2][ch]] = pre_in * input_gain + m2 * fb;
        st.fdnBuffer[3][ch][st.fdnWriteIdx[3][ch]] = pre_in * input_gain + m3 * fb;

        for (int s = 0; s < 4; ++s)
            st.fdnWriteIdx[s][ch] = (st.fdnWriteIdx[s][ch] + 1) & 16383;

        // ──────────────────────────────────────────────────────────
        // 4. 出力ミックス
        //    全 Hadamard 出力の平均 → 単一出力 m0 より滑らかで密な応答
        //    ドライ + ウェット: AGC (processSample 末尾) が RMS を正規化
        // ──────────────────────────────────────────────────────────
        const float reverb_out = (m0 + m1 + m2 + m3) * 0.25f;

        // ドライ 50% + リバーブ: プラグイン全体の Dry/Wet でさらに調整可能
        return dry * 0.5f + reverb_out;
    }

} // namespace TptFilter_Experimental
