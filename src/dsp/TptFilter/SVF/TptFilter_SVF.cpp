// ==========================================
// TptFilter/SVF/TptFilter_SVF.cpp
// Model 0,3,4,6,7,9,14,16,23
// ==========================================
#include "TptFilter_SVF.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_SVF
{

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 0, 4: Clean SVF / Bitcrush (SVF part) -----
        if (m == 0 || m == 4)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }
        // ----- Model 3: SEM (Oberheim) - OTA 非線形モデル -----
        // 実機解析:
        //   Oberheim SEM の VCF は OTA ベース 2 極 SVF。
        //   飽和は OTA のテール電流（共鳴 BP フィードバック経路）で発生。
        //   インテグレータはキャパシタ → 線形。
        //   入力への直接 tanh は回路的に誤り（廃止）。
        //
        // 数学モデル（半陰的 ZDF）:
        //   bp_sat = tanh(s1)             ← OTA 飽和
        //   hp = (in - 2R·bp_sat - g·s1 - s2) / (1 + g²)
        //   bp = g·hp + s1
        //   lp = g·bp + s2
        //   s1_new = g·hp + bp            ← 線形インテグレータ更新
        //   s2_new = g·bp + lp
        //   notch  = lp + hp              ← = in - 2R·bp_sat（SEM 固有）
        else if (m == 3)
        {
            const float inv1g2 = 1.0f / (1.0f + st.g * st.g);
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                const float bp_sat = std::tanh(st.s1[stage][ch]);
                float hp = (out
                            - 2.0f * st.R * bp_sat
                            - st.g * st.s1[stage][ch]
                            - st.s2[stage][ch]) * inv1g2;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp; // = in - 2R·bp_sat
            }
        }
        // ----- Model 14: CS-80 (Yamaha) - 入力飽和付き SVF -----
        // 実機解析:
        //   Yamaha CS-80 VCF は 2極 SVF (LP / HP のみ)。
        //   トランジスタ入力段・OTA の軽い飽和が独特の "温かみ" と倍音を生む。
        //   Res が上がると飽和深度も増え、Clean SVF との差異が際立つ。
        //
        // 数学モデル:
        //   sat = tanh(in × Drive)   Drive = 1.0 + Res × 0.2
        //   SVF は線形 (Clean SVF と同一トポロジー)
        //   Slope = maxSlope=0 (ModelCapabilities) → 2極固定 → currentStages は常に 1
        else if (m == 14)
        {
            const float cs80Drive = 1.0f + st.currentResVal * 0.2f;
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                const float sat = std::tanh(out * cs80Drive);
                float hp = (sat - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;
                if (st.filterType == 0)      out = lp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;  // Notch も HP 扱い（LP/HP のみ有効）
            }
        }
        // ----- Model 6: Comb Filter -----
        // 遅延時間は smoothedDigitalCutoff を使用してジッパーノイズを防ぐ（Model 23 と同一方針）。
        // currentCutoffVal（スムージングなし）を使うとカットオフ自動化時にグリッチが発生する。
        // ループ不変量（delaySamples / dInt / dFrac / eta / fb）はループ外で一度だけ計算する。
        else if (m == 6)
        {
            const float delaySamples = (float)st.sampleRate
                / juce::jlimit(20.0f, 20000.0f, st.smoothedDigitalCutoff);
            const int   dInt  = (int)delaySamples;
            const float dFrac = delaySamples - (float)dInt;
            const float eta   = (1.0f - dFrac) / (1.0f + dFrac);

            float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.95f);
            if (st.filterType == 1 || st.filterType == 3) fb = -fb;

            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                const int readIdx1 = (st.combWriteIdx[stage][ch] - dInt) & 16383;
                const int readIdx2 = (readIdx1 - 1) & 16383;
                const float delayed = st.combBuffer[stage][ch][readIdx2]
                    + eta * (st.combBuffer[stage][ch][readIdx1]
                        - st.comb_ap_state[stage][ch]);
                st.comb_ap_state[stage][ch] = delayed;

                float combOut = 0.0f;
                if (st.filterType == 0 || st.filterType == 1) {
                    float toBuffer = std::tanh(out + delayed * fb);
                    st.combBuffer[stage][ch][st.combWriteIdx[stage][ch]] = toBuffer;
                    combOut = toBuffer;
                }
                else {
                    st.combBuffer[stage][ch][st.combWriteIdx[stage][ch]] = out;
                    combOut = out + delayed * fb;
                }
                st.combWriteIdx[stage][ch] = (st.combWriteIdx[stage][ch] + 1) & 16383;
                out = combOut;
            }
        }
        // ----- Model 7: MS-20 Screaming -----
        // DC ブロッカー: 実機 MS-20 フィードバック経路のコンデンサを模倣。
        // 非対称 tanh（正側1.5×、負側0.8×）が生む DC 成分を各段で遮断し、
        // 高 Peak 値でも動作点のドリフトが起きないようにする。
        // α = 0.9995 ≈ 3.5Hz カットオフ（44100Hz 時）
        else if (m == 7)
        {
            constexpr float DC_ALPHA = 0.9995f;
            float ms_k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 2.5f);
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                // フィードバック信号の DC 成分を除去してから非対称 tanh を適用
                const float fb_raw = st.sk_s2[stage][ch] * ms_k;
                const float fb_blocked = fb_raw
                    - st.ms20_dc_x1[stage][ch]
                    + DC_ALPHA * st.ms20_dc_y1[stage][ch];
                st.ms20_dc_x1[stage][ch] = fb_raw;
                st.ms20_dc_y1[stage][ch] = fb_blocked;

                float fb = (fb_blocked > 0.0f) ? std::tanh(fb_blocked * 1.5f)
                                               : std::tanh(fb_blocked * 0.8f);
                float v1 = (out - fb - st.sk_s1[stage][ch]) * st.g / (1.0f + st.g);
                float y1 = v1 + st.sk_s1[stage][ch];
                st.sk_s1[stage][ch] = y1 + v1;
                float v2 = (y1 - st.sk_s2[stage][ch]) * st.g / (1.0f + st.g);
                float y2 = v2 + st.sk_s2[stage][ch];
                st.sk_s2[stage][ch] = y2 + v2;
                float lp = y2, hp = out - y1, bp = y1 - y2;
                if (st.filterType == 0) out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
        }
        // ----- Model 9: Wavefolder (1st-order ADAA + cascade) -----
        // アーキテクチャ: (SVF 1段 → ADAA sin) × N  ← Slope で N=1/2/4/8 を選択
        // ADAA 式: output = (F1(x_n) - F1(x_z1)) / (x_n - x_z1)
        //   F1(x) = -cos(x)  (sin の不定積分)
        // 特異点回避: |dx| < 1e-5 のとき Taylor 展開でフォールバック
        //   Taylor: sin(x_z1) + 0.5 * cos(x_z1) * dx
        // 全演算を double 精度で実行 → float では差分商の情報落ちが可聴ノイズになる
        //
        // 【カスケードゲイン正規化】
        //   fold_gain を各段にそのまま掛けると、段をまたぐ x 空間が指数的に拡大し
        //   1サンプル間で sin() が多周期にわたってしまう → ADAA 平均値が擬似乱数 = ノイズ。
        //   解決策: 全体 fold_gain を N 段で等分割 → per_stage_gain = fold_gain^(1/N)
        //   これで各段の x_n 変動幅が一定に保たれ、ADAA が正しく機能する。
        else if (m == 9)
        {
            constexpr double ADAA_THRESHOLD = 1e-5;
            const double fold_gain = static_cast<double>(
                juce::jmap(st.currentResVal, 0.1f, 10.0f, 1.0f, 10.0f));

            // N 段カスケード用の per-stage ゲイン（fold_gain の N 乗根）
            // N=1 のときは fold_gain そのまま（pow(..., 1.0) = 無変化）
            const double per_stage_gain = std::pow(fold_gain,
                1.0 / static_cast<double>(st.currentStages));

            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                // ── SVF（プリフォールド音色整形） ──
                const float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                               - st.s2[stage][ch]) * st.h;
                const float bp = st.g * hp + st.s1[stage][ch];
                const float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + lp;

                float svf_out;
                if      (st.filterType == 0) svf_out = lp;
                else if (st.filterType == 1) svf_out = bp;
                else if (st.filterType == 2) svf_out = hp;
                else                         svf_out = lp + hp;

                // ── 1次 ADAA sin() wavefold ──
                // per_stage_gain を使用: 各段の x 空間を一定幅に制限してノイズを防ぐ
                const double x_n  = static_cast<double>(svf_out) * per_stage_gain;
                const double x_z1 = st.wf_x_z1[stage][ch];
                const double F_n  = -std::cos(x_n);          // F1(x_n) = -cos(x_n)
                const double F_z1 = st.wf_F_z1[stage][ch];   // F1(x_z1)
                const double dx   = x_n - x_z1;

                double folded;
                if (std::abs(dx) < ADAA_THRESHOLD)
                    // Taylor 展開フォールバック（特異点・情報落ち回避）
                    folded = std::sin(x_z1) + 0.5 * std::cos(x_z1) * dx;
                else
                    // ADAA 差分商
                    folded = (F_n - F_z1) / dx;

                // 状態更新（次サンプルのために保存）
                st.wf_x_z1[stage][ch] = x_n;
                st.wf_F_z1[stage][ch] = F_n;

                out = static_cast<float>(folded);
            }
        }
        // ----- Model 16: EDP Wasp CMOS -----
        // 実機解析:
        //   EDP Wasp は CD4007 CMOS ロジックを VCF に使用（非常に異例）。
        //   CMOS の pull-up（PMOS）は pull-down（NMOS）より駆動力が弱いため
        //   正側が早くクリップし、負側はやや余裕がある → 非対称クリッピング。
        //   この非対称性が偶数次高調波（2次歪み）を生み、Wasp 固有の "ギザギザ" 感を作る。
        //
        // 非対称 waspClip:
        //   正側: v / (1 + v * 3.0)   → 飽和限界 ≈ +0.33  (強くクリップ)
        //   負側: v / (1 - v * 1.5)   → 飽和限界 ≈ -0.67  (弱めにクリップ)
        //   小信号ゲイン (v→0) は両側とも 1.0 で連続
        //
        // ゲイン正規化:
        //   旧実装の `out * 1.5f + clamp` は常時 +3.5dB のゲインオフセットをもたらし
        //   モーフィング時に音量が突然変化する原因となっていた。
        //   削除して AGC（processSample 末尾）に委ねる。
        else if (m == 16)
        {
            auto waspClip = [](float v) -> float {
                return (v >= 0.0f)
                    ? v / (1.0f + v * 3.0f)    // PMOS pull-up 制限: 強めクリップ
                    : v / (1.0f - v * 1.5f);   // NMOS pull-down: 弱めクリップ (v<0 なので分母>1)
            };
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float hp = (out - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float lp = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = waspClip(st.g * hp + bp);
                st.s2[stage][ch] = waspClip(st.g * bp + lp);
                if (st.filterType == 0)      out = lp;
                else if (st.filterType == 1) out = bp;
                else if (st.filterType == 2) out = hp;
                else                         out = lp + hp;
            }
            // ゲイン補正なし: AGC が RMS ベースで自動補正する
        }
        // ----- Model 23: Waveguide Mesh (多段反射) -----
        // アーキテクチャ: 反射ループ × N 段カスケード (N = currentStages = 1/2/4/8)
        //   各段が独立した遅延ライン + SVF スキャッタリング + tanh 励振を持ち、
        //   前段出力が次段の励振として使われる。
        //
        // 遅延バッファ: combBuffer[stage][ch] を流用
        //   (Model 6 Comb Filter と Model 23 は排他使用のため安全)
        //   同様に combWriteIdx / comb_ap_state も流用。
        //
        // Type 選択:
        //   Type 0 (Wet):  scattered のみ (waveguide 共鳴音だけ取り出す)
        //   Type 1 (Mix):  dry 50% + scattered 50% (励振音と共鳴音のブレンド)
        //
        // Cutoff → 遅延長 = SR / Cutoff → 共鳴ピッチ ("Tune")
        // Res    → フィードバック係数 → 音の持続時間 ("Decay")
        else if (m == 23)
        {
            const float delaySamples = (float)st.sampleRate
                / juce::jlimit(20.0f, 20000.0f, st.smoothedDigitalCutoff);
            const int   dInt  = (int)delaySamples;
            const float dFrac = delaySamples - (float)dInt;
            const float eta   = (1.0f - dFrac) / (1.0f + dFrac);
            const float fb    = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.99f);

            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                // ── 遅延ライン読み出し（1次 allpass 補間）──
                const int r1 = (st.combWriteIdx[stage][ch] - dInt) & 16383;
                const int r2 = (r1 - 1) & 16383;
                float wgDelayed = st.combBuffer[stage][ch][r2]
                    + eta * (st.combBuffer[stage][ch][r1] - st.comb_ap_state[stage][ch]);
                st.comb_ap_state[stage][ch] = wgDelayed;

                // ── SVF スキャッタリング (波の反射シミュレーション) ──
                float hp = (wgDelayed - (2.0f * st.R + st.g) * st.s1[stage][ch]
                    - st.s2[stage][ch]) * st.h;
                float bp = st.g * hp + st.s1[stage][ch];
                float scattered = st.g * bp + st.s2[stage][ch];
                st.s1[stage][ch] = st.g * hp + bp;
                st.s2[stage][ch] = st.g * bp + scattered;

                // ── 励振 + tanh 非線形サチュレーション → 遅延ラインへ書き込み ──
                const float excitation = out + scattered * fb;
                st.combBuffer[stage][ch][st.combWriteIdx[stage][ch]] = std::tanh(excitation);
                st.combWriteIdx[stage][ch] = (st.combWriteIdx[stage][ch] + 1) & 16383;

                // ── Type 選択 ──
                if (st.filterType == 0)
                    out = scattered;                        // Wet: 共鳴音のみ
                else
                    out = out * 0.5f + scattered * 0.5f;   // Mix: 励振+共鳴ブレンド
            }
        }

        return out;
    }

} // namespace TptFilter_SVF