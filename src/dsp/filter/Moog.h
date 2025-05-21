// Copyright 2025 tilr
// An adapted version of the JUCE Ladder filter
#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "Filter.h"
#include <cmath>

class Moog : public Filter
{
public:
	Moog(FilterPoles p) : Filter(p == k12p ? kMoog12 : kMoog24) {}
	~Moog(){}

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	double eval(double sample) override;
	void setLerp(int duration) override;
	void tick() override; // update interpolation of coefficients
	void setDrive(double drive_) override;
	void setMode(FilterMode mode_) override;
	void updateState();

private:
	double drive = 1.0;
	double drive2 = 1.0;
	double gain = 1.0;
	double gain2 = 1.0;
	double comp = 0.0;
	double mix = 1.0;
	Lerp f0;
	Lerp k;

	static constexpr int numStates = 5;
	std::array<double, numStates> state = {0.0};
	std::array<double, numStates> A = {0.0};
};