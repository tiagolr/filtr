// Copyright 2025 tilr
// Based off Vital synth one_pole
#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "Filter.h"

class OnePole {
public:
    double coeff = 0.0;
    double state = 0.0;
    double curr = 0.0;

    OnePole() {}

    void init(double freq, double srate) {
        Filter::getCoeff(freq, srate);
    }

    double eval(double sample) {
        double delta = coeff * (sample - state);
        state += delta;
        curr = state;
        state += delta;
        return curr;
    }

    void reset(double sample) {
        state = sample;
        curr = sample;
    }
};