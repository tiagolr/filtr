// ==========================================
// TptFilter/DigitalPrecision/TptFilter_DigitalPrecision.h
// Model 17,18,19,20
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_DigitalPrecision
{
    float processSample(int channel, float x, TptFilterState& st);
    void  updateCoeffs(TptFilterState& st);   // calcDigitalPrecisionCoeffs
}