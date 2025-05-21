#include "Phaser.h"

void Phaser::init(double srate, double freq, double q)
{
    g.set(getCoeff(freq, srate));
    k = q;
    double gg = g.get();
    remove_lows_stage.coeff = std::min(gg * kClearRatio, 0.9);
    remove_highs_stage.coeff = gg * (1.0 / kClearRatio);
    for (int i = 0; i < kMaxStages; ++i) {
        stages[i].coeff = gg;
    }
}

double Phaser::eval(double sample)
{
    double peak1 = std::clamp(1.0 - 2.0 * morph, 0.0, 1.0);
    double peak5 = std::clamp(2.0 * morph - 1.0, 0.0, 1.0);
    double peak3 = -peak1 - peak5 + 1.0;
    int invert = type == kPhaserPos ? 1 : -1;

    double lows = remove_lows_stage.eval(allpass_output);
    double highs = remove_highs_stage.eval(lows);
    double state = k.get() * (lows - highs);

    double input = sample + invert * state;
    double output;

    for (int i = 0; i < kPeakStage; ++i) {
        output = stages[i].eval(input);
        input = input + output * -2.0;
    }

    double peak1out = input;

    for (int i = kPeakStage; i < 2 * kPeakStage; ++i) {
        output = stages[i].eval(input);
        input = input + output * -2.0;
    }

    double peak3out = input;

    for (int i = 2 * kPeakStage; i < 3 * kPeakStage; ++i) {
        output = stages[i].eval(input);
        input = input + output * -2.0;
    }

    double peak5out = input;
    double peak13out = (peak1 * peak1out) + peak3 * peak3out;
    allpass_output = peak13out + peak5 * peak5out;

    return (sample + invert * allpass_output) * 0.5;
}

void Phaser::reset(double sample)
{
    remove_highs_stage.reset(sample);
    remove_lows_stage.reset(sample);
    for (int i = 0; i < kMaxStages; ++i) {
        stages[i].reset(sample);
    }
}

void Phaser::tick()
{
    g.tick();
    k.tick();
}

void Phaser::setLerp(int duration)
{
    g.setDuration(duration);
    k.setDuration(duration);
};

void Phaser::setDrive(double drive_)
{
    (void)drive_;
}