// ==========================================
// TptFilter/AnalogEmulation/TptFilter_AnalogEmulation.h
// Model 5,21,22,27
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_AnalogEmulation
{
    float processSample(int channel, float x, TptFilterState& st);
    void  updateCoeffs(TptFilterState& st);  // Model 5, 22
}