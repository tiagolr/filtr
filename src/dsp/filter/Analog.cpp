#include "Analog.h"

void Analog::init(double srate, double freq, double q)
{
	g.set(getCoeff(freq, srate));
	stage1.coeff = g.get();
	stage2.coeff = g.get();
	pre_stage1.coeff = g.get();
	pre_stage2.coeff = g.get();
	double k_t = mode == BS ? q : q * 2.15;
	k_t += drivenorm * q * kDriveResonanceBoost;
	k.set(k_t);
	double resScale = q * q * 2.0f + 1.0f;
	idrive = 1.0 / std::sqrt(resScale * drive);
}

double Analog::eval(double sample)
{
	double output = 0.0;
	double s1in = 0.0;

	if (type == kAnalog12 || mode == BS) {
		double feedback = -stage1.state + stage2.state;
		s1in = tanhLUT(drive * sample - k.get() * feedback);
		double s1out = stage1.eval(s1in);
		stage2.eval(s1out);
	}
	else {
		double feedback = -pre_stage1.state + pre_stage2.state;
		s1in = sample - feedback;
		double s1out = pre_stage1.eval(s1in);
		double s2out = pre_stage2.eval(s1out);
		double lowout = s2out;
		double bandout = s1out - lowout;
		double highout = s1in - s1out + bandout;
		double preout = mode == LP ? lowout
			: mode == BP ? bandout 
			: mode == PK ? sample + bandout 
			: highout;

		feedback = -stage1.state + stage2.state;
		s1in = tanhLUT(drive * preout - k.get() * feedback);
		s1out = stage1.eval(s1in);
		stage2.eval(s1out);
	}

	double s2in = stage1.curr;
	double low = stage2.curr;
	double band = s2in - low;
	double high = s1in - s2in - band;

	output = mode == LP ? low 
		: mode == BP ? band 
		: mode == BS ? sample - band 
		: mode == PK ? sample + band 
		: high;

	return output * idrive;
}

void Analog::reset(double sample)
{
	stage1.reset(sample);
	stage2.reset(sample);
	pre_stage1.reset(sample);
	pre_stage2.reset(sample);
	g.reset();
	k.reset();
}

void Analog::tick()
{
	g.tick();
	k.tick();
}

void Analog::setLerp(int duration) 
{
	g.setDuration(duration);
	k.setDuration(duration);
};

void Analog::setDrive(double drive_)
{
	drivenorm = drive_;
	drive = std::pow(10.0f, drive_ * F_MAX_DRIVE / 20.0f);
}