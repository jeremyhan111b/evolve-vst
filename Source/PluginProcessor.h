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
    // Called from editor when user clicks "Capture"
    void startCapture();
    void stopCapture();
    bool isCapturing() const { return capturing.load(); }

    // Returns captured notes as JSON string
    juce::String getCapturedNotesJSON() const;

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
    std::atomic<bool>  capturing { false };
    std::vector<CapturedNote> capturedNotes;
    juce::CriticalSection     noteLock;

    // Notes currently sounding (note number -> start beat)
    struct ActiveNote { double startBeat; };
    std::map<int, ActiveNote> activeNotes;

    // MIDI output queue for playback
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
