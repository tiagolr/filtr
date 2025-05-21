// Copyright 2025 tilr
// Based of Vital Sallen-Key filter
#pragma once

#include <JuceHeader.h>
#include "Filter.h"
#include "OnePole.h"
#include <cmath>

class Analog : public Filter
{

public:
	static constexpr double kDriveResonanceBoost = 2.0;

	Analog(FilterPoles p) : Filter(p == k12p ? kAnalog12 : kAnalog24) {}
	~Analog(){}

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	double eval(double sample) override;
	void setLerp(int duration) override;
	void setDrive(double drive) override;
	void tick() override;

private:
	OnePole pre_stage1;
	OnePole pre_stage2;
	OnePole stage1;
	OnePole stage2;

	Lerp g;
	Lerp k;

	double drivenorm = 0.0;
	double drive = 1.0;
	double idrive = 1.0;
};