# EVOLVE — AI Melody Evolution VST3/AU Plugin

## For Users

### Mac Installation
1. Download `EVOLVE-Mac.zip` from the latest release
2. Extract the zip
3. Copy `EVOLVE.vst3` → `~/Library/Audio/Plug-Ins/VST3/`
4. Copy `EVOLVE.component` → `~/Library/Audio/Plug-Ins/Components/`
5. In Ableton: Preferences → Plugins → Rescan

**First open on Mac:** Right-click the plugin in Ableton → Open. macOS will ask if you're sure — click Open. This only happens once.

### Windows Installation
1. Download `EVOLVE-Windows.zip` from the latest release
2. Extract the zip
3. Copy `EVOLVE.vst3` → `C:\Program Files\Common Files\VST3\`
4. In Ableton: Preferences → Plugins → Rescan

### First Launch
Enter your Anthropic API key when the welcome screen appears.
Get a free key at https://console.anthropic.com

### How to Use
1. Load EVOLVE on any MIDI track
2. Set a loop region in Ableton around the MIDI you want to evolve
3. Hit play in Ableton so the loop is playing
4. Click **RECORD** in EVOLVE — it captures the notes in real time
5. Click **STOP** when the loop has played through once
6. Click **PLANT SEED**
7. Click **EVOLVE SELECTED** — AI generates 8 chained variations
8. Browse the evolution tree, click **▶ HEAR** to audition any generation
9. Click **→ APPLY** to send the evolved MIDI back to Ableton

---

## For Developers — Building from Source

### Requirements
- CMake 3.22+
- Xcode 14+ (Mac) or Visual Studio 2022 (Windows)
- Git
- Internet connection (CMake downloads JUCE automatically)

### Build
```bash
git clone https://github.com/YOUR_USERNAME/evolve-vst
cd evolve-vst
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built plugin will be in `build/EVOLVE_artefacts/Release/`.

### Automatic Cloud Build (GitHub Actions)
Push to `main` and GitHub builds Mac + Windows automatically.
Download the finished `.vst3` files from the Releases page — no compiler needed.

1. Fork or create a repo with this code
2. Push to main
3. Go to Actions tab — build runs automatically (~10 min)
4. Go to Releases — download `EVOLVE-Mac.zip` and `EVOLVE-Windows.zip`
