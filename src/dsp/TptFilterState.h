// ==========================================
// TptFilterState.h
// TptFilter の全 DSP 状態変数
// カテゴリファイルから共有されるデータ構造
// ==========================================
#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

// BiquadCoeffs: DigitalPrecision / Z-Plane で使用
struct BiquadCoeffs
{
    float g     = 0.0f;
    float R     = 0.0f;
    float h     = 0.0f;
    // Elliptic 伝達零点ブレンド係数: alpha = gp / gz (零点周波数から逆算)
    // LP出力: lp + alpha² * hp → 零点 fz = fp/alpha で完全ノッチ
    // 非Ellipticモデルは 1.0f（効果なし）
    float alpha = 1.0f;
};

struct TptFilterState
{
    // ===== コンテキスト (TptFilter が dispatch 前に設定) =====
    double sampleRate = 44100.0;
    int    filterModel = 0;
    int    filterType = 0;
    int    slopeIdx = 0;
    int    currentStages = 1;
    int    filterOrder = 2;
    float  smoothedDigitalCutoff = 1000.0f;
    float  scalerSVF = 1.0f;
    float  scalerMoog = 1.0f;
    float  scalerDiode = 1.0f;

    // SmoothedValue のキャッシュ (processSample dispatch 前に更新)
    float  currentCutoffVal = 1000.0f;
    float  currentResVal = 0.707f;

    // ===== 共通係数 =====
    float g = 0.0f;
    float R = 0.0f;
    float h = 0.0f;
    float ladderG = 0.0f;
    float ladderRes = 0.0f;

    // ===== SVF 状態変数 =====
    float s1[8][2] = {};
    float s2[8][2] = {};

    // ===== Ladder 状態変数 =====
    float zdfState[8][2][4] = {};

    // ===== Bitcrush / SRR =====
    float srrPhase[2] = {};
    float srrHeld[2] = {};
    // srrBitSteps: std::pow(2, bits) を updateCoefficients() で事前計算してキャッシュ。
    // processSample() 内で std::pow を呼ばないためのリアルタイム安全対策。
    float srrBitSteps = 65536.0f;  // デフォルト = 2^16 (16bit)

    // ===== Formant (Vowel) =====
    float form_g[3] = {};
    float form_R[3] = {};
    float form_h[3] = {};
    float form_s1[3][2] = {};
    float form_s2[3][2] = {};

    // ===== Comb Filter =====
    float combBuffer[8][2][16384] = {};
    int   combWriteIdx[8][2] = {};
    float comb_ap_state[8][2] = {};

    // ===== MS-20 SK Filter =====
    float sk_s1[8][2] = {};
    float sk_s2[8][2] = {};
    // MS-20 DCブロッカー: 実機のフィードバック経路コンデンサを模倣。
    // 非対称 tanh が生む DC 成分を各段で遮断し、動作点のドリフトを防ぐ。
    float ms20_dc_x1[8][2] = {};  // 1サンプル前のフィードバック入力
    float ms20_dc_y1[8][2] = {};  // 1サンプル前の DC ブロッカー出力

    // ===== All-Pass Phaser =====
    float ap_s[16][2] = {};
    float ap_out_prev[2] = {};

    // ===== Kilo All-Pass =====
    float kilo_g[16] = {};
    float kilo_R[16] = {};
    float kilo_h[16] = {};
    float kilo_s1[16][2] = {};
    float kilo_s2[16][2] = {};

    // ===== Digital Precision =====
    std::vector<BiquadCoeffs> dpCoeffs;
    float dp_s1[8][2] = {};
    float dp_s2[8][2] = {};

    // ===== Vactrol LPG =====
    float lpgEnv[2] = {};

    // ===== Modal Resonator =====
    static constexpr int numModalBands = 8;
    float modal_g[numModalBands] = {};
    float modal_R[numModalBands] = {};
    float modal_h[numModalBands] = {};
    float modal_s1[numModalBands][2] = {};
    float modal_s2[numModalBands][2] = {};

    // ===== Waveguide Mesh =====
    float wgBuffer[2][16384] = {};
    int   wgWriteIdx[2] = {};
    float wg_ap_state[2] = {};

    // ===== Bode Frequency Shifter =====
    float hilbertCoeffsA[4] = { 0.4794008656f, 0.8762184935f, 0.9765975895f, 0.9974992559f };
    float hilbertCoeffsB[4] = { 0.1617584983f, 0.7330289323f, 0.9453497003f, 0.9905991567f };
    float hilbertStateA[4][2] = {};
    float hilbertStateB[4][2] = {};
    float bodePhase[2] = {};
    // 真のフィードバックループ用: 前サンプルのシフト出力を保持
    // FB増加でシフトが累積し Shepard Tone 的金属質サウンドを生む
    float bodeOutPrev[2] = {};

    // ===== Z-Plane 2D Morph =====
    std::vector<BiquadCoeffs> zplaneCoeffs;
    float zplane_s1[7][2] = {};
    float zplane_s2[7][2] = {};
    float zp_fc[7] = {};
    float zp_q[7] = {};

    // ===== Phased Array =====
    // Model 26: Slope 最大 16 段 (slopeIdx=3) に対応するため 16 段分確保。
    // 旧サイズ [8][2] では slopeIdx=3 で stage=8〜15 の範囲外書き込みが発生していた。
    float pa_s[16][2] = {};

    // ===== Nyquist Anti-alias =====
    // Model 27: Slope=0→2段, 1→4段, 2→6段, 3→8段 に対応するため8段分確保
    float aa_s1[8][2] = {};
    float aa_s2[8][2] = {};

    // ===== 【既存コード: FDN Reverb ここから】=====
    float fdnBuffer[4][2][16384] = {};
    int   fdnWriteIdx[4][2] = {};      // ← ここは触らない
    float fdn_ap_state[4][2] = {};
    float fdnDelayTimes[4] = { 1.0f, 1.313f, 1.637f, 1.911f };
    // ===== 【既存コード: FDN Reverb ここまで】=====

    // ===== FDN Reverb: Per-delay LP ダンピング =====
    // 各遅延ライン内の 1極 LP フィルター状態変数。
    // y[n] = (1-damp_a)*x[n] + damp_a*y[n-1] で高周波を吸収し
    // 実空間の周波数依存吸収（空気・素材）を模倣する。
    float fdnLpState[4][2] = {};

    // ===== 【新規追加】TB-303 Diode Ladder 専用 =====
    // ↑ fdnDelayTimes の直後、AGC の直前に挿入する
    float diode_g = 0.0f;   // ZDF係数 tan(pi*fc/fs)
    float diode_h = 0.0f;   // 8Hz HPF係数（固定）
    float diodeHpfS[2] = {};   // 8Hz HPF 状態変数

    // ===== 【新規追加】前サンプルの y4 を保持 =====
    // s4（状態変数）ではなく y4（実出力）を
    // フィードバックに使うために必要
    float diodePrevY4[2] = {};

    // ===== ADAA (Antiderivative Antialiasing) 用前サンプル入力値 =====
    // 各 tanh の前サンプル入力 x_prev を保持する。
    // ADAA 式: y = (log(cosh(x)) - log(cosh(x_prev))) / (x - x_prev)
    //
    // Moog (model 1): 入力段 tanh のみ [stage][channel]
    float moogAdaaPrevInput[8][2] = {};
    //
    // TB-303 (model 2): 入力段 + 4 ZDF 段の LP 出力 tanh
    float diodeAdaaPrevInput[2]   = {};   // u_pre  [channel]
    float diodeAdaaPrevLp[4][2]   = {};   // lp1-4  [stage_idx 0-3][channel]


    // ===== 【既存コード: AGC ここから】=====
    float rmsIn[2] = {};
    float rmsOut[2] = {};
    float agcGain[2] = { 1.0f, 1.0f };
    // ===== 【既存コード: AGC ここまで】=====

    // ===== Wavefolder ADAA (Model 9) =====
    // 1次 ADAA: f(x) = sin(x)、F1(x) = -cos(x)
    // double 精度必須: float では特異点付近の差分商で量子化ノイズが可聴域に現れる。
    // wf_F_z1 の厳密な初期値は -cos(0) = -1.0 だが、
    // 無音スタート時は |dx| < THRESHOLD で Taylor フォールバックが機能するため実用上問題なし。
    // 各カスケード段（最大 8 段）× 各チャンネル（2ch）分を確保。
    double wf_x_z1[8][2] = {};   // 前サンプルの fold 入力 x[n-1]
    double wf_F_z1[8][2] = {};   // 前サンプルの F1(x[n-1]) = -cos(x[n-1])
};