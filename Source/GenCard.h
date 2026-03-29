#pragma once
#include <JuceHeader.h>
#include "EvolveTypes.h"
#include "PianoRoll.h"

//==============================================================================
class GenCard : public juce::Component
{
public:
    std::function<void()>            onSelect;
    std::function<void()>            onHear;
    std::function<void()>            onApply;
    std::function<void(int stepIdx)> onHearStep;
    std::function<void(int stepIdx)> onPlantStep;

    GenCard (const Generation& gen, bool selected)
    {
        setGen (gen, selected);
    }

    void setGen (const Generation& g, bool sel)
    {
        gen      = g;
        isSelected = sel;
        trailOpen  = false;
        rebuildChildren();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // Card background
        g.fillAll (EvolveColours::surface);

        // Border
        g.setColour (isSelected ? EvolveColours::accent : EvolveColours::border);
        g.drawRect (getLocalBounds(), 1);

        // Header area background
        g.setColour (EvolveColours::raised);
        g.fillRect (0, 0, getWidth(), headerH);

        // Param dot
        g.setColour (EvolveColours::forParam (gen.param));
        g.fillEllipse (10.0f, (float)(headerH/2) - 4.0f, 8.0f, 8.0f);

        // Gen number
        g.setColour (EvolveColours::muted);
        g.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
        g.drawText ("GEN " + juce::String(gen.genIndex),
                    22, 0, 42, headerH, juce::Justification::centredLeft);

        // Title
        g.setColour (EvolveColours::text);
        g.setFont (juce::Font(juce::FontOptions{}.withHeight(11.0f)));
        g.drawText (gen.title, 68, 0, getWidth() - 80, headerH,
                    juce::Justification::centredLeft, true);

        // Step count badge
        if (!gen.steps.empty())
        {
            g.setColour (EvolveColours::muted);
            g.setFont (juce::Font(juce::FontOptions{}.withHeight(9.0f)));
            g.drawText (juce::String(gen.steps.size()) + "\u2736",
                        getWidth() - 32, 0, 28, headerH, juce::Justification::centredRight);
        }
    }

    void resized() override
    {
        // Mini roll
        miniRoll.setBounds (6, headerH + 2, getWidth() - 12, miniRollH);

        // Action buttons
        const int bw = getWidth() / 3;
        hearBtn.setBounds  (0,             headerH + miniRollH + 4, bw,     btnH);
        selectBtn.setBounds(bw,            headerH + miniRollH + 4, bw,     btnH);
        applyBtn.setBounds (bw*2,          headerH + miniRollH + 4, getWidth()-bw*2, btnH);

        // Trail toggle
        const int trailToggleY = headerH + miniRollH + 4 + btnH;
        trailToggle.setBounds (0, trailToggleY, getWidth(), trailTogglH);

        // Trail steps
        if (trailOpen)
        {
            int y = trailToggleY + trailTogglH;
            for (int i = 0; i < (int)stepPanels.size(); ++i)
            {
                stepPanels[i]->setBounds (0, y, getWidth(), stepH);
                y += stepH;
            }
        }
    }

    int getPreferredHeight() const
    {
        int h = headerH + miniRollH + 4 + btnH;
        if (!gen.steps.empty()) h += trailTogglH;
        if (trailOpen) h += (int)stepPanels.size() * stepH;
        return h;
    }

private:
    static constexpr int headerH     = 28;
    static constexpr int miniRollH   = 32;
    static constexpr int btnH        = 24;
    static constexpr int trailTogglH = 20;
    static constexpr int stepH       = 52;

    Generation gen;
    bool       isSelected  = false;
    bool       trailOpen   = false;

    MiniRoll   miniRoll;

    juce::TextButton hearBtn   { "HEAR" };
    juce::TextButton selectBtn { "SELECT" };
    juce::TextButton applyBtn  { "APPLY" };
    juce::TextButton trailToggle;

    struct StepPanel : public juce::Component
    {
        MiniRoll     roll;
        juce::Label  titleLabel;
        juce::TextButton hearBtn  { "HEAR" };
        juce::TextButton plantBtn { "PLANT" };

        StepPanel()
        {
            addAndMakeVisible (roll);
            addAndMakeVisible (titleLabel);
            addAndMakeVisible (hearBtn);
            addAndMakeVisible (plantBtn);
            titleLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f)));
            titleLabel.setColour (juce::Label::textColourId, EvolveColours::text);
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (EvolveColours::border);
            g.fillRect (0, 0, getWidth(), 1);
        }

        void resized() override
        {
            roll.setBounds (6, 2, getWidth()-12, 20);
            titleLabel.setBounds (6, 24, getWidth()-70, 18);
            hearBtn.setBounds  (getWidth()-64, 26, 28, 16);
            plantBtn.setBounds (getWidth()-34, 26, 30, 16);
        }
    };

    std::vector<std::unique_ptr<StepPanel>> stepPanels;

    void rebuildChildren()
    {
        removeAllChildren();
        stepPanels.clear();

        miniRoll.setNotes (gen.notes);
        addAndMakeVisible (miniRoll);

        styleButton (hearBtn,   false);
        styleButton (selectBtn, false);
        styleButton (applyBtn,  true);

        hearBtn.onClick   = [this]{ if (onHear)   onHear();   };
        selectBtn.onClick = [this]{ if (onSelect) onSelect(); };
        applyBtn.onClick  = [this]{ if (onApply)  onApply();  };

        addAndMakeVisible (hearBtn);
        addAndMakeVisible (selectBtn);
        addAndMakeVisible (applyBtn);

        if (!gen.steps.empty())
        {
            trailToggle.setButtonText ("TRAIL (" + juce::String(gen.steps.size()) + " steps)");
            styleButton (trailToggle, false);
            trailToggle.onClick = [this]
            {
                trailOpen = !trailOpen;
                trailToggle.setButtonText ((trailOpen ? "HIDE" : "SHOW") + juce::String(" TRAIL (") + juce::String(gen.steps.size()) + " steps)");
                rebuildTrailSteps();
                if (auto* p = getParentComponent())
                    p->resized();
            };
            addAndMakeVisible (trailToggle);
        }

        resized();
    }

    void rebuildTrailSteps()
    {
        for (auto& sp : stepPanels) removeChildComponent (sp.get());
        stepPanels.clear();

        if (!trailOpen) return;

        for (int i = 0; i < (int)gen.steps.size(); ++i)
        {
            const auto& step = gen.steps[i];
            auto panel = std::make_unique<StepPanel>();
            panel->roll.setNotes (step.snapshot);
            panel->titleLabel.setText (juce::String(step.step) + ". " + step.title, juce::dontSendNotification);
            panel->titleLabel.setColour (juce::Label::textColourId,
                                         EvolveColours::forParam (step.param));

            const int idx = i;
            panel->hearBtn.onClick  = [this, idx]{ if (onHearStep)  onHearStep(idx);  };
            panel->plantBtn.onClick = [this, idx]{ if (onPlantStep) onPlantStep(idx); };

            styleButton (panel->hearBtn,  false);
            styleButton (panel->plantBtn, false);

            addAndMakeVisible (*panel);
            stepPanels.push_back (std::move (panel));
        }
        resized();
    }

    void styleButton (juce::TextButton& btn, bool accent)
    {
        btn.setColour (juce::TextButton::buttonColourId,
                       accent ? EvolveColours::accent : EvolveColours::raised);
        btn.setColour (juce::TextButton::textColourOnId,
                       accent ? EvolveColours::bg : EvolveColours::text);
        btn.setColour (juce::TextButton::textColourOffId,
                       accent ? EvolveColours::bg : EvolveColours::muted);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GenCard)
};
