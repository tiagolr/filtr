// Three band splitter - 6dB based of 3-band splitter JSFX by LOSER
// 12dB and 24dB based of Frequency Splitter - Linkwitz-Riley Minimum Phase (lewloiwc)
// Tilr
#pragma once
#include <algorithm>
#include <cmath>

struct SVFAllpass1p
{
	static constexpr double PI = 3.14159265358979323846;
	double i;
	double c;
	double cut;

	void setFreq(float srate, double cutoff)
	{
		if (cutoff != cut) {
			c = std::tan((PI * (cutoff / srate - 0.25f))) * 0.5 + 0.5;
			cut = cutoff;
		}
	}

	double process(double x)
	{
		double r = (1 - c) * i + c * x;
		i = 2 * r - i;
		return x - 2 * r;
	}

	void copyFrom(SVFAllpass1p& ap)
	{
		c = ap.c;
		cut = ap.cut;
	}

	void clear()
	{
		i = 0.;
	}
};

struct SVFAllpass2p
{
	static constexpr double PI = 3.14159265358979323846;
	double g;
	double k;
	double a1;
	double a2;
	double ic1eq;
	double ic2eq;
	double cut;

	void setFreq(float srate, double cutoff, double Q)
	{
		if (cutoff + Q != cut) {
			g = std::tan(PI * cutoff / srate);
			k = 1.0 / Q;
			a1 = 1.0 / (1.0 + g * (g + k));
			a2 = g * a1;
			cut = cutoff + Q;
		}
	}

	double process(double x)
	{
		double v1 = a1 * ic1eq + a2 * (x - ic2eq);
		double v2 = ic2eq + g * v1;
		ic1eq = 2 * v1 - ic1eq;
		ic2eq = 2 * v2 - ic2eq;

		return x - 2 * k * v1;
	}

	void copyFrom(SVFAllpass2p& ap)
	{
		g = ap.g;
		k = ap.k;
		a1 = ap.a1;
		a2 = ap.a2;
		cut = ap.cut;
	}

	void clear()
	{
		ic1eq = ic2eq = 0.;
	}
};

struct SVFLow
{
	static constexpr float PI = 3.14159265358979323846f;
	double g;
	double k;
	double a1;
	double a2;
	double ic1eq;
	double ic2eq;
	double cut;

	void setFreq(float srate, double cutoff, double Q)
	{
		if (cutoff + Q != cut) {
			g = std::tan(PI * cutoff / srate);
			k = 1 / Q;
			a1 = 1 / (1 + g * (g + k));
			a2 = g * a1;
			cut = cutoff + Q;
		}
	}

	double process(double x)
	{
		double v1 = a1 * ic1eq + a2 * (x - ic2eq);
		double v2 = ic2eq + g * v1;
		ic1eq = 2 * v1 - ic1eq;
		ic2eq = 2 * v2 - ic2eq;

		return v2;
	}

	void copyFrom(SVFLow& ap)
	{
		g = ap.g;
		k = ap.k;
		a1 = ap.a1;
		a2 = ap.a2;
		cut = ap.cut;
	}

	void clear()
	{
		ic1eq = ic2eq = 0.f;
	}
};

struct SVFHigh
{
	static constexpr double PI = 3.14159265358979323846;
	double g;
	double k;
	double a1;
	double a2;
	double ic1eq;
	double ic2eq;
	double cut;

	void setFreq(float srate, double cutoff, double Q)
	{
		if (cutoff + Q != cut) {
			g = std::tan(PI * cutoff / srate);
			k = 1 / Q;
			a1 = 1 / (1 + g * (g + k));
			a2 = g * a1;
			cut = cutoff + Q;
		}
	}

	double process(double x)
	{
		double v1 = a1 * ic1eq + a2 * (x - ic2eq);
		double v2 = ic2eq + g * v1;
		ic1eq = 2 * v1 - ic1eq;
		ic2eq = 2 * v2 - ic2eq;

		return x - k * v1 - v2;
	}

	void copyFrom(SVFHigh& ap)
	{
		g = ap.g;
		k = ap.k;
		a1 = ap.a1;
		a2 = ap.a2;
		cut = ap.cut;
	}

	void clear()
	{
		ic1eq = ic2eq = 0.;
	}
};

struct SVFStack
{
	SVFLow lowa;
	SVFLow lowa2;
	SVFLow lowb;
	SVFLow lowb2;
	SVFHigh mida;
	SVFHigh mida2;
	SVFLow midb;
	SVFLow midb2;
	SVFAllpass1p higha;
	SVFAllpass2p higha2;
	SVFHigh highb;
	SVFHigh highb2;

	void copyFrom(SVFStack b)
	{
		lowa.copyFrom(b.lowa);
		lowa2.copyFrom(b.lowa2);
		lowb.copyFrom(b.lowb);
		lowb2.copyFrom(b.lowb2);
		mida.copyFrom(b.mida);
		mida2.copyFrom(b.mida2);
		midb.copyFrom(b.midb);
		midb2.copyFrom(b.midb2);
		higha.copyFrom(b.higha);
		higha2.copyFrom(b.higha2);
		highb.copyFrom(b.highb);
		highb2.copyFrom(b.highb2);
	}

	void clear()
	{
		lowa.clear();
		lowa2.clear();
		lowb.clear();
		lowb2.clear();
		mida.clear();
		mida2.clear();
		midb.clear();
		midb2.clear();
		higha.clear();
		higha2.clear();
		highb.clear();
		highb2.clear();
	}
};

class Splitter
{
public:
	static constexpr double PI = 3.14159265358979323846;
	static constexpr double q12 = 0.5;
	static constexpr double q24 = 0.7071067811865476;

	double freqLP = 20;
	double freqHP = 20000;
	bool active = false;

	Splitter() {}
	~Splitter() {}

	void setFreqs(float srate, float hp, float lp, int slope);
	void processBlock(int slope, const double* left, const double* right, double* lowl, double* lowr, double* midl, double* midr, double* hil, double* hir, int nsamps);
	void processBlock6dB(const double* left, const double* right, double* lowl, double* lowr, double* midl, double* midr, double* hil, double* hir, int nsamps);
	void processBlock12dB(const double* left, const double* right, double* lowl, double* lowr, double* midl, double* midr, double* hil, double* hir, int nsamps);
	void processBlock24dB(const double* left, const double* right, double* lowl, double* lowr, double* midl, double* midr, double* hil, double* hir, int nsamps);
	void clear();

private:
	SVFStack svfL{};
	SVFStack svfR{};

	// 6dB
	double xHP = 0.f;
	double a0HP = 0.f;
	double b1HP = 0.f;
	double hpL = 0.f;
	double hpR = 0.f;

	double xLP = 0.f;
	double a0LP = 0.f;
	double b1LP = 0.f;
	double lpL = 0.f;
	double lpR = 0.f;
};