// Copyright 2025 tilr
// Based off Vital synth phaser filter
#pragma once

#include <JuceHeader.h>
#include "Filter.h"
#include "OnePole.h"
#include <cmath>

class Phaser : public Filter
{

public:
	static constexpr int kPeakStage = 4;
	static constexpr int kMaxStages = 3 * kPeakStage;
	static constexpr double kClearRatio = 20.0;

	Phaser(bool pos) : Filter(pos ? kPhaserPos : kPhaserNeg) {}
	~Phaser(){}

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	double eval(double sample) override;
	void setLerp(int duration) override;
	void setDrive(double drive) override;
	void tick() override; // update interpolation of coefficients

private:
	Lerp g = 0.0;
	Lerp k = 0.0;

	OnePole remove_lows_stage;
	OnePole remove_highs_stage;
	OnePole stages[kMaxStages];
	double allpass_output = 0.0;
};