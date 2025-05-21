// Copyright 2025 tilr
// Based off Saikes Yutani bass filters MS-20
// https://github.com/JoepVanlier/JSFX/blob/master/Yutani/Yutani_Dependencies/Saike_Yutani_Filters.jsfx-inc
#pragma once

#include <JuceHeader.h>
#include "Filter.h"
#include <cmath>

class MS20 : public Filter
{

public:
	MS20() : Filter(kMS20) {}
	~MS20(){}

	static constexpr int maxiter = 6;
	static constexpr double epsilon = 0.00000001;

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	double eval(double sample) override;
	double evalLP(double sample);
	double evalBP(double sample);
	double evalHP(double sample);
	void setLerp(int duration) override;
	void setDrive(double drive) override;
	void tick() override; // update interpolation of coefficients

private:
	double drive = 1.0;
	double idrive = 1.0;

	Lerp hh;
	Lerp k;

	double y1 = 0.0;
	double y2 = 0.0;
	double d1 = 0.0;
	double d2 = 0.0;
	double obs = 0.0;
};