// Copyright 2025 tilr
#pragma once

#include <cmath>
#include "../../Globals.h"
#include "../Utils.h"

using namespace globals;

enum FilterType
{
	kLinear12,
	kLinear24,
	kAnalog12,
	kAnalog24,
	kMoog12,
	kMoog24,
	kMS20,
	kTB303,
	kPhaserPos,
	kPhaserNeg
};

enum FilterPoles
{
	k12p,
	k24p
};

enum FilterMode
{
	LP,
	BP,
	HP,
	BS,
	PK
};

class Filter
{
protected:
	FilterType type;
	FilterMode mode;
	double morph = 0.0;

public:
	static constexpr double kMinNyquistMult = 0.48;

	inline static LookupTable tanhLUT = LookupTable(
		[](double x) { return std::tanh(x); },
		-5.0, 5.0, 1024
	);

	inline static LookupTable coeffLUT = LookupTable(
		[] (double ratio) {
			constexpr double kMaxRads = 0.499 * juce::MathConstants<double>::pi;
			double scaled = ratio * juce::MathConstants<double>::pi;
			return std::tan(std::min(kMaxRads, scaled));
		},
		0.0, 0.5, 2048
	);

	inline static double hardTanh(double value) {
		static constexpr double kHardness = 0.66f;
		static constexpr double kHardnessInv = 1.0f - kHardness;
		static constexpr double kHardnessInvRec = 1.0f / kHardnessInv;

		double clamped = std::max(std::min(value, kHardness), -kHardness);
		return clamped + tanhLUT((value - clamped) * kHardnessInvRec) * (1.0f - kHardness);
	}

	Filter(FilterType type) : type(type), mode(LP) {}
	virtual ~Filter() {}
	virtual void setMode(FilterMode mode_) { mode = mode_; }
	virtual void setDrive(double norm) { (void)norm; };
	virtual void setMorph(double norm) { morph = norm; };

	virtual void init(double srate, double freq, double qnorm) = 0;
	virtual void reset(double sample) = 0;
	virtual double eval(double sample) = 0;
	virtual void setLerp(int duration) = 0;
	virtual void tick() = 0; // update interpolation of coefficients

	inline static double getCoeff(double freq, double srate) {
		freq = jlimit(20.0, srate * kMinNyquistMult, freq);
		double ratio = jlimit(0.0, 0.5, freq / srate);
		return coeffLUT.cubic(ratio);
	}
};

class Lerp {
	double value, target;
	double step = 0.0;
	int samplesLeft = 0;
	int duration = 0;
	bool isReset = true;

public:
	Lerp(double start = 0.0) : value(start), target(start) {}

	void setDuration(int duration_) {
		duration = duration_;
	}

	void set(double target_) {
		target = target_;
		if (duration > 0 && !isReset) {
			samplesLeft = duration;
			step = (target - value) / samplesLeft;
		} else {
			value = target;
			step = 0.0;
			samplesLeft = 0;
			isReset = false;
		}
	}

	void tick() {
		if (samplesLeft > 0) {
			value += step;
			--samplesLeft;
		}
	}

	void reset() {
		isReset = true;
		value = target;
		samplesLeft = 0;
		step = 0.0;
	}
	inline double get() const { return value; }
	bool isDone() const { return samplesLeft == 0; }
};