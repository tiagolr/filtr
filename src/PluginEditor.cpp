// Copyright 2025 tilr

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Globals.h"

FILTRAudioProcessorEditor::FILTRAudioProcessorEditor (FILTRAudioProcessor& p)
    : AudioProcessorEditor (&p)
    , audioProcessor (p)
{
    audioProcessor.loadSettings(); // load saved paint patterns from other plugin instances
    setResizable(true, false);
    setResizeLimits(PLUG_WIDTH, PLUG_HEIGHT, MAX_PLUG_WIDTH, MAX_PLUG_HEIGHT);
    setSize (audioProcessor.plugWidth, audioProcessor.plugHeight);
    setScaleFactor(audioProcessor.scale);

    audioProcessor.addChangeListener(this);
    audioProcessor.params.addParameterListener("sync", this);
    audioProcessor.params.addParameterListener("trigger", this);
    audioProcessor.params.addParameterListener("cutenvon", this);
    audioProcessor.params.addParameterListener("resenvon", this);

    auto col = PLUG_PADDING;
    auto row = PLUG_PADDING;

    // FIRST ROW

    addAndMakeVisible(logoLabel);
    logoLabel.setColour(juce::Label::ColourIds::textColourId, Colours::white);
    logoLabel.setFont(FontOptions(26.0f));
    logoLabel.setText("FILT-R", NotificationType::dontSendNotification);
    logoLabel.setBounds(col, row-3, 90, 30);
    col += 85;

#if defined(DEBUG)
    addAndMakeVisible(presetExport);
    presetExport.setAlpha(0.f);
    presetExport.setTooltip("DEBUG ONLY - Exports preset to debug console");
    presetExport.setButtonText("Export");
    presetExport.setBounds(10, 10, 100, 25);
    presetExport.onClick = [this] {
        std::ostringstream oss;
        auto points = audioProcessor.pattern->points;
        for (const auto& point : points) {
            oss << point.x << " " << point.y << " " << point.tension << " " << point.type << " ";
        }
        DBG(oss.str() << "\n");
        std::ostringstream oss2;
        points = audioProcessor.respattern->points;
        for (const auto& point : points) {
            oss2 << point.x << " " << point.y << " " << point.tension << " " << point.type << " ";
        }
        DBG(oss2.str() << "\n");
    };
#endif

    addAndMakeVisible(syncMenu);
    syncMenu.setTooltip("Tempo sync");
    syncMenu.addSectionHeading("Tempo Sync");
    syncMenu.addItem("Rate Hz", 1);
    syncMenu.addSectionHeading("Straight");
    syncMenu.addItem("1/16", 2);
    syncMenu.addItem("1/8", 3);
    syncMenu.addItem("1/4", 4);
    syncMenu.addItem("1/2", 5);
    syncMenu.addItem("1 Bar", 6);
    syncMenu.addItem("2 Bars", 7);
    syncMenu.addItem("4 Bars", 8);
    syncMenu.addSectionHeading("Triplet");
    syncMenu.addItem("1/16T", 9);
    syncMenu.addItem("1/8T", 10);
    syncMenu.addItem("1/4T", 11);
    syncMenu.addItem("1/2T", 12);
    syncMenu.addItem("1/1T", 13);
    syncMenu.addSectionHeading("Dotted");
    syncMenu.addItem("1/16.", 14);
    syncMenu.addItem("1/8.", 20);
    syncMenu.addItem("1/4.", 16);
    syncMenu.addItem("1/2.", 17);
    syncMenu.addItem("1/1.", 18);
    syncMenu.setBounds(col, row, 90, 25);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.params, "sync", syncMenu);
    col += 100;

    addAndMakeVisible(triggerLabel);
    triggerLabel.setColour(juce::Label::ColourIds::textColourId, Colour(COLOR_NEUTRAL_LIGHT));
    triggerLabel.setFont(FontOptions(16.0f));
    triggerLabel.setJustificationType(Justification::centredRight);
    triggerLabel.setText("Trigger", NotificationType::dontSendNotification);
    triggerLabel.setBounds(col, row, 60, 25);
    col += 70;

    addAndMakeVisible(triggerMenu);
    triggerMenu.setTooltip("Envelope trigger:\nSync - song playback\nMIDI - midi notes\nAudio - audio input");
    triggerMenu.addSectionHeading("Trigger");
    triggerMenu.addItem("Sync", 1);
    triggerMenu.addItem("MIDI", 2);
    triggerMenu.addItem("Audio", 3);
    triggerMenu.setBounds(col, row, 75, 25);
    triggerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.params, "trigger", triggerMenu);
    col += 85;

    addAndMakeVisible(algoMenu);
    algoMenu.setTooltip("Algorithm used for transient detection");
    algoMenu.addItem("Simple", 1);
    algoMenu.addItem("Drums", 2);
    algoMenu.setBounds(col,row,75,25);
    algoMenu.setColour(ComboBox::arrowColourId, Colour(COLOR_AUDIO));
    algoMenu.setColour(ComboBox::textColourId, Colour(COLOR_AUDIO));
    algoMenu.setColour(ComboBox::outlineColourId, Colour(COLOR_AUDIO));
    algoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.params, "algo", algoMenu);
    col += 85;

    addAndMakeVisible(audioSettingsButton);
    audioSettingsButton.setBounds(col, row, 25, 25);
    audioSettingsButton.onClick = [this]() {
        audioProcessor.showAudioKnobs = !audioProcessor.showAudioKnobs;
        if (audioProcessor.showAudioKnobs) {
            audioProcessor.showEnvelopeKnobs = false;
        }
        toggleUIComponents();
    };
    audioSettingsButton.setAlpha(0.0f);
    col += 35;

    // FIRST ROW RIGHT

    col = getWidth() - PLUG_PADDING;
    settingsButton = std::make_unique<SettingsButton>(p);
    addAndMakeVisible(*settingsButton);
    settingsButton->onScaleChange = [this]() { setScaleFactor(audioProcessor.scale); };
    settingsButton->toggleUIComponents = [this]() { toggleUIComponents(); };
    settingsButton->toggleAbout = [this]() { about.get()->setVisible(true); };
    settingsButton->setBounds(col-20,row,25,25);

    mixDial = std::make_unique<TextDial>(p, "mix", "Mix", "", TextDialLabel::tdPercx100, 12.f, COLOR_NEUTRAL_LIGHT);
    addAndMakeVisible(*mixDial);
    mixDial->setBounds(col - 20 - 10 - 30, row, 30, 25);

    meter = std::make_unique<Meter>(p);
    addAndMakeVisible(*meter);
    meter->setBounds(mixDial->getBounds().getX() - 10 - 78, row, 78, 25);

    // SECOND ROW

    row += 35;
    col = PLUG_PADDING;
    for (int i = 0; i < 12; ++i) {
        auto btn = std::make_unique<TextButton>(std::to_string(i + 1));
        btn->setRadioGroupId (1337);
        btn->setToggleState(audioProcessor.pattern->index == i, dontSendNotification);
        btn->setClickingTogglesState (false);
        btn->setColour (TextButton::textColourOffId,  Colour(COLOR_BG));
        btn->setColour (TextButton::textColourOnId,   Colour(COLOR_BG));
        btn->setColour (TextButton::buttonColourId,   Colours::white.darker(0.8f));
        btn->setColour (TextButton::buttonOnColourId, Colours::white);
        btn->setBounds (col + i * 22, row, 22+1, 25); // width +1 makes it seamless on higher DPI
        btn->setConnectedEdges (((i != 0) ? Button::ConnectedOnLeft : 0) | ((i != 11) ? Button::ConnectedOnRight : 0));
        btn->setComponentID(i == 0 ? "leftPattern" : i == 11 ? "rightPattern" : "pattern");
        btn->onClick = [i, this]() {
            audioProcessor.queuePattern(i + 1);
        };
        addAndMakeVisible(*btn);
        patterns.push_back(std::move(btn));

        btn = std::make_unique<TextButton>(std::to_string(i + 1));
        btn->setRadioGroupId (1338);
        btn->setToggleState(audioProcessor.respattern->index == i + 12, dontSendNotification);
        btn->setClickingTogglesState (false);
        btn->setColour (TextButton::textColourOffId,  Colour(COLOR_BG));
        btn->setColour (TextButton::textColourOnId,   Colour(COLOR_BG));
        btn->setColour (TextButton::buttonColourId,   Colour(COLOR_ACTIVE).darker(0.8f));
        btn->setColour (TextButton::buttonOnColourId, Colour(COLOR_ACTIVE));
        btn->setBounds (col + i * 22, row, 22+1, 25);
        btn->setConnectedEdges (((i != 0) ? Button::ConnectedOnLeft : 0) | ((i != 11) ? Button::ConnectedOnRight : 0));
        btn->setComponentID(i == 0 ? "leftPattern" : i == 11 ? "rightPattern" : "pattern");
        btn->onClick = [i, this]() {
            MessageManager::callAsync([i, this] {
                bool linkpats = (bool)audioProcessor.params.getRawParameterValue("linkpats")->load();
                if (linkpats)
                    audioProcessor.queuePattern(i + 1);
                else 
                    audioProcessor.queueResPattern(i + 1);
            });
        };
        addAndMakeVisible(*btn);
        respatterns.push_back(std::move(btn));
    }
    col += 265 + 10;

    addAndMakeVisible(linkPatsButton);
    linkPatsButton.setTooltip("Link cutoff and resonance patterns");
    linkPatsButton.setComponentID("button");
    linkPatsButton.setBounds(col,row,25,25);
    linkPatsButton.setAlpha(0.0f);
    linkPatsButton.onClick = [this] {
        MessageManager::callAsync([this] {
            bool patsync = (bool)audioProcessor.params.getRawParameterValue("linkpats")->load();
            patsync = !patsync;
            audioProcessor.params.getParameter("linkpats")->setValueNotifyingHost(patsync ? 1.0f : 0.0f);
            if (patsync) {
                if (audioProcessor.pattern->index + 12 != audioProcessor.respattern->index) {
                    audioProcessor.queueResPattern(audioProcessor.pattern->index);
                }
            }
            toggleUIComponents();
        });
    };
    col += 35;

    addAndMakeVisible(patSyncLabel);
    patSyncLabel.setColour(juce::Label::ColourIds::textColourId, Colour(COLOR_NEUTRAL_LIGHT));
    patSyncLabel.setFont(FontOptions(16.0f));
    patSyncLabel.setText("Pat. Sync", NotificationType::dontSendNotification);
    patSyncLabel.setJustificationType(Justification::centredLeft);
    patSyncLabel.setBounds(col, row, 70, 25);
    col += 80;

    addAndMakeVisible(patSyncMenu);
    patSyncMenu.setTooltip("Changes pattern in sync with song position during playback");
    patSyncMenu.addSectionHeading("Pattern Sync");
    patSyncMenu.addItem("Off", 1);
    patSyncMenu.addItem("1/4 Beat", 2);
    patSyncMenu.addItem("1/2 Beat", 3);
    patSyncMenu.addItem("1 Beat", 4);
    patSyncMenu.addItem("2 Beats", 5);
    patSyncMenu.addItem("4 Beats", 6);
    patSyncMenu.setBounds(col, row, 75, 25);
    patSyncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.params, "patsync", patSyncMenu);
    

    // KNOBS ROW
    row += 35;
    col = PLUG_PADDING;

    cutoff = std::make_unique<Rotary>(p, "cutoff", "Cutoff", RotaryLabel::hz, false, COLOR_ACTIVE, CutoffKnob);
    addAndMakeVisible(*cutoff);
    cutoff->setBounds(col,row,80,65);
    col += 75;

    res = std::make_unique<Rotary>(p, "res", "Res", RotaryLabel::percx100, false, COLOR_ACTIVE, ResKnob);
    addAndMakeVisible(*res);
    res->setBounds(col,row,80,65);
    col += 75;

    cutoffset = std::make_unique<Rotary>(p, "cutoffset", "Offset", RotaryLabel::percx100, true);
    addAndMakeVisible(*cutoffset);
    cutoffset->setTooltip("Automate this param instead of Cutoff");
    cutoffset->setBounds(col,row,80,65);

    resoffset = std::make_unique<Rotary>(p, "resoffset", "Offset", RotaryLabel::percx100, true);
    addAndMakeVisible(*resoffset);
    resoffset->setTooltip("Automate this param instead of Resonance");
    resoffset->setBounds(col,row,80,65);
    col += 75;

    drive = std::make_unique<Rotary>(p, "fdrive", "Drive", RotaryLabel::percx100);
    addAndMakeVisible(*drive);
    drive->setBounds(col,row,80,65);

    morph = std::make_unique<Rotary>(p, "fmorph", "Morph", RotaryLabel::percx100);
    addAndMakeVisible(*morph);
    morph->setBounds(col,row,80,65);
    col += 75;

    rate = std::make_unique<Rotary>(p, "rate", "Rate", RotaryLabel::hz1f);
    addAndMakeVisible(*rate);
    rate->setBounds(col,row,80,65);
    col += 75;

    smooth = std::make_unique<Rotary>(p, "smooth", "Smooth", RotaryLabel::percx100);
    addAndMakeVisible(*smooth);
    smooth->setBounds(col,row,80,65);
    col += 75;

    attack = std::make_unique<Rotary>(p, "attack", "Attack", RotaryLabel::percx100);
    addAndMakeVisible(*attack);
    attack->setBounds(col,row,80,65);
    col += 75;

    release = std::make_unique<Rotary>(p, "release", "Release", RotaryLabel::percx100);
    addAndMakeVisible(*release);
    release->setBounds(col,row,80,65);
    col += 75;

    tension = std::make_unique<Rotary>(p, "tension", "Tension", RotaryLabel::percx100, true);
    addAndMakeVisible(*tension);
    tension->setBounds(col,row,80,65);

    tensionatk = std::make_unique<Rotary>(p, "tensionatk", "TAtk", RotaryLabel::percx100, true);
    addAndMakeVisible(*tensionatk);
    tensionatk->setBounds(col,row,80,65);
    col += 75;

    tensionrel = std::make_unique<Rotary>(p, "tensionrel", "TRel", RotaryLabel::percx100, true);
    addAndMakeVisible(*tensionrel);
    tensionrel->setBounds(col,row,80,65);
    col += 75;

    // AUDIO KNOBS
    audioWidget = std::make_unique<AudioWidget>(p);
    addAndMakeVisible(*audioWidget);
    audioWidget->setBounds(PLUG_PADDING, row, PLUG_WIDTH - PLUG_PADDING * 2, 65 + 10);

    // ENVELOPE WIDGETS
    auto b = Rectangle<int>(PLUG_PADDING + 75 * 2, row, PLUG_WIDTH - (PLUG_PADDING + 75 * 2) + 10, 65);
    cutenv = std::make_unique<EnvelopeWidget>(p, false, b.getWidth());
    addAndMakeVisible(*cutenv);
    cutenv->setBounds(b.expanded(0,4));

    resenv = std::make_unique<EnvelopeWidget>(p, true, b.getWidth());
    addAndMakeVisible(*resenv);
    resenv->setBounds(b.expanded(0,5));
    

    // 3RD ROW
    col = PLUG_PADDING;
    row += 75;

    addAndMakeVisible(filterTypeMenu);
    filterTypeMenu.setComponentID("small");
    filterTypeMenu.addSectionHeading("Filter Type");
    filterTypeMenu.addItem("Linear 12", 1);
    filterTypeMenu.addItem("Linear 24", 2);
    filterTypeMenu.addItem("Analog 12", 3);
    filterTypeMenu.addItem("Analog 24", 4);
    filterTypeMenu.addItem("Moog 12", 5);
    filterTypeMenu.addItem("Moog 24", 6);
    filterTypeMenu.addItem("MS-20", 7);
    filterTypeMenu.addItem("303", 8);
    filterTypeMenu.addItem("Phaser +", 9);
    filterTypeMenu.addItem("Phaser -", 10);
    filterTypeMenu.setBounds(col, row, 75, 25);
    filterTypeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.params, "ftype", filterTypeMenu);
    col += 85;

    addAndMakeVisible(filterModeMenu);
    filterModeMenu.addSectionHeading("Filter Mode");
    filterModeMenu.addItem("LP", 1);
    filterModeMenu.addItem("BP", 2);
    filterModeMenu.addItem("HP", 3);
    filterModeMenu.addItem("Notch", 4);
    filterModeMenu.addItem("Peak", 5);
    filterModeMenu.setBounds(col, row, 75, 25);
    filterModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.params, "fmode", filterModeMenu);
    col += 85;

    col += 25;

    // 3rd ROW RIGHT
    col = getWidth() - PLUG_PADDING;
    addAndMakeVisible(cutEnvButton);
    cutEnvButton.setButtonText("Envelope");
    cutEnvButton.setComponentID("button");
    cutEnvButton.setColour(TextButton::buttonColourId, Colours::white);
    cutEnvButton.setColour(TextButton::buttonOnColourId, Colours::white);
    cutEnvButton.setColour(TextButton::textColourOnId, Colour(COLOR_BG));
    cutEnvButton.setColour(TextButton::textColourOffId, Colours::white);
    cutEnvButton.setBounds(col-90, row, 90, 25);
    cutEnvButton.onClick = [this]() {
        audioProcessor.showEnvelopeKnobs = !audioProcessor.showEnvelopeKnobs;
        if (audioProcessor.showEnvelopeKnobs && audioProcessor.showAudioKnobs) {
            audioProcessor.showAudioKnobs = false;
        }
        toggleUIComponents();
    };

    addAndMakeVisible(resEnvButton);
    resEnvButton.setButtonText("Envelope");
    resEnvButton.setComponentID("button");
    resEnvButton.setBounds(col-90, row, 90, 25);
    resEnvButton.onClick = [this]() {
        audioProcessor.showEnvelopeKnobs = !audioProcessor.showEnvelopeKnobs;
        if (audioProcessor.showEnvelopeKnobs && audioProcessor.showAudioKnobs) {
            audioProcessor.showAudioKnobs = false;
        }
        toggleUIComponents();
    };
    col -= 100;

    addAndMakeVisible(cutEnvOnButton);
    cutEnvOnButton.setBounds(col-25, row, 25, 25);
    cutEnvOnButton.setAlpha(0.f);
    cutEnvOnButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            bool on = (bool)audioProcessor.params.getRawParameterValue("cutenvon")->load();
            audioProcessor.params.getParameter("cutenvon")->setValueNotifyingHost(on ? 0.f : 1.f);
            toggleUIComponents();
        });
    };

    addAndMakeVisible(resEnvOnButton);
    resEnvOnButton.setBounds(col-25, row, 25, 25);
    resEnvOnButton.setAlpha(0.f);
    resEnvOnButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            bool on = (bool)audioProcessor.params.getRawParameterValue("resenvon")->load();
            audioProcessor.params.getParameter("resenvon")->setValueNotifyingHost(on ? 0.f : 1.f);
            toggleUIComponents();
        });
    };

    // 4th ROW
    col = PLUG_PADDING;
    row += 35;
    addAndMakeVisible(paintButton);
    paintButton.setButtonText("Paint");
    paintButton.setComponentID("button");
    paintButton.setBounds(col, row, 75, 25);
    paintButton.onClick = [this]() {
        if (audioProcessor.uimode == UIMode::PaintEdit && audioProcessor.luimode == UIMode::Paint) {
            audioProcessor.setUIMode(UIMode::Normal);
        }
        else {
            audioProcessor.togglePaintMode();
        }
    };
    col += 85;

    addAndMakeVisible(sequencerButton);
    sequencerButton.setButtonText("Seq");
    sequencerButton.setComponentID("button");
    sequencerButton.setBounds(col, row, 75, 25);
    sequencerButton.onClick = [this]() {
        if (audioProcessor.uimode == UIMode::PaintEdit && audioProcessor.luimode == UIMode::Seq) {
            audioProcessor.setUIMode(UIMode::Normal);
        }
        else {
            audioProcessor.toggleSequencerMode();
        }
    };

    col += 85;
    addAndMakeVisible(pointLabel);
    pointLabel.setText("p", dontSendNotification);
    pointLabel.setBounds(col-2,row,25,25);
    pointLabel.setVisible(false);
    col += 35-4;

    addAndMakeVisible(pointMenu);
    pointMenu.setTooltip("Point mode\nRight click points to change mode");
    pointMenu.addSectionHeading("Point Mode");
    pointMenu.addItem("Hold", 1);
    pointMenu.addItem("Curve", 2);
    pointMenu.addItem("S-Curve", 3);
    pointMenu.addItem("Pulse", 4);
    pointMenu.addItem("Wave", 5);
    pointMenu.addItem("Triangle", 6);
    pointMenu.addItem("Stairs", 7);
    pointMenu.addItem("Smooth St", 8);
    pointMenu.setBounds(col, row, 75, 25);
    pointMenu.setSelectedId(audioProcessor.pointMode + 1, dontSendNotification);
    pointMenu.onChange = [this]() {
        MessageManager::callAsync([this]() {
            audioProcessor.pointMode = pointMenu.getSelectedId() - 1;
        });
    };
    col += 85;

    addAndMakeVisible(loopButton);
    loopButton.setTooltip("Toggle continuous play");
    loopButton.setColour(TextButton::buttonColourId, Colours::transparentWhite);
    loopButton.setColour(ComboBox::outlineColourId, Colours::transparentWhite);
    loopButton.setBounds(col, row, 25, 25);
    loopButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            audioProcessor.alwaysPlaying = !audioProcessor.alwaysPlaying;
            repaint();
        });
    };
    col += 35;

    // 4TH ROW RIGHT
    col = getWidth() - PLUG_PADDING - 60;

    addAndMakeVisible(snapButton);
    snapButton.setTooltip("Toggle snap by using ctrl key");
    snapButton.setButtonText("Snap");
    snapButton.setComponentID("button");
    snapButton.setBounds(col, row, 60, 25);
    snapButton.setClickingTogglesState(true);
    snapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(audioProcessor.params, "snap", snapButton);

    col -= 60;
    gridSelector = std::make_unique<GridSelector>(p);
    gridSelector.get()->setTooltip("Grid size can also be set using mouse wheel on view");
    addAndMakeVisible(*gridSelector);
    gridSelector->setBounds(col,row,50,25);

    col -= 10+20+5;
    addAndMakeVisible(nudgeRightButton);
    nudgeRightButton.setAlpha(0.f);
    nudgeRightButton.setBounds(col, row, 20, 25);
    nudgeRightButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            if (audioProcessor.uimode == UIMode::Seq) {
                audioProcessor.sequencer->rotateRight();
                return;
            }
            double grid = (double)audioProcessor.getCurrentGrid();
            auto snapshot = audioProcessor.viewPattern->points;
            audioProcessor.viewPattern->rotate(1.0/grid);
            audioProcessor.viewPattern->buildSegments();
            audioProcessor.createUndoPointFromSnapshot(snapshot);
        });
    };

    col -= 10+20-5;
    addAndMakeVisible(nudgeLeftButton);
    nudgeLeftButton.setAlpha(0.f);
    nudgeLeftButton.setBounds(col, row, 20, 25);
    nudgeLeftButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            if (audioProcessor.uimode == UIMode::Seq) {
                audioProcessor.sequencer->rotateLeft();
                return;
            }
            double grid = (double)audioProcessor.getCurrentGrid();
            auto snapshot = audioProcessor.viewPattern->points;
            audioProcessor.viewPattern->rotate(-1.0/grid);
            audioProcessor.viewPattern->buildSegments();
            audioProcessor.createUndoPointFromSnapshot(snapshot);
        });
    };

    col -= 30;
    addAndMakeVisible(redoButton);
    redoButton.setButtonText("redo");
    redoButton.setComponentID("button");
    redoButton.setBounds(col, row, 20, 25);
    redoButton.setAlpha(0.f);
    redoButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            if (audioProcessor.uimode == UIMode::Seq) {
                audioProcessor.sequencer->redo();
            }
            else {
                audioProcessor.viewPattern->redo();
                audioProcessor.updateResFromPattern();
                audioProcessor.updateCutoffFromPattern();
            }
            repaint();
        });
    };

    col -= 35;
    addAndMakeVisible(undoButton);
    undoButton.setButtonText("undo");
    undoButton.setComponentID("button");
    undoButton.setBounds(col, row, 20, 25);
    undoButton.setAlpha(0.f);
    undoButton.onClick = [this]() {
        MessageManager::callAsync([this] {
            if (audioProcessor.uimode == UIMode::Seq) {
                audioProcessor.sequencer->undo();
            }
            else {
                audioProcessor.viewPattern->undo();
                audioProcessor.updateResFromPattern();
                audioProcessor.updateCutoffFromPattern();
            }
            repaint();
        });
    };
    row += 35;

    paintWidget = std::make_unique<PaintToolWidget>(p);
    addAndMakeVisible(*paintWidget);
    paintWidget->setBounds(PLUG_PADDING,row,PLUG_WIDTH - PLUG_PADDING * 2, 40);

    row += 50;
    col = PLUG_PADDING;
    seqWidget = std::make_unique<SequencerWidget>(p);
    addAndMakeVisible(*seqWidget);
    seqWidget->setBounds(col,row,PLUG_WIDTH - PLUG_PADDING*2, 25*2+10);

    // VIEW
    col = 0;
    row += 50;
    view = std::make_unique<View>(p);
    addAndMakeVisible(*view);
    view->setBounds(col,row,getWidth(), getHeight() - row);


    addAndMakeVisible(latencyWarning);
    latencyWarning.setText("Plugin latency has changed, restart playback", dontSendNotification);
    latencyWarning.setColour(Label::backgroundColourId, Colours::black.withAlpha(0.5f));
    latencyWarning.setJustificationType(Justification::centred);
    latencyWarning.setColour(Label::textColourId, Colour(COLOR_ACTIVE));
    latencyWarning.setBounds(view->getBounds().getCentreX() - 200, PLUG_HEIGHT - 20 - 25, 300, 25);

    // ABOUT
    about = std::make_unique<About>();
    addAndMakeVisible(*about);
    about->setBounds(getBounds());
    about->setVisible(false);

    customLookAndFeel = new CustomLookAndFeel();
    setLookAndFeel(customLookAndFeel);

    init = true;
    resized();
    toggleUIComponents();
}

FILTRAudioProcessorEditor::~FILTRAudioProcessorEditor()
{
    audioProcessor.saveSettings(); // save paint patterns to disk
    setLookAndFeel(nullptr);
    delete customLookAndFeel;
    audioProcessor.params.removeParameterListener("sync", this);
    audioProcessor.params.removeParameterListener("trigger", this);
    audioProcessor.params.removeParameterListener("cutenvon", this);
    audioProcessor.params.removeParameterListener("resenvon", this);
    audioProcessor.removeChangeListener(this);
}

void FILTRAudioProcessorEditor::changeListenerCallback(ChangeBroadcaster* source)
{
    (void)source;

    MessageManager::callAsync([this] { toggleUIComponents(); });
}

void FILTRAudioProcessorEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    (void)parameterID;
    (void)newValue;
    MessageManager::callAsync([this]() { toggleUIComponents(); });
};

void FILTRAudioProcessorEditor::toggleUIComponents()
{
    patterns[audioProcessor.pattern->index].get()->setToggleState(true, dontSendNotification);
    respatterns[audioProcessor.respattern->index - 12].get()->setToggleState(true, dontSendNotification);
    bool isResMode = audioProcessor.resonanceEditMode;
    for (int i = 0; i < 12; ++i) {
        patterns[i]->setVisible(!isResMode);
        respatterns[i]->setVisible(isResMode);
    }
    cutoffset->setVisible(!isResMode);
    resoffset->setVisible(isResMode);

    auto ftype = (int)audioProcessor.params.getRawParameterValue("ftype")->load();
    auto trigger = (int)audioProcessor.params.getRawParameterValue("trigger")->load();
    auto triggerColor = trigger == 0 ? COLOR_ACTIVE : trigger == 1 ? COLOR_MIDI : COLOR_AUDIO;
    triggerMenu.setColour(ComboBox::arrowColourId, Colour(triggerColor));
    triggerMenu.setColour(ComboBox::textColourId, Colour(triggerColor));
    triggerMenu.setColour(ComboBox::outlineColourId, Colour(triggerColor));
    algoMenu.setVisible(trigger == Trigger::Audio);
    audioSettingsButton.setVisible(trigger == Trigger::Audio);
    if (!audioSettingsButton.isVisible()) {
        audioProcessor.showAudioKnobs = false;
    }


    loopButton.setVisible(trigger > 0);

    int sync = (int)audioProcessor.params.getRawParameterValue("sync")->load();
    bool showAudioKnobs = audioProcessor.showAudioKnobs;
    bool isPhaser = ftype == kPhaserPos || ftype == kPhaserNeg;

    filterModeMenu.setVisible(!isPhaser);

    // layout knobs
    drive->setVisible(!isPhaser);
    morph->setVisible(isPhaser);
    tension->setVisible(!audioProcessor.dualTension);
    tensionatk->setVisible(audioProcessor.dualTension);
    tensionrel->setVisible(audioProcessor.dualTension);
    
    {
        auto col = drive->getBounds().getX();
        auto row = drive->getBounds().getY();
        col += 75;
        
        if (audioProcessor.dualSmooth) {
            smooth->setVisible(false);
            attack->setVisible(true);
            release->setVisible(true);
            attack->setTopLeftPosition(col, row);
            col += 75;
            release->setTopLeftPosition(col, row);
            col+= 75;
        }
        else {
            smooth->setVisible(true);
            attack->setVisible(false);
            release->setVisible(false);
            smooth->setTopLeftPosition(col, row);
            col += 75;
        }
        tension->setTopLeftPosition(col, row);
        tensionatk->setTopLeftPosition(col, row);
        col += 75;
        tensionrel->setTopLeftPosition(col, row);
        if (audioProcessor.dualTension) col += 75;
        rate->setVisible(sync == 0);
        rate->setTopLeftPosition(col, row);
        if (rate->isVisible())
            col += 75;
    }

    audioWidget->setVisible(showAudioKnobs);
    audioWidget->toggleUIComponents();

    latencyWarning.setVisible(audioProcessor.showLatencyWarning);

    paintWidget->setVisible(audioProcessor.showPaintWidget);
    seqWidget->setVisible(audioProcessor.showSequencer);
    seqWidget->setBounds(seqWidget->getBounds().withY(paintWidget->isVisible() 
        ? paintWidget->getBounds().getBottom() + 10
        : paintWidget->getBounds().getY()
    ).withWidth(getWidth() - PLUG_PADDING * 2));

    if (seqWidget->isVisible()) {
        view->setBounds(view->getBounds().withTop(seqWidget->getBottom()));
    }
    else if (paintWidget->isVisible()) {
        view->setBounds(view->getBounds().withTop(paintWidget->getBounds().getBottom()));
    }
    else {
        view->setBounds(view->getBounds().withTop(paintWidget->getBounds().getY() - 10));
    }

    auto uimode = audioProcessor.uimode;
    paintButton.setToggleState(uimode == UIMode::Paint || (uimode == UIMode::PaintEdit && audioProcessor.luimode == UIMode::Paint), dontSendNotification);
    sequencerButton.setToggleState(audioProcessor.sequencer->isOpen, dontSendNotification);
    paintWidget->toggleUIComponents();

    cutEnvButton.setVisible(!isResMode);
    cutEnvButton.setToggleState(audioProcessor.showEnvelopeKnobs, dontSendNotification);
    cutEnvOnButton.setVisible(!isResMode);
    resEnvButton.setVisible(isResMode);
    resEnvButton.setToggleState(audioProcessor.showEnvelopeKnobs, dontSendNotification);
    resEnvOnButton.setVisible(isResMode);

    cutenv->setVisible(!isResMode && audioProcessor.showEnvelopeKnobs);
    resenv->setVisible(isResMode && audioProcessor.showEnvelopeKnobs);

    cutenv->layoutComponents();
    resenv->layoutComponents();

    repaint();
}

//==============================================================================

void FILTRAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll(Colour(COLOR_BG));
    auto bounds = getLocalBounds().withTop(view->getBounds().getY() + 10).withHeight(3).toFloat();
    if (audioProcessor.uimode == UIMode::Seq)
        bounds = bounds.withY((float)seqWidget->getBounds().getBottom() + 10);

    auto grad = ColourGradient(
        Colours::black.withAlpha(0.25f),
        bounds.getTopLeft(),
        Colours::transparentBlack,
        bounds.getBottomLeft(),
        false
    );
    g.setGradientFill(grad);
    g.fillRect(bounds);

    // draw point mode icon
    g.setColour(Colour(COLOR_NEUTRAL));
    g.drawEllipse(pointLabel.getBounds().expanded(-2,-2).toFloat(), 1.f);
    g.fillEllipse(pointLabel.getBounds().expanded(-10,-10).toFloat());

    // draw filter icon
    //bounds = Rectangle<int>(filterModeMenu.getRight() + 10, filterModeMenu.getY(), 20, 25).expanded(0, -7).toFloat();
    //Path fpath;
    //fpath.startNewSubPath(bounds.getX(), bounds.getY());
    //fpath.lineTo(bounds.getX()+6, bounds.getY());
    //fpath.cubicTo((bounds.getX() + 6 + bounds.getRight()) / 2, bounds.getY(),(bounds.getX() + 6 + bounds.getRight()) / 2, bounds.getY(), bounds.getRight(), bounds.getBottom());
    //g.strokePath(fpath, PathStrokeType(1.f));

    bounds = audioProcessor.resonanceEditMode ? res->getBounds().toFloat() : cutoff->getBounds().toFloat();
    bounds.removeFromTop(50.f);
    g.setColour((audioProcessor.resonanceEditMode ? Colour(COLOR_ACTIVE) : Colours::white).withAlpha(0.3f));
    g.fillRoundedRectangle(bounds.toFloat().expanded(-8.f, 2.f).translated(0.5f, 0.5f), 3.f);

    // draw loop play button
    auto trigger = (int)audioProcessor.params.getRawParameterValue("trigger")->load();
    if (trigger != Trigger::Sync) {
        if (audioProcessor.alwaysPlaying) {
            g.setColour(Colours::yellow);
            auto loopBounds = loopButton.getBounds().expanded(-5);
            g.fillRect(loopBounds.removeFromLeft(5));
            loopBounds = loopButton.getBounds().expanded(-5);
            g.fillRect(loopBounds.removeFromRight(5));
        }
        else {
            g.setColour(Colour(0xff00ff00));
            juce::Path triangle;
            auto loopBounds = loopButton.getBounds().expanded(-5);
            triangle.startNewSubPath(0.0f, 0.0f);
            triangle.lineTo(0.0f, (float)loopBounds.getHeight());
            triangle.lineTo((float)loopBounds.getWidth(), loopBounds.getHeight() / 2.f);
            triangle.closeSubPath();
            g.fillPath(triangle, AffineTransform::translation((float)loopBounds.getX(), (float)loopBounds.getY()));
        }
    }

    // draw audio settings button outline
    if (audioSettingsButton.isVisible() && audioProcessor.showAudioKnobs) {
        g.setColour(Colour(COLOR_AUDIO));
        g.fillRoundedRectangle(audioSettingsButton.getBounds().toFloat(), 3.0f);
        drawGear(g, audioSettingsButton.getBounds(), 10, 6, Colour(COLOR_BG), Colour(COLOR_AUDIO));
    }
    else if (audioSettingsButton.isVisible()) {
        drawGear(g, audioSettingsButton.getBounds(), 10, 6, Colour(COLOR_AUDIO), Colour(COLOR_BG));
    }

    // draw pattern link button
    bool linkpats = (bool)audioProcessor.params.getRawParameterValue("linkpats")->load();
    g.setColour(Colour(COLOR_ACTIVE));
    if (linkpats) {
        g.fillRoundedRectangle(linkPatsButton.getBounds().toFloat(), 3.0f);
        drawChain(g, linkPatsButton.getBounds(), Colour(COLOR_BG), Colour(COLOR_ACTIVE));
    }
    else {
        drawChain(g, linkPatsButton.getBounds(), Colour(COLOR_ACTIVE), Colour(COLOR_BG));
    }

    // draw rotate pat triangles
    g.setColour(Colour(COLOR_ACTIVE));
    auto triCenter = nudgeLeftButton.getBounds().toFloat().getCentre();
    auto triRadius = 5.f;
    juce::Path nudgeLeftTriangle;
    nudgeLeftTriangle.addTriangle(
        triCenter.translated(-triRadius, 0),
        triCenter.translated(triRadius, -triRadius),
        triCenter.translated(triRadius, triRadius)
    );
    g.fillPath(nudgeLeftTriangle);

    triCenter = nudgeRightButton.getBounds().toFloat().getCentre();
    juce::Path nudgeRightTriangle;
    nudgeRightTriangle.addTriangle(
        triCenter.translated(-triRadius, -triRadius),
        triCenter.translated(-triRadius, triRadius),
        triCenter.translated(triRadius, 0)
    );
    g.fillPath(nudgeRightTriangle);

    // draw undo redo buttons
    auto canUndo = audioProcessor.uimode == UIMode::Seq
        ? !audioProcessor.sequencer->undoStack.empty()
        : !audioProcessor.viewPattern->undoStack.empty();

    auto canRedo = audioProcessor.uimode == UIMode::Seq
        ? !audioProcessor.sequencer->redoStack.empty()
        : !audioProcessor.viewPattern->redoStack.empty();

    drawUndoButton(g, undoButton.getBounds().toFloat(), true, Colour(canUndo ? COLOR_ACTIVE : COLOR_NEUTRAL));
    drawUndoButton(g, redoButton.getBounds().toFloat(), false, Colour(canRedo ? COLOR_ACTIVE : COLOR_NEUTRAL));

    // envelope draws
    bool isResMode = audioProcessor.resonanceEditMode;
    bool isCutEnvOn = (bool)audioProcessor.params.getRawParameterValue("cutenvon")->load();
    bool isResEnvOn = (bool)audioProcessor.params.getRawParameterValue("resenvon")->load();

    // draw envelope button extension
    if (audioProcessor.showEnvelopeKnobs) {
        g.setColour(Colour(isResMode ? COLOR_ACTIVE : 0xffffffff));
        g.fillRect(cutEnvButton.getBounds().expanded(0, 20).translated(0, -20-15));
    }

    if (!isResMode) {
        g.setColour(Colours::white);
        bounds = cutEnvOnButton.getBounds().toFloat().translated(0.5f, 0.5f);
        if (isCutEnvOn) {
            drawPowerButton(g, bounds, Colour(COLOR_ACTIVE));
        }
        else {
            drawPowerButton(g, bounds, Colour(COLOR_NEUTRAL));
        }
    }
    else {
        g.setColour(Colour(COLOR_ACTIVE));
        bounds = resEnvOnButton.getBounds().toFloat().translated(0.5f, 0.5f);
        if (isResEnvOn) {
            drawPowerButton(g, bounds, Colour(COLOR_ACTIVE));
        }
        else {
            drawPowerButton(g, bounds, Colour(COLOR_NEUTRAL));
        }
    }
}

void FILTRAudioProcessorEditor::drawPowerButton(Graphics& g, Rectangle<float> bounds, Colour color)
{
    bounds.expand(-6,-6);
    auto pi = MathConstants<float>::pi;
    g.setColour(color);
    Path p;
    p.addArc(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 0.75f, 2.f * pi - 0.75f, true);
    p.startNewSubPath(bounds.getCentreX(), bounds.getY() - 2);
    p.lineTo(bounds.getCentreX(), bounds.getY() + 4);
    g.strokePath(p, PathStrokeType(2.f, PathStrokeType::curved, PathStrokeType::rounded));
}

void FILTRAudioProcessorEditor::drawGear(Graphics& g, Rectangle<int> bounds, float radius, int segs, Colour color, Colour background)
{
    float x = bounds.toFloat().getCentreX();
    float y = bounds.toFloat().getCentreY();
    float oradius = radius;
    float iradius = radius / 3.f;
    float cradius = iradius / 1.5f; 
    float coffset = MathConstants<float>::twoPi;
    float inc = MathConstants<float>::twoPi / segs;

    g.setColour(color);
    g.fillEllipse(x-oradius,y-oradius,oradius*2.f,oradius*2.f);

    g.setColour(background);
    for (int i = 0; i < segs; i++) {
        float angle = coffset + i * inc;
        float cx = x + std::cos(angle) * oradius;
        float cy = y + std::sin(angle) * oradius;
        g.fillEllipse(cx - cradius, cy - cradius, cradius * 2, cradius * 2);
    }
    g.fillEllipse(x-iradius, y-iradius, iradius*2.f, iradius*2.f);
}

void FILTRAudioProcessorEditor::drawChain(Graphics& g, Rectangle<int> bounds, Colour color, Colour background)
{
    (void)background;
    float x = bounds.toFloat().getCentreX();
    float y = bounds.toFloat().getCentreY();
    float rx = 10.f;
    float ry = 5.f;

    g.setColour(color);
    Path p;
    p.addRoundedRectangle(x-rx, y-ry/2, rx, ry, 2.0f, 2.f);
    p.addRoundedRectangle(x, y-ry/2, rx, ry, 2.0f, 2.f);
    p.applyTransform(AffineTransform::rotation(MathConstants<float>::pi / 4.f, x, y));
    g.strokePath(p, PathStrokeType(2.f));
}

void FILTRAudioProcessorEditor::drawUndoButton(Graphics& g, juce::Rectangle<float> area, bool invertx, Colour color)
{
        auto bounds = area;
        auto thickness = 2.f;
        float left = bounds.getX();
        float right = bounds.getRight();
        float top = bounds.getCentreY() - 4;
        float bottom = bounds.getCentreY() + 4;
        float centerY = bounds.getCentreY();
        float shaftStart = right - 7;

        Path arrowPath;
        // arrow head
        arrowPath.startNewSubPath(right, centerY);
        arrowPath.lineTo(shaftStart, top);
        arrowPath.startNewSubPath(right, centerY);
        arrowPath.lineTo(shaftStart, bottom);

        // shaft
        float radius = (bottom - centerY);
        arrowPath.startNewSubPath(right, centerY);
        arrowPath.lineTo(left + radius - 1, centerY);

        // semi circle
        arrowPath.startNewSubPath(left + radius, centerY);
        arrowPath.addArc(left, centerY, radius, radius, 2.f * float_Pi, float_Pi);

        if (invertx) {
            AffineTransform flipTransform = AffineTransform::scale(-1.0f, 1.0f)
                .translated(bounds.getWidth(), 0);

            // First move the path to origin, apply transform, then move back
            arrowPath.applyTransform(AffineTransform::translation(-bounds.getPosition()));
            arrowPath.applyTransform(flipTransform);
            arrowPath.applyTransform(AffineTransform::translation(bounds.getPosition()));
        }

        g.setColour(color);
        g.strokePath(arrowPath, PathStrokeType(thickness));
}

void FILTRAudioProcessorEditor::resized()
{
    if (!init) return; // defer resized() call during constructor

    // layout right aligned components and view
    // first row
    auto col = getWidth() - PLUG_PADDING;
    auto bounds = settingsButton->getBounds();
    settingsButton->setBounds(bounds.withX(col - bounds.getWidth()));
    mixDial->setBounds(mixDial->getBounds().withRightX(settingsButton->getBounds().getX() - 10));
    meter->setBounds(meter->getBounds().withRightX(mixDial->getBounds().getX() - 10));

    audioWidget->setBounds(audioWidget->getBounds().withWidth(getWidth() - PLUG_PADDING * 2));

    about->setBounds(0,0,getWidth(), getHeight());

    resenv->setBounds(resenv->getBounds().withRightX(getWidth() + 10));
    cutenv->setBounds(resenv->getBounds().withRightX(getWidth() + 10));

    // 3rd row
    resEnvButton.setBounds(resEnvButton.getBounds().withRightX(col));
    cutEnvButton.setBounds(cutEnvButton.getBounds().withRightX(col));
    cutEnvOnButton.setBounds(cutEnvOnButton.getBounds().withRightX(cutEnvButton.getBounds().getX() - 10));
    resEnvOnButton.setBounds(resEnvOnButton.getBounds().withRightX(resEnvButton.getBounds().getX() - 10));

    // 4th row
    bounds = snapButton.getBounds();
    auto dx = (col - bounds.getWidth()) - bounds.getX();
    snapButton.setBounds(snapButton.getBounds().translated(dx, 0));
    gridSelector->setBounds(gridSelector->getBounds().translated(dx, 0));
    nudgeLeftButton.setBounds(nudgeLeftButton.getBounds().translated(dx, 0));
    nudgeRightButton.setBounds(nudgeRightButton.getBounds().translated(dx, 0));
    redoButton.setBounds(redoButton.getBounds().translated(dx, 0));
    undoButton.setBounds(undoButton.getBounds().translated(dx, 0));

    // view
    bounds = view->getBounds();
    view->setBounds(bounds.withWidth(getWidth()).withHeight(getHeight() - bounds.getY()));

    bounds = seqWidget->getBounds();
    seqWidget->setBounds(bounds.withWidth(getWidth() - PLUG_PADDING * 2));

    bounds = latencyWarning.getBounds();
    latencyWarning.setBounds(bounds
        .withX(view->getBounds().getCentreX() - bounds.getWidth() / 2)
        .withY(getHeight() - 20 - bounds.getHeight())
    );

    audioProcessor.plugWidth = getWidth();
    audioProcessor.plugHeight = getHeight();
}
