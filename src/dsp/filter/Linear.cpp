#include "Linear.h"

void Linear::init(double srate, double freq, double q)
{
    if (type == kLinear12) q *= 0.99;
    else q *= 0.95;

    double g_t = getCoeff(freq, srate);
    double k_t = 2 - 2*q;
    if (mode == BS) k_t = 2 - k_t; // invert notch to k relation

    double a1_t = 1 / (1 + g_t * (g_t + k_t));
    double a2_t = g_t * a1_t;
    double a3_t = g_t * a2_t;

    // set lerp targets
    g.set(g_t);
    k.set(k_t);
    a1.set(a1_t);
    a2.set(a2_t);
    a3.set(a3_t);
}

double Linear::eval(double sample)
{
    sample *= drive;

    // 12p first stage
    auto v3 = sample - ic2;
    auto v1 = a1.get() * ic1 + a2.get() * v3; // band
    auto v2 = ic2 + a2.get() * ic1 + a3.get() * v3; // low
    ic1 = 2.0 * v1 - ic1;
    ic2 = 2.0 * v2 - ic2;

    double output = 0.0;
    if (mode == LP) output = v2;
    else if (mode == BP) output = v1;
    else if (mode == HP) output = sample - k.get() * v1 - v2;
    else if (mode == BS) output = sample - k.get() * v1;
    else output = sample + (2.0 - k.get()) * v1;

        

    if (type == kLinear12) {
        if (drive > 1.0) output = hardTanh(output);
        return output * idrive;
    }

    // 24p second stage
    v3 = output - ic4;
    v1 = a1.get() * ic3 + a2.get() * v3;
    v2 = ic4 + a2.get() * ic3 + a3.get() * v3;
    ic3 = 2.0 * v1 - ic3;
    ic4 = 2.0 * v2 - ic4;

    if (mode == LP) output = v2;
    else if (mode == BP) output = v1;
    else if (mode == HP) output = output - k.get() * v1 - v2;
    else if (mode == BS) output = output - k.get() * v1;
    else output = output + (2.0 - k.get()) * v1; // peak

    if (drive > 1.0) output = hardTanh(output);
    return output * idrive;
}

void Linear::reset(double sample)
{
    ic1 = ic2 = ic3 = ic4 = sample;
    g.reset();
    k.reset();
    a1.reset();
    a2.reset();
    a3.reset();
}

void Linear::tick()
{
    g.tick();
    k.tick();
    a1.tick();
    a2.tick();
    a3.tick();
}

void Linear::setLerp(int duration) 
{
    g.setDuration(duration);
    k.setDuration(duration);
    a1.setDuration(duration);
    a2.setDuration(duration);
    a3.setDuration(duration);
};

void Linear::setDrive(double drive_)
{
    drive = std::pow(10.0f, drive_ * F_MAX_DRIVE / 20.0f);
    idrive = std::pow(drive, -0.6);
}