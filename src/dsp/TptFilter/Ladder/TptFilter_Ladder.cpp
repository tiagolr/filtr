// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.cpp
// ==========================================
#include "TptFilter_Ladder.h"
#include <cmath>
#include <algorithm>
namespace TptFilter_Ladder
{
    // ============================================================
    // ADAA (Antiderivative Antialiasing) ヘルパー
    //
    // 【原理】
    //   非線形関数 f(x) = tanh(x) のエイリアスを低減するため、
    //   連続信号の区間積分を近似する。
    //
    //   f の不定積分: F(x) = ∫tanh(x)dx = log(cosh(x))
    //
    //   一次 ADAA:
    //     |x - x_prev| > ε の場合:
    //       y = (F(x) - F(x_prev)) / (x - x_prev)   [区間平均]
    //     |x - x_prev| ≤ ε の場合:
    //       y = tanh((x + x_prev) / 2)               [中点フォールバック]
    //
    // 【数値安定化】
    //   log(cosh(x)) = |x| + log(1 + exp(-2|x|)) - log(2)
    //   |x| > 12 では exp(-24) < float 精度のため近似値を使用。
    // ============================================================

    static inline float logCosh(float x) noexcept
    {
        const float ax = std::abs(x);
        if (ax > 12.0f)
            return ax - 0.6931471806f;  // log(cosh(x)) ≈ |x| - log(2)
        return ax + std::log1p(std::exp(-2.0f * ax)) - 0.6931471806f;
    }

    // tanh の一次 ADAA（前サンプル入力 x_prev を渡す）
    static inline float tanhAdaa(float x, float x_prev) noexcept
    {
        constexpr float kEps = 1e-4f;
        const float delta = x - x_prev;
        if (std::abs(delta) > kEps)
            return (logCosh(x) - logCosh(x_prev)) / delta;
        return std::tanh(0.5f * (x + x_prev));
    }

    // ============================================================
    // SSM2040 非対称クリップ（各段出力バッファ）
    //
    // 【実機の物理特性】
    //   SSM2040 の各 OTA 段はカレントミラー出力バッファを持ち、
    //   負方向スイングが約 -500mV でハード飽和する（Q2 カットオフ）。
    //   正方向はソフト飽和（tanh）。
    //   4 段全てが電流反転型 OTA のため、奇数段通過後は信号が反転する。
    //   → 偶数段: 入力負側がクリップ / 奇数段: 入力正側がクリップ（反転後等価）
    //
    // 【生成される高調波】
    //   非対称クリップは偶数次高調波（2nd, 4th…）を主に生成し、
    //   SSM2040 特有のウォームで倍音豊かな音色の源となる。
    //
    // 【係数】
    //   kHard = 3.0: 約 ±333mV相当（−500mV 実機をノーマライズ）
    //   正方向 tanh はそのまま使用（ゲイン補正は 1/1 でスケール維持）
    // ============================================================
    static inline float ssm2040Sat(float x, int stageIdx) noexcept
    {
        constexpr float kHard = 3.0f;
        if ((stageIdx & 1) == 0)
        {
            // 偶数段 (0, 2): 負側ハードクリップ、正側 tanh ソフト飽和
            if (x >= 0.0f)
                return std::tanh(x);
            else
                return x / (1.0f - x * kHard);   // x<0 なので分母 > 1
        }
        else
        {
            // 奇数段 (1, 3): 正側ハードクリップ、負側 tanh ソフト飽和（反転後等価）
            if (x <= 0.0f)
                return -std::tanh(-x);
            else
                return x / (1.0f + x * kHard);
        }
    }

    static inline float tpt1HPF(float x, float& s, float g)
    {
        float v = (x - s) * g / (1.0f + g);
        float lp = v + s;
        s = lp + v;
        return x - lp;
    }
    void updateCoeffs(TptFilterState& st)
    {
        if (st.filterModel != 2) return;
        const float fs = (float)st.sampleRate;

        // =====================================================
        // Accent モードによる実効カットオフ上方シフト（Phi 関数）
        //
        // 実機 TB-303 では Accent トリガー時に MEG エンベロープ電圧が
        // VCF カットオフ制御電流に加算され、フィルターが高域側にシフトする。
        // 静的実装ではエンベロープが走らないため、そのピーク効果を
        // 定常カットオフへの乗算として "投影" する。
        //
        // 研究値（Tim Stinchcombe / Open303 解析より）:
        //   Off  → 1.00× (シフトなし)
        //   Low  → 1.21× (約 3.2 半音 = Accent ポット中点相当)
        //   High → 1.56× (約 7.7 半音 = Accent ポット最大相当)
        // =====================================================
        static constexpr float accentPhi[4] = { 1.0f, 1.21f, 1.56f, 1.56f };
        const float phi = accentPhi[juce::jlimit(0, 3, st.slopeIdx)];

        const float safeFc = std::clamp(st.smoothedDigitalCutoff * phi,
                                        20.0f, fs * 0.45f);
        st.diode_g = std::tan(juce::MathConstants<float>::pi * safeFc / fs);
        st.diode_h = std::tan(juce::MathConstants<float>::pi * 8.0f / fs);
    }
    // ========================================
    // 【高精度版】ダイオード特性関数
    // Koren の近似式を使用
    // y = tanh(x) で実装済み
    // ========================================
    static inline float diodeSat(float x, float strength = 1.0f)
    {
        // strength: Accent によって非線形強度を変更
        // 0.8f (Accent Off) → 1.0f (Low) → 1.2f (High)
        return std::tanh(x * strength);
    }
    static float processDiodeLadder(int ch, float x, TptFilterState& st)
    {
        const float g      = st.diode_g;
        const float inv1pg = 1.0f / (1.0f + g);
        const float G      = g * inv1pg;   // = g/(1+g)
        const float G2     = G * G;
        const float G3     = G * G2;
        const float G4     = G * G3;

        float& s1 = st.zdfState[0][ch][0];
        float& s2 = st.zdfState[0][ch][1];
        float& s3 = st.zdfState[0][ch][2];
        float& s4 = st.zdfState[0][ch][3];

        // ===== Accent による k_max の制御 =====
        // slopeIdx: 0=Accent Off, 1=Accent Low, 2=Accent High
        const float k_max = (st.slopeIdx == 0) ? 4.2f
                          : (st.slopeIdx == 1) ? 4.6f
                          :                      5.0f;
        float k = juce::jmap(st.currentResVal, 0.1f, 10.0f, 0.0f, k_max);
        k = juce::jlimit(0.0f, k_max, k);

        // ===== 正規化状態 Si = si/(1+g) =====
        const float S1 = s1 * inv1pg;
        const float S2 = s2 * inv1pg;
        const float S3 = s3 * inv1pg;
        const float S4 = s4 * inv1pg;
        const float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;

        // ============================================================
        // Newton-Raphson 陰的ソルバー (TB-303 Diode Ladder)
        //
        // 【明示的 ZDF の問題】
        //   sigma を線形帰還として使用し、各段の tanh を無視した近似。
        //   自己発振域（k > 4.0）では誤差が特に顕著。
        //
        // 【NR が解く方程式】
        //   u_pre + k · y4_nl(tanh(u_pre)) = x
        //
        //   y4_nl は 4 段 ZDF を tanh 付きで通した非線形出力:
        //     u_sat = tanh(u_pre)
        //     lp_i  = G · y_{i-1} + S_i   (y_0 := u_sat)
        //     y_i   = tanh(lp_i)
        //
        // 【連鎖律による微分 f'】
        //   dy4/du_pre = G⁴ · ∏ sech²(各段)
        //              = G⁴ · (1-u_sat²)·(1-y1²)·(1-y2²)·(1-y3²)·(1-y4²)
        //
        //   f'(u_pre) = 1 + k · dy4/du_pre
        //
        // 初期推定値 = 明示的 ZDF の解
        // 3 回反復で通常 10⁻⁶ 以下に収束する。
        // ============================================================

        // 初期推定値（明示的 ZDF 近似）
        float u_pre = (x - k * sigma) / (1.0f + k * G4);
        if (k > 3.5f) u_pre += 1e-5f;  // 自己発振シードノイズ（アナログ回路の熱雑音に相当）

        // ============================================================
        // ハイブリッド ソルバー
        //
        // 【NR の自己発振問題】
        //   x=0（無音）かつ全状態=0 の時、f(u) = u + k·y4(u) = 0 の
        //   唯一解は u=0。NR は常に trivial 解へ収束し自己発振しない。
        //
        // 【解決策】
        //   k ≤ 4.0（安定域）: NR で高精度な解を求める
        //   k > 4.0（不安定域）: 明示的 ZDF をそのまま使用
        //     → 明示的 ZDF の数値誤差がシードとなり limit cycle が成長する
        // ============================================================
        if (k <= 4.0f)
        {
            for (int nr = 0; nr < 3; ++nr)
            {
                const float u_sat = std::tanh(u_pre);
                const float y1    = std::tanh(G * u_sat + S1);
                const float y2    = std::tanh(G * y1    + S2);
                const float y3    = std::tanh(G * y2    + S3);
                const float y4    = std::tanh(G * y3    + S4);

                const float f = u_pre + k * y4 - x;

                // 連鎖律: dy4/du_pre = G⁴ · ∏ sech²
                const float dchain = G4
                    * (1.0f - u_sat * u_sat)
                    * (1.0f - y1   * y1)
                    * (1.0f - y2   * y2)
                    * (1.0f - y3   * y3)
                    * (1.0f - y4   * y4);

                const float f_prime = 1.0f + k * dchain;
                if (std::abs(f_prime) < 1e-10f) break;
                u_pre -= f / f_prime;
            }
        }
        // k > 4.0: 明示的 ZDF の初期推定値をそのまま使用（自己発振維持）

        // ===== NR 収束値で状態を更新（1 回のみ）=====
        //
        // 【TB-303 に ADAA を適用しない理由】
        //   TB-303 Diode Ladder は Stateful System（再帰的帰還フィルター）である。
        //   ADAA の実体は H_AA1(z) = (1+z⁻¹)/2 という 0.5 サンプル遅延フィルターであり、
        //   これを帰還ループ内の非線形要素（入力 tanh）に適用すると、
        //   ループ全体の位相が狂い、4 極ラダーの共振周波数・Q が変化してリップルが発生する。
        //   （Bilbao et al., "Antiderivative Antialiasing for Stateful Systems", MDPI 2020 参照）
        //
        //   Stateful System への ADAA 適用には線形ブランチ全体への同期フィルター挿入が必要で、
        //   単純な入力 tanh への適用は音響的に正しくない。
        //   エイリアス抑制は既存の Oversampling (OS) に委ねる。
        //
        // ===== 入力段 tanh（NR ソルバーと一致させる）=====
        //
        // 【diodeSat strength を入力段に適用しない理由】
        //   NR ソルバーは tanh(u_pre) で収束解を求めている。
        //   状態更新で tanh(u_pre * strength) を使うと NR の解と矛盾し、
        //   フィードバックゲインが変化して自己発振条件（k > 4.0）が
        //   崩れるため、strength=1.0 の std::tanh を使用する。
        //   Accent の音色差は k_max（Off=4.2/Low=4.6/High=5.0）によって実現する。
        //
        const float u_sat = std::tanh(u_pre);

        const float v1 = (u_sat - s1) * G;
        const float y1 = std::tanh(v1 + s1);
        s1 += 2.0f * v1;

        const float v2 = (y1 - s2) * G;
        const float y2 = std::tanh(v2 + s2);
        s2 += 2.0f * v2;

        const float v3 = (y2 - s3) * G;
        const float y3 = std::tanh(v3 + s3);
        s3 += 2.0f * v3;

        const float v4 = (y3 - s4) * G;
        const float y4 = std::tanh(v4 + s4);
        s4 += 2.0f * v4;

        // ===== 8Hz HPF =====
        float& hpf_s = st.diodeHpfS[ch];
        float hpf_out = tpt1HPF(y4, hpf_s, st.diode_h);
        hpf_out *= 0.95f;
        return hpf_out;
    }
    float processSample(int channel, float x, TptFilterState& st)
    {
        float out = x;
        const int m = st.filterModel;
        const int ch = channel;
        if (m == 2)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
                out = processDiodeLadder(ch, out, st);
            return out;
        }
        if (m == 1 || m == 12 || m == 13 || m == 15)
        {
            for (int stage = 0; stage < st.currentStages; ++stage)
            {
                float s1_ = st.zdfState[stage][ch][0];
                float s2_ = st.zdfState[stage][ch][1];
                float s3_ = st.zdfState[stage][ch][2];
                float s4_ = st.zdfState[stage][ch][3];
                const float inv1pg = 1.0f / (1.0f + st.g);
                float S1 = s1_ * inv1pg;
                float S2 = s2_ * inv1pg;
                float S3 = s3_ * inv1pg;
                float S4 = s4_ * inv1pg;
                const float G  = st.ladderG;  // = g/(1+g)
                const float G2 = G * G;
                const float G3 = G * G2;
                const float G4 = G * G3;
                float sigma = G3 * S1 + G2 * S2 + G * S3 + S4;
                float c = out - st.ladderRes * sigma;  // c = x_in - k·sigma
                float u;
                if (m == 1)
                {
                    // ============================================================
                    // Newton-Raphson 陰的ソルバー (Moog Ladder)
                    //
                    // 【明示的 ZDF の問題】
                    //   sigma は状態の線形結合で帰還を近似しており、
                    //   入力 tanh の非線形性を無視した近似。
                    //   高レゾナンスで誤差が顕著になる。
                    //
                    // 【NR が解く方程式】
                    //   u_hat + k·G⁴·tanh(u_hat · inv_norm) = c
                    //   （ここで inv_norm = 1/(1+k·G⁴)、c = x_in - k·sigma）
                    //
                    // 【微分（f'）】
                    //   f'(u_hat) = 1 + k·G⁴·inv_norm·sech²(u_hat·inv_norm)
                    //
                    // 初期推定値 = 明示的 ZDF の解（tanh ≈ x 線形近似）
                    // 3回反復で通常 10⁻⁶ 以下に収束する。
                    // ============================================================
                    const float inv_norm = 1.0f / (1.0f + st.ladderRes * G4);
                    float u_hat = c;
                    if (st.ladderRes > 3.5f) u_hat += 1e-5f;  // 自己発振シードノイズ

                    // ハイブリッド: ladderRes ≤ 4.0 は NR、> 4.0 は明示的 ZDF
                    if (st.ladderRes <= 4.0f)
                    {
                        for (int nr = 0; nr < 3; ++nr)
                        {
                            const float t     = std::tanh(u_hat * inv_norm);
                            const float sech2 = 1.0f - t * t;
                            const float f_val = u_hat + st.ladderRes * G4 * t - c;
                            const float f_prime = 1.0f
                                + st.ladderRes * G4 * inv_norm * sech2;
                            if (std::abs(f_prime) < 1e-10f) break;
                            u_hat -= f_val / f_prime;
                        }
                    }
                    // ladderRes > 4.0: 明示的 ZDF の初期推定値をそのまま使用（自己発振維持）

                    // ADAA: 入力 tanh に適用
                    const float tanh_input = u_hat * inv_norm;
                    u = tanhAdaa(tanh_input, st.moogAdaaPrevInput[stage][ch]);
                    st.moogAdaaPrevInput[stage][ch] = tanh_input;
                }
                else
                {
                    // model 12 / 13 / 15: 明示的 ZDF
                    float u_pre = c;
                    if (st.ladderRes > 3.5f) u_pre += 1e-6f;
                    if (m == 12)
                    {
                        // CEM3320: 入力 OTA は対称飽和
                        // ゲイン 1.4 = Prophet-5 Rev3 / Pro-One 等の入力レベル相当
                        // LP/BP/HP は外付け部品でチップ能力をフル活用
                        u = std::tanh(u_pre * 1.4f) / 1.4f;
                    }
                    else if (m == 13)
                    {
                        // SSM2040: 入力は線形（飽和は各段バッファで発生）
                        u = u_pre;
                    }
                    else
                    {
                        // IR3109 (Jupiter-8): ソフトクリップ（OTA 入力差動対）
                        u = u_pre / (1.0f + std::abs(u_pre * 0.5f));
                    }
                }
                float v1 = (u - s1_) * st.ladderG;
                float y1 = v1 + s1_;
                if      (m == 13) y1 = ssm2040Sat(y1, 0);
                else if (m == 15) y1 = std::tanh(y1);
                st.zdfState[stage][ch][0] = s1_ + 2.0f * v1;
                float v2 = (y1 - s2_) * st.ladderG;
                float y2 = v2 + s2_;
                if      (m == 13) y2 = ssm2040Sat(y2, 1);
                else if (m == 15) y2 = std::tanh(y2);
                st.zdfState[stage][ch][1] = s2_ + 2.0f * v2;
                float v3 = (y2 - s3_) * st.ladderG;
                float y3 = v3 + s3_;
                if      (m == 13) y3 = ssm2040Sat(y3, 2);
                else if (m == 15) y3 = std::tanh(y3);
                st.zdfState[stage][ch][2] = s3_ + 2.0f * v3;
                float v4 = (y3 - s4_) * st.ladderG;
                float y4 = v4 + s4_;
                if      (m == 13) y4 = ssm2040Sat(y4, 3);
                else if (m == 15) y4 = std::tanh(y4);
                st.zdfState[stage][ch][3] = s4_ + 2.0f * v4;
                if (st.slopeIdx == 0) {
                    if (st.filterType == 0) out = y2;
                    else if (st.filterType == 1) out = 2.0f * (y1 - y2);
                    else if (st.filterType == 2) out = out - 2.0f * y1 + y2;
                    else                         out = y2 + (out - 2.0f * y1 + y2);
                }
                else {
                    if (st.filterType == 0) out = y4;
                    else if (st.filterType == 1) out = 4.0f * (y2 - y4);
                    else if (st.filterType == 2) out = out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4;
                    else                         out = y4 + (out - 4.0f * y1 + 6.0f * y2 - 4.0f * y3 + y4);
                }
            }
        }
        return out;
    }
} // namespace TptFilter_Ladder