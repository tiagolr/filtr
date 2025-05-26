#include "Follower.h"

void Follower::prepare(double srate, double thresh_, bool rms_, bool autorel_, double attack_, double hold_, double release_)
{
	thresh = thresh_;
	rms = rms_;
	autorel = autorel_;
	attack = (ENV_MIN_ATTACK + (ENV_MAX_ATTACK - ENV_MIN_ATTACK) * attack_) / 1000.0;
	hold = hold_;
	release = (ENV_MIN_RELEASE + (ENV_MAX_RELEASE - ENV_MIN_RELEASE) * release_) / 1000.0;

	double targetLevel = 0.2; // -14dB or something slow
	attackcoeff = std::exp(std::log(targetLevel) / (attack * srate));
	releasecoeff = std::exp(std::log(targetLevel) / (release * srate));
}

double Follower::process(double amp)
{
	double in = std::max(0.0, amp - thresh);
	if (in > envelope)
		envelope = attackcoeff * envelope + (1.0 - attackcoeff) * in;
	else
		envelope = releasecoeff * envelope + (1.0 - releasecoeff) * in;

	return envelope;
}