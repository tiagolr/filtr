// Copyright 2025 tilr
// QuadFilter by OTODESK4193 Tpt-filter wrapper
#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "Filter.h"
#include "../TptFilter.h"

class TptWrapper : public Filter
{

public:
	TptWrapper(int model) 
		: Filter(FilterType::kTpt) 
	{
		filter.setModel(model);
	}
	~TptWrapper(){}

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	void setMode(FilterMode mode_) override;
	double eval(double sample) override;
	void setLerp(int duration) override;
	void setDrive(double drive) override;
	void setSlope(int slope) override;
	void tick() override;

private:
	TptFilter filter;
	double srate = 44100.;
};