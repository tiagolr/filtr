#pragma once

#include <JuceHeader.h>
#include "../Globals.h"
#include "../dsp/Utils.h"

using namespace globals;
class FILTRAudioProcessor;

class Meter : public juce::SettableTooltipClient, public juce::Component, private juce::AudioProcessorValueTreeState::Listener, private juce::Timer 
{
public:
    Meter(FILTRAudioProcessor& p);
    ~Meter() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;

    void paint(juce::Graphics& g) override;

    void mouseDown(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;


private:
    FILTRAudioProcessor& audioProcessor;
    double gain = 0.0;
    double gainMeter = 0.0;
    double zeroMeter = 0.0;
    bool mouse_down = false;
    float cur_normed_value = 0.0;
    Point<int> last_mouse_position;
};