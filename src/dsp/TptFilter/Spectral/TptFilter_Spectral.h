// ==========================================
// TptFilter/Spectral/TptFilter_Spectral.h
// Model 8,11,24,25,26
// ==========================================
#pragma once
#include "../../TptFilterState.h"

namespace TptFilter_Spectral
{
    float processSample(int channel, float x, TptFilterState& st);
    void  updateCoeffs(TptFilterState& st);  // Kilo(11), Z-Plane(25)
}