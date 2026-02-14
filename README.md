# WORK IN PROGRESS

This repository is under construction — things may not work yet.

# Augmatic GRE

Algorithmic drum pattern generator. AUv3 MIDI plugin and app for iPad, iPhone, and an app for Mac (no plugin).

**Website**: [augmaticaudio.github.io/gre](https://augmaticaudio.github.io/gre/)

## Features

- **6 Algorithmic Channels** - Bass drum, snare, hi-hat, plus accents which create complementary rhythms, not duplicates. Each channel can generate its own rhythmic voice for rich, layered drum patterns.

- **Grids Engine** - Morphable, evolving beats based on the [Mutable Instruments Grids](https://pichenettes.github.io/mutable-instruments-documentation/modules/grids/) Eurorack module. Navigate an X/Y map of interconnected drum patterns extracted from actual drum loops with smooth interpolation between patterns.
- **Euclidean Engine** - Generate evenly distributed, world‑inspired rhythm patterns using Euclidean algorithms. Configure steps, hits, and rotation independently on each channel for precise control.
- **Blend Control** - Crossfade between Blend Grids and Euclidean patterns to introduce variation while preserving the core character of your groove.
- **Linear Drumming** - Control which drums play together or play alone. Eeach sound has room to breathe. Creates clean, articulate patterns. Used in Hip-Hop, IDM, Funk, Fusion.
- **Velocity Bender** - Shape velocity dynamics for a more expressive, dynamic pulse. Create ghost notes, and rhythmic emphasis that bring patterns to life.
- **Groove Tools** - Swing, humanization, clock division, and timing shift. Fine-tune the feel of your patterns from tight and mechanical to loose and human.

## Compatibility
- iPadOS and iOS 15+
- macOS 11+ (Apple Silicon)
- Works on iPad and iPhone with AUv3-compatible host, e.g. AUM, Loopy Pro, Drambo, Logic Pro.

# License
Code: GPL3.0.

# Frameworks
- JUCE 8.0.12 - C++ framework for audio plugin development, providing the AUv3 wrapper, MIDI processing, GUI components, and cross-platform abstraction
- CMake - Build system with Xcode generator (required for AUv3 code signing)
- C++17 - Core language standard

# VIBE CODING WARNING
This project is 100% vibe‑coded. Reuse with caution.

# Contact
augmatic.gre@gmail.com