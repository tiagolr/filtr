// ==========================================
// TptFilter/Ladder/TptFilter_Ladder.h
// Model 1: Moog Ladder
// Model 2: Diode TB-303 (Wurtz/Stinchcombe)
// Model 12: CEM3320
// Model 13: SSM 2040
// Model 15: Jupiter Roland
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_Ladder
{
    float processSample(int channel, float x, TptFilterState& st);
    void  updateCoeffs(TptFilterState& st);
    inline void resetDiodeState(TptFilterState& st);
}