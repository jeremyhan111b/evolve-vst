#pragma once
#include <JuceHeader.h>
#include "EvolveTypes.h"

//==============================================================================
// Read-only mini piano roll — used in generation cards and trail steps
class MiniRoll : public juce::Component
{
public:
    MiniRoll() {}

    void setNotes (const NoteList& n)
    {
        notes = n;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (EvolveColours::bg);

        if (notes.empty()) return;

        int minP = 127, maxP = 0;
        double maxB = 0.0;
        for (auto& n : notes)
        {
            minP = std::min (minP, n.pitch);
            maxP = std::max (maxP, n.pitch);
            maxB = std::max (maxB, n.beat + n.dur);
        }
        const int range  = std::max (maxP - minP, 7);
        const double len = std::max (maxB, 1.0);
        const float w    = (float)getWidth();
        const float h    = (float)getHeight();

        g.setColour (EvolveColours::accent);
        for (auto& n : notes)
        {
            const float x  = (float)(n.beat / len) * w;
            const float bw = std::max ((float)(n.dur / len) * w - 0.5f, 1.0f);
            const float y  = h - 2.0f - ((float)(n.pitch - minP) / (float)range) * (h - 4.0f);
            g.fillRect (x, y, bw, 2.0f);
        }
    }

private:
    NoteList notes;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiniRoll)
};

//==============================================================================
// Full piano roll with note display (read-only, shows captured seed)
class PianoRollDisplay : public juce::Component
{
public:
    PianoRollDisplay() {}

    void setNotes (const NoteList& n)
    {
        notes = n;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (EvolveColours::bg);

        const float w  = (float)getWidth();
        const float h  = (float)getHeight();

        // Draw bar lines
        g.setColour (EvolveColours::border);
        for (int bar = 0; bar <= 8; bar++)
            g.fillRect ((float)bar / 8.0f * w, 0.0f, 1.0f, h);

        if (notes.empty())
        {
            g.setColour (EvolveColours::muted);
            g.setFont (juce::Font (11.0f));
            g.drawText ("No notes captured", getLocalBounds(), juce::Justification::centred);
            return;
        }

        int minP = 127, maxP = 0;
        double maxB = 0.0;
        for (auto& n : notes)
        {
            minP = std::min (minP, n.pitch);
            maxP = std::max (maxP, n.pitch);
            maxB = std::max (maxB, n.beat + n.dur);
        }
        const int    range = std::max (maxP - minP + 2, 8);
        const double len   = std::max (maxB, 1.0);

        g.setColour (EvolveColours::accent);
        for (auto& n : notes)
        {
            const float x  = (float)(n.beat / len) * w;
            const float bw = std::max ((float)(n.dur / len) * w - 1.0f, 2.0f);
            const float y  = h - 4.0f - ((float)(n.pitch - minP + 1) / (float)range) * (h - 8.0f);
            g.fillRect (x, y, bw, 4.0f);
            g.setColour (EvolveColours::accent.withAlpha (0.3f));
            g.fillRect (x, y, bw, 1.0f);
            g.setColour (EvolveColours::accent);
        }
    }

private:
    NoteList notes;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoRollDisplay)
};
