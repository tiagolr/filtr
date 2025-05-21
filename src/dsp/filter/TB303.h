// Copyright 2025 tilr
// Based off Saikes Yutani bass filters 303
// https://github.com/JoepVanlier/JSFX/blob/master/Yutani/Yutani_Dependencies/Saike_Yutani_Filters.jsfx-inc
#pragma once

#include <JuceHeader.h>
#include "Filter.h"
#include <cmath>

class TB303 : public Filter
{

public:
	TB303() : Filter(kTB303) {}
	~TB303(){}

	void init(double srate, double freq, double q) override;
	void reset(double sample) override;
	double eval(double sample) override;
	void setLerp(int duration) override;
	void setDrive(double drive) override;
	void tick() override; // update interpolation of coefficients

private:
	double drive = 1.0;
	double idrive = 1.0;

	Lerp wc1;
	Lerp k;
	double wc2 = 0.0;
	double wc3 = 0.0;
	double wc4 = 0.0;

	double A = 0.0; 
	double b = 0.0;
	double g = 0.0;
	
	double z0 = 0.0;
	double z1 = 0.0;
	double z2 = 0.0;
	double z3 = 0.0;
	double y1 = 0.0;
	double y2 = 0.0;
	double y3 = 0.0;
	double y4 = 0.0;
		
	double b0 = 0.0;
	double a0 = 0.0;
	double a1 = 0.0;
	double a2 = 0.0;
	double a3 = 0.0;
	double b10 = 0.0;
	double a10 = 0.0;
	double a11 = 0.0;
	double a12 = 0.0;
	double a13 = 0.0;
	double b20 = 0.0;
	double a20 = 0.0;
	double a21 = 0.0;
	double a22 = 0.0;
	double a23 = 0.0;
	double c2 = 0.0;
	double c3 = 0.0;
	double sc = 0.0;
};