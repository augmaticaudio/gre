# Augmatic GRE — Source Code

This folder contains the JUCE/C++ source code for Augmatic GRE, exported from the private development repository.

## What's included

| Folder | Contents |
|--------|----------|
| `Source/` | All C++/Obj-C++ source files — plugin processor, editor, DSP engines, UI panels |
| `Source/DSP/` | Grids engine, Euclidean engine, clock, swing, velocity, accent bender, linear drumming |
| `Source/UI/` | Preset panel, MIDI mapping panel, accent bender panel, XY pad animation |
| `CMakeLists.txt` | Full CMake build configuration (JUCE plugin setup, factory presets, MIDI mappings) |
| `Resources/` | App entitlements and Apple privacy manifest |
| `scripts-mac/` | macOS/iOS build scripts (build, install, version bump, signing cleanup) |

## What's NOT included

- **JUCE framework** — the audio plugin framework itself is not bundled
- **Factory presets** — the bundled preset library is not included
- **Factory MIDI mappings** — the bundled controller mappings are not included
- **App icon and image assets** — icons and graphics are not included

## Can I fork and build this?

**Not directly.** The missing items above mean the project will not configure out of the box. This source is published for reference, learning, and code review — not as a turnkey buildable project.

## Redacted lines

Some lines have been replaced with `// REDACTED`, `# REDACTED`, or `<!-- REDACTED -->`. These are lines that contained personal identifiers (author name, bundle IDs, App Group IDs, local filesystem paths). The redaction affects 14 lines across 8 files. All plugin logic, DSP code, and UI code is unredacted and complete.

## License

GPL-3.0
