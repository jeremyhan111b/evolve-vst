#pragma once
#include <JuceHeader.h>
#include <map>
#include <set>
#include <string>
#include "EvolveTypes.h"
#include <functional>

//==============================================================================
// Music theory helpers
namespace MusicTheory
{
    const juce::StringArray NOTE_NAMES { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    const juce::StringArray FLAT_NAMES { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

    const std::map<std::string, std::vector<int>> KEY_NOTES
    {
        {"C major",  {0,2,4,5,7,9,11}},
        {"D major",  {2,4,6,7,9,11,1}},
        {"F major",  {5,7,9,10,0,2,4}},
        {"G major",  {7,9,11,0,2,4,6}},
        {"Bb major", {10,0,2,3,5,7,9}},
        {"A minor",  {9,11,0,2,4,5,7}},
        {"D minor",  {2,4,5,7,9,10,0}},
        {"E minor",  {4,6,7,9,11,0,2}},
        {"G minor",  {7,9,10,0,2,3,5}},
        {"B minor",  {11,1,2,4,6,7,9}},
        {"C minor",  {0,2,3,5,7,8,10}},
    };

    const std::set<std::string> FLAT_KEYS { "F major","Bb major","D minor","G minor","C minor" };

    inline juce::String noteName (int midi)
    {
        return NOTE_NAMES[midi % 12] + juce::String (midi / 12 - 1);
    }

    inline int nameToMidi (const juce::String& name)
    {
        if (name.isEmpty()) return -1;
        auto trimmed = name.trim();
        // Parse note name + octave e.g. "A4", "C#3", "Bb4"
        const std::map<std::string,int> flatMap {
            {"Bb",10},{"Eb",3},{"Ab",8},{"Db",1},{"Gb",6},{"Cb",11},{"Fb",4}
        };
        int pc = -1;
        juce::String rest;
        if (trimmed.length() >= 2 && (trimmed[1] == '#' || trimmed[1] == 'b'))
        {
            auto key = trimmed.substring (0, 2);
            auto keyStd = key.toStdString();
            if (flatMap.count (keyStd)) pc = flatMap.at (keyStd);
            else
            {
                auto idx = NOTE_NAMES.indexOf (key);
                if (idx >= 0) pc = idx;
            }
            rest = trimmed.substring (2);
        }
        else
        {
            auto key = trimmed.substring (0, 1);
            auto idx = NOTE_NAMES.indexOf (key);
            if (idx >= 0) pc = idx;
            rest = trimmed.substring (1);
        }
        if (pc < 0 || rest.isEmpty()) return -1;
        int octave = rest.getIntValue();
        return pc + (octave + 1) * 12;
    }

    inline juce::String getDiatonic (const juce::String& key)
    {
        const auto keyStr = key.toStdString();
        auto it = KEY_NOTES.find (keyStr);
        if (it == KEY_NOTES.end()) it = KEY_NOTES.find ("A minor");
        const auto& ns = FLAT_KEYS.count (keyStr) ? FLAT_NAMES : NOTE_NAMES;
        juce::StringArray out;
        for (int pc : it->second) out.add (ns[pc]);
        return out.joinIntoString (", ");
    }
}

//==============================================================================
// Apply a list of note changes to a NoteList
class NoteChanger
{
public:
    static NoteList apply (const NoteList& base, const juce::var& changes)
    {
        NoteList ns = base;
        if (auto* arr = changes.getArray())
        {
            for (const auto& c : *arr)
            {
                const auto type = c["type"].toString();
                if      (type == "add")    applyAdd    (ns, c);
                else if (type == "remove") applyRemove (ns, c);
                else if (type == "move")   applyMove   (ns, c);
                else if (type == "rhythm") applyRhythm (ns, c);
            }
        }
        return ns;
    }

private:
    static void applyAdd (NoteList& ns, const juce::var& c)
    {
        int midi = MusicTheory::nameToMidi (c["note"].toString());
        if (midi < 0) return;
        Note n;
        n.pitch = midi;
        n.beat  = (double)c["beat"];
        n.dur   = (double)c["dur"];
        if (n.dur <= 0) n.dur = 1.0;
        ns.push_back (n);
    }

    static void applyRemove (NoteList& ns, const juce::var& c)
    {
        int midi = MusicTheory::nameToMidi (c["note"].toString());
        if (midi < 0) return;
        double beat = (double)c["beat"];
        ns.erase (std::remove_if (ns.begin(), ns.end(), [&](const Note& n)
        {
            return n.pitch == midi && std::abs (n.beat - beat) < 0.26;
        }), ns.end());
    }

    static void applyMove (NoteList& ns, const juce::var& c)
    {
        int fromMidi = MusicTheory::nameToMidi (c["fromNote"].toString());
        if (fromMidi < 0) return;
        int   toMidi   = MusicTheory::nameToMidi (c["toNote"].toString());
        double fromBeat = (double)c["fromBeat"];
        double toBeat   = c["toBeat"].isUndefined() ? -1.0 : (double)c["toBeat"];

        // Find closest matching note
        auto it = std::min_element (ns.begin(), ns.end(), [&](const Note& a, const Note& b)
        {
            double da = (a.pitch == fromMidi) ? std::abs(a.beat - fromBeat) : 1e9;
            double db = (b.pitch == fromMidi) ? std::abs(b.beat - fromBeat) : 1e9;
            return da < db;
        });
        if (it == ns.end() || it->pitch != fromMidi) return;

        if (toMidi > 0)   it->pitch = toMidi;
        if (toBeat >= 0)  it->beat  = toBeat;
    }

    static void applyRhythm (NoteList& ns, const juce::var& c)
    {
        int midi    = MusicTheory::nameToMidi (c["note"].toString());
        if (midi < 0) return;
        double beat   = (double)c["beat"];
        double newDur = (double)c["newDur"];
        if (newDur <= 0) return;

        for (auto& n : ns)
            if (n.pitch == midi && std::abs (n.beat - beat) < 0.26)
                n.dur = newDur;
    }
};

//==============================================================================
// Builds prompts and parses AI responses
class EvolveEngine
{
public:
    struct Result
    {
        bool              ok = false;
        juce::String      title;
        std::vector<EvolveStep> steps;
        NoteList          finalNotes;
        juce::String      error;
    };

    static juce::String buildPrompt (
        const NoteList&    notes,
        const juce::String& key,
        const juce::String& genre,
        const juce::String& param,
        const juce::String& lineageDesc,
        int                 genIndex)
    {
        // Note table
        juce::StringArray noteLines;
        auto sorted = notes;
        std::sort (sorted.begin(), sorted.end(), [](const Note& a, const Note& b){ return a.beat < b.beat; });
        for (int i = 0; i < (int)sorted.size(); ++i)
        {
            const auto& n = sorted[i];
            noteLines.add ("Note " + juce::String(i) + ": " + MusicTheory::noteName(n.pitch)
                + " beat=" + juce::String(n.beat, 2) + " dur=" + juce::String(n.dur, 2));
        }

        // Bar summary
        std::map<int,juce::StringArray> bars;
        for (auto& n : sorted) bars[(int)(n.beat/4)+1].add (MusicTheory::noteName(n.pitch));
        juce::StringArray barLines;
        for (auto& [b, ns] : bars) barLines.add ("Bar " + juce::String(b) + ": " + ns.joinIntoString(", "));

        const int totalBars = (int)std::ceil (std::max (1.0, [&]{ double m=0; for(auto& n:notes) m=std::max(m,n.beat+n.dur); return m; }()) / 4.0);
        const auto diatonic = MusicTheory::getDiatonic (key);

        juce::String paramLine;
        if (param == "auto")
            paramLine = "Choose parameter freely (rhythm/melody/harmony). Vary — avoid 3 of same in a row.";
        else
            paramLine = "All 8 steps change: " + param + " only.";

        return "You are a senior music producer making 8 sequential improvements to a MIDI melody.\n\n"
               "Key: " + key + " | Genre: " + genre + " | " + juce::String(totalBars) + " bars\n"
               "Diatonic pitches (add/move only): " + diatonic + "\n"
               + (lineageDesc.isNotEmpty() ? lineageDesc + "\n" : "") +
               "\nCURRENT MELODY:\n" + barLines.joinIntoString (" | ") +
               "\n\nNOTE REFERENCE:\n" + noteLines.joinIntoString ("\n") +
               "\n\nRULES:\n"
               + paramLine + "\n"
               "- Each step applies to result of previous step. Min 3 noteChanges per step.\n"
               "- Stay in key. Cover all " + juce::String(totalBars) + " bars across 8 steps.\n"
               "- Rhythm steps: affect 3+ notes to create a groove. Snap beats to nearest 0.5.\n"
               "- Melody steps: move by step or 3rd. End phrases on stable tones (root/3rd/5th).\n\n"
               "Return ONLY raw JSON:\n"
               "{\"title\":\"5-word title\",\"steps\":["
               "{\"step\":1,\"param\":\"rhythm\",\"title\":\"Short title\",\"body\":\"What changed\","
               "\"noteChanges\":[{\"type\":\"rhythm\",\"note\":\"A4\",\"beat\":0,\"newDur\":0.5}]}]}\n\n"
               "noteChange types:\n"
               "{\"type\":\"rhythm\",\"note\":\"N\",\"beat\":B,\"newDur\":D}\n"
               "{\"type\":\"move\",\"fromNote\":\"N\",\"fromBeat\":B,\"toNote\":\"N\",\"toBeat\":B}\n"
               "{\"type\":\"add\",\"note\":\"N\",\"beat\":B,\"dur\":D}\n"
               "{\"type\":\"remove\",\"note\":\"N\",\"beat\":B}";
    }

    static Result parseResponse (const juce::String& json, const NoteList& baseNotes)
    {
        Result r;
        auto parsed = juce::JSON::parse (json);
        if (!parsed.isObject()) { r.error = "Invalid JSON response"; return r; }

        r.title = parsed["title"].toString();
        auto* stepsArr = parsed["steps"].getArray();
        if (!stepsArr || stepsArr->isEmpty()) { r.error = "No steps in response"; return r; }

        NoteList cur = baseNotes;
        for (const auto& s : *stepsArr)
        {
            EvolveStep step;
            step.step  = (int)s["step"];
            step.param = s["param"].toString();
            step.title = s["title"].toString();
            step.body  = s["body"].toString();

            cur = NoteChanger::apply (cur, s["noteChanges"]);
            step.snapshot = cur;
            r.steps.push_back (step);
        }

        if (cur.empty()) { r.error = "Evolution produced empty melody"; return r; }
        r.finalNotes = cur;
        r.ok = true;
        return r;
    }

    // Extract JSON from response that may have markdown fences
    static juce::String cleanJSON (const juce::String& text)
    {
        auto t = text.trim();
        // Strip ```json ... ```
        if (t.startsWith ("```"))
        {
            int start = t.indexOf ("\n");
            int end   = t.lastIndexOf ("```");
            if (start >= 0 && end > start)
                t = t.substring (start+1, end).trim();
        }
        // Find first {
        int fi = t.indexOf ("{");
        if (fi > 0) t = t.substring (fi);
        return t;
    }
};
