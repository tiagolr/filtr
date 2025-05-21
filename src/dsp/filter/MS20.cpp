#include "MS20.h"

void MS20::init(double srate, double freq, double q)
{ 
    hh.set(getCoeff(freq, srate));
    k.set(mode == BS ? q : 2 * q * 0.98);
}

double MS20::eval(double sample)
{
    sample *= drive;
    double output = 0.0;
    if (mode == LP) output = evalLP(sample);
    else if (mode == BP || mode == BS || mode == PK) output = evalBP(sample);
    else output = evalHP(sample);
    
    output *= idrive;

    if (mode == BS)
        return sample - output;
    else if (mode == PK)
        return sample + output;
    else return output;
}

double MS20::evalLP(double sample) 
{
    double gd2k = std::clamp(d2 * k.get(), -1.0, 1.0);
    double tanhterm1 = tanhLUT(-d1 + sample - gd2k);
    double tanhterm2 = tanhLUT(d1 - d2 + gd2k);

    int iter = 0;
    double res = 0.0;

    do {
        iter++;
        double ky2 = k.get()*y2;
        double gky2 = std::clamp(ky2, -1.0, 1.0);
        double dgky2 = std::abs(ky2) > 1.0 ? 0.0 : 1.0;

        double sig1 = sample - y1 - gky2;
        double thsig1 = tanhLUT(sig1);
        double thsig1sq = thsig1 * thsig1;

        double sig2 = y1 - y2 + gky2;
        double thsig2 = tanhLUT(sig2);
        double thsig2sq = thsig2 * thsig2;
        double hhthsig1sqm1 = hh.get()*(thsig1sq - 1.0);
        double hhthsig2sqm1 = hh.get()*(thsig2sq - 1.0);

        double f1 = y1 - d1 - hh.get()*(tanhterm1 + thsig1);
        double f2 = y2 - d2 - hh.get()*(tanhterm2 + thsig2);
        res = std::abs(f1) + std::abs(f2);

        double a = -hhthsig1sqm1 + 1;
        double b = -k.get()*hhthsig1sqm1*dgky2;
        double c = hhthsig2sqm1;
        double d = (k.get()*dgky2 - 1)*hhthsig2sqm1 + 1.0;

        double norm = 1.0 / ( a*d - b*c );
        y1 = y1 - ( d*f1 - b*f2 ) * norm;
        y2 = y2 - ( a*f2 - c*f1 ) * norm;

    } while (res > epsilon && iter < maxiter);

    d1 = y1;
    d2 = y2;

    return d2;
}

double MS20::evalBP(double sample) 
{
    double gd2k = std::clamp(d2 * k.get(), -1.0, 1.0);
    double tanhterm1 = tanhLUT(-d1 - sample - gd2k);
    double tanhterm2 = tanhLUT(d1 - d2 + sample + gd2k);

    int iter = 0;
    double res = 0.0;

    do {
        iter++;
        double ky2 = k.get()*y2;
        double gky2 = std::clamp(ky2, -1.0, 1.0);
        double dgky2 = std::abs(ky2) > 1.0 ? 0.0 : 1.0;

        double sig1 = -sample - y1 - gky2;
        double thsig1 = tanhLUT(sig1);
        double thsig1sq = thsig1 * thsig1;

        double sig2 = sample + y1 - y2 + gky2;
        double thsig2 = tanhLUT(sig2);
        double thsig2sq = thsig2 * thsig2;
        double hhthsig1sqm1 = hh.get()*(thsig1sq - 1.0);
        double hhthsig2sqm1 = hh.get()*(thsig2sq - 1.0);

        double f1 = y1 - d1 - hh.get()*(tanhterm1 + thsig1);
        double f2 = y2 - d2 - hh.get()*(tanhterm2 + thsig2);
        res = std::abs(f1) + std::abs(f2);

        double a = 1 - hhthsig1sqm1;
        double b = -k.get()*hhthsig1sqm1*dgky2;
        double c = hhthsig2sqm1;
        double d = (k.get()*dgky2 - 1)*hhthsig2sqm1 + 1.0;

        double norm = 1.0 / ( a*d - b*c );
        y1 = y1 - ( d*f1 - b*f2 ) * norm;
        y2 = y2 - ( a*f2 - c*f1 ) * norm;

    } while (res > epsilon && iter < maxiter);

    d1 = y1;
    d2 = y2;

    return d2;
}

double MS20::evalHP(double sample)
{
    double kc = k.get() * 0.9;
    double gkd2px = std::clamp(kc * (d2 + sample), -1.0, 1.0);
    double tanhterm1 = tanhLUT(-d1 - gkd2px);
    double tanhterm2 = tanhLUT(d1 - d2 - sample + gkd2px);

    int iter = 0;
    double res = 0.0;

    do {
        iter++;
        double kxpy2 = kc*(sample + y2);
        double gkxpy2 = std::clamp(kxpy2, -1.0, 1.0);
        double dgky2px = std::abs(kxpy2) > 1.0 ? 0.0 : 1.0;

        double sig1 = -y1 - gkxpy2;
        double thsig1 = tanhLUT(sig1);
        double thsig1sq = thsig1 * thsig1;

        double sig2 = -sample + y1 - y2 + gkxpy2;
        double thsig2 = tanhLUT(sig2);
        double thsig2sq = thsig2 * thsig2;

        double hhthsig1sqm1 = (thsig1sq - 1);
        double hhthsig2sqm1 = (thsig2sq - 1);

        double f1 = y1 - d1 - hh.get()*(tanhterm1 + thsig1);
        double f2 = y2 - d2 - hh.get()*(tanhterm2 + thsig2);
        res = std::abs(f1) + std::abs(f2);

        double a = -hhthsig1sqm1 + 1.0;
        double b = -kc*hhthsig1sqm1*dgky2px;
        double c = hhthsig2sqm1;
        double d = (kc*dgky2px - 1.0)*hhthsig2sqm1 + 1.0;

        double norm = 1.0 / ( a*d - b*c );
        y1 = y1 - ( d*f1 - b*f2 ) * norm;
        y2 = y2 - ( a*f2 - c*f1 ) * norm;

    } while (res > epsilon && iter < maxiter);

    d1 = y1;
    d2 = y2;

    return y2 + sample;
}

void MS20::reset(double sample)
{
    d2 = d1 = y1 = y2 = sample;
    hh.reset();
    k.reset();
}

void MS20::tick()
{
    hh.tick();
    k.tick();
}

void MS20::setLerp(int duration)
{
    hh.setDuration(duration);
    k.setDuration(duration);
};

void MS20::setDrive(double drive_)
{
    drive = std::pow(10.0f, drive_ * F_MAX_DRIVE / 20.0f);
    idrive = 1.0 / std::pow(drive, 0.75);
}