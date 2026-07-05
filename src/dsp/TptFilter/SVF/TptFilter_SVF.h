// ==========================================
// TptFilter/SVF/TptFilter_SVF.h
// Model 0,3,4,6,7,9,14,16,23
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_SVF
{
    // processSample: SVFカテゴリモデルの1サンプル処理
    float processSample(int channel, float x, TptFilterState& st);

    // updateCoeffs: SVFカテゴリの係数更新
    // (共通g,R,hはTptFilter.cppで計算済み。ここでは何もしない)
    inline void updateCoeffs(TptFilterState&) {}
}