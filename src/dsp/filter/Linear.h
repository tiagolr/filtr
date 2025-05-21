// Copyright 2025 tilr
// Based off Saikes Yutani bass filters
// https://github.com/JoepVanlier/JSFX/blob/master/Yutani/Yutani_Dependencies/Saike_Yutani_Filters.jsfx-inc
#pragma once

#include <JuceHeader.h>
#include "Filter.h"
#include <cmath>

class Linear : public Filter
{

public:
	Linear(FilterPoles p) : Filter(p == k12p ? kLinear12 : kLinear24) {}
	~Linear(){}

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	double eval(double sample) override;
	void setLerp(int duration) override;
	void setDrive(double drive) override;
	void tick() override; // update interpolation of coefficients

private:
	double ic1 = 0.0;
	double ic2 = 0.0;
	double ic3 = 0.0;
	double ic4 = 0.0;

	double drive = 1.0;
	double idrive = 1.0;

	Lerp g;
	Lerp k;
	Lerp a1;
	Lerp a2;
	Lerp a3;

	double b0 = 0.0;
	double b1 = 0.0;
	double b2 = 0.0;
	double x1 = 0.0;
	double x2 = 0.0;
	double y1 = 0.0;
	double y2 = 0.0;
};