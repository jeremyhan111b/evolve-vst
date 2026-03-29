#pragma once
#include <JuceHeader.h>
#include <vector>
#include <string>
#include <functional>

//==============================================================================
struct Note
{
    int    pitch = 60;   // MIDI note number
    double beat  = 0.0;  // start beat
    double dur   = 1.0;  // duration in beats
};

using NoteList = std::vector<Note>;

//==============================================================================
struct EvolveStep
{
    int        step     = 0;
    juce::String param;
    juce::String title;
    juce::String body;
    NoteList   snapshot; // notes after this step
};

//==============================================================================
struct Generation
{
    juce::String id;
    juce::String parentId;
    NoteList     notes;
    juce::String param;   // "seed","auto","rhythm","melody","harmony","form"
    juce::String title;
    int          genIndex = 0;
    std::vector<EvolveStep> steps;
};

//==============================================================================
// Colours
namespace EvolveColours
{
    const juce::Colour bg      { 0xff0c0c10 };
    const juce::Colour surface { 0xff13131a };
    const juce::Colour raised  { 0xff1a1a24 };
    const juce::Colour border  { 0xff22222e };
    const juce::Colour accent  { 0xffc8ff00 };
    const juce::Colour orange  { 0xffff6b35 };
    const juce::Colour purple  { 0xff7b61ff };
    const juce::Colour pink    { 0xffff61d8 };
    const juce::Colour text    { 0xffdddde8 };
    const juce::Colour muted   { 0xff4a4a60 };
    const juce::Colour dim     { 0xff2a2a38 };
    const juce::Colour red     { 0xffff4444 };

    inline juce::Colour forParam (const juce::String& param)
    {
        if (param == "rhythm")  return orange;
        if (param == "harmony") return purple;
        if (param == "form")    return pink;
        return accent; // auto, melody, seed
    }
}
