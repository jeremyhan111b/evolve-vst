#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "EvolveTypes.h"
#include "EvolveEngine.h"
#include "PianoRoll.h"
#include "GenCard.h"

//==============================================================================
class EvolveEditor : public juce::AudioProcessorEditor,
                     public juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit EvolveEditor (EvolveProcessor&);
    ~EvolveEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    EvolveProcessor& processor;

    //── Header ──────────────────────────────────────────────────
    juce::Label      logoLabel;
    juce::ComboBox   keyBox, genreBox;
    juce::Slider     bpmSlider;
    juce::TextButton settingsBtn { "KEY" };

    //── Left panel ──────────────────────────────────────────────
    juce::Label          seedLabel     { {}, "SEED CAPTURE" };
    PianoRollDisplay     seedRoll;
    juce::Label          captureInfo;
    juce::TextButton     captureBtn   { "CAPTURE LOOP" };
    juce::TextButton     hearSeedBtn  { "HEAR" };
    juce::TextButton     plantBtn     { "PLANT SEED" };

    juce::Label          evolveLabel  { {}, "EVOLVE" };
    juce::TextButton     autoBtn      { "AUTO" };
    juce::TextButton     rhythmBtn    { "RHYTHM" };
    juce::TextButton     melodyBtn    { "MELODY" };
    juce::TextButton     harmonyBtn   { "HARMONY" };
    juce::TextButton     formBtn      { "FORM" };
    juce::TextButton     evolveBtn    { "EVOLVE SELECTED" };

    juce::Label          statusLabel;

    //── Right panel: tree ───────────────────────────────────────
    juce::Label          treeLabel    { {}, "EVOLUTION TREE" };
    juce::Viewport       treeViewport;
    juce::Component      treeContainer;

    //── API key overlay ─────────────────────────────────────────
    juce::Component      apiKeyOverlay;
    juce::Label          apiKeyLabel  { {}, "Enter your Anthropic API key to begin" };
    juce::Label          apiKeyTitle;
    juce::TextEditor     apiKeyEditor;
    juce::TextButton     apiSaveBtn   { "START EVOLVE" };

    //── Layout constants ─────────────────────────────────────────
    static constexpr int leftW = 280;
    int seedDivY   = 194;
    int evolveDivY = 282;

    //── State ───────────────────────────────────────────────────
    std::vector<Generation> generations;
    juce::String            selectedId;
    juce::String            evolveParam { "auto" };
    NoteList                seedNotes;
    bool                    isCapturing = false;

    std::vector<std::unique_ptr<GenCard>>       genCards;
    std::vector<std::unique_ptr<juce::Label>>    treeLabels;

    //── Audio ───────────────────────────────────────────────────
    std::unique_ptr<juce::AudioFormatManager>  formatManager;
    juce::AudioDeviceManager*                  deviceManager = nullptr;
    std::unique_ptr<juce::AudioSourcePlayer>   player;

    //── Methods ─────────────────────────────────────────────────
    void timerCallback() override;

    void setupHeader();
    void setupLeftPanel();
    void setupTree();
    void setupApiKeyOverlay();
    void updateApiKeyVisibility();

    void setParam (const juce::String& p);
    void plantSeed();
    void evolveSelected();
    void playNotes (const NoteList& notes);
    void applyToAbleton (const NoteList& notes);
    void setStatus (const juce::String& msg);
    void rebuildTree();
    void scrollTreeToBottom();

    juce::String uid();
    juce::String getLineageDesc (const juce::String& id);

    static void styleParamButton (juce::TextButton& btn, bool active,
                                  juce::Colour activeColour = EvolveColours::accent);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvolveEditor)
};
