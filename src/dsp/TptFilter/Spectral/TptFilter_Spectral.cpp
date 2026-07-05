// ==========================================
// TptFilter/Spectral/TptFilter_Spectral.cpp
// Model 8,11,24,25,26
// ==========================================
#include "TptFilter_Spectral.h"
#include <cmath>
#include <algorithm>

namespace TptFilter_Spectral
{

    void updateCoeffs(TptFilterState& st)
    {
        const float fc = st.smoothedDigitalCutoff;
        const float res = st.currentResVal;

        // ----- Model 11: Phase Shift (旧 Kilo All-Pass) -----
        // 2次 ZDF オールパス × N 段カスケード。段間周波数を Type で選択したパターンで分散。
        //
        // Type 0: 線形分散 (Lin)  — fc ± spread×0.5×fc の等 Hz 間隔
        // Type 1: 対数分散 (Log)  — fc × 2^(±spread) のオクターブ単位（旧実装を維持）
        // Type 2: 鏡像分散 (Mirror)— 偶数段=+方向、奇数段=-方向に交互に展開
        // Type 3: 固定疑似乱数 (Rand) — 黄金比シーケンスで各段を決定論的に分散
        //
        // Q は Spread と連動: q = 0.5 + spread×2.0
        //   広いスプレッド → 高 Q → 鋭い位相ノッチ（パターンが際立つ）
        if (st.filterModel == 11)
        {
            const float maxSafeFreq = (float)st.sampleRate * 0.45f;
            const float spread      = juce::jmap(res, 0.1f, 10.0f, 0.0f, 2.5f);
            const float q           = 0.5f + (spread * 2.0f);

            // Type 3 (Rand) 用: 黄金比シーケンス（決定論的疑似乱数、ランタイム乱数なし）
            static constexpr float randOffsets[16] = {
                 0.000f,  0.618f, -0.382f,  0.854f,
                -0.146f,  0.472f, -0.764f,  0.236f,
                 0.944f, -0.528f,  0.090f, -0.910f,
                 0.708f, -0.292f,  0.562f, -0.438f
            };

            for (int k = 0; k < st.currentStages; ++k)
            {
                float st_fc = fc;

                switch (st.filterType)
                {
                    case 0: // 線形分散 (Lin): fc 周辺を等 Hz 間隔で配置
                    {
                        const float offset_lin = (st.currentStages > 1)
                            ? ((float)k / (st.currentStages - 1)) * 2.0f - 1.0f
                            : 0.0f;
                        st_fc = fc + offset_lin * fc * spread * 0.5f;
                        break;
                    }
                    case 1: // 対数分散 (Log): オクターブ単位 (旧実装を維持)
                    {
                        const float offset = (st.currentStages > 1)
                            ? ((float)k / (st.currentStages - 1)) * 2.0f - 1.0f
                            : 0.0f;
                        st_fc = fc * std::pow(2.0f, offset * spread);
                        break;
                    }
                    case 2: // 鏡像分散 (Mirror): 偶数段=+、奇数段=- に交互展開
                    {
                        const float sign  = (k % 2 == 0) ? 1.0f : -1.0f;
                        const float magK  = (st.currentStages > 1)
                            ? (float)((k / 2) + 1) / (float)((st.currentStages + 1) / 2)
                            : 0.0f;
                        st_fc = fc * std::pow(2.0f, sign * magK * spread);
                        break;
                    }
                    default: // case 3: 固定疑似乱数 (Rand): 黄金比シーケンス
                    {
                        st_fc = fc * std::pow(2.0f, randOffsets[k % 16] * spread);
                        break;
                    }
                }

                st_fc = std::clamp(st_fc, 20.0f, maxSafeFreq);
                const float wd = juce::MathConstants<float>::pi * st_fc / (float)st.sampleRate;
                st.kilo_g[k] = std::tan(wd);
                st.kilo_R[k] = 1.0f / (2.0f * q);
                st.kilo_h[k] = 1.0f / (1.0f + 2.0f * st.kilo_R[k] * st.kilo_g[k]
                    + st.kilo_g[k] * st.kilo_g[k]);
            }
        }
        // ----- Model 25: Z-Plane 2D Morph -----
        else if (st.filterModel == 25)
        {
            float maxSafeFreq = (float)st.sampleRate * 0.45f;
            float xv = juce::jmap(std::log10(fc),
                std::log10(20.0f), std::log10(20000.0f), 0.0f, 1.0f);
            float yv = juce::jmap(res, 0.1f, 10.0f, 0.0f, 1.0f);
            xv = std::clamp(xv, 0.0f, 1.0f);
            yv = std::clamp(yv, 0.0f, 1.0f);

            const float fA[7] = { 730,1090,2440,4000,6000, 8000,10000 };
            const float qA[7] = { 4,4,3,1,1,1,1 };
            const float fB[7] = { 200, 500,1200,2800,5000, 8500,12000 };
            const float qB[7] = { .5f,.5f,.5f,.5f,.5f,.5f,.5f };
            const float fC[7] = { 300, 870,2240,4000,6000, 8000,10000 };
            const float qC[7] = { 5,4,2,1,1,1,1 };
            const float fD[7] = { 80, 120, 200,4000,8000,12000,16000 };
            const float qD[7] = { 3,2,1,1,2,3,4 };

            if ((int)st.zplaneCoeffs.size() < 7)
                st.zplaneCoeffs.resize(7);

            for (int k = 0; k < 7; ++k)
            {
                float interpFc = fA[k] * (1 - xv) * (1 - yv) + fB[k] * xv * (1 - yv)
                    + fC[k] * (1 - xv) * yv + fD[k] * xv * yv;
                float interpQ = qA[k] * (1 - xv) * (1 - yv) + qB[k] * xv * (1 - yv)
                    + qC[k] * (1 - xv) * yv + qD[k] * xv * yv;

                st.zp_fc[k] = std::clamp(interpFc, 20.0f, maxSafeFreq);
                st.zp_q[k] = std::max(0.5f, interpQ);

                float wd = juce::MathConstants<float>::pi * st.zp_fc[k] / (float)st.sampleRate;
                st.zplaneCoeffs[k].g = std::tan(wd);
                st.zplaneCoeffs[k].R = 1.0f / (2.0f * st.zp_q[k]);
                st.zplaneCoeffs[k].h = 1.0f / (1.0f + 2.0f * st.zplaneCoeffs[k].R
                    * st.zplaneCoeffs[k].g
                    + st.zplaneCoeffs[k].g * st.zplaneCoeffs[k].g);
            }
        }
    }

    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;

        // ----- Model 8: All-Pass Phaser -----
        if (m == 8)
        {
            float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.95f);
            if (st.filterType == 1 || st.filterType == 3) fb = -fb;
            float in_ap = std::tanh(out + fb * st.ap_out_prev[ch]);
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float v = (in_ap - st.ap_s[stage][ch]) * st.ladderG;
                float lp = v + st.ap_s[stage][ch];
                st.ap_s[stage][ch] = lp + v;
                in_ap = 2.0f * lp - in_ap;
            }
            st.ap_out_prev[ch] = in_ap;
            out = (out + in_ap) * 0.5f;
        }
        // ----- Model 11: Kilo All-Pass -----
        else if (m == 11)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float cur_g = st.kilo_g[stage];
                float cur_R = st.kilo_R[stage];
                float cur_h = st.kilo_h[stage];
                float hp = (out - (2.0f * cur_R + cur_g) * st.kilo_s1[stage][ch]
                    - st.kilo_s2[stage][ch]) * cur_h;
                float bp = cur_g * hp + st.kilo_s1[stage][ch];
                float lp = cur_g * bp + st.kilo_s2[stage][ch];
                st.kilo_s1[stage][ch] = cur_g * hp + bp;
                st.kilo_s2[stage][ch] = cur_g * bp + lp;
                out = lp - 2.0f * cur_R * bp + hp;
            }
        }
        // ----- Model 24: Bode Frequency Shifter -----
        // アーキテクチャ: 4段 IIR APF ヒルベルト変換ペア + 正弦波乗算器 + フィードバックループ
        //
        // 真のフィードバック:
        //   入力 = ドライ + FB × 前サンプル出力
        //   → FB を増やすと出力が入力に戻り続け、シフトが累積
        //   → Shepard Tone 的な金属質・無限ループ感 (Bode 周波数シフターの真骨頂)
        //
        // Shift マッピング（対数中心対称）:
        //   Cutoff = sqrt(20 × 20000) ≈ 632Hz → 0Hz シフト (中立点)
        //   Cutoff = 20Hz    → −1000Hz シフト
        //   Cutoff = 20000Hz → +1000Hz シフト
        //   ノブを中央に置いたとき「ほぼシフトなし」になる直感的なマッピング
        //
        // Type 0 (Up):   上側帯域 (A·cos − B·sin) = 入力スペクトル全体を +shiftHz 上方移動
        // Type 1 (Down): 下側帯域 (A·cos + B·sin) = 入力スペクトル全体を −shiftHz 下方移動
        else if (m == 24)
        {
            // ── フィードバック入力（tanh で安定化）──
            const float fb  = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.95f);
            const float fb_in = std::tanh(out + fb * st.bodeOutPrev[ch]);

            // ── ヒルベルト変換: 2系列 4段 IIR APF ──
            float in_A = fb_in, in_B = fb_in;
            for (int i = 0; i < 4; ++i)
            {
                float vA = (in_A - st.hilbertStateA[i][ch]) * st.hilbertCoeffsA[i];
                float outA = vA + st.hilbertStateA[i][ch];
                st.hilbertStateA[i][ch] = outA + vA;
                in_A = 2.0f * outA - in_A;

                float vB = (in_B - st.hilbertStateB[i][ch]) * st.hilbertCoeffsB[i];
                float outB = vB + st.hilbertStateB[i][ch];
                st.hilbertStateB[i][ch] = outB + vB;
                in_B = 2.0f * outB - in_B;
            }

            // ── 対数中心対称 Shift マッピング ──
            const float logMin    = std::log10(20.0f);
            const float logMax    = std::log10(20000.0f);
            const float logCenter = (logMin + logMax) * 0.5f;   // ≈ log10(632)
            const float logCutoff = std::log10(
                juce::jlimit(20.0f, 20000.0f, st.smoothedDigitalCutoff));
            const float normalized = (logCutoff < logCenter)
                ? (logCutoff - logCenter) / (logCenter - logMin)   // -1 to 0
                : (logCutoff - logCenter) / (logMax - logCenter);  //  0 to +1
            const float shiftHz = normalized * 1000.0f;

            // ── 位相発振器 ──
            const float phaseInc = juce::MathConstants<float>::twoPi
                                   * shiftHz / (float)st.sampleRate;
            st.bodePhase[ch] += phaseInc;
            if (st.bodePhase[ch] > juce::MathConstants<float>::twoPi)
                st.bodePhase[ch] -= juce::MathConstants<float>::twoPi;
            if (st.bodePhase[ch] < 0.0f)
                st.bodePhase[ch] += juce::MathConstants<float>::twoPi;

            const float oscCos = std::cos(st.bodePhase[ch]);
            const float oscSin = std::sin(st.bodePhase[ch]);

            // ── 帯域選択 + フィードバック状態更新 ──
            const float shifted = (st.filterType == 0 || st.filterType == 2)
                ? (in_A * oscCos - in_B * oscSin)   // Up: 上側帯域
                : (in_A * oscCos + in_B * oscSin);  // Down: 下側帯域

            st.bodeOutPrev[ch] = shifted;  // 次サンプルのフィードバック用に保存
            out = shifted;                  // AGC がレベルを正規化
        }
        // ----- Model 25: 2D Morph (旧 Z-Plane) -----
        // Cutoff (X軸) × Res (Y軸) の 2次元空間で 4コーナープリセット間を双線形補間し、
        // 7段の SVF を駆動するモーフィングEQ。
        //
        // Type 別処理モード:
        //   LP (Type 0): 7段 LP 直列カスケード → 多バンド LP 傾斜 EQ
        //   BP (Type 1): 7段 BP 並列合計 / 7 → フォルマント共鳴バンク（リゾネーター）
        //   HP (Type 2): 7段 HP 直列カスケード → 多バンド HP 傾斜 EQ
        //   Notch (Type 3): 7段 Notch(LP+HP) 並列合計 / 7 → ハーモニックノッチバンク
        //
        // 並列合成モード (BP/Notch): 各段が元の信号を独立処理し出力を合計。
        //   → 「7バンドグラフィックEQ」的な効果（各段が異なる fc で特定帯域を処理）
        else if (m == 25)
        {
            if (st.filterType == 1)
            {
                // BP 並列合成: 7バンドリゾネーター
                const float original = out;
                float sum_bp = 0.0f;
                for (int stage = 0; stage < 7; ++stage)
                {
                    if (stage >= (int)st.zplaneCoeffs.size()) break;
                    const float cur_g = st.zplaneCoeffs[stage].g;
                    const float cur_R = st.zplaneCoeffs[stage].R;
                    const float cur_h = st.zplaneCoeffs[stage].h;
                    float hp = (original - (2.0f * cur_R + cur_g) * st.zplane_s1[stage][ch]
                        - st.zplane_s2[stage][ch]) * cur_h;
                    float bp = cur_g * hp + st.zplane_s1[stage][ch];
                    float lp = cur_g * bp + st.zplane_s2[stage][ch];
                    st.zplane_s1[stage][ch] = cur_g * hp + bp;
                    st.zplane_s2[stage][ch] = cur_g * bp + lp;
                    sum_bp += bp;
                }
                out = sum_bp / 7.0f;
            }
            else if (st.filterType == 3)
            {
                // Notch 並列合成: 7点ハーモニックノッチバンク
                const float original = out;
                float sum_notch = 0.0f;
                for (int stage = 0; stage < 7; ++stage)
                {
                    if (stage >= (int)st.zplaneCoeffs.size()) break;
                    const float cur_g = st.zplaneCoeffs[stage].g;
                    const float cur_R = st.zplaneCoeffs[stage].R;
                    const float cur_h = st.zplaneCoeffs[stage].h;
                    float hp = (original - (2.0f * cur_R + cur_g) * st.zplane_s1[stage][ch]
                        - st.zplane_s2[stage][ch]) * cur_h;
                    float bp = cur_g * hp + st.zplane_s1[stage][ch];
                    float lp = cur_g * bp + st.zplane_s2[stage][ch];
                    st.zplane_s1[stage][ch] = cur_g * hp + bp;
                    st.zplane_s2[stage][ch] = cur_g * bp + lp;
                    sum_notch += (lp + hp);  // Notch = LP + HP
                }
                out = sum_notch / 7.0f;
            }
            else
            {
                // LP (Type 0) / HP (Type 2): 直列カスケード
                for (int stage = 0; stage < 7; ++stage)
                {
                    if (stage >= (int)st.zplaneCoeffs.size()) break;
                    const float cur_g = st.zplaneCoeffs[stage].g;
                    const float cur_R = st.zplaneCoeffs[stage].R;
                    const float cur_h = st.zplaneCoeffs[stage].h;
                    float hp = (out - (2.0f * cur_R + cur_g) * st.zplane_s1[stage][ch]
                        - st.zplane_s2[stage][ch]) * cur_h;
                    float bp = cur_g * hp + st.zplane_s1[stage][ch];
                    float lp = cur_g * bp + st.zplane_s2[stage][ch];
                    st.zplane_s1[stage][ch] = cur_g * hp + bp;
                    st.zplane_s2[stage][ch] = cur_g * bp + lp;
                    if (st.filterType == 0) out = lp;
                    else                    out = hp;  // Type 2 = HP
                }
            }
        }
        // ----- Model 26: Phased Array -----
        // アーキテクチャ: 1次 APF × N 段を交互フィードバック (偶数段=+fb, 奇数段=-fb) で駆動し、
        //   各段の出力を後段優先の重み付きで加算する。
        //   Depth(Res) が増えるほど干渉が強まり複雑なコム状着色が生まれる。
        //
        // Type 0 (Blend): ドライ + mixedPhase (自然なブレンド)
        // Type 2 (Wet):   mixedPhase のみ (完全ウェット)
        //
        // 旧実装の出力 ×0.3f は常時 -10dB のオフセットをもたらし、
        // 他モデルとのモーフィング時に音量段差が生じていた。削除して AGC に委ねる。
        else if (m == 26)
        {
            float mixedPhase = 0.0f;
            float in_ap = out;
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float fb = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, 0.8f);
                if (stage % 2 != 0) fb = -fb;
                float v = (in_ap - st.pa_s[stage][ch]) * st.ladderG;
                float lp = v + st.pa_s[stage][ch];
                st.pa_s[stage][ch] = lp + v;
                float stage_out = 2.0f * lp - in_ap;
                mixedPhase += stage_out * ((float)(stage + 1) / (float)st.currentStages);
                in_ap = std::tanh(stage_out * 1.1f);
            }
            // Blend (Type 0): (ドライ + ウェット) × 0.5 → 穏やかな位相干渉（構成的）
            // Wet   (Type 2): ドライ − ウェット → 逆相ミックス → 位相キャンセルでコムノッチ発生
            //   APF は振幅フラット（|H|=1）なのでドライ+ウェットは差が少ないが、
            //   ドライ−ウェットは位相差分が振幅差として現れ明確なノッチが発生する
            if (st.filterType == 0)
                out = (out + mixedPhase) * 0.5f;
            else
                out = out - mixedPhase;
        }

        return out;
    }

} // namespace TptFilter_Spectral