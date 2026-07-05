#include "TptWrapper.h"

void TptWrapper::init(double _srate, double freq, double q)
{
    if (_srate != srate) {
        filter.prepare(_srate, 2000, 1);
        srate = _srate;
    }

    constexpr float sqrt2 = 0.7071067811865476f;
    filter.setCutoff((float)freq);
    filter.setResonance(sqrt2 + (float)q * 10.f);
}

void TptWrapper::setMode(FilterMode mode_)
{
    mode = mode_;
    filter.setType((int)mode);
}

double TptWrapper::eval(double sample)
{
    return (double)filter.processMono((float)sample);
}

void TptWrapper::reset(double sample)
{
    (void)sample;
    filter.reset();
}

void TptWrapper::tick()
{
    filter.updateCoefficients();
}

void TptWrapper::setLerp(int duration)
{
    filter.setLerp(duration);
};

void TptWrapper::setDrive(double drive_)
{
    (void)drive_;
}