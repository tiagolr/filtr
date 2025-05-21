#include "TB303.h"

void TB303::init(double srate, double freq, double q)
{
    // Copyright (c) 2012 Dominique Wurtz (www.blaukraut.info)
    wc1.set(coeffLUT(0.45 * MathConstants<double>::pi * freq / srate * 0.5));
    double wc = wc1.get();
    wc2 = wc * wc;

    wc2 = wc*wc;
    wc3 = wc2*wc;
    wc4 = wc3*wc;
    b   = 1.0 / ( 1.0 + 8.0*wc + 20.0*wc2 + 16.0*wc3 + 2.0*wc4);
    g   = 2.0 * wc4 * b;

    if (mode == BS) q *= 0.13;
    k   = 16.95*q;
    A   = 1 + 0.5 * k.get();

    double dwc = 2*wc;
    double dwc2 = 2*wc2;
    double qwc2 = 4*wc2;
    double dwc3 = 2*wc3;
    double qwc3 = 4*wc3;

    b0 = dwc+12*wc2+20*wc3+8*wc4;
    a0 = 1+6*wc+10*wc2+qwc3;
    a1 = dwc+8*wc2+6*wc3;
    a2 = dwc2+wc3;
    a3 = dwc3;

    b10 = dwc2+8*wc3+6*wc4;
    a10 = wc+4*wc2+3*wc3;
    a11 = 1+6*wc+11*wc2+6*wc3;
    a12 = wc+qwc2+qwc3;
    a13 = wc2+dwc3;

    b20 = dwc3+4*wc4;
    a20 = a13;
    a21 = wc+qwc2+4*wc3;
    a22 = 1+6*wc+10*wc2+qwc3;
    a23 = wc+qwc2+dwc3;

    c2  = a21 - a3;
    c3  = 1+6*wc+9*wc2+dwc3;

    sc = (57.96533646143774 - 26.63612328945456*exp(- 0.44872755850609214 * k.get())) / 31.329213171983177;
}

double TB303::eval(double sample)
{
    sample *= drive;
    double wc = wc1.get();
    double s = (z0*wc3 + z1*a20 + z2*c2 + z3*c3) * b;
    y4 = (g * sample + s) / (1.0 + g*k.get());

    double fb = sample - k.get()*y4;
    double y0 = std::clamp(fb, -1.0, 1.0);

    y1 = b * (y0*b0 + z0*a0 + z1*a1 + z2*a2 + z3*a3);
    y2 = b * (y0*b10 + z0*a10 + z1*a11 + z2*a12 + z3*a13);  
    y3 = b * (y0*b20 + z0*a20 + z1*a21 + z2*a22 + z3*a23);
    y4 = g*y0 + s;

    z0 += 4*wc*(y0 - y1   + y2);
    z1 += 2*wc*(y1 - 2*y2 + y3);
    z2 += 2*wc*(y2 - 2*y3 + y4);
    z3 += 2*wc*(y3 - 2*y4);

    double output = 0.0;

    if (mode == LP) output = A*y4;
    else if (mode == BP) output = y4 + y2 - y1;
    else if (mode == HP) output = -(y0 - y4)*.5;
    else if (mode == BS) output = sample + (y4 + y2 - y1);
    else if (mode == PK) output = sample - (y4 + y2 - y1);
    
    return output * idrive;
}

void TB303::reset(double sample)
{
    z1 = z2 = z3 = 0.0;
    y1 = y2 = y3 = y4 = sample;
    wc1.reset();
    k.reset();
}

void TB303::tick()
{
    wc1.tick();
    k.tick();
}

void TB303::setLerp(int duration)
{
    wc1.setDuration(duration);
    k.setDuration(duration);
};

void TB303::setDrive(double drive_)
{
    drive = std::pow(10.0f, drive_ * F_MAX_DRIVE / 20.0f);
    idrive = 1.0 / std::pow(drive, 0.7);
}