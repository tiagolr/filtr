// ==========================================
// TptFilter/Experimental/TptFilter_Experimental.h
// Model 10: Reverb FDN
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_Experimental
{
    float processSample(int channel, float x, TptFilterState& st);
    inline void updateCoeffs(TptFilterState&) {}
}