#include "PluginEditor.h"
#include "DSP/Constants.h"
#include "LinearDrummingMatrix.h"
#include "UI/AccentBenderPanel.h"
#include "StorageManager.h"
#if JUCE_MAC
#include "NativeTitleBar_mac.h"
#endif

AugmaticGREEditor::AugmaticGREEditor(AugmaticGREProcessor& p)
    : AudioProcessorEditor(p), audioProcessor(p)
{

    constexpr double aspectRatio = 2.7;
    constexpr int defaultWidth = 1200;
    constexpr int defaultHeight = static_cast<int>(defaultWidth / aspectRatio); // ~444
    constexpr int minWidth = 600;
    constexpr int minHeight = static_cast<int>(minWidth / aspectRatio);
    constexpr int maxWidth = 1920;
    constexpr int maxHeight = static_cast<int>(maxWidth / aspectRatio);

    // contentWrapper is the single direct child that holds all UI elements.
    // On iPhone AUv3, a scale transform shrinks it to fit small host containers.
    addAndMakeVisible(contentWrapper);

    setSize(defaultWidth, defaultHeight);

#if JUCE_IOS
    // iOS: allow host/OS to resize editor; letterboxing computed in resized()
    {
        const bool isStandalone = (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
        setResizable(true, false);  // Allow host to resize (both standalone and AUv3)
        if (!isStandalone)
        {
            // AUv3: explicit constrainer with broad bounds so wrapper accepts any host-offered size
            // iPhone screens can be as narrow as 375pt — accept everything
            // CRITICAL: setConstrainer() is required for AUv3 size negotiation
            // (setResizeLimits alone doesn't work with setResizable(true, false))
            iosAUv3Constrainer.setSizeLimits(200, 100, 4000, 2000);
            setConstrainer(&iosAUv3Constrainer);
        }
    }
#else
    // macOS: resize handle + aspect ratio constraint only in standalone
    {
        const bool isStandalone = (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
        setResizable(isStandalone, isStandalone);
        if (isStandalone)
        {
            setResizeLimits(minWidth, minHeight, maxWidth, maxHeight);
            getConstrainer()->setFixedAspectRatio(aspectRatio);
        }
    }
#endif
    setOpaque(true);

    // Standalone mode: grab keyboard focus so spacebar can toggle playback
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        setWantsKeyboardFocus(true);

    // Create preset panel — positioned above XY Pad in resized() (v0.4.120)
    if (auto* presetMgr = audioProcessor.getPresetManager())
    {
        presetPanel = std::make_unique<PresetPanel>(*presetMgr, audioProcessor.getParameters());
        contentWrapper.addAndMakeVisible(presetPanel.get());
    }

    // Create LED indicators - aligned with title and preset panel
    createLEDIndicators();

    // Create tabbed interface - position to the right of XY pad, below header
    tabbedComponent = std::make_unique<DrumTabbedComponent>(TabbedButtonBar::TabsAtTop);
    // Initial bounds will be set properly in resized() - use placeholder
    tabbedComponent->setBounds(10, 100, 860, 810);
    tabbedComponent->setColour(TabbedComponent::outlineColourId, Colour(0xff101010)); // Match grid background
    tabbedComponent->setColour(TabbedComponent::backgroundColourId, Colour(0xff101010));
    tabbedComponent->getTabbedButtonBar().setLookAndFeel(&tabLookAndFeel);
    contentWrapper.addAndMakeVisible(tabbedComponent.get());

    // Create tab content
    createPatternTab();
    // Create remaining tabs

    // Create VELOCITY tab with APVTS for parameter attachments and Accent Bender controller
    velocityTab = std::make_unique<VelocityTabComponent>(
        audioProcessor.getParameters(),
        audioProcessor.getAccentBenderController(),
        audioProcessor);

    // Create MIX tab with APVTS for probability knobs, mute buttons, and Priority Matrix
    mixTab = std::make_unique<MixTabComponent>(audioProcessor.getParameters(), audioProcessor);

    // Create new MIDI tab with grid structure
    newMidiTab = std::make_unique<MIDITabComponent>(audioProcessor.getParameters(), audioProcessor);

    // Pass XY pad reference to MIDI tab for animation control
    if (xyPad)
    {
        newMidiTab->setXYPadReference(xyPad.get());
        // Initialize animation state from switch default (enabled)
        if (xyPad->animation)
            xyPad->animation->setAnimationEnabled(true);
    }

    // Add tabs to component (PATTERN, LINEAR, VELOCITY, Settings)
    // Tab content background matches grid background
    tabbedComponent->addTab("PATTERN", Colour(0xff101010), patternTab.get(), false);
    tabbedComponent->addTab("LINEAR", Colour(0xff101010), mixTab.get(), false);
    tabbedComponent->addTab("VELOCITY", Colour(0xff101010), velocityTab.get(), false);
    tabbedComponent->addTab("Settings", Colour(0xff101010), newMidiTab.get(), false);  // Displays hamburger icon

    // Update random drum words when switching tabs
    tabbedComponent->onTabChanged = [this](int /*newTabIndex*/, const String& /*tabName*/) {
        if (patternTab) patternTab->updateRandomDrumWord();
        if (velocityTab) velocityTab->updateRandomDrumWord();
        if (mixTab) mixTab->updateRandomDrumWord();
        if (newMidiTab) newMidiTab->updateRandomDrumWord();
    };

    // Apply tab styling AFTER tabs are added (buttons must exist first)
    updateTabBarStyle();

    // Wire preset load callback to refresh BPM display in Settings tab
    if (auto* presetMgr = audioProcessor.getPresetManager())
    {
        presetMgr->onPresetLoaded = [this]()
        {
            if (newMidiTab)
                newMidiTab->refreshBPM();
        };
    }

    // Register LED callback with processor for LED feedback on tabs
    // channelIndex is passed directly to avoid MIDI note matching issues
    audioProcessor.ledNotifyCallback = [this](int midiNote, uint8_t velocity, int channelIndex, int stepInPattern, double ppqPosition, double bpm) {
        onMIDINoteGenerated(midiNote, velocity, channelIndex, stepInPattern, ppqPosition, bpm);
    };

    // Register transport start callback for animation line randomization
    audioProcessor.onTransportStart = [this]() {
        if (xyPad && xyPad->animation)
            xyPad->animation->randomizeLineAssignments();
    };

    // Create About overlay (initially hidden)
    aboutOverlay = std::make_unique<AboutOverlay>();
    aboutOverlay->setBounds(getLocalBounds());
    addChildComponent(aboutOverlay.get());  // Use addChildComponent instead of addAndMakeVisible to keep it hidden
    aboutOverlay->setVisible(false);

#if JUCE_IOS
    // AUv3: show "Enable Files App Access" button if no bookmark is set
    {
        const bool isAUv3 = (audioProcessor.wrapperType != juce::AudioProcessor::wrapperType_Standalone);
        auto* bookmarkMgr = StorageManager::getInstance().getDocumentsBookmarkManager();

        if (isAUv3 && bookmarkMgr && !bookmarkMgr->hasValidBookmark())
        {
            bookmarkSetupButton = std::make_unique<juce::TextButton>("Enable Files App Access");
            bookmarkSetupButton->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff3a7bd5));
            bookmarkSetupButton->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
            bookmarkSetupButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            bookmarkSetupButton->onClick = [this, bookmarkMgr]()
            {
                auto safeThis = juce::Component::SafePointer<AugmaticGREEditor>(this);
                bookmarkMgr->requestBookmark(this, [safeThis](bool success)
                {
                    if (safeThis == nullptr) return;
                    if (success)
                    {
                        // Rescan presets and mappings from the new Documents location
                        if (auto* pm = safeThis->audioProcessor.getPresetManager())
                            pm->rescanUserPresets();
                        if (auto* mm = safeThis->audioProcessor.getMidiMappingManager())
                            mm->rescanUserMappings();

                        // Hide the button
                        if (safeThis->bookmarkSetupButton)
                        {
                            safeThis->bookmarkSetupButton->setVisible(false);
                            safeThis->bookmarkSetupButton.reset();
                        }
                    }
                });
            };
            contentWrapper.addAndMakeVisible(bookmarkSetupButton.get());
        }
    }
#endif

    // Ensure all components are properly positioned after creation
    // (setSize() triggers resized() before XY pad exists, so we need a second call)
    resized();

    // Standalone: grab keyboard focus so spacebar toggles playback immediately
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
        grabKeyboardFocus();

#if JUCE_MAC
    // macOS standalone: switch to native title bar with traffic light controls
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        juce::Timer::callAfterDelay(50, [safeComp = juce::Component::SafePointer<AugmaticGREEditor>(this)]()
        {
            if (safeComp == nullptr) return;
            if (auto* topWindow = safeComp->findParentComponentOfClass<juce::DocumentWindow>())
            {
                topWindow->setUsingNativeTitleBar(true);
                applyNativeTitleBarStyle(topWindow);
            }
        });
    }
#endif
}

AugmaticGREEditor::~AugmaticGREEditor()
{
    // Clear TabLookAndFeel before destroying tabbedComponent
    if (tabbedComponent)
    {
        auto& tabBar = tabbedComponent->getTabbedButtonBar();
        for (int i = 0; i < tabBar.getNumTabs(); ++i)
        {
            if (auto* button = tabBar.getTabButton(i))
                button->setLookAndFeel(nullptr);
        }
        tabBar.setLookAndFeel(nullptr);
    }

    // Remove Euclidean Steps slider listeners
    for (int ch = 0; ch < 6; ++ch) {
        if (euclideanStepsSliders[ch]) {
            euclideanStepsSliders[ch]->removeListener(this);
        }
    }

    // Clear LED callback from processor
    audioProcessor.ledNotifyCallback = nullptr;
    audioProcessor.onTransportStart = nullptr;
}

void AugmaticGREEditor::createPatternTab()
{
    // Dynamic 12x6 grid with labels in C1, LEDs in C12, and density knobs
    // v0.4.087: Added audioProcessor reference for clickable LEDs
    patternTab = std::make_unique<PatternTabComponent>(audioProcessor.getParameters(), audioProcessor);
}

void AugmaticGREEditor::createAdvancedTab()
{
    // PRODUCTION: Standard component with no development colors
    advancedTab = std::make_unique<Component>();

    // All 6 channels: 3 main + 3 accent
    const char* channelLabels[] = {"BD", "SN", "HH", "BD Acc", "SN Acc", "HH Acc"};

    int labelWidth = 120;
    int checkboxX = 210;  // Fixed position for all checkboxes
    int controlWidth = 240;  // Increased from 200 to 240px for easier mouse control
    int rowHeight = 20;  // Reduced from 40 to 20 (50% reduction for compact layout)

    // Left column: Main channels (BD, SN, HH) - channels 0,1,2
    // Right column: Accent channels (BD_Acc, SN_Acc, HH_Acc) - channels 3,4,5

    int leftColumnX = 20;
    int rightColumnX = 450;

    // Align all velocity controls to start at same vertical position as Grids/Euclidean tabs
    const int velocityStartY = 15;  // Starting Y position - moved up by 15px

    for (int ch = 0; ch < 6; ++ch) {
        // Determine position based on channel type
        bool isAccentChannel = (ch >= 3);
        int columnX = isAccentChannel ? rightColumnX : leftColumnX;
        int channelIndex = isAccentChannel ? (ch - 3) : ch; // 0,1,2 for both main and accent

        // Compact layout: 4 sliders × 20px = 80px + 25px header = 105px per section
        int sectionY = velocityStartY + channelIndex * 105;
        int yPos = sectionY + 25; // Space for section header

        // Channel section header (instrument name) - narrower to make room for aligned checkbox
        instrumentLabels[ch] = std::make_unique<Label>("instrumentLabel" + String(ch), String(channelLabels[ch]));
        instrumentLabels[ch]->setBounds(columnX, sectionY, 50, 25);
        instrumentLabels[ch]->setJustificationType(Justification::centredLeft);
        instrumentLabels[ch]->setColour(Label::textColourId, Colour(0xff9b74f6));
        instrumentLabels[ch]->setFont(Font(16.0f, Font::bold));
        advancedTab->addAndMakeVisible(instrumentLabels[ch].get());

        // Adjust control positioning based on column
        int controlXPos = columnX + labelWidth + 10;

        // Level (velocity value)
        velocityValueLabels[ch] = std::make_unique<Label>("velocityValueLabel" + String(ch), "Value");
        velocityValueLabels[ch]->setBounds(columnX, yPos, labelWidth, 25);
        velocityValueLabels[ch]->setJustificationType(Justification::centredLeft);
        advancedTab->addAndMakeVisible(velocityValueLabels[ch].get());

        velocityValueSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        velocityValueSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        velocityValueSliders[ch]->setRange(1, 127, 1);
        velocityValueSliders[ch]->setValue(120);
        velocityValueSliders[ch]->setBounds(controlXPos, yPos, controlWidth, 25);
        velocityValueSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        velocityValueSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        velocityValueSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        advancedTab->addAndMakeVisible(velocityValueSliders[ch].get());
        yPos += rowHeight;

        // Randomize
        velocityRandomizeLabels[ch] = std::make_unique<Label>("velocityRandomizeLabel" + String(ch), "Randomize");
        velocityRandomizeLabels[ch]->setBounds(columnX, yPos, labelWidth, 25);
        velocityRandomizeLabels[ch]->setJustificationType(Justification::centredLeft);
        advancedTab->addAndMakeVisible(velocityRandomizeLabels[ch].get());

        velocityRandomizeSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        velocityRandomizeSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        velocityRandomizeSliders[ch]->setRange(0.0, 100.0, 1.0);
        velocityRandomizeSliders[ch]->setValue(0.0);
        velocityRandomizeSliders[ch]->setBounds(controlXPos, yPos, controlWidth, 25);
        velocityRandomizeSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        velocityRandomizeSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        velocityRandomizeSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        advancedTab->addAndMakeVisible(velocityRandomizeSliders[ch].get());
        yPos += rowHeight;

        // Create parameter attachments (Clock Ratio removed - moved to Shift tab)
        String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;

        velocityValueAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + VEL_VALUE_SUFFIX, *velocityValueSliders[ch]);
        velocityRandomizeAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + VEL_RANDOMIZE_SUFFIX, *velocityRandomizeSliders[ch]);
    }

    // Add Accent Bender Panel at the bottom (replaces separate Accent tab)
    if (audioProcessor.getAccentBenderController()) {
        accentBenderPanel = new AccentBenderPanel(
            audioProcessor.getParameters(),
            *audioProcessor.getAccentBenderController(),
            audioProcessor.getMidiMappingManager()
        );

        // Position below the 6 velocity control sections
        // With velocityStartY=30, 105px section spacing, and rowHeight=20:
        // HH row: section starts at 30 + (2 * 105) = 240
        // HH row ends at 240 + 25 (header) + (3 sliders * 20) = 325
        // Add 30px clearance as requested
        accentBenderPanel->setBounds(0, 355, 860, 430);
        advancedTab->addAndMakeVisible(accentBenderPanel);  // advancedTab takes ownership
    }
}

// OldMIDI tab deleted — all controls moved to MIDI tab (v0.4.133)
// Shift tab deleted — all controls moved to Pattern tab

void AugmaticGREEditor::paint(Graphics& g)
{
    g.fillAll(Colour(0xff101010));
}

void AugmaticGREEditor::mouseDown(const MouseEvent& /* event */)
{
    // Title removed (v0.4.121) — about overlay trigger removed
}

bool AugmaticGREEditor::keyPressed(const KeyPress& key)
{
    // Spacebar toggles playback in standalone mode
    if (key.getKeyCode() == KeyPress::spaceKey
        && audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        if (auto* clock = audioProcessor.getInternalClock())
        {
            bool shouldPlay = !clock->isPlaying();
            clock->setPlaying(shouldPlay);

            if (transportButton)
                transportButton->setPlaying(shouldPlay);

            return true;
        }
    }
    return false;
}

void AugmaticGREEditor::createLEDIndicators()
{
    // XY Pad (Map X / Map Y) - positioned on the left side of tabs
    xyPad = std::make_unique<XYPadComponent>(
        *audioProcessor.getParameters().getParameter("grids_x"),
        *audioProcessor.getParameters().getParameter("grids_y"),
        audioProcessor.getParameters());
    contentWrapper.addAndMakeVisible(xyPad.get());

    // Chaos slider - positioned directly below XY pad
    chaosSliderPad = std::make_unique<ChaosSliderComponent>(
        *audioProcessor.getParameters().getParameter("chaos"));
    contentWrapper.addAndMakeVisible(chaosSliderPad.get());

    // Transport button - standalone mode only (positioned next to Chaos slider)
    if (audioProcessor.wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        transportButton = std::make_unique<TransportButton>();
        transportButton->onToggle = [this](bool shouldPlay) {
            if (auto* clock = audioProcessor.getInternalClock())
            {
                clock->setPlaying(shouldPlay);
            }
            // Force immediate visual update
            transportButton->setPlaying(shouldPlay);
        };
        transportButton->getPlayingState = [this]() -> bool {
            if (auto* clock = audioProcessor.getInternalClock())
                return clock->isPlaying();
            return false;
        };
        transportButton->isAudioActive = [this]() -> bool {
            return audioProcessor.isAudioProcessing();
        };
        transportButton->startSyncTimer(100);  // Sync every 100ms
        contentWrapper.addAndMakeVisible(transportButton.get());
    }
}

void AugmaticGREEditor::onMIDINoteGenerated(int /* midiNote */, uint8_t velocity, int channelIndex, int /* stepInPattern */, double ppqPosition, double bpm)
{
    // Trigger LED feedback using channel index directly
    // Channel index is passed from processBlock, avoiding MIDI note parameter matching
    // which could fail if parameter values are out of sync with channel struct values
    if (channelIndex >= 0 && channelIndex < 6)
    {
        if (patternTab) patternTab->triggerLEDForChannel(channelIndex);
        if (velocityTab) velocityTab->triggerLEDForChannel(channelIndex);
        if (mixTab) mixTab->triggerLEDForChannel(channelIndex);
        if (newMidiTab) newMidiTab->triggerLEDForChannel(channelIndex);
        if (xyPad) xyPad->triggerNoteAnimation(channelIndex, velocity, ppqPosition, bpm);
    }
}

void AugmaticGREEditor::sliderValueChanged(Slider* slider)
{
    // Handle Euclidean Steps slider changes — clamp Pulses and Shift (v0.4.124)
    //
    // CRITICAL: setValue() MUST be called BEFORE setRange().
    // Reason: setRange() clamps the slider value silently (dontSendNotification),
    // so the SliderAttachment is never informed and the APVTS parameter keeps the
    // old unclamped value. By calling setValue() first while the slider still has
    // its old (wider) range, the value change is genuine and fires the attachment,
    // which propagates the clamped value to the APVTS parameter. setRange() is then
    // called afterwards to constrain the visual range for future user interaction.
    for (int ch = 0; ch < 6; ++ch) {
        if (slider == euclideanStepsSliders[ch].get()) {
            int steps = static_cast<int>(slider->getValue());

            // --- PULSES: clamp value, then constrain range ---
            double currentPulses = euclideanPulsesSliders[ch]->getValue();
            if (currentPulses > static_cast<double>(steps))
                euclideanPulsesSliders[ch]->setValue(static_cast<double>(steps), juce::sendNotificationSync);
            euclideanPulsesSliders[ch]->setRange(0, steps, 1);

            // --- START ON: clamp value, then constrain range ---
            double currentStartOn = euclideanStartOnSliders[ch]->getValue();
            double clampedStartOn = juce::jlimit(1.0, static_cast<double>(steps), currentStartOn);
            if (clampedStartOn != currentStartOn)
                euclideanStartOnSliders[ch]->setValue(clampedStartOn, juce::sendNotificationSync);
            euclideanStartOnSliders[ch]->setRange(1, steps, 1);

            break;
        }
    }
}

void AugmaticGREEditor::resized()
{
    // =========================================================================
    // Position and optionally scale contentWrapper to fit the host container.
    // On iPhone AUv3, a GPU-accelerated AffineTransform shrinks the layout
    // to fit small host containers while keeping all controls interactive.
    // =========================================================================

#if JUCE_IOS
    {
        const int hostW = getWidth();
        const int hostH = getHeight();

        constexpr double kAspectRatio = 2.7;

        // Below this fitted width, the layout's minimum guards (40px cells, 150px XY pad)
        // cause overflow. At 900px+ the layout computes naturally without clipping.
        constexpr int kNativeMinW = 900;

        // When scaling is needed, render at the proven default size where the layout
        // is well-proportioned (no minimum guards hit). The GPU scales it to fit.
        constexpr int kDesignW = 1200;
        constexpr int kDesignH = static_cast<int>(kDesignW / kAspectRatio); // 444

        // Compute the largest 2.7:1 rectangle that fits in the host container
        int fitW = hostW;
        int fitH = static_cast<int>(fitW / kAspectRatio);
        if (fitH > hostH)
        {
            fitH = hostH;
            fitW = static_cast<int>(fitH * kAspectRatio);
        }

        if (fitW >= kNativeMinW)
        {
            // Large enough for native layout (iPad, large screens):
            // Render at fitted size, centered with letterbox bars
            contentWrapper.setTransform(juce::AffineTransform());
            contentWrapper.setBounds((hostW - fitW) / 2, (hostH - fitH) / 2, fitW, fitH);
            layoutAllControls(contentWrapper.getLocalBounds());
        }
        else
        {
            // Too small for native layout (iPhone portrait & landscape):
            // Render at 1200×444 design size, GPU-scale uniformly to fit
            const float scaleX = static_cast<float>(hostW) / static_cast<float>(kDesignW);
            const float scaleY = static_cast<float>(hostH) / static_cast<float>(kDesignH);
            const float scale = std::min(scaleX, scaleY);

            const float visualW = kDesignW * scale;
            const float visualH = kDesignH * scale;
            const float offsetX = (hostW - visualW) / 2.0f;
            const float offsetY = (hostH - visualH) / 2.0f;

            // Scale then translate to center within host container
            contentWrapper.setTransform(
                juce::AffineTransform::scale(scale)
                    .translated(offsetX, offsetY)
            );
            contentWrapper.setBounds(0, 0, kDesignW, kDesignH);
            layoutAllControls(contentWrapper.getLocalBounds());
        }
    }
#else
    // macOS: contentWrapper fills entire editor, no transform
    contentWrapper.setTransform(juce::AffineTransform());
    contentWrapper.setBounds(getLocalBounds());
    layoutAllControls(contentWrapper.getLocalBounds());
#endif

    // About overlay covers full editor (including letterbox bars on iOS)
    if (aboutOverlay)
        aboutOverlay->setBounds(getLocalBounds());
}

void AugmaticGREEditor::layoutAllControls(juce::Rectangle<int> contentBounds)
{
    int cX = contentBounds.getX();
    int cY = contentBounds.getY();
    int cW = contentBounds.getWidth();
    int cH = contentBounds.getHeight();

    // Title removed (v0.4.121) — elements moved to top with 5px edge distance
    const int headerHeight = 5;    // 5px from top edge to PM/tab headers
    const int margin = 10;
    const int chaosGap = 22;  // Gap between XY pad and chaos slider (+2px down)
    const int chaosBottomPadding = 16; // Extra padding below chaos slider (-4px reduction)
    const int pmTopPadding = 5;    // Extra 5px padding above preset menu
    const int pmHeightReduction = 16; // Reduce PM height by 16px total (v0.4.140: +2px taller on bottom)

    int availableHeight = cH - headerHeight - margin;
    int availableWidth = cW - margin * 2;

    // Use fixed chaos slider height for layout calculation
    const int chaosSliderLayoutHeight = 30;

    // Temporarily position tabs to determine tab bar depth
    int maxPadWidth = availableWidth / 3;
    if (tabbedComponent)
    {
        int prelimTabX = cX + margin + maxPadWidth + margin;
        int prelimTabWidth = juce::jmax(100, (cX + cW) - prelimTabX - margin);
        tabbedComponent->setBounds(prelimTabX, cY + headerHeight, prelimTabWidth, availableHeight);
        updateTabBarStyle();
    }

    // XY pad starts below the tab highlight line (aligned with tab content area)
    int tabBarDepth = tabbedComponent ? tabbedComponent->getTabBarDepth() : 50;
    int padY = cY + headerHeight + tabBarDepth + 4;  // +4px down to reduce pad size
    int padAvailableHeight = (cY + cH) - padY - margin;

    // Calculate XY pad square size
    int maxPadSide = padAvailableHeight - chaosSliderLayoutHeight - chaosGap - chaosBottomPadding;
    int padSide = juce::jmin(maxPadSide, maxPadWidth) - 4;  // -4px size reduction
    padSide = juce::jmax(padSide, 150);  // Minimum 150px

    // Compute dot size using the EXACT same formula as tab LEDs
    // This ensures the XY pad dot always matches LED/LINEAR DRUMMING dot sizes
    int finalTabWidth = juce::jmax(100, cW - (margin + padSide + margin) - margin);
    int tabContentHeight = availableHeight - tabBarDepth;
    int cellWidth = std::max(40, finalTabWidth / 12);
    float totalRowUnits = 0.333f + 6.0f;
    int cellHeight = std::max(40, static_cast<int>(tabContentHeight / totalRowUnits));

    int ledSize;
    if (finalTabWidth <= 900)
    {
        ledSize = 25;
    }
    else
    {
        int knobMaxSize = std::min(cellWidth, cellHeight) - 10;
        int maxSize = static_cast<int>(knobMaxSize * 0.6f);
        float progress = std::min(1.0f, static_cast<float>(finalTabWidth - 900) / 400.0f);
        ledSize = 25 + static_cast<int>((maxSize - 25) * progress);
    }
    float dotRadius = static_cast<float>(ledSize) / 2.0f;

    // Chaos slider height (accommodates dot)
    int chaosSliderHeight = juce::jmax(30, ledSize + 4);

    int padX = cX + margin;

    // Position XY pad (square)
    if (xyPad)
    {
        xyPad->setBounds(padX, padY, padSide, padSide);
        xyPad->setDotRadius(dotRadius);
    }

    // Position chaos slider directly below XY pad
    // In standalone mode, shorten to make room for transport button on the right
    if (chaosSliderPad)
    {
        int chaosSliderY = padY + padSide + chaosGap;
        int chaosSliderWidth = padSide;

        if (transportButton)
        {
            // Transport button is square, sized to match MUT/SOLO buttons in grid
            // MUT buttons use: (std::min(cellWidth, cellHeight) - 10) * 0.9
            int knobMaxSize = std::min(cellWidth, cellHeight) - 10;
            int transportButtonSize = static_cast<int>(knobMaxSize * 0.9f);
            chaosSliderWidth = padSide - transportButtonSize - margin;

            // Center button vertically with the chaos slider line
            int transportY = chaosSliderY + chaosSliderHeight / 2 - transportButtonSize / 2;

            transportButton->setBounds(padX + padSide - transportButtonSize, transportY,
                                       transportButtonSize, transportButtonSize);
        }

        chaosSliderPad->setBounds(padX, chaosSliderY, chaosSliderWidth, chaosSliderHeight);
        chaosSliderPad->setDotRadius(dotRadius);
    }

    // Final tab position to the right of XY pad
    int tabX = padX + padSide + margin;
    int tabWidth = juce::jmax(100, (cX + cW) - tabX - margin);

    // Position preset panel above XY Pad, same width as XY Pad (v0.4.120)
    // Height reduced by 10px, extra 5px padding above (v0.4.121)
    int presetMenuH = tabBarDepth - pmHeightReduction;

    // Compute R1 header font size (same formula as tab grid headers)
    int r1HeaderHeight = static_cast<int>(cellHeight * 0.333f);
    float r1HeaderFontSize = std::max(11.0f, static_cast<float>(r1HeaderHeight) * 0.55f);

    // CRITICAL: Pass preset menu height to MIDI tab BEFORE setting tab bounds
    // This ensures MIDI tab's resized() uses the correct height
    if (newMidiTab)
    {
        newMidiTab->setPresetMenuHeight(presetMenuH);
    }

    if (tabbedComponent)
    {
        tabbedComponent->setBounds(tabX, cY + headerHeight, tabWidth, availableHeight);
        updateTabBarStyle();
    }

    if (presetPanel)
    {
        presetPanel->setBounds(padX, cY + headerHeight + pmTopPadding, padSide, presetMenuH);
        presetPanel->setScaledFontSize(r1HeaderFontSize);
    }

#if JUCE_IOS
    // Position "Enable Files App Access" button (overlay banner at top of content area)
    if (bookmarkSetupButton && bookmarkSetupButton->isVisible())
    {
        int btnW = juce::jmin(250, cW / 2);
        int btnH = 30;
        bookmarkSetupButton->setBounds(cX + (cW - btnW) / 2, cY + 2, btnW, btnH);
    }
#endif
}

void AugmaticGREEditor::updateTabBarStyle()
{
    if (!tabbedComponent) return;

    auto& tabBar = tabbedComponent->getTabbedButtonBar();
    int tabBarWidth = tabbedComponent->getWidth();
    int numTabs = tabBar.getNumTabs();

    if (numTabs <= 0 || tabBarWidth <= 0) return;

    // Calculate font size to match instrument names (based on content area height)
    // Content area is tabbedComponent height minus tab bar
    int contentHeight = tabbedComponent->getHeight() - tabBar.getHeight();
    float totalRowUnits = 0.333f + 6.0f;  // Header ratio + 6 rows
    int cellHeight = std::max(40, static_cast<int>(contentHeight / totalRowUnits));
    float fontSize = static_cast<float>(cellHeight) * 0.38f;

    // Calculate LED column width using actual content area width with discrete scaling
    // This ensures Settings tab aligns perfectly with LED column below
    // Use same MIN_CELL_SIZE (40) logic as tab content for synchronized discrete scaling
    int ledColumnWidth = tabBarWidth / 12;  // Fallback
    if (auto* currentContent = tabbedComponent->getCurrentContentComponent())
    {
        int contentWidth = currentContent->getWidth();
        // Match the discrete scaling logic used in tab content: std::max(MIN_CELL_SIZE, width / NUM_COLS)
        int cellWidth = std::max(40, contentWidth / 12);  // MIN_CELL_SIZE = 40, NUM_COLS = 12
        ledColumnWidth = cellWidth;  // LED column is one cell wide
    }

    // Set font and dimensions in LookAndFeel BEFORE resizing
    tabLookAndFeel.setFont(Font(fontSize, Font::plain));
    tabLookAndFeel.setTabBarDimensions(tabBarWidth, numTabs, ledColumnWidth);

    // Tab bar height = font size + 20px padding (10px top + 10px bottom) + 3px indicator + 10px gap below
    int tabBarHeight = static_cast<int>(fontSize) + 33;
    tabbedComponent->setTabBarDepth(tabBarHeight);

    // Set tab bar text colors
    tabBar.setColour(TabbedButtonBar::tabTextColourId, Colour(0xff888888));
    tabBar.setColour(TabbedButtonBar::frontTextColourId, Colours::white);

    // Apply LookAndFeel to each tab button individually
    for (int i = 0; i < numTabs; ++i)
    {
        if (auto* button = tabBar.getTabButton(i))
        {
            button->setLookAndFeel(&tabLookAndFeel);
            button->setColour(TabbedButtonBar::tabTextColourId, Colour(0xff888888));
            button->setColour(TabbedButtonBar::frontTextColourId, Colours::white);
            button->setVisible(true);
            button->setOpaque(true);
            button->repaint();
        }
    }

    // Force tab bar to recalculate button sizes and repaint
    tabBar.resized();
    tabBar.repaint();
    tabbedComponent->repaint();
}

// createLinearDrummingTab removed - OldMix tab deleted (v0.4.085)

void AugmaticGREEditor::createEuclideanTab()
{
    // PRODUCTION: Standard component with no development colors
    euclideanTab = std::make_unique<Component>();

    // Two-column layout matching Pattern tab
    int leftColumnX = 20;
    int rightColumnX = 450;
    int labelWidth = 120;
    int controlWidth = 240;  // Increased from 200 to 240px for easier mouse control
    int controlX = leftColumnX + labelWidth + 10;
    int rightControlX = rightColumnX + labelWidth + 10;
    int rowHeight = 20;  // Reduced from 25 to 20 for ultra-compact layout

    // Channel names for display
    const std::array<String, 6> channelNames = {"BD", "SN", "HH", "BD Acc", "SN Acc", "HH Acc"};

    // ===== GLOBAL PATTERN CONTROLS (moved from Grids tab) =====
    int yPos = 15;  // Reduced from 30 to move content up by 15px

    // Map X control (left column)
    gridsXLabel = std::make_unique<Label>("gridsXLabel", "Map X");
    gridsXLabel->setBounds(leftColumnX, yPos, labelWidth, 30);
    gridsXLabel->setJustificationType(Justification::centredLeft);
    euclideanTab->addAndMakeVisible(gridsXLabel.get());

    gridsXSlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
    gridsXSlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
    gridsXSlider->setRange(0, 255, 1);
    gridsXSlider->setValue(128);
    gridsXSlider->setBounds(controlX, yPos, controlWidth, 30);
    gridsXSlider->setColour(Slider::backgroundColourId, Colour(0xff555555));
    gridsXSlider->setColour(Slider::trackColourId, Colour(0xff9b74f6));
    gridsXSlider->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
    euclideanTab->addAndMakeVisible(gridsXSlider.get());

    // Chaos All control (right column, same Y as Map X)
    chaosLabel = std::make_unique<Label>("chaosLabel", "Chaos All");
    chaosLabel->setBounds(rightColumnX, yPos, labelWidth, 30);
    chaosLabel->setJustificationType(Justification::centredLeft);
    euclideanTab->addAndMakeVisible(chaosLabel.get());

    chaosSlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
    chaosSlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
    chaosSlider->setRange(0, 127, 1);
    chaosSlider->setValue(0);
    chaosSlider->setBounds(rightControlX, yPos, controlWidth, 30);
    chaosSlider->setColour(Slider::backgroundColourId, Colour(0xff555555));
    chaosSlider->setColour(Slider::trackColourId, Colour(0xff9b74f6));
    chaosSlider->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
    euclideanTab->addAndMakeVisible(chaosSlider.get());
    yPos += rowHeight;

    // Map Y control (left column)
    gridsYLabel = std::make_unique<Label>("gridsYLabel", "Map Y");
    gridsYLabel->setBounds(leftColumnX, yPos, labelWidth, 30);
    gridsYLabel->setJustificationType(Justification::centredLeft);
    euclideanTab->addAndMakeVisible(gridsYLabel.get());

    gridsYSlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
    gridsYSlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
    gridsYSlider->setRange(0, 255, 1);
    gridsYSlider->setValue(128);
    gridsYSlider->setBounds(controlX, yPos, controlWidth, 30);
    gridsYSlider->setColour(Slider::backgroundColourId, Colour(0xff555555));
    gridsYSlider->setColour(Slider::trackColourId, Colour(0xff9b74f6));
    gridsYSlider->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
    euclideanTab->addAndMakeVisible(gridsYSlider.get());
    yPos += rowHeight + 15;  // Extra spacing before channel controls

    // ===== PER-CHANNEL CONTROLS =====
    // Capture starting Y position for BD section (for aligning BD Acc in right column)
    int bdStartY = yPos;

    // Process main channels (BD, SN, HH) in left column
    for (int ch = 0; ch < 3; ++ch) {
        // Channel header with enable toggle (instrument name where label was)
        euclideanChannelHeaderLabels[ch] = std::make_unique<Label>("euclideanHeaderLabel" + String(ch), channelNames[ch]);
        euclideanChannelHeaderLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        euclideanChannelHeaderLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanChannelHeaderLabels[ch]->setColour(Label::textColourId, Colour(0xff9b74f6));
        euclideanChannelHeaderLabels[ch]->setFont(Font(16.0f, Font::bold));
        euclideanTab->addAndMakeVisible(euclideanChannelHeaderLabels[ch].get());
        yPos += 10;  // Move buttons down by 10px

        // "G" button (100% Grids)
        int buttonWidth = 30;
        int buttonHeight = 20;  // Buttons remain 20px height
        int buttonSpacing = 5;
        engineProbGridsButtons[ch] = std::make_unique<TextButton>("G");
        engineProbGridsButtons[ch]->setBounds(controlX + 10, yPos, buttonWidth, buttonHeight);  // Moved right by 10px
        engineProbGridsButtons[ch]->onClick = [this, ch]() {
            if (engineProbabilitySliders[ch]) {
                engineProbabilitySliders[ch]->setValue(0.0);  // 100% Grids
            }
        };
        euclideanTab->addAndMakeVisible(engineProbGridsButtons[ch].get());

        // Engine Probability slider (no text box)
        int sliderX = controlX + 10 + buttonWidth + buttonSpacing;  // Moved right by 10px
        int sliderWidth = controlWidth - (2 * buttonWidth) - (2 * buttonSpacing) - 10;  // Reduced by 10px
        engineProbabilitySliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::NoTextBox);
        engineProbabilitySliders[ch]->setRange(0.0, 1.0, 0.01);
        engineProbabilitySliders[ch]->setValue(0.0);
        engineProbabilitySliders[ch]->setBounds(sliderX, yPos - 5, sliderWidth, 30);  // Moved up by 5px
        engineProbabilitySliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        engineProbabilitySliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        engineProbabilitySliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(engineProbabilitySliders[ch].get());

        // "E" button (100% Euclidean)
        int eButtonX = sliderX + sliderWidth + buttonSpacing;
        engineProbEuclidButtons[ch] = std::make_unique<TextButton>("E");
        engineProbEuclidButtons[ch]->setBounds(eButtonX, yPos, buttonWidth, buttonHeight);
        engineProbEuclidButtons[ch]->onClick = [this, ch]() {
            if (engineProbabilitySliders[ch]) {
                engineProbabilitySliders[ch]->setValue(1.0);  // 100% Euclidean
            }
        };
        euclideanTab->addAndMakeVisible(engineProbEuclidButtons[ch].get());
        yPos += rowHeight;  // Normal spacing after [T]/[E] buttons

        // G Density slider (moved from Grids tab)
        Label* densityLabel = nullptr;
        if (ch == 0) {
            bdDensityLabel = std::make_unique<Label>("bdDensityLabel", "G Density");
            densityLabel = bdDensityLabel.get();
        } else if (ch == 1) {
            sdDensityLabel = std::make_unique<Label>("sdDensityLabel", "G Density");
            densityLabel = sdDensityLabel.get();
        } else if (ch == 2) {
            hhDensityLabel = std::make_unique<Label>("hhDensityLabel", "G Density");
            densityLabel = hhDensityLabel.get();
        }
        if (densityLabel) {
            densityLabel->setBounds(leftColumnX, yPos, labelWidth, 30);
            densityLabel->setJustificationType(Justification::centredLeft);
            euclideanTab->addAndMakeVisible(densityLabel);
        }

        Slider* densitySlider = nullptr;
        if (ch == 0) {
            bdDensitySlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            bdDensitySlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
            bdDensitySlider->setRange(0, 255, 1);
            bdDensitySlider->setValue(128);
            densitySlider = bdDensitySlider.get();
        } else if (ch == 1) {
            sdDensitySlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            sdDensitySlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
            sdDensitySlider->setRange(0, 255, 1);
            sdDensitySlider->setValue(128);
            densitySlider = sdDensitySlider.get();
        } else if (ch == 2) {
            hhDensitySlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            hhDensitySlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
            hhDensitySlider->setRange(0, 255, 1);
            hhDensitySlider->setValue(128);
            densitySlider = hhDensitySlider.get();
        }
        if (densitySlider) {
            densitySlider->setBounds(controlX, yPos, controlWidth, 30);
            densitySlider->setColour(Slider::backgroundColourId, Colour(0xff555555));
            densitySlider->setColour(Slider::trackColourId, Colour(0xff9b74f6));
            densitySlider->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
            euclideanTab->addAndMakeVisible(densitySlider);
        }
        yPos += rowHeight;

        // G Chaos slider (moved from Grids tab)
        chaosChannelLabels[ch] = std::make_unique<Label>("chaosChannelLabel" + String(ch), "G Chaos");
        chaosChannelLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        chaosChannelLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(chaosChannelLabels[ch].get());

        chaosChannelSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        chaosChannelSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        chaosChannelSliders[ch]->setRange(0, 127, 1);
        chaosChannelSliders[ch]->setValue(0);
        chaosChannelSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);
        chaosChannelSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        chaosChannelSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        chaosChannelSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(chaosChannelSliders[ch].get());
        yPos += rowHeight;  // Same spacing as other sliders

        // E Steps slider
        euclideanStepsLabels[ch] = std::make_unique<Label>("euclideanStepsLabel" + String(ch), "E Steps");
        euclideanStepsLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        euclideanStepsLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(euclideanStepsLabels[ch].get());

        euclideanStepsSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        euclideanStepsSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        euclideanStepsSliders[ch]->setRange(2, 32, 1);
        euclideanStepsSliders[ch]->setValue(16);
        euclideanStepsSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);
        euclideanStepsSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        euclideanStepsSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        euclideanStepsSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(euclideanStepsSliders[ch].get());
        yPos += rowHeight;

        // Pulses slider
        euclideanPulsesLabels[ch] = std::make_unique<Label>("euclideanPulsesLabel" + String(ch), "E Pulses");
        euclideanPulsesLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        euclideanPulsesLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(euclideanPulsesLabels[ch].get());

        euclideanPulsesSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        euclideanPulsesSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        euclideanPulsesSliders[ch]->setRange(0, 32, 1);
        euclideanPulsesSliders[ch]->setValue(4);
        euclideanPulsesSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);
        euclideanPulsesSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        euclideanPulsesSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        euclideanPulsesSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(euclideanPulsesSliders[ch].get());
        yPos += rowHeight;

        // Start On slider
        euclideanStartOnLabels[ch] = std::make_unique<Label>("euclideanStartOnLabel" + String(ch), "E Start On");
        euclideanStartOnLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        euclideanStartOnLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(euclideanStartOnLabels[ch].get());

        euclideanStartOnSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        euclideanStartOnSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        euclideanStartOnSliders[ch]->setRange(1, 32, 1);
        euclideanStartOnSliders[ch]->setValue(1);
        euclideanStartOnSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);
        euclideanStartOnSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        euclideanStartOnSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        euclideanStartOnSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(euclideanStartOnSliders[ch].get());
        yPos += rowHeight;

        // Humanize slider (moved from Shift tab)
        humanizeLabels[ch] = std::make_unique<Label>("humanizeLabel" + String(ch), "Humanize");
        humanizeLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        humanizeLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(humanizeLabels[ch].get());

        humanizeSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        humanizeSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        humanizeSliders[ch]->setRange(0, 127, 1);
        humanizeSliders[ch]->setValue(0);
        humanizeSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);
        humanizeSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        humanizeSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        humanizeSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        humanizeSliders[ch]->setTextValueSuffix("");
        humanizeSliders[ch]->setPopupDisplayEnabled(false, false, nullptr);
        euclideanTab->addAndMakeVisible(humanizeSliders[ch].get());
        yPos += rowHeight;  // Standard spacing

        // Swing label
        swingLabels[ch] = std::make_unique<Label>("swingLabel" + String(ch), "Swing");
        swingLabels[ch]->setBounds(leftColumnX, yPos, labelWidth, 30);
        swingLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(swingLabels[ch].get());

        // Swing slider (Roger Linn algorithm)
        swingSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        swingSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        swingSliders[ch]->setRange(-99, 99, 1);
        swingSliders[ch]->setValue(0);
        swingSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);
        swingSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        swingSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        swingSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        swingSliders[ch]->setTextValueSuffix("%");
        swingSliders[ch]->setPopupDisplayEnabled(false, false, nullptr);
        euclideanTab->addAndMakeVisible(swingSliders[ch].get());
        yPos += rowHeight;  // Standard spacing

        // Shift label (separate from Clock)
        Label* shiftLabel = new Label("shiftLabel" + String(ch), "Shift");
        shiftLabel->setBounds(leftColumnX, yPos, labelWidth, 30);
        shiftLabel->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(shiftLabel);

        // Shift slider (bipolar: 0-126, where 63 is center/OFF)
        shiftSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        shiftSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        shiftSliders[ch]->setRange(0, 126, 1);
        shiftSliders[ch]->setValue(63);  // Default to center (OFF)
        shiftSliders[ch]->setBounds(controlX, yPos, controlWidth, 30);  // Full width like other sliders
        shiftSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        shiftSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        shiftSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        shiftSliders[ch]->setTextValueSuffix("");
        shiftSliders[ch]->setPopupDisplayEnabled(false, false, nullptr);
        euclideanTab->addAndMakeVisible(shiftSliders[ch].get());
        yPos += rowHeight + 10;  // Extra spacing before Clock section

        // Clock label (separate line)
        Label* clockLabel = new Label("clockLabel" + String(ch), "Clock");
        clockLabel->setBounds(leftColumnX, yPos, labelWidth, 30);
        clockLabel->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(clockLabel);

        // Clock Ratio control (separate line below Shift)
        clockRatioComboBoxes[ch] = std::make_unique<ComboBox>();
        ClockDivider tempDivider;
        for (int ratioIndex = 0; ratioIndex < 18; ++ratioIndex) {  // 18 ratios (removed /16 and /12)
            tempDivider.setRatioIndex(ratioIndex);
            clockRatioComboBoxes[ch]->addItem(tempDivider.getRatioString(), ratioIndex + 1);
        }
        clockRatioComboBoxes[ch]->setSelectedId(9); // Default to "x1" (index 8, ID 9)
        clockRatioComboBoxes[ch]->setBounds(controlX, yPos, controlWidth, 30);  // Full width like other controls
        clockRatioComboBoxes[ch]->setColour(ComboBox::backgroundColourId, Colour(0xff232323));
        clockRatioComboBoxes[ch]->setColour(ComboBox::textColourId, Colours::white);
        clockRatioComboBoxes[ch]->setColour(ComboBox::outlineColourId, Colour(0xff595e5f));
        clockRatioComboBoxes[ch]->setColour(PopupMenu::backgroundColourId, Colour(0xff2d2d2d));
        clockRatioComboBoxes[ch]->setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff404040));
        clockRatioComboBoxes[ch]->setColour(PopupMenu::textColourId, Colours::white);
        clockRatioComboBoxes[ch]->setLookAndFeel(&customLookAndFeel);
        euclideanTab->addAndMakeVisible(clockRatioComboBoxes[ch].get());

        yPos += rowHeight + 15;  // Spacing before next channel
    }

    // Process accent channels (BD Acc, SN Acc, HH Acc) in right column
    // Start at same Y position as BD section in left column for horizontal alignment
    yPos = bdStartY;
    for (int ch = 3; ch < 6; ++ch) {
        // Channel header with enable toggle (instrument name where label was)
        euclideanChannelHeaderLabels[ch] = std::make_unique<Label>("euclideanHeaderLabel" + String(ch), channelNames[ch]);
        euclideanChannelHeaderLabels[ch]->setBounds(rightColumnX, yPos, labelWidth, 30);
        euclideanChannelHeaderLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanChannelHeaderLabels[ch]->setColour(Label::textColourId, Colour(0xff9b74f6));
        euclideanChannelHeaderLabels[ch]->setFont(Font(16.0f, Font::bold));
        euclideanTab->addAndMakeVisible(euclideanChannelHeaderLabels[ch].get());
        yPos += 10;  // Move buttons down by 10px

        // "G" button (100% Grids)
        int buttonWidth = 30;
        int buttonHeight = 20;  // Buttons remain 20px height
        int buttonSpacing = 5;
        engineProbGridsButtons[ch] = std::make_unique<TextButton>("G");
        engineProbGridsButtons[ch]->setBounds(rightControlX + 10, yPos, buttonWidth, buttonHeight);  // Moved right by 10px
        engineProbGridsButtons[ch]->onClick = [this, ch]() {
            if (engineProbabilitySliders[ch]) {
                engineProbabilitySliders[ch]->setValue(0.0);  // 100% Grids
            }
        };
        euclideanTab->addAndMakeVisible(engineProbGridsButtons[ch].get());

        // Engine Probability slider (no text box)
        int sliderX = rightControlX + 10 + buttonWidth + buttonSpacing;  // Moved right by 10px
        int sliderWidth = controlWidth - (2 * buttonWidth) - (2 * buttonSpacing) - 10;  // Reduced by 10px
        engineProbabilitySliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::NoTextBox);
        engineProbabilitySliders[ch]->setRange(0.0, 1.0, 0.01);
        engineProbabilitySliders[ch]->setValue(0.0);
        engineProbabilitySliders[ch]->setBounds(sliderX, yPos - 5, sliderWidth, 30);  // Moved up by 5px
        engineProbabilitySliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        engineProbabilitySliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        engineProbabilitySliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(engineProbabilitySliders[ch].get());

        // "E" button (100% Euclidean)
        int eButtonX = sliderX + sliderWidth + buttonSpacing;
        engineProbEuclidButtons[ch] = std::make_unique<TextButton>("E");
        engineProbEuclidButtons[ch]->setBounds(eButtonX, yPos, buttonWidth, buttonHeight);
        engineProbEuclidButtons[ch]->onClick = [this, ch]() {
            if (engineProbabilitySliders[ch]) {
                engineProbabilitySliders[ch]->setValue(1.0);  // 100% Euclidean
            }
        };
        euclideanTab->addAndMakeVisible(engineProbEuclidButtons[ch].get());
        yPos += rowHeight;  // Normal spacing after [T]/[E] buttons

        // G Density slider (moved from Grids tab)
        Label* densityLabel = nullptr;
        if (ch == 3) {
            bdAccentDensityLabel = std::make_unique<Label>("bdAccentDensityLabel", "G Density");
            densityLabel = bdAccentDensityLabel.get();
        } else if (ch == 4) {
            snAccentDensityLabel = std::make_unique<Label>("snAccentDensityLabel", "G Density");
            densityLabel = snAccentDensityLabel.get();
        } else if (ch == 5) {
            hhAccentDensityLabel = std::make_unique<Label>("hhAccentDensityLabel", "G Density");
            densityLabel = hhAccentDensityLabel.get();
        }
        if (densityLabel) {
            densityLabel->setBounds(rightColumnX, yPos, labelWidth, 30);
            densityLabel->setJustificationType(Justification::centredLeft);
            euclideanTab->addAndMakeVisible(densityLabel);
        }

        Slider* densitySlider = nullptr;
        if (ch == 3) {
            bdAccentDensitySlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            bdAccentDensitySlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
            bdAccentDensitySlider->setRange(0, 255, 1);
            bdAccentDensitySlider->setValue(128);
            densitySlider = bdAccentDensitySlider.get();
        } else if (ch == 4) {
            snAccentDensitySlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            snAccentDensitySlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
            snAccentDensitySlider->setRange(0, 255, 1);
            snAccentDensitySlider->setValue(128);
            densitySlider = snAccentDensitySlider.get();
        } else if (ch == 5) {
            hhAccentDensitySlider = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
            hhAccentDensitySlider->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
            hhAccentDensitySlider->setRange(0, 255, 1);
            hhAccentDensitySlider->setValue(128);
            densitySlider = hhAccentDensitySlider.get();
        }
        if (densitySlider) {
            densitySlider->setBounds(rightControlX, yPos, controlWidth, 30);
            densitySlider->setColour(Slider::backgroundColourId, Colour(0xff555555));
            densitySlider->setColour(Slider::trackColourId, Colour(0xff9b74f6));
            densitySlider->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
            euclideanTab->addAndMakeVisible(densitySlider);
        }
        yPos += rowHeight;

        // G Chaos slider (moved from Grids tab)
        chaosAccentLabels[ch - 3] = std::make_unique<Label>("chaosAccentLabel" + String(ch - 3), "G Chaos");
        chaosAccentLabels[ch - 3]->setBounds(rightColumnX, yPos, labelWidth, 30);
        chaosAccentLabels[ch - 3]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(chaosAccentLabels[ch - 3].get());

        chaosAccentSliders[ch - 3] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        chaosAccentSliders[ch - 3]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        chaosAccentSliders[ch - 3]->setRange(0, 127, 1);
        chaosAccentSliders[ch - 3]->setValue(0);
        chaosAccentSliders[ch - 3]->setBounds(rightControlX, yPos, controlWidth, 30);
        chaosAccentSliders[ch - 3]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        chaosAccentSliders[ch - 3]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        chaosAccentSliders[ch - 3]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(chaosAccentSliders[ch - 3].get());
        yPos += rowHeight;  // Same spacing as other sliders

        // E Steps slider
        euclideanStepsLabels[ch] = std::make_unique<Label>("euclideanStepsLabel" + String(ch), "E Steps");
        euclideanStepsLabels[ch]->setBounds(rightColumnX, yPos, labelWidth, 30);
        euclideanStepsLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(euclideanStepsLabels[ch].get());

        euclideanStepsSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        euclideanStepsSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        euclideanStepsSliders[ch]->setRange(2, 32, 1);
        euclideanStepsSliders[ch]->setValue(16);
        euclideanStepsSliders[ch]->setBounds(rightControlX, yPos, controlWidth, 30);
        euclideanStepsSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        euclideanStepsSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        euclideanStepsSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(euclideanStepsSliders[ch].get());
        yPos += rowHeight;

        // Pulses slider
        euclideanPulsesLabels[ch] = std::make_unique<Label>("euclideanPulsesLabel" + String(ch), "E Pulses");
        euclideanPulsesLabels[ch]->setBounds(rightColumnX, yPos, labelWidth, 30);
        euclideanPulsesLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(euclideanPulsesLabels[ch].get());

        euclideanPulsesSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        euclideanPulsesSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        euclideanPulsesSliders[ch]->setRange(0, 32, 1);
        euclideanPulsesSliders[ch]->setValue(4);
        euclideanPulsesSliders[ch]->setBounds(rightControlX, yPos, controlWidth, 30);
        euclideanPulsesSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        euclideanPulsesSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        euclideanPulsesSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(euclideanPulsesSliders[ch].get());
        yPos += rowHeight;

        // Start On slider
        euclideanStartOnLabels[ch] = std::make_unique<Label>("euclideanStartOnLabel" + String(ch), "E Start On");
        euclideanStartOnLabels[ch]->setBounds(rightColumnX, yPos, labelWidth, 30);
        euclideanStartOnLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(euclideanStartOnLabels[ch].get());

        euclideanStartOnSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        euclideanStartOnSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        euclideanStartOnSliders[ch]->setRange(1, 32, 1);
        euclideanStartOnSliders[ch]->setValue(1);
        euclideanStartOnSliders[ch]->setBounds(rightControlX, yPos, controlWidth, 30);
        euclideanStartOnSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        euclideanStartOnSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        euclideanStartOnSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        euclideanTab->addAndMakeVisible(euclideanStartOnSliders[ch].get());
        yPos += rowHeight;

        // Humanize slider (moved from Shift tab)
        humanizeLabels[ch] = std::make_unique<Label>("humanizeLabel" + String(ch), "Humanize");
        humanizeLabels[ch]->setBounds(rightColumnX, yPos, labelWidth, 30);
        humanizeLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(humanizeLabels[ch].get());

        humanizeSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        humanizeSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        humanizeSliders[ch]->setRange(0, 127, 1);
        humanizeSliders[ch]->setValue(0);
        humanizeSliders[ch]->setBounds(rightControlX, yPos, controlWidth, 30);
        humanizeSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        humanizeSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        humanizeSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        humanizeSliders[ch]->setTextValueSuffix("");
        humanizeSliders[ch]->setPopupDisplayEnabled(false, false, nullptr);
        euclideanTab->addAndMakeVisible(humanizeSliders[ch].get());
        yPos += rowHeight;  // Standard spacing

        // Swing label
        swingLabels[ch] = std::make_unique<Label>("swingLabel" + String(ch), "Swing");
        swingLabels[ch]->setBounds(rightColumnX, yPos, labelWidth, 30);
        swingLabels[ch]->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(swingLabels[ch].get());

        // Swing slider (Roger Linn algorithm)
        swingSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        swingSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        swingSliders[ch]->setRange(-99, 99, 1);
        swingSliders[ch]->setValue(0);
        swingSliders[ch]->setBounds(rightControlX, yPos, controlWidth, 30);
        swingSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        swingSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        swingSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        swingSliders[ch]->setTextValueSuffix("%");
        swingSliders[ch]->setPopupDisplayEnabled(false, false, nullptr);
        euclideanTab->addAndMakeVisible(swingSliders[ch].get());
        yPos += rowHeight;  // Standard spacing

        // Shift label (separate from Clock)
        Label* shiftLabelRight = new Label("shiftLabelRight" + String(ch), "Shift");
        shiftLabelRight->setBounds(rightColumnX, yPos, labelWidth, 30);
        shiftLabelRight->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(shiftLabelRight);

        // Shift slider (bipolar: 0-126, where 63 is center/OFF)
        shiftSliders[ch] = std::make_unique<Slider>(Slider::LinearHorizontal, Slider::TextBoxRight);
        shiftSliders[ch]->setTextBoxStyle(Slider::TextBoxRight, false, 40, 18);
        shiftSliders[ch]->setRange(0, 126, 1);
        shiftSliders[ch]->setValue(63);  // Default to center (OFF)
        shiftSliders[ch]->setBounds(rightControlX, yPos, controlWidth, 30);  // Full width like other sliders
        shiftSliders[ch]->setColour(Slider::backgroundColourId, Colour(0xff555555));
        shiftSliders[ch]->setColour(Slider::trackColourId, Colour(0xff9b74f6));
        shiftSliders[ch]->setColour(Slider::thumbColourId, Colour(0xff9b74f6));
        shiftSliders[ch]->setTextValueSuffix("");
        shiftSliders[ch]->setPopupDisplayEnabled(false, false, nullptr);
        euclideanTab->addAndMakeVisible(shiftSliders[ch].get());
        yPos += rowHeight + 10;  // Extra spacing before Clock section

        // Clock label (separate line)
        Label* clockLabelRight = new Label("clockLabelRight" + String(ch), "Clock");
        clockLabelRight->setBounds(rightColumnX, yPos, labelWidth, 30);
        clockLabelRight->setJustificationType(Justification::centredLeft);
        euclideanTab->addAndMakeVisible(clockLabelRight);

        // Clock Ratio control (separate line below Shift)
        clockRatioComboBoxes[ch] = std::make_unique<ComboBox>();
        ClockDivider tempDivider;
        for (int ratioIndex = 0; ratioIndex < 18; ++ratioIndex) {  // 18 ratios (removed /16 and /12)
            tempDivider.setRatioIndex(ratioIndex);
            clockRatioComboBoxes[ch]->addItem(tempDivider.getRatioString(), ratioIndex + 1);
        }
        clockRatioComboBoxes[ch]->setSelectedId(9); // Default to "x1" (index 8, ID 9)
        clockRatioComboBoxes[ch]->setBounds(rightControlX, yPos, controlWidth, 30);  // Full width like other controls
        clockRatioComboBoxes[ch]->setColour(ComboBox::backgroundColourId, Colour(0xff232323));
        clockRatioComboBoxes[ch]->setColour(ComboBox::textColourId, Colours::white);
        clockRatioComboBoxes[ch]->setColour(ComboBox::outlineColourId, Colour(0xff595e5f));
        clockRatioComboBoxes[ch]->setColour(PopupMenu::backgroundColourId, Colour(0xff2d2d2d));
        clockRatioComboBoxes[ch]->setColour(PopupMenu::highlightedBackgroundColourId, Colour(0xff404040));
        clockRatioComboBoxes[ch]->setColour(PopupMenu::textColourId, Colours::white);
        clockRatioComboBoxes[ch]->setLookAndFeel(&customLookAndFeel);
        euclideanTab->addAndMakeVisible(clockRatioComboBoxes[ch].get());

        yPos += rowHeight + 15;  // Spacing before next channel
    }

    // Create parameter attachments for all channels
    for (int ch = 0; ch < 6; ++ch) {
        String prefix = String(CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;

        engineProbabilityAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + ENGINE_PROBABILITY_SUFFIX, *engineProbabilitySliders[ch]);

        euclideanStepsAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + EUCLIDEAN_STEPS_SUFFIX, *euclideanStepsSliders[ch]);

        euclideanPulsesAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + EUCLIDEAN_PULSES_SUFFIX, *euclideanPulsesSliders[ch]);

        euclideanStartOnAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + EUCLIDEAN_START_SUFFIX, *euclideanStartOnSliders[ch]);

        // Humanize attachments (moved from Shift tab)
        humanizeAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + "humanize", *humanizeSliders[ch]);

        // Swing attachments (Roger Linn algorithm)
        swingAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + SWING_SUFFIX, *swingSliders[ch]);

        // Shift and Clock Ratio attachments (moved from Shift tab)
        shiftAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + "shift", *shiftSliders[ch]);
        clockRatioAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(
            audioProcessor.getParameters(), prefix + CLOCK_RATIO_SUFFIX, *clockRatioComboBoxes[ch]);

        // Attach listener to Steps slider to dynamically update Pulses and Shift ranges
        euclideanStepsSliders[ch]->addListener(this);

        // Initialize ranges based on current Steps value
        int steps = static_cast<int>(euclideanStepsSliders[ch]->getValue());
        euclideanPulsesSliders[ch]->setRange(0, steps, 1);
        euclideanStartOnSliders[ch]->setRange(1, steps, 1);
    }

    // ===== PARAMETER ATTACHMENTS FOR DENSITY AND CHAOS SLIDERS =====
    // These sliders are now on the Euclidean tab, so attachments must be created here

    // Main channel density controls
    bdDensityAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "density_bd", *bdDensitySlider);
    sdDensityAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "density_sd", *sdDensitySlider);
    hhDensityAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "density_hh", *hhDensitySlider);

    // Main channel chaos controls
    for (size_t ch = 0; ch < 3; ++ch) {
        String prefix = String(MAIN_CHANNEL_NAMES[ch]) + PARAMETER_SEPARATOR;
        chaosChannelAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + CHAOS_SUFFIX, *chaosChannelSliders[ch]);
    }

    // Accent density controls
    bdAccentDensityAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "density_bd_acc", *bdAccentDensitySlider);
    snAccentDensityAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "density_sn_acc", *snAccentDensitySlider);
    hhAccentDensityAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "density_hh_acc", *hhAccentDensitySlider);

    // Accent chaos controls
    const char* accentChannelNames[] = {"bd_acc", "sn_acc", "hh_acc"};
    for (size_t ch = 0; ch < 3; ++ch) {
        String prefix = String(accentChannelNames[ch]) + "_";
        chaosAccentAttachments[ch] = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            audioProcessor.getParameters(), prefix + "chaos", *chaosAccentSliders[ch]);
    }

    // Global pattern controls (Map X, Map Y, Chaos All) - moved from Grids tab
    gridsXAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "grids_x", *gridsXSlider);
    gridsYAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "grids_y", *gridsYSlider);
    chaosAttachment = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.getParameters(), "chaos", *chaosSlider);
}

