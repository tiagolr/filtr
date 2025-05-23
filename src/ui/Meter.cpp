#include "Meter.h"
#include "../PluginProcessor.h"

double gainToScale(double g) 
{
    return (std::log10(std::max(g, 0.001)) - std::log10(0.001)) / (std::log10(10) - std::log10(0.001));
}

Meter::Meter(FILTRAudioProcessor& p) : audioProcessor(p)
{
    audioProcessor.params.addParameterListener("gain", this);
    gain = audioProcessor.params.getRawParameterValue("gain")->load();
    gainMeter = gainToScale(gain);
    zeroMeter = gainToScale(1.0);

    startTimerHz(60);
}

void Meter::timerCallback()
{
    repaint();
}

Meter::~Meter()
{
    audioProcessor.params.removeParameterListener("gain", this);
}

void Meter::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "gain") {
        gain = newValue;
        gainMeter = gainToScale(gain);
    }
}

void Meter::paint(juce::Graphics& g) {
    g.fillAll(Colour(globals::COLOR_BG));
    g.setColour(Colour(COLOR_NEUTRAL));
    g.drawRoundedRectangle(getLocalBounds().expanded(-1,-1).toFloat().translated(0.5f, 0.5f), 3.f, 1.f);
    auto bounds = getLocalBounds().expanded(-4, -4).toFloat().translated(0.5f, 0.5f);

    double rmsLeft = gainToScale(audioProcessor.rmsLeft.load());
    double rmsRight = gainToScale(audioProcessor.rmsRight.load());

    g.setColour(Colour(COLOR_ACTIVE));
    if (rmsLeft > -60.0)
        g.fillRect(bounds.withTrimmedBottom(bounds.getHeight() / 2).withRight(bounds.getWidth() * (float)rmsLeft));
    if (rmsRight > -60.0)
        g.fillRect(bounds.withTrimmedTop(bounds.getHeight() / 2).withRight(bounds.getWidth() * (float)rmsRight));

    if (mouse_down) {
        g.setColour(Colour(COLOR_BG).withAlpha(0.8f));
        g.fillRect(bounds);
        g.setFont(FontOptions(16.f));
        g.setColour(Colours::white);
        g.drawFittedText(String(gain > 1.0 ? "+" : "") + String((int)(Utils::gainTodB(gain) * 10) / 10.0) + " dB", bounds.toNearestInt(), Justification::centred, 1);
    }

    g.setColour(Colour(COLOR_NEUTRAL));
    g.drawVerticalLine((int)(bounds.getX() + bounds.getWidth() * zeroMeter), (float)bounds.getY(), (float)bounds.getBottom());
    g.setColour(Colour(COLOR_ACTIVE));
    g.drawVerticalLine((int)(bounds.getX() + bounds.getWidth() * gainMeter), (float)bounds.getY(), (float)bounds.getBottom());
}

void Meter::mouseDown(const juce::MouseEvent& e)
{
    e.source.enableUnboundedMouseMovement(true);
    mouse_down = true;
    auto param = audioProcessor.params.getParameter("gain");
    cur_normed_value = param->getValue();
    last_mouse_position = e.getPosition();
    setMouseCursor(MouseCursor::NoCursor);
    param->beginChangeGesture();
}

void Meter::mouseUp(const juce::MouseEvent& e) {
    mouse_down = false;
    setMouseCursor(MouseCursor::NormalCursor);
    e.source.enableUnboundedMouseMovement(false);
    Desktop::getInstance().setMousePosition(e.getMouseDownScreenPosition());
    auto param = audioProcessor.params.getParameter("gain");
    param->endChangeGesture();
}

void Meter::mouseDrag(const juce::MouseEvent& e) {
    auto change = e.getPosition() - last_mouse_position;
    last_mouse_position = e.getPosition();
    auto speed = (e.mods.isShiftDown() ? 40.0f : 4.0f) * 200.0f;
    auto slider_change = float(change.getX() - change.getY()) / speed;
    cur_normed_value += slider_change;
    cur_normed_value = jlimit(0.0f, 1.0f, cur_normed_value);
    auto param = audioProcessor.params.getParameter("gain");

    param->setValueNotifyingHost(cur_normed_value);
}

void Meter::mouseDoubleClick(const juce::MouseEvent& e)
{
    (void)e;
    auto param = audioProcessor.params.getParameter("gain");
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getDefaultValue());
    param->endChangeGesture();
};

void Meter::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (mouse_down) return;
    auto speed = (event.mods.isShiftDown() ? 0.01f : 0.05f);
    auto slider_change = wheel.deltaY > 0 ? speed : wheel.deltaY < 0 ? -speed : 0;
    auto param = audioProcessor.params.getParameter("gain");
    param->beginChangeGesture();
    param->setValueNotifyingHost(param->getValue() + slider_change);
    while (wheel.deltaY > 0.0f && param->getValue() == 0.0f) { // FIX wheel not working when value is zero, first step takes more than 0.05%
        slider_change += 0.05f;
        param->setValueNotifyingHost(param->getValue() + slider_change);
    }
    param->endChangeGesture();
}