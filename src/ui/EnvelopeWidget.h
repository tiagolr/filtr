#pragma once

#include <JuceHeader.h>
#include "../Globals.h"
#include "Rotary.h"

using namespace globals;
class FILTRAudioProcessor;

class EnvelopeWidget : public juce::Component
{
public:
    std::unique_ptr<Rotary> thresh;
    std::unique_ptr<Rotary> amount;
    std::unique_ptr<Rotary> attack;
    std::unique_ptr<Rotary> release;
    Slider filterRange;
    Label filterLabel;

    TextButton sidechainBtn;
    TextButton monitorBtn;
    TextButton rmsBtn;
    TextButton arelBtn;

    EnvelopeWidget(FILTRAudioProcessor& p, bool isResenv, int width);

    void paint(juce::Graphics& g) override;
    void layoutComponents();

    void drawHeadphones(Graphics& g, Rectangle<int> bounds, Colour c);
    void drawSidechain(Graphics& g, Rectangle<int> bounds, Colour c);

private:
    bool isResenv;
    FILTRAudioProcessor& audioProcessor;
};