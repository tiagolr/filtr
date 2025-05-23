 // Copyright 2025 tilr

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <ctime>

static float noSnap(float min, float max, float value)
{
    (void)min;
    (void)max;
    return value;
}

FILTRAudioProcessor::FILTRAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
         .withInput("Input", juce::AudioChannelSet::stereo(), true)
         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
     )
    , settings{}
    , params(*this, &undoManager, "PARAMETERS", {
        std::make_unique<juce::AudioParameterFloat>("mix", "Mix", 0.0f, 1.0f, 1.0f),
        std::make_unique<juce::AudioParameterInt>("pattern", "Cutoff Pattern", 1, 12, 1),
        std::make_unique<juce::AudioParameterInt>("respattern", "Res Pattern", 1, 12, 1),
        std::make_unique<juce::AudioParameterBool>("linkpats", "Link Patterns", true),
        std::make_unique<juce::AudioParameterChoice>("patsync", "Pattern Sync", StringArray { "Off", "1/4 Beat", "1/2 Beat", "1 Beat", "2 Beats", "4 Beats"}, 0),
        std::make_unique<juce::AudioParameterChoice>("trigger", "Trigger", StringArray { "Sync", "MIDI", "Audio" }, 0),
        std::make_unique<juce::AudioParameterChoice>("sync", "Sync", StringArray { "Rate Hz", "1/16", "1/8", "1/4", "1/2", "1/1", "2/1", "4/1", "1/16t", "1/8t", "1/4t", "1/2t", "1/1t", "1/16.", "1/8.", "1/4.", "1/2.", "1/1." }, 5),
        std::make_unique<juce::AudioParameterFloat>("rate", "Rate Hz", juce::NormalisableRange<float>(0.01f, 5000.0f, 0.01f, 0.2f), 1.0f),
        std::make_unique<juce::AudioParameterFloat>("phase", "Phase", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("min", "Min", 0.0f, 1.0f, 0.0f),
        std::make_unique<juce::AudioParameterFloat>("max", "Max", 0.0f, 1.0f, 1.0f),
        std::make_unique<juce::AudioParameterFloat>("smooth", "Smooth", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("attack", "Attack", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("release", "Release", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("tension", "Tension", -1.0f, 1.0f, 0.0f),
        std::make_unique<juce::AudioParameterFloat>("tensionatk", "Attack Tension", -1.0f, 1.0f, 0.0f),
        std::make_unique<juce::AudioParameterFloat>("tensionrel", "Release Tension", -1.0f, 1.0f, 0.0f),
        std::make_unique<juce::AudioParameterBool>("snap", "Snap", false),
        std::make_unique<juce::AudioParameterInt>("grid", "Grid", 0, (int)std::size(GRID_SIZES)-1, 2),
        std::make_unique<juce::AudioParameterFloat>("gain", "GainDb", juce::NormalisableRange<float> (0.0f, 10.0f, 0.001f, 0.5f), 1.0f),
        // filter params
        std::make_unique<juce::AudioParameterChoice>("ftype", "Filter Type", StringArray { "Linear 12", "Linear 24", "Analog 12", "Analog 24", "Moog 12", "Moog 24", "MS-20", "303", "Phaser+", "Phaser-" }, 0),
        std::make_unique<juce::AudioParameterChoice>("fmode", "Filter Mode", StringArray { "Low Pass", "Band Pass", "High Pass", "Band Stop", "Peak" }, 0),
        std::make_unique<juce::AudioParameterFloat>("flerp", "Filter Lerp", juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f),
        std::make_unique<juce::AudioParameterFloat>("fdrive", "Filter Drive", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("fmorph", "Filter Morph", juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("cutoff", "Cutoff", juce::NormalisableRange<float>((float)F_MIN_FREQ, (float)F_MAX_FREQ, Utils::normalToFreqf, Utils::freqToNormalf, noSnap), (float)F_MIN_FREQ),
        std::make_unique<juce::AudioParameterFloat>("res", "Resonance", juce::NormalisableRange<float> (0.0f, 1.0f), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("cutoffset", "Cutoff Offset", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f, 0.75f, true), 0.0f),
        std::make_unique<juce::AudioParameterFloat>("resoffset", "Resonance Offset", juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f, 0.75f, true), 0.0f),
        // audio trigger params
        std::make_unique<juce::AudioParameterChoice>("algo", "Audio Algorithm", StringArray { "Simple", "Drums" }, 0),
        std::make_unique<juce::AudioParameterFloat>("threshold", "Audio Threshold", NormalisableRange<float>(0.0f, 1.0f), 0.5f),
        std::make_unique<juce::AudioParameterFloat>("sense", "Audio Sensitivity", 0.0f, 1.0f, 0.5f),
        std::make_unique<juce::AudioParameterFloat>("lowcut", "Audio LowCut", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f) , 20.f),
        std::make_unique<juce::AudioParameterFloat>("highcut", "Audio HighCut", juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 0.3f) , 20000.f),
        std::make_unique<juce::AudioParameterFloat>("offset", "Audio Offset", -1.0f, 1.0f, 0.0f),
        // envelope follower params
        std::make_unique<juce::AudioParameterBool>("cutenvon", "Cut Envelope ON", false),
        std::make_unique<juce::AudioParameterBool>("resenvon", "Res Envelope ON", false),
    })
#endif
{
    srand(static_cast<unsigned int>(time(nullptr))); // seed random generator
    juce::PropertiesFile::Options options{};
    options.applicationName = ProjectInfo::projectName;
    options.filenameSuffix = ".settings";
#if defined(JUCE_LINUX) || defined(JUCE_BSD)
    options.folderName = "~/.config/filtr";
#endif
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = PropertiesFile::storeAsXML;
    settings.setStorageParameters(options);

    for (auto* param : getParameters()) {
        param->addListener(this);
    }

    // init patterns
    for (int i = 0; i < 12; ++i) {
        patterns[i] = new Pattern(i);
        patterns[i]->insertPoint(0, 1, 0, 1);
        patterns[i]->insertPoint(0.5, 0, 0, 1);
        patterns[i]->insertPoint(1, 1, 0, 1);
        patterns[i]->buildSegments();

        respatterns[i] = new Pattern(i+12);
        respatterns[i]->insertPoint(0.0, 0.5, 0, 1);
        respatterns[i]->insertPoint(1.0, 0.5, 0, 1);
        respatterns[i]->buildSegments();
    }

    // init paintMode Patterns
    for (int i = 0; i < PAINT_PATS; ++i) {
        paintPatterns[i] = new Pattern(i + PAINT_PATS_IDX);
        if (i < 8) {
            auto preset = Presets::getPaintPreset(i);
            for (auto& point : preset) {
                paintPatterns[i]->insertPoint(point.x, point.y, point.tension, point.type);
            }
        }
        else {
            paintPatterns[i]->insertPoint(0.0, 1.0, 0.0, 1);
            paintPatterns[i]->insertPoint(1.0, 0.0, 0.0, 1);
        }
        paintPatterns[i]->buildSegments();
    }

    sequencer = new Sequencer(*this);
    pattern = patterns[0];
    respattern = respatterns[0];
    viewPattern = pattern;
    viewSubPattern = respattern;
    preSamples.resize(MAX_PLUG_WIDTH, 0); // samples array size must be >= viewport width
    postSamples.resize(MAX_PLUG_WIDTH, 0);
    monSamples.resize(MAX_PLUG_WIDTH, 0); // samples array size must be >= audio monitor width
    value = new RCSmoother();
    resvalue = new RCSmoother();

    loadSettings();
}

FILTRAudioProcessor::~FILTRAudioProcessor()
{
}

void FILTRAudioProcessor::parameterValueChanged (int parameterIndex, float newValue)
{
    (void)newValue;
    (void)parameterIndex;
    paramChanged = true;
}

void FILTRAudioProcessor::parameterGestureChanged (int parameterIndex, bool gestureIsStarting)
{
    (void)parameterIndex;
    (void)gestureIsStarting;
}

void FILTRAudioProcessor::loadSettings ()
{
    settings.closeFiles(); // FIX files changed by other plugin instances not loading
    if (auto* file = settings.getUserSettings()) {
        scale = (float)file->getDoubleValue("scale", 1.0f);
        plugWidth = file->getIntValue("width", PLUG_WIDTH);
        plugHeight = file->getIntValue("height", PLUG_HEIGHT);
        auto tensionparam = (double)params.getRawParameterValue("tension")->load();
        auto tensionatk = (double)params.getRawParameterValue("tensionatk")->load();
        auto tensionrel = (double)params.getRawParameterValue("tensionrel")->load();

        for (int i = 0; i < PAINT_PATS; ++i) {
            auto str = file->getValue("paintpat" + String(i),"").toStdString();
            if (!str.empty()) {
                paintPatterns[i]->clear();
                paintPatterns[i]->clearUndo();
                double x, y, tension;
                int type;
                std::istringstream iss(str);
                while (iss >> x >> y >> tension >> type) {
                    paintPatterns[i]->insertPoint(x,y,tension,type);
                }
                paintPatterns[i]->setTension(tensionparam, tensionatk, tensionrel, dualTension);
                paintPatterns[i]->buildSegments();
            }
        }
    }
}

void FILTRAudioProcessor::saveSettings ()
{
    settings.closeFiles(); // FIX files changed by other plugin instances not loading
    if (auto* file = settings.getUserSettings()) {
        file->setValue("scale", scale);
        file->setValue("width", plugWidth);
        file->setValue("height", plugHeight);
        for (int i = 0; i < PAINT_PATS; ++i) {
            std::ostringstream oss;
            auto points = paintPatterns[i]->points;
            for (const auto& point : points) {
                oss << point.x << " " << point.y << " " << point.tension << " " << point.type << " ";
            }
            file->setValue("paintpat"+juce::String(i), var(oss.str()));
        }
    }
    settings.saveIfNeeded();
}

void FILTRAudioProcessor::setScale(float s)
{
    scale = s;
    saveSettings();
}

int FILTRAudioProcessor::getCurrentGrid()
{
    auto gridIndex = (int)params.getRawParameterValue("grid")->load();
    return GRID_SIZES[gridIndex];
}

void FILTRAudioProcessor::createUndoPoint(int patindex)
{
    if (patindex == -1) {
        viewPattern->createUndo();
    }
    else {
        if (patindex < 12) {
            patterns[patindex]->createUndo();
        }
        else if (patindex < 24) {
            respatterns[patindex - 12]->createUndo();
        }
        else {
            paintPatterns[patindex - PAINT_PATS_IDX]->createUndo();
        }
    }
    sendChangeMessage(); // UI repaint
}

/*
    Used to create an undo point from a previously saved state
    Assigns the snapshot points to the pattern temporarily
    Creates an undo point and finally replaces back the points
*/
void FILTRAudioProcessor::createUndoPointFromSnapshot(std::vector<PPoint> snapshot)
{
    if (!Pattern::comparePoints(snapshot, viewPattern->points)) {
        auto points = viewPattern->points;
        viewPattern->points = snapshot;
        createUndoPoint();
        viewPattern->points = points;
    }
}

void FILTRAudioProcessor::setResonanceEditMode(bool isResonance)
{
    MessageManager::callAsync([this, isResonance] {
        resonanceEditMode = isResonance;
        if (uimode != UIMode::PaintEdit) {
            viewPattern = resonanceEditMode ? respattern : pattern;
            viewSubPattern = resonanceEditMode ? pattern : respattern;
        }
        sendChangeMessage();
    });
}

void FILTRAudioProcessor::setUIMode(UIMode mode)
{
    MessageManager::callAsync([this, mode]() {
        if (mode != UIMode::Seq && uimode == UIMode::Seq)
            sequencer->close();

        if (mode == UIMode::Normal) {
            viewPattern = resonanceEditMode ? respattern : pattern;
            viewSubPattern = resonanceEditMode ? pattern : respattern;
            showSequencer = false;
            showPaintWidget = false;
        }
        else if (mode == UIMode::Paint) {
            viewPattern = resonanceEditMode ? respattern : pattern;
            viewSubPattern = resonanceEditMode ? pattern : respattern;
            showPaintWidget = true;
            showSequencer = false;
        }
        else if (mode == UIMode::PaintEdit) {
            viewPattern = paintPatterns[paintTool];
            showPaintWidget = true;
            showSequencer = false;
        }
        else if (mode == UIMode::Seq) {
            sequencer->open();
            viewPattern = resonanceEditMode ? respattern : pattern;
            viewSubPattern = resonanceEditMode ? pattern : respattern;
            showPaintWidget = sequencer->selectedShape == CellShape::SPTool;
            showSequencer = true;
        }
        luimode = uimode;
        uimode = mode;
        sendChangeMessage();
    });
}

void FILTRAudioProcessor::togglePaintMode()
{
    setUIMode(uimode == UIMode::Paint
        ? UIMode::Normal
        : UIMode::Paint
    );
}

void FILTRAudioProcessor::togglePaintEditMode()
{
    setUIMode(uimode == UIMode::PaintEdit
        ? luimode
        : UIMode::PaintEdit
    );
}

void FILTRAudioProcessor::toggleSequencerMode()
{
    setUIMode(uimode == UIMode::Seq
        ? UIMode::Normal
        : UIMode::Seq
    );
}

Pattern* FILTRAudioProcessor::getPaintPatern(int index)
{
    return paintPatterns[index];
}

void FILTRAudioProcessor::setViewPattern(int index)
{
    if (index >= 0 && index < 12) {
        viewPattern = patterns[index];
    }
    else if (index >= PAINT_PATS && index < PAINT_PATS_IDX + PAINT_PATS) {
        viewPattern = paintPatterns[index - PAINT_PATS_IDX];
    }
    sendChangeMessage();
}

void FILTRAudioProcessor::restorePaintPatterns()
{
    for (int i = 0; i < 8; ++i) {
        paintPatterns[i]->clear();
        paintPatterns[i]->clearUndo();
        auto preset = Presets::getPaintPreset(i);
        for (auto& point : preset) {
            paintPatterns[i]->insertPoint(point.x, point.y, point.tension, point.type);
        }
        paintPatterns[i]->buildSegments();
    }
    sendChangeMessage();
}

//==============================================================================
const juce::String FILTRAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FILTRAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FILTRAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FILTRAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FILTRAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FILTRAudioProcessor::getNumPrograms()
{
    return 40;
}

int FILTRAudioProcessor::getCurrentProgram()
{
    return currentProgram == -1 ? 0 : currentProgram;
}

void FILTRAudioProcessor::setCurrentProgram (int index)
{
    if (currentProgram == index) return;
    loadProgram(index);
}

void FILTRAudioProcessor::loadProgram (int index)
{
    if (uimode == UIMode::Seq)
        sequencer->close();

    currentProgram = index;
    auto loadPreset = [](Pattern& pat, int idx) {
        auto preset = Presets::getPreset(idx);
        pat.clear();
        for (auto p = preset.begin(); p < preset.end(); ++p) {
            pat.insertPoint(p->x, p->y, p->tension, p->type);
        }
        pat.buildSegments();
        pat.clearUndo();
    };

    if (index == 0) { // Init
        for (int i = 0; i < 12; ++i) {
            patterns[i]->loadTriangle();
            patterns[i]->buildSegments();
            patterns[i]->clearUndo();
        }
    }
    else if (index == 1 || index == 14 || index == 27) {
        for (int i = 0; i < 12; ++i) {
            loadPreset(*patterns[i], index + i);
        }
    }
    else {
        loadPreset(*pattern, index - 1);
    }

    updateCutoffFromPattern();
    updateResFromPattern();
    setUIMode(UIMode::Normal);
    sendChangeMessage(); // UI Repaint
}

const juce::String FILTRAudioProcessor::getProgramName (int index)
{
    static const std::array<juce::String, 40> progNames = {
        "Init",
        "Load Patterns 01-12", "Empty", "Gate 2", "Gate 4", "Gate 8", "Gate 12", "Gate 16", "Gate 24", "Gate 32", "Trance 1", "Trance 2", "Trance 3", "Trance 4",
        "Load Patterns 13-25", "Saw 1", "Saw 2", "Step 1", "Step 1 FadeIn", "Step 4 Gate", "Off Beat", "Dynamic 1/4", "Swing", "Gate Out", "Gate In", "Speed up", "Speed Down",
        "Load Patterns 26-38", "End Fade", "End Gate", "Tremolo Slow", "Tremolo Fast", "Sidechain", "Drum Loop", "Copter", "AM", "Fade In", "Fade Out", "Fade OutIn", "Mute"
    };
    return progNames.at(index);
}

void FILTRAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    (void)index;
    (void)newName;
}

//==============================================================================
void FILTRAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    (void)samplesPerBlock;
    oversampler.initProcessing(samplesPerBlock);
    oversampler.reset();

    int trigger = (int)params.getRawParameterValue("trigger")->load();
    setLatencySamples(trigger == Trigger::Audio
        ? (int)std::ceil(oversampler.getLatencyInSamples() + sampleRate * LATENCY_MILLIS / 1000.0)
        : (int)std::ceil(oversampler.getLatencyInSamples())
    );
    lpFilterL.clear(0.0);
    lpFilterR.clear(0.0);
    hpFilterL.clear(0.0);
    hpFilterR.clear(0.0);
    transDetectorL.clear(sampleRate);
    transDetectorR.clear(sampleRate);
    std::fill(monSamples.begin(), monSamples.end(), 0.0);
    resetFilters(sampleRate);
    clearLatencyBuffers();
    onSlider();
}

void FILTRAudioProcessor::resetFilters(double srate)
{
    auto ftype = (FilterType)(int)params.getRawParameterValue("ftype")->load();
    auto fmode = (FilterMode)(int)params.getRawParameterValue("fmode")->load();
    auto flerp = (double)params.getRawParameterValue("flerp")->load();
    auto fdrive = (double)params.getRawParameterValue("fdrive")->load();
    auto fmorph = (double)params.getRawParameterValue("fmorph")->load();

    if (ftype == FilterType::kLinear12) {
        lFilter = std::make_unique<Linear>(k12p);
        rFilter = std::make_unique<Linear>(k12p);
    }
    else if (ftype == FilterType::kLinear24) {
        lFilter = std::make_unique<Linear>(k24p);
        rFilter = std::make_unique<Linear>(k24p);
    }
    else if (ftype == FilterType::kAnalog12) {
        lFilter = std::make_unique<Analog>(k12p);
        rFilter = std::make_unique<Analog>(k12p);
    }
    else if (ftype == FilterType::kAnalog24) {
        lFilter = std::make_unique<Analog>(k24p);
        rFilter = std::make_unique<Analog>(k24p);
    }
    else if (ftype == FilterType::kMoog12) {
        lFilter = std::make_unique<Moog>(k12p);
        rFilter = std::make_unique<Moog>(k12p);
    }
    else if (ftype == FilterType::kMoog24) {
        lFilter = std::make_unique<Moog>(k24p);
        rFilter = std::make_unique<Moog>(k24p);
    }
    else if (ftype == FilterType::kMS20) {
        lFilter = std::make_unique<MS20>();
        rFilter = std::make_unique<MS20>();
    }
    else if (ftype == FilterType::kTB303) {
        lFilter = std::make_unique<TB303>();
        rFilter = std::make_unique<TB303>();
    }
    else if (ftype == FilterType::kPhaserPos) {
        lFilter = std::make_unique<Phaser>(true);
        rFilter = std::make_unique<Phaser>(true);
    }
    else if (ftype == FilterType::kPhaserNeg) {
        lFilter = std::make_unique<Phaser>(false);
        rFilter = std::make_unique<Phaser>(false);
    }

    lFilter->setMode(fmode);
    rFilter->setMode(fmode);
    lFilter->setDrive(fdrive);
    rFilter->setDrive(fdrive);
    lFilter->reset(0.0);
    rFilter->reset(0.0);
    lFilter->setMorph(fmorph);
    rFilter->setMorph(fmorph);
    lFilter->setLerp((int)(srate * F_LERP_MILLIS * flerp / 1000.0));
    rFilter->setLerp((int)(srate * F_LERP_MILLIS * flerp / 1000.0));

    lftype = ftype;
    lfmode = fmode;
    lflerp = flerp;
    lfdrive = fdrive;
    lfmorph = fmorph;

    MessageManager::callAsync([this]{ sendChangeMessage(); });
}

void FILTRAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FILTRAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FILTRAudioProcessor::onSlider()
{
    setSmooth();
    auto srate = getSampleRate();

    int trigger = (int)params.getRawParameterValue("trigger")->load();
    if (trigger != ltrigger) {
        auto latency = getLatencySamples();
        setLatencySamples(trigger == Trigger::Audio
            ? (int)std::ceil(oversampler.getLatencyInSamples() + getSampleRate() * LATENCY_MILLIS / 1000.0)
            : (int)std::ceil(oversampler.getLatencyInSamples())
        );
        if (getLatencySamples() != latency && playing) {
            showLatencyWarning = true;
            MessageManager::callAsync([this]() { sendChangeMessage(); });
        }
        clearLatencyBuffers();
        ltrigger = trigger;
    }
    if (trigger == Trigger::Sync && alwaysPlaying)
        alwaysPlaying = false; // force alwaysPlaying off when trigger is not MIDI or Audio

    if (trigger != Trigger::MIDI && midiTrigger)
        midiTrigger = false;

    if (trigger != Trigger::Audio && audioTrigger)
        audioTrigger = false;

    int pat = (int)params.getRawParameterValue("pattern")->load();
    if (pat != pattern->index + 1 && pat != queuedPattern) {
        queuePattern(pat);
    }
    int respat = (int)params.getRawParameterValue("respattern")->load();
    if (respat != respattern->index + 1 - 12 && respat != queuedResPattern) {
        queueResPattern(respat);
    }

    auto tension = (double)params.getRawParameterValue("tension")->load();
    auto tensionatk = (double)params.getRawParameterValue("tensionatk")->load();
    auto tensionrel = (double)params.getRawParameterValue("tensionrel")->load();
    if (tension != ltension || tensionatk != ltensionatk || tensionrel != ltensionrel) {
        onTensionChange();
        ltensionatk = tensionatk;
        ltensionrel = tensionrel;
        ltension = tension;
    }

    auto sync = (int)params.getRawParameterValue("sync")->load();
    if (sync == 0) syncQN = 1.; // not used
    else if (sync == 1) syncQN = 1./4.; // 1/16
    else if (sync == 2) syncQN = 1./2.; // 1/8
    else if (sync == 3) syncQN = 1./1.; // 1/4
    else if (sync == 4) syncQN = 1.*2.; // 1/2
    else if (sync == 5) syncQN = 1.*4.; // 1bar
    else if (sync == 6) syncQN = 1.*8.; // 2bar
    else if (sync == 7) syncQN = 1.*16.; // 4bar
    else if (sync == 8) syncQN = 1./6.; // 1/16t
    else if (sync == 9) syncQN = 1./3.; // 1/8t
    else if (sync == 10) syncQN = 2./3.; // 1/4t
    else if (sync == 11) syncQN = 4./3.; // 1/2t
    else if (sync == 12) syncQN = 8./3.; // 1/1t
    else if (sync == 13) syncQN = 1./4.*1.5; // 1/16.
    else if (sync == 14) syncQN = 1./2.*1.5; // 1/8.
    else if (sync == 15) syncQN = 1./1.*1.5; // 1/4.
    else if (sync == 16) syncQN = 2./1.*1.5; // 1/2.
    else if (sync == 17) syncQN = 4./1.*1.5; // 1/1.

    auto highcut = (double)params.getRawParameterValue("highcut")->load();
    auto lowcut = (double)params.getRawParameterValue("lowcut")->load();
    lpFilterL.lp(srate, highcut, 0.707);
    lpFilterR.lp(srate, highcut, 0.707);
    hpFilterL.hp(srate, lowcut, 0.707);
    hpFilterR.hp(srate, lowcut, 0.707);

    auto ftype = (FilterType)(int)params.getRawParameterValue("ftype")->load();
    auto fmode = (FilterMode)(int)params.getRawParameterValue("fmode")->load();
    auto flerp = (double)params.getRawParameterValue("flerp")->load();
    auto fdrive = (double)params.getRawParameterValue("fdrive")->load();
    auto fmorph = (double)params.getRawParameterValue("fmorph")->load();

    if (lftype != ftype) {
        resetFilters(srate);
        lFilter->reset(lastOutL); // prevent popping when changing filters
        rFilter->reset(lastOutR);
        lftype = ftype;
    }

    if (lflerp != flerp) {
        int duration = (int)(srate * F_LERP_MILLIS * flerp / 1000.0);
        lFilter->setLerp(duration);
        rFilter->setLerp(duration);
        lflerp = flerp;
    }

    if (lfdrive != fdrive) {
        lFilter->setDrive(fdrive);
        rFilter->setDrive(fdrive);
        lfdrive = fdrive;
    }

    if (lfmode != fmode) {
        lFilter->setMode(fmode);
        rFilter->setMode(fmode);
        lfmode = fmode;
    }

    if (lfmorph != fmorph) {
        lFilter->setMorph(fmorph);
        rFilter->setMorph(fmorph);
        lfmorph = fmorph;
    }

    if (cutoffDirty) {
        float avg = (float)pattern->getavgY();
        params.getParameter("cutoff")->setValueNotifyingHost(avg);
        lcutoff = (double)params.getRawParameterValue("cutoff")->load();
        cutoffDirty = false;
        cutoffDirtyCooldown = 5; // ignore cutoff updates for 5 blocks
    }

    if (resDirty) {
        float avg = (float)respattern->getavgY();
        params.getParameter("res")->setValueNotifyingHost(avg);
        lres = (double)params.getRawParameterValue("res")->load();
        resDirty = false;
        resDirtyCooldown = 5;
    }

    double cutoff = (double)params.getRawParameterValue("cutoff")->load();
    double res = (double)params.getRawParameterValue("res")->load();
    
    // Ignores DAW updates for cutoff which was changed internally
    // DAW param updates are not reliable, on standalone works fine
    if (cutoffDirtyCooldown > 0) {
        lcutoff = cutoff;
    }
    else if (cutoff != lcutoff) {
        updatePatternFromCutoff();
        lcutoff = cutoff;
    }

    if (resDirtyCooldown > 0) {
        lres = res;
    } 
    else if (res != lres) {
        updateResPatternFromRes();
        lres = res;
    }
}

void FILTRAudioProcessor::updatePatternFromCutoff()
{
    double cutnorm = (double)params.getParameter("cutoff")->getValue();
    pattern->transform(cutnorm);
}

void FILTRAudioProcessor::updateResPatternFromRes()
{
    double resnorm = (double)params.getParameter("res")->getValue();
    respattern->transform(resnorm);
}

void FILTRAudioProcessor::updateCutoffFromPattern()
{
    cutoffDirty = true;
    paramChanged = true;
}

void FILTRAudioProcessor::updateResFromPattern()
{
    resDirty = true;
    paramChanged = true;
}

void FILTRAudioProcessor::onTensionChange()
{
    auto tension = (double)params.getRawParameterValue("tension")->load();
    auto tensionatk = (double)params.getRawParameterValue("tensionatk")->load();
    auto tensionrel = (double)params.getRawParameterValue("tensionrel")->load();
    pattern->setTension(tension, tensionatk, tensionrel, dualTension);
    respattern->setTension(tension, tensionatk, tensionrel, dualTension);
    pattern->buildSegments();
    for (int i = 0; i < PAINT_PATS; ++i) {
        paintPatterns[i]->setTension(tension, tensionatk, tensionrel, dualTension);
        paintPatterns[i]->buildSegments();
    }
}

void FILTRAudioProcessor::onPlay()
{
    clearDrawBuffers();
    clearLatencyBuffers();
    int trigger = (int)params.getRawParameterValue("trigger")->load();
    double ratehz = (double)params.getRawParameterValue("rate")->load();
    double phase = (double)params.getRawParameterValue("phase")->load();

    midiTrigger = false;
    audioTrigger = false;

    beatPos = ppqPosition;
    ratePos = beatPos * secondsPerBeat * ratehz;
    trigpos = 0.0;
    trigposSinceHit = 1.0;
    trigphase = phase;

    audioTriggerCountdown = -1;
    double srate = getSampleRate();
    transDetectorL.clear(srate);
    transDetectorR.clear(srate);

    lFilter->reset(0.0);
    rFilter->reset(0.0);

    if (trigger == 0 || alwaysPlaying) {
        restartEnv(false);
    }
}

void FILTRAudioProcessor::restartEnv(bool fromZero)
{
    int sync = (int)params.getRawParameterValue("sync")->load();
    double min = (double)params.getRawParameterValue("min")->load();
    double max = (double)params.getRawParameterValue("max")->load();
    double phase = (double)params.getRawParameterValue("phase")->load();
    double cutoffset = (double)params.getRawParameterValue("cutoffset")->load();
    double resoffset = (double)params.getRawParameterValue("resoffset")->load();

    if (fromZero) { // restart from phase
        xpos = phase;
    }
    else { // restart from beat pos
        xpos = sync > 0
            ? beatPos / syncQN + phase
            : ratePos + phase;
        xpos -= std::floor(xpos);
    }

    value->reset(getY(xpos, min, max, cutoffset)); // reset smooth
    resvalue->reset(getY(xpos, min, max, resoffset));
}

void FILTRAudioProcessor::onStop()
{
    if (showLatencyWarning) {
        showLatencyWarning = false;
        MessageManager::callAsync([this]() { sendChangeMessage(); });
    }
}

void FILTRAudioProcessor::clearDrawBuffers()
{
    std::fill(preSamples.begin(), preSamples.end(), 0.0);
    std::fill(postSamples.begin(), postSamples.end(), 0.0);
}

void FILTRAudioProcessor::clearLatencyBuffers()
{
    int trigger = (int)params.getRawParameterValue("trigger")->load();
    auto latency = trigger == Trigger::Audio 
        ? (int)std::ceil(getSampleRate() * LATENCY_MILLIS / 1000.0)
        : 0;
    latency *= (int)oversampler.getOversamplingFactor();
    latBufferL.resize(latency, 0.0);
    latBufferR.resize(latency, 0.0);
    monLatBufferL.resize(getLatencySamples());
    monLatBufferR.resize(getLatencySamples());
    latpos = 0;
    monlatpos = 0;
}

void FILTRAudioProcessor::toggleUseSidechain()
{
    useSidechain = !useSidechain;
    hpFilterL.clear(0.0);
    hpFilterR.clear(0.0);
    lpFilterL.clear(0.0);
    lpFilterR.clear(0.0);
}

void FILTRAudioProcessor::toggleMonitorSidechain()
{
    useMonitor = !useMonitor;
    hpFilterL.clear(0.0);
    hpFilterR.clear(0.0);
    lpFilterL.clear(0.0);
    lpFilterR.clear(0.0);
}

double inline FILTRAudioProcessor::getY(double x, double min, double max, double offset)
{
    return std::clamp(min + (max - min) * (1 - pattern->get_y_at(x)) + offset, 0.0, 1.0);
}

double inline FILTRAudioProcessor::getYres(double x, double min, double max, double offset)
{
    return std::clamp(min + (max - min) * (1 - respattern->get_y_at(x)) + offset, 0.0, 1.0);
}

void FILTRAudioProcessor::setSmooth()
{
    if (dualSmooth) {
        float attack = params.getRawParameterValue("attack")->load();
        float release = params.getRawParameterValue("release")->load();
        attack *= attack;
        release *= release;
        value->setup(attack * 0.25, release * 0.25, getSampleRate() * oversampler.getOversamplingFactor());
        resvalue->setup(attack * 0.25, release * 0.25, getSampleRate() * oversampler.getOversamplingFactor());
    }
    else {
        float lfosmooth = params.getRawParameterValue("smooth")->load();
        lfosmooth *= lfosmooth;
        value->setup(lfosmooth * 0.25, lfosmooth * 0.25, getSampleRate() * oversampler.getOversamplingFactor());
        resvalue->setup(lfosmooth * 0.25, lfosmooth * 0.25, getSampleRate() * oversampler.getOversamplingFactor());
    }
}

void FILTRAudioProcessor::queuePattern(int patidx)
{
    queuedPattern = patidx;
    queuedPatternCountdown = 0;
    int patsync = (int)params.getRawParameterValue("patsync")->load();
    bool linkpats = (bool)params.getRawParameterValue("linkpats")->load();
    if (linkpats) queueResPattern(patidx);

    if (playing && patsync != PatSync::Off) {
        int interval = samplesPerBeat;
        if (patsync == PatSync::QuarterBeat)
            interval = interval / 4;
        else if (patsync == PatSync::HalfBeat)
            interval = interval / 2;
        else if (patsync == PatSync::Beat_x2)
            interval = interval * 2;
        else if (patsync == PatSync::Beat_x4)
            interval = interval * 4;
        queuedPatternCountdown = (interval - timeInSamples % interval) % interval;
    }

    auto param = params.getParameter("pattern");
    param->setValueNotifyingHost(param->convertTo0to1((float)(patidx)));
}

void FILTRAudioProcessor::queueResPattern(int patidx)
{
    queuedResPattern = patidx;
    queuedResPatternCountdown = 0;
    int patsync = (int)params.getRawParameterValue("patsync")->load();

    if (playing && patsync != PatSync::Off) {
        int interval = samplesPerBeat;
        if (patsync == PatSync::QuarterBeat)
            interval = interval / 4;
        else if (patsync == PatSync::HalfBeat)
            interval = interval / 2;
        else if (patsync == PatSync::Beat_x2)
            interval = interval * 2;
        else if (patsync == PatSync::Beat_x4)
            interval = interval * 4;
        queuedResPatternCountdown = (interval - timeInSamples % interval) % interval;
    }

    auto param = params.getParameter("respattern");
    param->setValueNotifyingHost(param->convertTo0to1((float)(patidx)));
}

bool FILTRAudioProcessor::supportsDoublePrecisionProcessing() const
{
    return true;
}

void FILTRAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockByType(buffer, midiMessages);
}
void FILTRAudioProcessor::processBlock(juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockByType(buffer, midiMessages);
}

template <typename FloatType>
void FILTRAudioProcessor::processBlockByType (AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals disableDenormals;
    double srate = getSampleRate();
    int samplesPerBlock = getBlockSize();
    int samplingFactor = (int)oversampler.getOversamplingFactor();
    bool looping = false;
    double loopStart = 0.0;
    double loopEnd = 0.0;

    // Get playhead info
    if (auto* phead = getPlayHead()) {
        if (auto pos = phead->getPosition()) {
            if (auto ppq = pos->getPpqPosition())
                ppqPosition = *ppq;
            if (auto tempo = pos->getBpm()) {
                beatsPerSecond = *tempo / 60.0;
                beatsPerSample = *tempo / (60.0 * srate * samplingFactor);
                samplesPerBeat = (int)((60.0 / *tempo) * srate * samplingFactor);
                secondsPerBeat = 60.0 / *tempo;
            }
            looping = pos->getIsLooping();
            if (auto loopPoints = pos->getLoopPoints()) {
                loopStart = loopPoints->ppqStart;
                loopEnd = loopPoints->ppqEnd;
            }
            auto play = pos->getIsPlaying();
            if (!playing && play) // playback started
                onPlay();
            else if (playing && !play) // playback stopped
                onStop();

            playing = play;
            if (playing) {
                if (auto samples = pos->getTimeInSamples()) {
                    timeInSamples = *samples;
                }
            }
        }
    }

    int inputBusCount = getBusCount(true);
    int audioOutputs = getTotalNumOutputChannels();
    int audioInputs = inputBusCount > 0 ? getChannelCountOfBus(true, 0) : 0;
    int sideInputs = inputBusCount > 1 ? getChannelCountOfBus(true, 1) : 0;
    int numSamples = buffer.getNumSamples();

    if (!audioInputs || !audioOutputs)
        return;

    // Prepare a double buffer for processing
    juce::AudioBuffer<double> doubleBuffer(2, numSamples);
    for (int channel = 0; channel < 2; ++channel) {
        auto* src = buffer.getReadPointer(audioInputs > 1 ? channel : 0);
        auto* dst = doubleBuffer.getWritePointer(channel);
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            dst[sample] = static_cast<double>(src[sample]);
    }

    // Oversample the double buffer
    juce::dsp::AudioBlock<double> block(doubleBuffer);
    auto upsampledBlock = oversampler.processSamplesUp(block);
    int numUpSamples = (int)upsampledBlock.getNumSamples();

    double mix = (double)params.getRawParameterValue("mix")->load();
    int trigger = (int)params.getRawParameterValue("trigger")->load();
    int sync = (int)params.getRawParameterValue("sync")->load();
    double min = (double)params.getRawParameterValue("min")->load();
    double max = (double)params.getRawParameterValue("max")->load();
    double ratehz = (double)params.getRawParameterValue("rate")->load();
    double phase = (double)params.getRawParameterValue("phase")->load();
    double lowcut = (double)params.getRawParameterValue("lowcut")->load();
    double highcut = (double)params.getRawParameterValue("highcut")->load();
    int algo = (int)params.getRawParameterValue("algo")->load();
    double threshold = (double)params.getRawParameterValue("threshold")->load();
    double sense = 1.0 - (double)params.getRawParameterValue("sense")->load();
    double gain = (double)params.getRawParameterValue("gain")->load();
    double cutoffset = (double)params.getRawParameterValue("cutoffset")->load();
    double resoffset = (double)params.getRawParameterValue("resoffset")->load();
    sense = std::pow(sense, 2); // make sensitivity more responsive

    // processes draw wave samples
    auto processDisplaySample = [&](int sampidx, double xpos, double prelsamp, double prersamp) {
        auto preamp = std::max(std::fabs(prelsamp), std::fabs(prersamp));
        auto postlsamp = (double)upsampledBlock.getSample(0, sampidx);
        auto postrsamp = audioInputs > 1 ? (double)upsampledBlock.getSample(1, sampidx) : postlsamp;
        auto postamp = std::max(std::fabs(postlsamp), std::fabs(postrsamp));
        winpos = (int)std::floor(xpos * viewW);
        if (lwinpos != winpos) {
            preSamples[winpos] = 0.0;
            postSamples[winpos] = 0.0;
        }
        lwinpos = winpos;
        if (preSamples[winpos] < preamp)
            preSamples[winpos] = preamp;
        if (postSamples[winpos] < postamp)
            postSamples[winpos] = postamp;
    };

    double monIncrementPerSample = 1.0 / ((srate * 2) / monW); // 2 seconds of audio displayed on monitor
    auto processMonitorSample = [&](double lsamp, double rsamp, bool hit) {
        double indexd = monpos.load();
        indexd += monIncrementPerSample;

        if (indexd >= monW)
            indexd -= monW;

        int index = (int)indexd;
        if (lmonpos != index)
            monSamples[index] = 0.0;
        lmonpos = index;

        double maxamp = std::max(std::fabs(lsamp), std::fabs(rsamp));
        if (hit || monSamples[index] >= 10.0)
            maxamp = std::max(maxamp + 10.0, hitamp + 10.0); // encode hits by adding +10 to amp

        monSamples[index] = std::max(monSamples[index], maxamp);
        monpos.store(indexd);
    };

    // applies envelope to a sample index
    auto applyFilter = [&](int sampidx, double env, double resenv, double lsample, double rsample) {
        double cutoff = Utils::normalToFreq(env);
        lFilter->init(srate * samplingFactor, cutoff, resenv);
        rFilter->init(srate * samplingFactor, cutoff, resenv);
        double outl = lFilter->eval(lsample) * gain;
        double outr = rFilter->eval(rsample) * gain;
        lFilter->tick();
        rFilter->tick();

        for (int channel = 0; channel < audioOutputs; ++channel) {
            auto wet = channel == 0 ? outl : outr;
            auto dry = (double)upsampledBlock.getSample(channel, sampidx);
            if (outputCV) {
                upsampledBlock.setSample(channel, sampidx, env);
            }
            else {
                upsampledBlock.setSample(channel, sampidx, wet * mix + dry * (1.0 - mix));
            }
        }
    };

    if (paramChanged) {
        onSlider();
        paramChanged = false;
    }
    if (cutoffDirtyCooldown > 0)
        cutoffDirtyCooldown--;
    if (resDirtyCooldown > 0)
        resDirtyCooldown--;

    // Process new MIDI messages
    for (const auto metadata : midiMessages) {
        juce::MidiMessage message = metadata.getMessage();
        if (message.isNoteOn() || message.isNoteOff()) {
            midiIn.push_back({ // queue midi message
                metadata.samplePosition,
                message.isNoteOn(),
                message.getNoteNumber(),
                message.getVelocity(),
                message.getChannel() - 1
            });
        }
    }
    midiMessages.clear();

    // Process midi out queue
    for (auto it = midiOut.begin(); it != midiOut.end();) {
        auto& [msg, offset] = *it;

        if (offset < samplesPerBlock) {
            midiMessages.addEvent(msg, offset);
            it = midiOut.erase(it);
        }
        else {
            offset -= samplesPerBlock;
            ++it;
        }
    }

    // remove midi in messages that have been processed
    midiIn.erase(std::remove_if(midiIn.begin(), midiIn.end(), [](const MidiInMsg& msg) {
        return msg.offset < 0;
    }), midiIn.end());

    // update outputs with last block information at the start of the new block
    if (outputCC > 0) {
        auto val = (int)std::round(ypos*127.0);
        if (bipolarCC) val -= 64;
        auto cc = MidiMessage::controllerEvent(outputCCChan + 1, outputCC-1, val);
        midiMessages.addEvent(cc, 0);
    }

    // keep beatPos in sync with playhead so plugin can be bypassed and return to its sync pos
    if (playing) {
        beatPos = ppqPosition;
        ratePos = beatPos * secondsPerBeat * ratehz;
    }

    // audio trigger transient detection and monitoring 
    // direct audio buffer processing, not oversampled
    if (trigger == Audio) {
        for (int sample = 0; sample < numSamples; ++sample) {
            // read audio samples
            double lsample = (double)buffer.getSample(0, sample);
            double rsample = (double)buffer.getSample(audioInputs > 1 ? 1 : 0, sample);

            // read sidechain samples
            double lsidesample = 0.0;
            double rsidesample = 0.0;
            if (useSidechain && sideInputs) {
                lsidesample = (double)buffer.getSample(audioInputs, sample);
                rsidesample = (double)buffer.getSample(sideInputs > 1 ? audioInputs + 1 : audioInputs, sample);
            }

            // Detect audio transients
            auto monSampleL = useSidechain ? lsidesample : lsample;
            auto monSampleR = useSidechain ? rsidesample : rsample;
            if (lowcut > 20.0) {
                monSampleL = hpFilterL.df1(monSampleL);
                monSampleR = hpFilterR.df1(monSampleR);
            }
            if (highcut < 20000.0) {
                monSampleL = lpFilterL.df1(monSampleL);
                monSampleR = lpFilterR.df1(monSampleR);
            }

            bool hit = false;
            if (transDetectorL.detect(algo, monSampleL, threshold, sense) ||
                transDetectorR.detect(algo, monSampleR, threshold, sense))
            {
                transDetectorL.startCooldown();
                transDetectorR.startCooldown();
                int offset = (int)(params.getRawParameterValue("offset")->load() * LATENCY_MILLIS / 1000.f * srate);
                audioTriggerCountdown = (sample + std::max(0, getLatencySamples() + offset)) * samplingFactor;
                hitamp = transDetectorL.hit ? std::fabs(monSampleL) : std::fabs(monSampleR);
                hit = true;
            }

            processMonitorSample(monSampleL, monSampleR, hit);
            monLatBufferL[monlatpos] = monSampleL;
            monLatBufferR[monlatpos] = monSampleR;

            int latency = (int)monLatBufferL.size();
            auto readpos = (monlatpos + latency - 1) % latency;
            if (useMonitor) {
                monSampleL = monLatBufferL[readpos];
                monSampleR = monLatBufferR[readpos];
                for (int channel = 0; channel < audioOutputs; ++channel) {
                    buffer.setSample(channel, sample, static_cast<FloatType>(channel == 0 ? monSampleL : monSampleR));
                }
            }

            monlatpos = (monlatpos + 1) % latency;
        }
    }

    for (int sample = 0; sample < numUpSamples; ++sample) {
        if (playing && looping && beatPos >= loopEnd) {
            beatPos = loopStart + (beatPos - loopEnd);
            ratePos = beatPos * secondsPerBeat * ratehz;
        }

        // process midi in queue
        for (auto& msg : midiIn) {
            if (msg.offset == 0) {
                if (msg.isNoteon) {
                    if (msg.channel == triggerChn || triggerChn == 16) {
                        auto patidx = msg.note % 12;
                        queuePattern(patidx + 1);
                    }
                    else if (trigger == Trigger::MIDI) {
                        clearDrawBuffers();
                        midiTrigger = !alwaysPlaying;
                        trigpos = 0.0;
                        trigphase = phase;
                        restartEnv(true);
                    }
                }
            }
            msg.offset -= 1;
        }

        // process queued pattern
        if (queuedPattern) {
            if (!playing || queuedPatternCountdown == 0) {
                if (uimode == UIMode::Seq) {
                    sequencer->close();
                    setUIMode(UIMode::Normal);
                }
                pattern = patterns[queuedPattern - 1];
                viewPattern = resonanceEditMode ? respattern : pattern;
                viewSubPattern = resonanceEditMode ? pattern : respattern;
                auto tension = (double)params.getRawParameterValue("tension")->load();
                auto tensionatk = (double)params.getRawParameterValue("tensionatk")->load();
                auto tensionrel = (double)params.getRawParameterValue("tensionrel")->load();
                pattern->setTension(tension, tensionatk, tensionrel, dualTension);
                pattern->buildSegments();
                updateCutoffFromPattern();
                MessageManager::callAsync([this]() { sendChangeMessage();});
                queuedPattern = 0;
            }
            if (queuedPatternCountdown > 0) {
                queuedPatternCountdown -= 1;
            }
        }

        // process queued res pattern
        if (queuedResPattern) {
            if (!playing || queuedResPatternCountdown == 0) {
                if (uimode == UIMode::Seq) {
                    sequencer->close();
                    setUIMode(UIMode::Normal);
                }
                respattern = respatterns[queuedResPattern - 1];
                viewPattern = resonanceEditMode ? respattern : pattern;
                viewSubPattern = resonanceEditMode ? pattern : respattern;
                auto tension = (double)params.getRawParameterValue("tension")->load();
                auto tensionatk = (double)params.getRawParameterValue("tensionatk")->load();
                auto tensionrel = (double)params.getRawParameterValue("tensionrel")->load();
                respattern->setTension(tension, tensionatk, tensionrel, dualTension);
                respattern->buildSegments();
                updateResFromPattern();
                MessageManager::callAsync([this]() {sendChangeMessage();});
                queuedResPattern = 0;
            }
            if (queuedResPatternCountdown > 0) {
                queuedResPatternCountdown -= 1;
            }
        }

        // Sync mode
        if (trigger == Trigger::Sync) {
            xpos = sync > 0
                ? beatPos / syncQN + phase
                : ratePos + phase;
            xpos -= std::floor(xpos);

            double newypos = getY(xpos, min, max, cutoffset);
            ypos = value->process(newypos, newypos > ypos);
            double newyres = getYres(xpos, min, max, resoffset);
            yres = resvalue->process(newyres, newyres > yres);

            auto lsample = (double)upsampledBlock.getSample(0, sample);
            auto rsample = (double)upsampledBlock.getSample(audioInputs == 1 ? 0 : 1, sample);
            applyFilter(sample, ypos, yres, lsample, rsample);
            processDisplaySample(sample, xpos, lsample, rsample);
        }

        // MIDI mode
        else if (trigger == Trigger::MIDI) {
            auto inc = sync > 0
                ? beatsPerSample / syncQN
                : 1 / (srate * samplingFactor) * ratehz;
            xpos += inc;
            trigpos += inc;
            xpos -= std::floor(xpos);

            if (!alwaysPlaying) {
                if (midiTrigger) {
                    if (trigpos >= 1.0) { // envelope finished, stop midiTrigger
                        midiTrigger = false;
                        xpos = phase ? phase : 1.0;
                    }
                }
                else {
                    xpos = phase ? phase : 1.0; // midiTrigger is stopped, hold last position
                }
            }

            double newypos = getY(xpos, min, max, cutoffset);
            ypos = value->process(newypos, newypos > ypos);
            double newyres = getYres(xpos, min, max, resoffset);
            yres = resvalue->process(newyres, newyres > yres);

            auto lsample = (double)upsampledBlock.getSample(0, sample);
            auto rsample = (double)upsampledBlock.getSample(1 % audioInputs, sample);
            applyFilter(sample, ypos, yres, lsample, rsample);
            double viewx = (alwaysPlaying || midiTrigger) ? xpos : (trigpos + trigphase) - std::floor(trigpos + trigphase);
            processDisplaySample(sample, viewx, lsample, rsample);
        }

        // Audio mode
        else if (trigger == Trigger::Audio) {
            // read the sample 'latency' samples ago
            int latency = (int)latBufferL.size();
            int readPos = (latpos + latency - 1) % latency;
            latBufferL[latpos] = upsampledBlock.getSample(0, sample);
            latBufferR[latpos] = upsampledBlock.getSample(1, sample);
            auto lsample = latBufferL[readPos];
            auto rsample = latBufferR[readPos];

            // write delayed samples to buffer to later apply dry/wet mix
            for (int channel = 0; channel < 2; ++channel) {
                upsampledBlock.setSample(channel, sample, channel == 0 ? lsample : rsample);
            }

            auto hit = audioTriggerCountdown == 0; // there was an audio transient trigger in this sample

            // envelope processing
            auto inc = sync > 0
                ? beatsPerSample / syncQN
                : 1 / (srate * samplingFactor) * ratehz;
            xpos += inc;
            trigpos += inc;
            trigposSinceHit += inc;
            xpos -= std::floor(xpos);

            // send output midi notes on audio trigger hit
            if (hit && outputATMIDI > 0) {
                auto noteOn = MidiMessage::noteOn(1, outputATMIDI - 1, (float)hitamp);
                midiMessages.addEvent(noteOn, sample);

                auto offnoteDelay = static_cast<int>(srate * AUDIO_NOTE_LENGTH_MILLIS / 1000.0);
                int noteOffSample = sample / samplingFactor + offnoteDelay;
                auto noteOff = MidiMessage::noteOff(1, outputATMIDI - 1);

                if (noteOffSample < samplesPerBlock) {
                    midiMessages.addEvent(noteOff, noteOffSample);
                }
                else {
                    int offset = noteOffSample - samplesPerBlock;
                    midiOut.push_back({ noteOff, offset });
                }
            }

            if (hit && (alwaysPlaying || !audioIgnoreHitsWhilePlaying || trigposSinceHit > 0.98)) {
                clearDrawBuffers();
                audioTrigger = !alwaysPlaying;
                trigpos = 0.0;
                trigphase = phase;
                trigposSinceHit = 0.0;
                restartEnv(true);
            }

            if (!alwaysPlaying) {
                if (audioTrigger) {
                    if (trigpos >= 1.0) { // envelope finished, stop trigger
                        audioTrigger = false;
                        xpos = phase ? phase : 1.0;
                    }
                }
                else {
                    xpos = phase ? phase : 1.0; // audioTrigger is stopped, hold last position
                }
            }

            double newypos = getY(xpos, min, max, cutoffset);
            ypos = value->process(newypos, newypos > ypos);
            double newyres = getYres(xpos, min, max, resoffset);
            yres = resvalue->process(newyres, newyres > yres);

            applyFilter(sample, ypos, yres, lsample, rsample);

            double viewx = (alwaysPlaying || audioTrigger) ? xpos : (trigpos + trigphase) - std::floor(trigpos + trigphase);
            processDisplaySample(sample, viewx, lsample, rsample);
            latpos = (latpos + 1) % latency;

            if (audioTriggerCountdown > -1)
                audioTriggerCountdown -= 1;
        }

        xenv.store(xpos);
        yenv.store(resonanceEditMode ? yres : ypos);
        beatPos += beatsPerSample;
        ratePos += 1 / (srate * samplingFactor) * ratehz;
        if (playing)
            timeInSamples += 1;
    }

    drawSeek.store(playing && (trigger == Trigger::Sync || midiTrigger || audioTrigger));
    oversampler.processSamplesDown(block);

    // Convert back to the original buffer type
    if (trigger != Trigger::Audio || !useMonitor) {
        for (int channel = 0; channel < audioOutputs; ++channel) {
            auto* src = doubleBuffer.getReadPointer(channel);
            auto* dst = buffer.getWritePointer(channel);
            for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                dst[sample] = static_cast<FloatType>(src[sample]);
        }
    }

    rmsLeft.store(0.9 * rmsLeft.load() + 0.1 * (double)buffer.getRMSLevel(0, 0, numSamples));
    rmsRight.store(0.9 * rmsRight.load() + 0.1 * (double)buffer.getRMSLevel(audioOutputs > 1 ? 1 : 0, 0, numSamples));

    lastOutL = buffer.getSample(0, numSamples - 1);
    lastOutR = buffer.getSample(audioOutputs == 1 ? 0 : 1, numSamples - 1);
}

//==============================================================================
bool FILTRAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FILTRAudioProcessor::createEditor()
{
    return new FILTRAudioProcessorEditor (*this);
}

//==============================================================================
void FILTRAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    bool isSeqMode = uimode == UIMode::Seq;
    if (isSeqMode)
        sequencer->close();

    auto state = ValueTree("PluginState");
    state.appendChild(params.copyState(), nullptr);
    state.setProperty("version", PROJECT_VERSION, nullptr);
    state.setProperty("currentProgram", currentProgram, nullptr);
    state.setProperty("alwaysPlaying",alwaysPlaying, nullptr);
    state.setProperty("dualSmooth",dualSmooth, nullptr);
    state.setProperty("dualTension",dualTension, nullptr);
    state.setProperty("triggerChn",triggerChn, nullptr);
    state.setProperty("useMonitor",useMonitor, nullptr);
    state.setProperty("useSidechain",useSidechain, nullptr);
    state.setProperty("outputCC", outputCC, nullptr);
    state.setProperty("outputCCChan", outputCCChan, nullptr);
    state.setProperty("outputCV", outputCV, nullptr);
    state.setProperty("outputATMIDI", outputATMIDI, nullptr);
    state.setProperty("bipolarCC", bipolarCC, nullptr);
    state.setProperty("paintTool", paintTool, nullptr);
    state.setProperty("paintPage", paintPage, nullptr);
    state.setProperty("pointMode", pointMode, nullptr);
    state.setProperty("audioIgnoreHitsWhilePlaying", audioIgnoreHitsWhilePlaying, nullptr);

    for (int i = 0; i < 12; ++i) {
        std::ostringstream oss;
        std::ostringstream ossres;
        auto points = patterns[i]->points;
        for (const auto& point : points) {
            oss << point.x << " " << point.y << " " << point.tension << " " << point.type << " ";
        }
        state.setProperty("pattern" + juce::String(i), var(oss.str()), nullptr);

        points = respatterns[i]->points;
        for (const auto& point : points) {
            ossres << point.x << " " << point.y << " " << point.tension << " " << point.type << " ";
        }
        state.setProperty("respattern" + juce::String(i), var(ossres.str()), nullptr);
    }

    // serialize sequencer cells
    std::ostringstream oss;
    for (const auto& cell : sequencer->cells) {
        oss << cell.shape << ' '
            << cell.lshape << ' '
            << cell.ptool << ' '
            << cell.invertx << ' '
            << cell.miny << ' '
            << cell.maxy << ' '
            << cell.tenatt << ' '
            << cell.tenrel << '\n';
    }
    state.setProperty("seqcells", var(oss.str()), nullptr);

    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);

    if (isSeqMode)
        sequencer->open();
}

void FILTRAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    bool isSeqMode = uimode == UIMode::Seq;
    if (isSeqMode)
        sequencer->close();

    std::unique_ptr<juce::XmlElement>xmlState (getXmlFromBinary (data, sizeInBytes));
    if (!xmlState) return;
    auto state = ValueTree::fromXml (*xmlState);
    if (!state.isValid()) return;

    params.replaceState(state.getChild(0));
    if (state.hasProperty("version")) {
        currentProgram = (int)state.getProperty("currentProgram");
        alwaysPlaying = (bool)state.getProperty("alwaysPlaying");
        dualSmooth = (bool)state.getProperty("dualSmooth");
        dualTension = (bool)state.getProperty("dualTension");
        triggerChn = (int)state.getProperty("triggerChn");
        useMonitor = (bool)state.getProperty("useMonitor");
        useSidechain = (bool)state.getProperty("useSidechain");
        outputCC = (int)state.getProperty("outputCC");
        outputCCChan = (int)state.getProperty("outputCCChan");
        bipolarCC = (bool)state.getProperty("bipolarCC");
        outputCV = (bool)state.getProperty("outputCV");
        outputATMIDI = (int)state.getProperty("outputATMIDI");
        paintTool = (int)state.getProperty("paintTool");
        paintPage = (int)state.getProperty("paintPage");
        pointMode = state.hasProperty("pointMode") ? (int)state.getProperty("pointMode") : 1;
        audioIgnoreHitsWhilePlaying = (bool)state.getProperty("audioIgnoreHitsWhilePlaying");

        for (int i = 0; i < 12; ++i) {
            patterns[i]->clear();
            patterns[i]->clearUndo();
            respatterns[i]->clear();
            respatterns[i]->clearUndo();

            auto str = state.getProperty("pattern" + String(i)).toString().toStdString();
            if (!str.empty()) {
                double x, y, tension;
                int type;
                std::istringstream iss(str);
                while (iss >> x >> y >> tension >> type) {
                    patterns[i]->insertPoint(x,y,tension,type);
                }
            }

            str = state.getProperty("respattern" + String(i)).toString().toStdString();
            if (!str.empty()) {
                double x, y, tension;
                int type;
                std::istringstream iss(str);
                while (iss >> x >> y >> tension >> type) {
                    respatterns[i]->insertPoint(x,y,tension,type);
                }
            }

            auto tension = (double)params.getRawParameterValue("tension")->load();
            auto tensionatk = (double)params.getRawParameterValue("tensionatk")->load();
            auto tensionrel = (double)params.getRawParameterValue("tensionrel")->load();
            patterns[i]->setTension(tension, tensionatk, tensionrel, dualTension);
            patterns[i]->buildSegments();
            respatterns[i]->setTension(tension, tensionatk, tensionrel, dualTension);
            respatterns[i]->buildSegments();
        }

        updateCutoffFromPattern();
        updateResFromPattern();

        if (state.hasProperty("seqcells")) {
            auto str = state.getProperty("seqcells").toString().toStdString();
            sequencer->cells.clear();
            std::istringstream iss(str);
            Cell cell;
            int shape, lshape;
            while (iss >> shape >> lshape >> cell.ptool >> cell.invertx
                >> cell.miny >> cell.maxy >> cell.tenatt >> cell.tenrel) {
                cell.shape = static_cast<CellShape>(shape);
                cell.lshape = static_cast<CellShape>(lshape);
                sequencer->cells.push_back(cell);
            }
        }
    }

    setUIMode(UIMode::Normal);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FILTRAudioProcessor();
}
