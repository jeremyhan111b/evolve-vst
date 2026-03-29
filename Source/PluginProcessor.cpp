#include "PluginProcessor.h"

//==============================================================================
EvolveProcessor::EvolveProcessor()
    : AudioProcessor (BusesProperties()
                      .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

EvolveProcessor::~EvolveProcessor() {}

//==============================================================================
void EvolveProcessor::prepareToPlay (double sr, int)
{
    sampleRate_ = sr;
    totalSamples = 0;
}

void EvolveProcessor::releaseResources() {}

//==============================================================================
void EvolveProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    audio.clear();

    // Get playhead info
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            isPlaying = pos->getIsPlaying();
            isLooping = pos->getIsLooping();

            if (auto b = pos->getBpm())           bpm       = *b;
            if (auto p = pos->getPpqPosition())   currentBeat = *p;

            if (auto loop = pos->getLoopPoints())
            {
                loopStart = loop->ppqStart;
                loopEnd   = loop->ppqEnd;
            }
        }
    }

    const double secondsPerBeat = 60.0 / bpm;

    // ── MIDI INPUT CAPTURE ──
    if (capturing.load())
    {
        for (auto meta : midi)
        {
            const auto msg = meta.getMessage();
            const double sampleOffset = meta.samplePosition;
            const double beatOffset   = sampleOffset / sampleRate_ / secondsPerBeat;
            const double noteBeat     = currentBeat + beatOffset;

            if (msg.isNoteOn())
            {
                const int pitch = msg.getNoteNumber();
                // Only capture notes within loop region if looping
                if (!isLooping || (noteBeat >= loopStart && noteBeat < loopEnd))
                {
                    juce::ScopedLock lock (noteLock);
                    activeNotes[pitch] = { noteBeat };
                }
            }
            else if (msg.isNoteOff())
            {
                const int pitch = msg.getNoteNumber();
                juce::ScopedLock lock (noteLock);
                if (activeNotes.count(pitch))
                {
                    const double startBeat = activeNotes[pitch].startBeat;
                    const double dur       = noteBeat - startBeat;
                    if (dur > 0.0)
                    {
                        // Normalise to loop start
                        const double offset = isLooping ? loopStart : startBeat;
                        capturedNotes.push_back ({
                            pitch,
                            std::max (0.0, startBeat - offset),
                            std::max (0.0625, dur)
                        });
                    }
                    activeNotes.erase (pitch);
                }
            }
        }
    }

    // ── MIDI OUTPUT (playback) ──
    {
        juce::ScopedLock lock (scheduleLock);
        if (!scheduledNotes.empty())
        {
            const double nowSec = totalSamples / sampleRate_;
            const int numSamples = audio.getNumSamples();

            for (const auto& sn : scheduledNotes)
            {
                const double blockStartSec = nowSec;
                const double blockEndSec   = nowSec + numSamples / sampleRate_;

                if (sn.startSec >= blockStartSec && sn.startSec < blockEndSec)
                {
                    const int offset = (int)((sn.startSec - blockStartSec) * sampleRate_);
                    midi.addEvent (juce::MidiMessage::noteOn  (1, sn.pitch, (juce::uint8)100), offset);
                }
                if (sn.endSec >= blockStartSec && sn.endSec < blockEndSec)
                {
                    const int offset = (int)((sn.endSec - blockStartSec) * sampleRate_);
                    midi.addEvent (juce::MidiMessage::noteOff (1, sn.pitch, (juce::uint8)0),   offset);
                }
            }

            // Remove notes that have finished
            const double nowPlusSome = nowSec + numSamples / sampleRate_;
            scheduledNotes.erase (
                std::remove_if (scheduledNotes.begin(), scheduledNotes.end(),
                    [nowPlusSome](const ScheduledNote& sn){ return sn.endSec < nowPlusSome; }),
                scheduledNotes.end());
        }
    }

    totalSamples += audio.getNumSamples();
}

//==============================================================================
void EvolveProcessor::startCapture()
{
    juce::ScopedLock lock (noteLock);
    capturedNotes.clear();
    activeNotes.clear();
    capturing.store (true);
}

void EvolveProcessor::stopCapture()
{
    // Close any held notes
    {
        juce::ScopedLock lock (noteLock);
        for (auto& kv : activeNotes)
        {
            const double offset = isLooping ? loopStart : kv.second.startBeat;
            capturedNotes.push_back ({
                kv.first,
                std::max (0.0, kv.second.startBeat - offset),
                0.25
            });
        }
        activeNotes.clear();
    }
    capturing.store (false);
    sendChangeMessage(); // notify editor
}

juce::String EvolveProcessor::getCapturedNotesJSON() const
{
    juce::ScopedLock lock (const_cast<juce::CriticalSection&>(noteLock));
    juce::String json = "[";
    for (int i = 0; i < (int)capturedNotes.size(); ++i)
    {
        const auto& n = capturedNotes[i];
        json += "{\"pitch\":" + juce::String(n.pitch)
             + ",\"beat\":"   + juce::String(n.beat, 3)
             + ",\"dur\":"    + juce::String(n.dur,  3)
             + "}";
        if (i < (int)capturedNotes.size() - 1) json += ",";
    }
    json += "]";
    return json;
}

//==============================================================================
void EvolveProcessor::scheduleNotesForPlayback (const juce::String& notesJSON, double playBpm)
{
    // Parse JSON array of {pitch, beat, dur}
    auto parsed = juce::JSON::parse (notesJSON);
    if (auto* arr = parsed.getArray())
    {
        const double spb = 60.0 / playBpm;
        const double nowSec = totalSamples / sampleRate_;

        juce::ScopedLock lock (scheduleLock);
        scheduledNotes.clear();

        for (const auto& item : *arr)
        {
            const int    pitch = (int)  item["pitch"];
            const double beat  = (double)item["beat"];
            const double dur   = (double)item["dur"];
            scheduledNotes.push_back ({
                pitch,
                nowSec + beat * spb + 0.1,
                nowSec + (beat + dur) * spb + 0.1
            });
        }
    }
}

//==============================================================================
void EvolveProcessor::callAnthropic (const juce::String& requestJSON,
                                      std::function<void(juce::String, juce::String)> callback)
{
    const juce::String key = apiKey.isNotEmpty() ? apiKey : userKey;

    if (key.isEmpty())
    {
        juce::MessageManager::callAsync ([callback]()
        {
            callback ("", "NO_API_KEY");
        });
        return;
    }

    threadPool.addJob ([this, requestJSON, key, callback]()
    {
        juce::URL url ("https://api.anthropic.com/v1/messages");
        url = url.withPOSTData (requestJSON);

        juce::String response;
        juce::String error;

        try
        {
            juce::Logger::writeToLog ("EVOLVE callAnthropic: key length=" + juce::String(key.length()) + " payload length=" + juce::String(requestJSON.length()));

            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inPostData)
                .withExtraHeaders ("x-api-key: " + key + "\r\n"
                                   "anthropic-version: 2023-06-01\r\n"
                                   "Content-Type: application/json")
                .withConnectionTimeoutMs (60000)
                .withHttpRequestCmd ("POST");

            auto stream = url.createInputStream (opts);

            if (stream != nullptr)
            {
                response = stream->readEntireStreamAsString();
            }
            else
            {
                error = "NO_INTERNET";
            }
        }
        catch (...)
        {
            error = "Request failed";
        }

        juce::MessageManager::callAsync ([callback, response, error]()
        {
            callback (response, error);
        });
    });
}

//==============================================================================
void EvolveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("EVOLVEState");
    state.setProperty ("apiKey",  apiKey,  nullptr);
    state.setProperty ("userKey", userKey, nullptr);

    juce::MemoryOutputStream stream (destData, false);
    state.writeToStream (stream);
}

void EvolveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream (data, (size_t)sizeInBytes, false);
    auto state = juce::ValueTree::readFromStream (stream);

    if (state.isValid())
    {
        apiKey  = state.getProperty ("apiKey",  "").toString();
        userKey = state.getProperty ("userKey", "").toString();
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EvolveProcessor();
}
