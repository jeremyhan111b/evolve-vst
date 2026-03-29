#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::String makeUID()
{
    return "g" + juce::String (juce::Time::currentTimeMillis())
               + juce::String (juce::Random::getSystemRandom().nextInt (9999));
}

//==============================================================================
EvolveEditor::EvolveEditor (EvolveProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setSize (1100, 720);
    setResizable (true, true);
    setResizeLimits (800, 560, 1600, 1200);

    setupHeader();
    setupLeftPanel();
    setupTree();
    setupApiKeyOverlay();
    updateApiKeyVisibility();

    processor.addChangeListener (this);
    startTimer (200);   // poll for auto-capture and BPM updates
}

EvolveEditor::~EvolveEditor()
{
    stopTimer();
    processor.removeChangeListener (this);
}

//==============================================================================
void EvolveEditor::setupHeader()
{
    logoLabel.setText ("EVOLVE", juce::dontSendNotification);
    logoLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(16.0f).withStyle("Bold")));
    logoLabel.setColour (juce::Label::textColourId, EvolveColours::accent);
    addAndMakeVisible (logoLabel);

    keyBox.addItem ("C major",  1); keyBox.addItem ("D major",  2);
    keyBox.addItem ("F major",  3); keyBox.addItem ("G major",  4);
    keyBox.addItem ("Bb major", 5); keyBox.addItem ("A minor",  6);
    keyBox.addItem ("D minor",  7); keyBox.addItem ("E minor",  8);
    keyBox.addItem ("G minor",  9); keyBox.addItem ("B minor",  10);
    keyBox.addItem ("C minor",  11);
    keyBox.setSelectedId (6, juce::dontSendNotification);
    keyBox.setColour (juce::ComboBox::backgroundColourId, EvolveColours::surface);
    keyBox.setColour (juce::ComboBox::textColourId,       EvolveColours::text);
    keyBox.setColour (juce::ComboBox::outlineColourId,    EvolveColours::border);
    addAndMakeVisible (keyBox);

    for (auto& g : juce::StringArray { "Lo-fi Hip Hop", "Electronic / House", "Techno",
                                       "Trap", "Drum & Bass", "Ambient", "Pop",
                                       "R&B / Soul", "Jazz", "Cinematic" })
        genreBox.addItem (g, genreBox.getNumItems() + 1);
    genreBox.setSelectedId (1, juce::dontSendNotification);
    genreBox.setColour (juce::ComboBox::backgroundColourId, EvolveColours::surface);
    genreBox.setColour (juce::ComboBox::textColourId,       EvolveColours::text);
    genreBox.setColour (juce::ComboBox::outlineColourId,    EvolveColours::border);
    addAndMakeVisible (genreBox);

    bpmSlider.setRange (40, 240, 1);
    bpmSlider.setValue (120);
    bpmSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 48, 22);
    bpmSlider.setColour (juce::Slider::textBoxBackgroundColourId, EvolveColours::surface);
    bpmSlider.setColour (juce::Slider::textBoxTextColourId,       EvolveColours::text);
    bpmSlider.setColour (juce::Slider::textBoxOutlineColourId,    EvolveColours::border);
    addAndMakeVisible (bpmSlider);
}

//==============================================================================
void EvolveEditor::setupLeftPanel()
{
    seedLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    seedLabel.setColour (juce::Label::textColourId, EvolveColours::muted);
    addAndMakeVisible (seedLabel);

    addAndMakeVisible (seedRoll);

    captureInfo.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    captureInfo.setColour (juce::Label::textColourId, EvolveColours::muted);
    captureInfo.setJustificationType (juce::Justification::centred);
    captureInfo.setText ("Loop a section in your DAW — seed loads automatically",
                         juce::dontSendNotification);
    addAndMakeVisible (captureInfo);

    hearSeedBtn.setEnabled (false);
    hearSeedBtn.setColour (juce::TextButton::buttonColourId,  EvolveColours::raised);
    hearSeedBtn.setColour (juce::TextButton::textColourOffId, EvolveColours::text);
    hearSeedBtn.onClick = [this]{ playNotes (seedNotes); };
    addAndMakeVisible (hearSeedBtn);

    plantBtn.setEnabled (false);
    plantBtn.setColour (juce::TextButton::buttonColourId,  EvolveColours::accent);
    plantBtn.setColour (juce::TextButton::textColourOnId,  EvolveColours::bg);
    plantBtn.setColour (juce::TextButton::textColourOffId, EvolveColours::bg);
    plantBtn.onClick = [this]{ plantSeed(); };
    addAndMakeVisible (plantBtn);

    evolveLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    evolveLabel.setColour (juce::Label::textColourId, EvolveColours::muted);
    addAndMakeVisible (evolveLabel);

    for (auto* btn : { &autoBtn, &rhythmBtn, &melodyBtn, &harmonyBtn, &formBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,  EvolveColours::raised);
        btn->setColour (juce::TextButton::textColourOffId, EvolveColours::muted);
        addAndMakeVisible (*btn);
    }
    autoBtn.onClick    = [this]{ setParam ("auto"); };
    rhythmBtn.onClick  = [this]{ setParam ("rhythm"); };
    melodyBtn.onClick  = [this]{ setParam ("melody"); };
    harmonyBtn.onClick = [this]{ setParam ("harmony"); };
    formBtn.onClick    = [this]{ setParam ("form"); };
    setParam ("auto");

    evolveBtn.setEnabled (false);
    evolveBtn.setColour (juce::TextButton::buttonColourId,  EvolveColours::accent);
    evolveBtn.setColour (juce::TextButton::textColourOnId,  EvolveColours::bg);
    evolveBtn.setColour (juce::TextButton::textColourOffId, EvolveColours::bg);
    evolveBtn.onClick = [this]{ evolveSelected(); };
    addAndMakeVisible (evolveBtn);

    statusLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f)));
    statusLabel.setColour (juce::Label::textColourId, EvolveColours::muted);
    setStatus ("Loop a section in your DAW to capture seed notes");
    addAndMakeVisible (statusLabel);
}

//==============================================================================
void EvolveEditor::setupTree()
{
    treeLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(10.0f).withStyle("Bold")));
    treeLabel.setColour (juce::Label::textColourId, EvolveColours::muted);
    addAndMakeVisible (treeLabel);

    treeContainer.setSize (400, 100);
    treeViewport.setViewedComponent (&treeContainer, false);
    treeViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (treeViewport);
}

//==============================================================================
void EvolveEditor::setupApiKeyOverlay()
{
    apiKeyOverlay.setVisible (false);
    addAndMakeVisible (apiKeyOverlay);

    apiKeyTitle.setText ("EVOLVE", juce::dontSendNotification);
    apiKeyTitle.setFont (juce::Font(juce::FontOptions{}.withHeight(28.0f).withStyle("Bold")));
    apiKeyTitle.setColour (juce::Label::textColourId, EvolveColours::accent);
    apiKeyTitle.setJustificationType (juce::Justification::centred);
    apiKeyOverlay.addAndMakeVisible (apiKeyTitle);

    apiKeyLabel.setFont (juce::Font(juce::FontOptions{}.withHeight(12.0f)));
    apiKeyLabel.setColour (juce::Label::textColourId, EvolveColours::muted);
    apiKeyLabel.setJustificationType (juce::Justification::centred);
    apiKeyLabel.setText ("Enter your Anthropic API key to get started.\nGet one free at console.anthropic.com",
                         juce::dontSendNotification);
    apiKeyOverlay.addAndMakeVisible (apiKeyLabel);

    apiKeyEditor.setPasswordCharacter ('*');
    apiKeyEditor.setColour (juce::TextEditor::backgroundColourId,     EvolveColours::surface);
    apiKeyEditor.setColour (juce::TextEditor::textColourId,           EvolveColours::text);
    apiKeyEditor.setColour (juce::TextEditor::outlineColourId,        EvolveColours::border);
    apiKeyEditor.setColour (juce::TextEditor::focusedOutlineColourId, EvolveColours::accent);
    apiKeyEditor.setFont (juce::Font(juce::FontOptions{}.withHeight(12.0f)));
    apiKeyEditor.setTextToShowWhenEmpty ("sk-ant-...", EvolveColours::muted);
    apiKeyOverlay.addAndMakeVisible (apiKeyEditor);

    apiSaveBtn.setColour (juce::TextButton::buttonColourId,  EvolveColours::accent);
    apiSaveBtn.setColour (juce::TextButton::textColourOnId,  EvolveColours::bg);
    apiSaveBtn.setColour (juce::TextButton::textColourOffId, EvolveColours::bg);
    apiSaveBtn.onClick = [this]
    {
        const auto key = apiKeyEditor.getText().trim();
        if (key.startsWith ("sk-"))
        {
            processor.setUserKey (key);
            updateApiKeyVisibility();
        }
        else
        {
            apiKeyLabel.setText ("Key must start with 'sk-'  —  get one at console.anthropic.com",
                                 juce::dontSendNotification);
            apiKeyLabel.setColour (juce::Label::textColourId, EvolveColours::orange);
        }
    };
    apiKeyOverlay.addAndMakeVisible (apiSaveBtn);
}

//==============================================================================
void EvolveEditor::updateApiKeyVisibility()
{
    const bool hasKey = processor.getUserKey().isNotEmpty();
    apiKeyOverlay.setVisible (!hasKey);
    resized();
}

//==============================================================================
void EvolveEditor::paint (juce::Graphics& g)
{
    g.fillAll (EvolveColours::bg);

    // Header
    g.setColour (EvolveColours::surface);
    g.fillRect (0, 0, getWidth(), 38);
    g.setColour (EvolveColours::border);
    g.fillRect (0, 37, getWidth(), 1);

    // Left panel
    g.setColour (EvolveColours::surface);
    g.fillRect (0, 38, leftW, getHeight() - 38);
    g.setColour (EvolveColours::border);
    g.fillRect (leftW, 38, 1, getHeight() - 38);
    g.fillRect (0, seedDivY,   leftW, 1);
    g.fillRect (0, evolveDivY, leftW, 1);

    // API key overlay
    if (apiKeyOverlay.isVisible())
    {
        g.setColour (EvolveColours::bg.withAlpha (0.92f));
        g.fillAll();
    }
}

//==============================================================================
void EvolveEditor::resized()
{
    const int w = getWidth(), h = getHeight();
    const int pad = 8;

    // Header
    logoLabel.setBounds  (10,  8, 80,  22);
    keyBox.setBounds     (95,  8, 90,  22);
    genreBox.setBounds   (191, 8, 130, 22);
    bpmSlider.setBounds  (327, 8, 100, 22);

    // Left panel — seed section
    seedLabel.setBounds   (pad, 46,  leftW-pad*2, 16);
    seedRoll.setBounds    (pad, 64,  leftW-pad*2, 64);
    captureInfo.setBounds (pad, 130, leftW-pad*2, 28);

    const int seedBtnRow = 160;
    const int seedBtnW   = leftW - pad*2 - 34;
    plantBtn.setBounds    (pad,                seedBtnRow, seedBtnW, 26);
    hearSeedBtn.setBounds (pad + seedBtnW + 2, seedBtnRow, 32,      26);

    seedDivY = seedBtnRow + 34;

    // Evolve section
    evolveLabel.setBounds (pad, seedDivY + 6, leftW-pad*2, 16);

    const int pbw = (leftW - pad*2 - 4) / 5;
    autoBtn.setBounds    (pad,           seedDivY+26, pbw,                    22);
    rhythmBtn.setBounds  (pad+pbw+1,     seedDivY+26, pbw,                    22);
    melodyBtn.setBounds  (pad+pbw*2+2,   seedDivY+26, pbw,                    22);
    harmonyBtn.setBounds (pad+pbw*3+3,   seedDivY+26, pbw,                    22);
    formBtn.setBounds    (pad+pbw*4+4,   seedDivY+26, leftW-pad-pbw*4-4,      22);
    evolveBtn.setBounds  (pad,           seedDivY+52, leftW-pad*2,            28);

    evolveDivY = seedDivY + 88;

    // Status bar
    statusLabel.setBounds (pad, h - 22, leftW - pad*2, 18);

    // Tree panel
    treeLabel.setBounds    (leftW+10, 46, w-leftW-20, 16);
    treeViewport.setBounds (leftW+1,  64, w-leftW-1,  h-64);
    treeContainer.setSize  (treeViewport.getWidth()-16,
                            std::max (treeContainer.getHeight(), treeViewport.getHeight()));

    // API key overlay
    if (apiKeyOverlay.isVisible())
    {
        apiKeyOverlay.setBounds (0, 0, w, h);
        const int cw = std::min (440, w-40);
        const int cx = (w-cw)/2, cy = h/2 - 90;
        apiKeyTitle.setBounds  (cx, cy,      cw, 38);
        apiKeyLabel.setBounds  (cx, cy+42,   cw, 36);
        apiKeyEditor.setBounds (cx, cy+84,   cw, 32);
        apiSaveBtn.setBounds   (cx, cy+122,  cw, 34);
    }
}

//==============================================================================
void EvolveEditor::timerCallback()
{
    bpmSlider.setValue (processor.bpm, juce::dontSendNotification);
    checkForAutoCapture();
}

//==============================================================================
void EvolveEditor::checkForAutoCapture()
{
    if (!processor.hasFreshCapture()) return;
    processor.clearFreshCapture();

    const auto json   = processor.getCapturedNotesJSON();
    const auto parsed = juce::JSON::parse (json);
    NoteList   notes;

    if (auto* arr = parsed.getArray())
    {
        for (const auto& item : *arr)
        {
            Note n;
            n.pitch = (int)   item["pitch"];
            n.beat  = (double)item["beat"];
            n.dur   = (double)item["dur"];
            if (n.pitch > 0 && n.dur > 0) notes.push_back (n);
        }
    }

    if (notes.empty()) return;

    seedNotes = notes;
    seedRoll.setNotes (notes);
    captureInfo.setText (juce::String (notes.size()) + " notes captured",
                         juce::dontSendNotification);
    captureInfo.setColour (juce::Label::textColourId, EvolveColours::accent);
    hearSeedBtn.setEnabled (true);
    plantBtn.setEnabled (true);
    setStatus (juce::String (notes.size()) + " notes captured — click PLANT SEED");
}

//==============================================================================
void EvolveEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // The change message is now used as a hint that fresh capture is available.
    // The actual check happens in timerCallback via checkForAutoCapture(),
    // which is safe to call from the message thread.
    checkForAutoCapture();
}

//==============================================================================
void EvolveEditor::setParam (const juce::String& p)
{
    evolveParam = p;
    styleParamButton (autoBtn,    p == "auto",    EvolveColours::accent);
    styleParamButton (rhythmBtn,  p == "rhythm",  EvolveColours::orange);
    styleParamButton (melodyBtn,  p == "melody",  EvolveColours::accent);
    styleParamButton (harmonyBtn, p == "harmony", EvolveColours::purple);
    styleParamButton (formBtn,    p == "form",    EvolveColours::pink);
}

void EvolveEditor::styleParamButton (juce::TextButton& btn, bool active, juce::Colour col)
{
    btn.setColour (juce::TextButton::buttonColourId,
                   active ? col.withAlpha (0.2f) : EvolveColours::raised);
    btn.setColour (juce::TextButton::textColourOffId,
                   active ? col : EvolveColours::muted);
}

//==============================================================================
void EvolveEditor::plantSeed()
{
    if (seedNotes.empty()) return;
    generations.clear();
    Generation g;
    g.id       = makeUID();
    g.parentId = "";
    g.notes    = seedNotes;
    g.param    = "seed";
    g.title    = "Seed";
    g.genIndex = 0;
    generations.push_back (g);
    selectedId = g.id;
    evolveBtn.setEnabled (true);
    rebuildTree();
    setStatus ("Seed planted — click EVOLVE SELECTED");
}

//==============================================================================
void EvolveEditor::evolveSelected()
{
    if (selectedId.isEmpty() || generations.empty()) return;

    const Generation* parent = nullptr;
    for (auto& g : generations)
        if (g.id == selectedId) { parent = &g; break; }
    if (!parent) return;

    const auto key    = keyBox.getText();
    const auto genre  = genreBox.getText();
    const auto param  = evolveParam;
    const auto lineage = getLineageDesc (selectedId);
    const auto prompt  = EvolveEngine::buildPrompt (parent->notes, key, genre, param, lineage, parent->genIndex);

    // Build request JSON manually
    juce::DynamicObject::Ptr root (new juce::DynamicObject());
    root->setProperty ("model",      juce::var ("claude-opus-4-6"));
    root->setProperty ("max_tokens", juce::var (4000));
    root->setProperty ("system",     juce::var ("You are a JSON API. OUTPUT ONLY RAW JSON starting with {. No markdown."));
    juce::Array<juce::var> msgs;
    juce::DynamicObject::Ptr msg (new juce::DynamicObject());
    msg->setProperty ("role",    juce::var ("user"));
    msg->setProperty ("content", juce::var (prompt));
    msgs.add (juce::var (msg.get()));
    root->setProperty ("messages", juce::var (msgs));
    const auto requestJSON = juce::JSON::toString (juce::var (root.get()));

    evolveBtn.setEnabled (false);
    setStatus ("Generating 8-step evolution...");

    const NoteList   parentNotes    = parent->notes;
    const int        parentGenIndex = parent->genIndex;
    const juce::String parentId     = parent->id;

    processor.callAnthropic (requestJSON,
        [this, parentNotes, parentGenIndex, parentId, param]
        (juce::String response, juce::String error)
        {
            evolveBtn.setEnabled (true);

            if (error.isNotEmpty())
            {
                if (error == "NO_INTERNET" || error == "TIMEOUT")
                    setStatus ("No internet connection — check your connection");
                else if (error == "NO_API_KEY")
                    setStatus ("No API key — enter it in the settings screen");
                else
                    setStatus ("Error: " + error);
                return;
            }

            const auto cleaned = EvolveEngine::cleanJSON (response);
            auto result = EvolveEngine::parseResponse (cleaned, parentNotes);

            if (!result.ok) { setStatus ("Error: " + result.error); return; }

            Generation g;
            g.id       = makeUID();
            g.parentId = parentId;
            g.notes    = result.finalNotes;
            g.param    = (param == "auto") ? juce::String ("auto") : param;
            g.title    = result.title;
            g.genIndex = parentGenIndex + 1;
            g.steps    = result.steps;

            generations.push_back (g);
            selectedId = g.id;
            rebuildTree();
            scrollTreeToBottom();
            setStatus ("Evolved: " + result.title);
        });
}

//==============================================================================
void EvolveEditor::playNotes (const NoteList& notes)
{
    if (notes.empty()) return;
    juce::Array<juce::var> arr;
    for (const auto& n : notes)
    {
        juce::DynamicObject::Ptr obj (new juce::DynamicObject());
        obj->setProperty ("pitch", juce::var (n.pitch));
        obj->setProperty ("beat",  juce::var (n.beat));
        obj->setProperty ("dur",   juce::var (n.dur));
        arr.add (juce::var (obj.get()));
    }
    processor.scheduleNotesForPlayback (juce::JSON::toString (juce::var (arr)),
                                        bpmSlider.getValue());
}

void EvolveEditor::applyToAbleton (const NoteList& notes)
{
    playNotes (notes);
    setStatus ("Playing to Ableton — record-arm a track to capture");
}

//==============================================================================
void EvolveEditor::setStatus (const juce::String& msg)
{
    statusLabel.setText (msg, juce::dontSendNotification);
}

//==============================================================================
juce::String EvolveEditor::getLineageDesc (const juce::String& id)
{
    juce::StringArray chain;
    juce::String cur = id;
    for (int i = 0; i < 20; ++i)
    {
        const Generation* g = nullptr;
        for (auto& gen : generations)
            if (gen.id == cur) { g = &gen; break; }
        if (!g || g->parentId.isEmpty()) break;
        chain.insert (0, g->param + (g->title.isNotEmpty() ? ":" + g->title : ""));
        cur = g->parentId;
    }
    if (chain.isEmpty()) return {};
    return "Previous changes: " + chain.joinIntoString (" -> ") + ". Do NOT reverse any.";
}

//==============================================================================
void EvolveEditor::rebuildTree()
{
    genCards.clear();
    treeLabels.clear();
    treeContainer.removeAllChildren();

    int y = 8;

    if (generations.empty())
    {
        auto empty = std::make_unique<juce::Label> (juce::String{},
            "Loop a section in your DAW to capture notes.\n"
            "Then click PLANT SEED and EVOLVE SELECTED.");
        empty->setFont (juce::Font(juce::FontOptions{}.withHeight(12.0f)));
        empty->setColour (juce::Label::textColourId, EvolveColours::muted);
        empty->setJustificationType (juce::Justification::centred);
        empty->setBounds (20, 60, treeContainer.getWidth()-40, 80);
        treeContainer.addAndMakeVisible (*empty);
        treeLabels.push_back (std::move (empty));
        treeContainer.setSize (treeContainer.getWidth(),
                               std::max (200, treeViewport.getHeight()));
        return;
    }

    for (int i = 0; i < (int)generations.size(); ++i)
    {
        const auto& gen = generations[i];

        if (i > 0)
        {
            auto conn = std::make_unique<juce::Label> (juce::String{}, "-> " + gen.param.toUpperCase());
            conn->setFont (juce::Font(juce::FontOptions{}.withHeight(9.0f)));
            conn->setColour (juce::Label::textColourId, EvolveColours::muted);
            conn->setBounds (8, y, 200, 14);
            treeContainer.addAndMakeVisible (*conn);
            treeLabels.push_back (std::move (conn));
            y += 16;
        }

        auto card = std::make_unique<GenCard> (gen, gen.id == selectedId);
        const int cardH = card->getPreferredHeight();
        card->setBounds (4, y, treeContainer.getWidth() - 8, cardH);

        const juce::String genId = gen.id;

        card->onSelect = [this, genId]
        {
            selectedId = genId;
            rebuildTree();
            setStatus ("Generation selected — EVOLVE SELECTED to branch");
        };
        card->onHear = [this, genId]
        {
            for (auto& g : generations)
                if (g.id == genId) { playNotes (g.notes); break; }
        };
        card->onApply = [this, genId]
        {
            for (auto& g : generations)
                if (g.id == genId) { applyToAbleton (g.notes); break; }
        };
        card->onHearStep = [this, genId](int idx)
        {
            for (auto& g : generations)
                if (g.id == genId && idx < (int)g.steps.size())
                    { playNotes (g.steps[idx].snapshot); break; }
        };
        card->onPlantStep = [this, genId](int idx)
        {
            for (auto& g : generations)
            {
                if (g.id == genId && idx < (int)g.steps.size())
                {
                    seedNotes = g.steps[idx].snapshot;
                    seedRoll.setNotes (seedNotes);
                    hearSeedBtn.setEnabled (true);
                    plantBtn.setEnabled (true);
                    captureInfo.setText ("Step loaded — click PLANT SEED",
                                         juce::dontSendNotification);
                    plantSeed();
                    break;
                }
            }
        };

        treeContainer.addAndMakeVisible (*card);
        y += cardH + 4;
        genCards.push_back (std::move (card));
    }

    treeContainer.setSize (treeContainer.getWidth(),
                           std::max (y + 20, treeViewport.getHeight()));
    treeViewport.repaint();
}

//==============================================================================
void EvolveEditor::scrollTreeToBottom()
{
    juce::Timer::callAfterDelay (80, [this]()
    {
        treeViewport.setViewPosition (0, treeContainer.getHeight());
    });
}

//==============================================================================
juce::AudioProcessorEditor* EvolveProcessor::createEditor()
{
    return new EvolveEditor (*this);
}
