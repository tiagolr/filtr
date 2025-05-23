/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Rotary.h"
#include "ui/TextDial.h"
#include "ui/GridSelector.h"
#include "ui/CustomLookAndFeel.h"
#include "ui/About.h"
#include "ui/View.h"
#include "dsp/filter/Filter.h"
#include "ui/SettingsButton.h"
#include "ui/AudioDisplay.h"
#include "ui/PaintToolWidget.h"
#include "ui/SequencerWidget.h"
#include "ui/Meter.h"

using namespace globals;

class FILTRAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::AudioProcessorValueTreeState::Listener, public juce::ChangeListener
{
public:
    FILTRAudioProcessorEditor (FILTRAudioProcessor&);
    ~FILTRAudioProcessorEditor() override;

    //==============================================================================
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void toggleUIComponents ();
    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback(ChangeBroadcaster* source) override;
    void drawGear(Graphics&g, Rectangle<int> bounds, float radius, int segs, Colour color, Colour bg);
    void drawChain(Graphics&g, Rectangle<int> boudns, Colour color, Colour bg);
    void drawUndoButton(Graphics& g, juce::Rectangle<float> area, bool invertx, Colour color);

private:
    bool init = false;
    FILTRAudioProcessor& audioProcessor;
    CustomLookAndFeel* customLookAndFeel = nullptr;
    std::unique_ptr<About> about;

    std::vector<std::unique_ptr<TextButton>> patterns;
    std::vector<std::unique_ptr<TextButton>> respatterns;

#if defined(DEBUG)
    juce::TextButton presetExport;
#endif

    Label logoLabel;
    ComboBox syncMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAttachment;
    Label patSyncLabel;
    ComboBox patSyncMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> patSyncAttachment;
    std::unique_ptr<SettingsButton> settingsButton;
    std::unique_ptr<TextDial> mixDial;

    std::unique_ptr<Rotary> cutoff;
    std::unique_ptr<Rotary> res;
    std::unique_ptr<Rotary> drive;
    std::unique_ptr<Rotary> morph;
    std::unique_ptr<Rotary> rate;
    std::unique_ptr<Rotary> smooth;
    std::unique_ptr<Rotary> attack;
    std::unique_ptr<Rotary> release;
    std::unique_ptr<Rotary> tension;
    std::unique_ptr<Rotary> tensionatk;
    std::unique_ptr<Rotary> tensionrel;
    std::unique_ptr<Rotary> threshold;
    std::unique_ptr<Rotary> sense;
    std::unique_ptr<Rotary> lowcut;
    std::unique_ptr<Rotary> highcut;
    std::unique_ptr<Rotary> offset;
    
    std::unique_ptr<Meter> meter;
    Slider cutoffset;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffsetAttachment;
    Slider resoffset;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> resoffsetAttachment;
    ComboBox algoMenu;
    TextButton linkPatsButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algoAttachment;
    ComboBox filterTypeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterTypeAttachment;
    ComboBox filterModeMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> filterModeAttachment;
    TextButton copyButton;
    TextButton pasteButton;
    TextButton useSidechain;
    TextButton useMonitor;
    TextButton nudgeRightButton;
    Label nudgeLabel;
    TextButton nudgeLeftButton;
    TextButton undoButton;
    TextButton redoButton;
    std::unique_ptr<AudioDisplay> audioDisplay;
    TextButton paintButton;
    TextButton sequencerButton;
    ComboBox pointMenu;
    Label pointLabel;
    TextButton loopButton;
    Label triggerLabel;
    ComboBox triggerMenu;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> triggerAttachment;
    TextButton audioSettingsButton;
    TextButton snapButton;
    std::unique_ptr<GridSelector> gridSelector;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> snapAttachment;
    std::unique_ptr<View> view;
    Label latencyWarning;
    std::unique_ptr<PaintToolWidget> paintWidget;
    std::unique_ptr<SequencerWidget> seqWidget;

    TooltipWindow tooltipWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FILTRAudioProcessorEditor)
};
