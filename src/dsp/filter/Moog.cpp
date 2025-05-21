#include "Moog.h"

void Moog::init(double srate, double freq, double q)
{
    freq = std::clamp(freq, 20.0, srate / 2.0);
    f0.set(std::exp(freq * -2.0 * juce::MathConstants<double>::pi / srate));
    k.set(mode == BS ? q * 0.5 : jmap(q, 0.1, 1.0));
}

double Moog::eval(double sample)
{
    const auto f = f0.get();
    const auto g = f * -1.0 + 1.0;
    const auto b0 = g * 0.76923076923;
    const auto b1 = g * 0.23076923076;

    const auto dx = gain * tanhLUT(drive * sample);
    const auto a  = dx + k.get() * -4.0 * (gain2 * tanhLUT(drive2 * state[4]) - dx * comp);

    const auto b = b1 * state[0] + f * state[1] + b0 * a;
    const auto c = b1 * state[1] + f * state[2] + b0 * b;
    const auto d = b1 * state[2] + f * state[3] + b0 * c;
    const auto e = b1 * state[3] + f * state[4] + b0 * d;

    state[0] = a;
    state[1] = b;
    state[2] = c;
    state[3] = d;
    state[4] = e;

    // For notch or peak mode it does not work perfectly, specially with 24db coefficients
    // Subtracting the band from the signal yields mild results, still better than nothing
    if (mode == BS)
        return sample - (b - c);

    if (mode == PK)
        return sample + (b - c);

    return a * A[0] + b * A[1] + c * A[2] + d * A[3] + e * A[4];
}

void Moog::setDrive(double drive_) {
    drive = std::pow(10.0f, drive_ * F_MAX_DRIVE / 20.0f);
    gain = 1.0 / std::pow(drive, 0.7);
    drive2 = drive * 0.01 + 0.99;
    gain2 = 1.0 / std::pow(drive2, 0.7);
};

void Moog::setMode(FilterMode mode_)
{
    mode = mode_;
    updateState();
};

void Moog::updateState()
{
    if (type == kMoog12) {
        if (mode == LP) {
            A = {{ 0.0, 0.0, 1.0, 0.0, 0.0 }};
            comp = 0.5;
        }
        else if (mode == BP) {
            A = {{ 0.0, 1.0, -1.0, 0.0, 0.0 }};
            comp = 0.5;
        }
        else {
            A = {{ 1.0, -2.0, 1.0, 0.0, 0.0 }};
            comp = 0.0;
        }
    }
    else {
        if (mode == LP) {
            A = {{ 0.0, 0.0, 0.0, 0.0, 1.0 }};
            comp = 0.5;
        }
        else if (mode == BP) {
            A = {{ 0.0, 0.0, 1.0, -2.0, 1.0 }};
            comp = 0.5;
        }
        else {
            A = {{ 1.0, -4.0, 6.0, -4.0,  1.0 }};
            comp = 0.0;
        }
    }
    if (mode == BS || mode == PK) {
        comp = 0.0;
    }

    for (auto& a : A)
        a *= 1.2; // output gain
}

void Moog::reset(double sample)
{
    state.fill(sample);
    f0.reset();
    k.reset();
}

void Moog::setLerp(int nsamples)
{
    f0.setDuration(nsamples);
    k.setDuration(nsamples);
}

void Moog::tick()
{
    f0.tick();
    k.tick();
}

