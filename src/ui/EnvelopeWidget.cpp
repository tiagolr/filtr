#include "EnvelopeWidget.h"
#include "../PluginProcessor.h"

EnvelopeWidget::EnvelopeWidget(FILTRAudioProcessor& p, bool isResenv, int width) : audioProcessor(p), isResenv(isResenv) 
{
    int col = 0;
    int row = 5;

    thresh = std::make_unique<Rotary>(p, isResenv ? "resenvthresh" : "cutenvthresh", "Thresh", RotaryLabel::gainTodB1f, false);
    addAndMakeVisible(*thresh);
    thresh->setBounds(col,row,80,65);
    col += 70;

    amount = std::make_unique<Rotary>(p, isResenv ? "resenvamt" : "cutenvamt", "Amount", RotaryLabel::percx100, true);
    addAndMakeVisible(*amount);
    amount->setBounds(col,row,80,65);
    col += 70;

    attack = std::make_unique<Rotary>(p, isResenv ? "resenvatk" : "cutenvatk", "Attack", RotaryLabel::envatk);
    addAndMakeVisible(*attack);
    attack->setBounds(col,row,80,65);
    col += 70;

    release = std::make_unique<Rotary>(p, isResenv ? "resenvrel" : "cutenvrel", "Release", RotaryLabel::envrel);
    addAndMakeVisible(*release);
    release->setBounds(col,row,80,65);
    col += 70;

    col = width - 10 - PLUG_PADDING;
    row += 3; // align buttons middle
    addAndMakeVisible(sidechainBtn);
    sidechainBtn.setTooltip("Use sidechain as envelope input");
    sidechainBtn.setBounds(col-25, row, 25, 25);
    sidechainBtn.setAlpha(0.0f);

    addAndMakeVisible(monitorBtn);
    monitorBtn.setTooltip("Monitor envelope input");
    monitorBtn.setBounds(col-25, row+35, 25,25);
    monitorBtn.setAlpha(0.0f);
    col -= 35;

    addAndMakeVisible(rmsBtn);
    rmsBtn.setTooltip("Toggle between RMS or Peak");
    rmsBtn.setBounds(col-40, row, 40, 25);
    rmsBtn.setComponentID("small");
    rmsBtn.setButtonText("RMS");

    addAndMakeVisible(arelBtn);
    arelBtn.setTooltip("Toggle between standard or adaptive release mode");
    arelBtn.setBounds(col-40, row+35, 40, 25);
    arelBtn.setComponentID("small");
    arelBtn.setButtonText("ARel");

    addAndMakeVisible(filterRange);
    filterRange.setTooltip("Frequency range of the envelope input signal");
    filterRange.setSliderStyle(Slider::SliderStyle::TwoValueHorizontal);
    filterRange.setRange(20.0, 20000.0);
    filterRange.setMinAndMaxValues(20.0, 20000.0, dontSendNotification);
    filterRange.setSkewFactor(0.5, false);
    filterRange.setTextBoxStyle(Slider::NoTextBox, false, 80, 20);
    filterRange.setBounds(release->getBounds().getRight() - 10, 20, rmsBtn.getBounds().getX() - release->getBounds().getRight() + 10 - 5, 25);
    filterRange.setColour(Slider::backgroundColourId, Colour(COLOR_BG).brighter(0.1f));
    filterRange.setColour(Slider::trackColourId, Colour(COLOR_ACTIVE).darker(0.5f));
    filterRange.setColour(Slider::thumbColourId, Colour(COLOR_ACTIVE));
    filterRange.onValueChange = [this]() {
        auto lowcut = filterRange.getMinValue();
        auto highcut = filterRange.getMaxValue();
        if (lowcut > highcut)
            filterRange.setMinAndMaxValues(highcut, highcut);

        auto lowcutstr = lowcut > 1000 ? String(int(lowcut * 10 / 1000.0) / 10.0) + "k" : String((int)lowcut);
        auto highcutstr = highcut > 1000 ? String(int(highcut * 10 / 1000.0) / 10.0) + "k" : String((int)highcut);
        filterLabel.setText(lowcutstr + "-" + highcutstr + " Hz", dontSendNotification);
    };
    filterRange.setVelocityModeParameters(1.0,1,0.0,true,ModifierKeys::Flags::shiftModifier);
    filterRange.onDragEnd = [this]() {
        filterLabel.setText("Filter", dontSendNotification);
    };

    addAndMakeVisible(filterLabel);
    filterLabel.setFont(FontOptions(16.f));
    filterLabel.setJustificationType(Justification::centredBottom);
    filterLabel.setText("Filter", dontSendNotification);
    filterLabel.setBounds(filterRange.getBounds().withBottomY(65 + 5 + 1));
}

void EnvelopeWidget::paint(juce::Graphics& g) 
{
    auto bounds = getLocalBounds().expanded(0, -1).toFloat();
    g.fillAll(Colour(COLOR_BG));
    g.setColour(Colour(isResenv ? COLOR_ACTIVE : 0xffffffff).withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.translated(0.5f, 0.5f), 3.f, 1.f);

    g.setColour(Colour(COLOR_ACTIVE));
    g.drawRoundedRectangle(sidechainBtn.getBounds().toFloat().translated(0.5f, 0.5f), 3.f, 1.f);
    g.drawRoundedRectangle(monitorBtn.getBounds().toFloat().translated(0.5f, 0.5f), 3.f, 1.f);
    drawSidechain(g, sidechainBtn.getBounds(), Colour(COLOR_ACTIVE));
    drawHeadphones(g, monitorBtn.getBounds(), Colour(COLOR_ACTIVE));
}

void EnvelopeWidget::drawHeadphones(Graphics& g, Rectangle<int> bounds, Colour c)
{
    g.setColour(c);
    auto b = bounds.toFloat().expanded(-6,-3).translated(0,4);
    Path p;
    p.startNewSubPath(b.getX(), b.getCentreY());
    p.addArc(b.getX(), b.getY(), b.getWidth(), b.getHeight(), -MathConstants<float>::halfPi, MathConstants<float>::halfPi);
    g.strokePath(p, PathStrokeType(1.5f));
    b = bounds.toFloat().expanded(-5,0).translated(0,4);
    g.fillRoundedRectangle(b.getX(), b.getCentreY() - 3.f, 3.f, 6.f, 3.f);
    g.fillRoundedRectangle(b.getRight()-3.f, b.getCentreY() - 3.f, 3.f, 6.f, 3.f);
}

void EnvelopeWidget::drawSidechain(Graphics& g, Rectangle<int> bounds, Colour c)
{
    auto b = bounds.toFloat().expanded(0.f,-6.f);
    Path p;
    float centerA = b.getX() + b.getWidth() / 3.f;
    float centerB = b.getX() + b.getWidth() / 3.f * 2;

    g.setColour(c);
    p.startNewSubPath(centerA, b.getBottom());
    p.lineTo(centerA, b.getY());
    p.startNewSubPath(centerB, b.getBottom());
    p.lineTo(centerB, b.getY());

    // draw arrowheads
    float r = 3.f;
    p.startNewSubPath(centerA-r, b.getY()+r);
    p.lineTo(centerA, b.getY());
    p.lineTo(centerA+r, b.getY()+r);

    p.startNewSubPath(centerB-r, b.getY()+r);
    p.lineTo(centerB, b.getY());
    p.lineTo(centerB+r, b.getY()+r);

    g.strokePath(p, PathStrokeType(1.f));
}

void EnvelopeWidget::layoutComponents()
{
    repaint();
}