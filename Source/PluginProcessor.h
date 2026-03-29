#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>

//==============================================================================
struct CapturedNote
{
    int   pitch;
    double beat;
    double dur;
};

//==============================================================================
class EvolveProcessor : public juce::AudioProcessor,
                        public juce::ChangeBroadcaster
{
public:
    EvolveProcessor();
    ~EvolveProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==========================================================================
    const juce::String getName() const override { return "EVOLVE"; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==========================================================================
    int  getNumPrograms() override    { return 1; }
    int  getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==========================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Auto-capture: always listening, returns latest captured notes
    juce::String getCapturedNotesJSON() const;

    // True when a complete loop pass has been captured and is ready
    bool hasFreshCapture() const { return freshCapture.load(); }
    void clearFreshCapture()     { freshCapture.store (false); }

    // Called from editor to play notes back out via MIDI
    void scheduleNotesForPlayback (const juce::String& notesJSON, double bpm);

    // Store/retrieve API key and settings
    void setApiKey    (const juce::String& key)   { apiKey   = key; }
    void setUserKey   (const juce::String& key)   { userKey  = key; }
    juce::String getApiKey()   const { return apiKey; }
    juce::String getUserKey()  const { return userKey; }

    // Make Anthropic API call (runs on background thread, calls callback on message thread)
    void callAnthropic (const juce::String& requestJSON,
                        std::function<void(juce::String response, juce::String error)> callback);

    // Current loop region from host (in beats)
    double loopStart = 0.0;
    double loopEnd   = 0.0;
    double bpm       = 120.0;
    bool   isLooping = false;
    bool   isPlaying = false;

private:
    //── Auto-capture state ───────────────────────────────────────
    // Notes being collected during the current loop pass
    std::vector<CapturedNote> pendingNotes;
    // Notes from the last completed loop pass (ready for editor)
    std::vector<CapturedNote> capturedNotes;
    juce::CriticalSection     noteLock;

    // Track active (held) notes for duration calculation
    struct ActiveNote { double startBeat; };
    std::map<int, ActiveNote> activeNotes;

    // Loop detection
    double prevBeat         = -1.0;
    bool   wasPlaying       = false;
    std::atomic<bool> freshCapture { false };

    //── MIDI output queue for playback ───────────────────────────
    struct ScheduledNote { int pitch; double startSec; double endSec; };
    std::vector<ScheduledNote> scheduledNotes;
    double playbackStartTime = 0.0;
    juce::CriticalSection scheduleLock;

    double sampleRate_ = 44100.0;
    double currentBeat = 0.0;
    juce::int64 totalSamples = 0;

    juce::String apiKey;
    juce::String userKey;

    juce::ThreadPool threadPool { 2 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EvolveProcessor)
};
