#include "PresetPanel.h"
#include <map>

// ============================================================================
// PresetDropdownMenu::PresetListContent
// ============================================================================

void PresetDropdownMenu::PresetListContent::paint(juce::Graphics& g)
{
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
    {
        auto rowBounds = juce::Rectangle<int>(0, i * rowHeight, getWidth(), rowHeight);

        // Background: selected > hovered > normal
        if (rows[i].isSelected)
            g.setColour(juce::Colour(0xff505050));
        else if (i == hoveredRow)
            g.setColour(juce::Colour(0xff404040));
        else
            g.setColour(juce::Colour(0xff101010));
        g.fillRect(rowBounds);

        // Text
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(fontSize));
        g.drawText(rows[i].displayName, rowBounds.reduced(12, 0),
                   juce::Justification::centredLeft);
    }
}

void PresetDropdownMenu::PresetListContent::mouseDown(const juce::MouseEvent& e)
{
    mouseDownPos = e.getPosition();
    wasDragged = false;
}

void PresetDropdownMenu::PresetListContent::mouseDrag(const juce::MouseEvent& e)
{
    if (!wasDragged)
    {
        auto dist = e.getPosition().getDistanceFrom(mouseDownPos);
        if (dist > 8.0f)
            wasDragged = true;
    }
}

void PresetDropdownMenu::PresetListContent::mouseUp(const juce::MouseEvent& e)
{
    if (!wasDragged)
    {
        int row = e.getPosition().y / rowHeight;
        if (row >= 0 && row < static_cast<int>(rows.size()))
        {
            if (onRowClicked)
                onRowClicked(row);
        }
    }
    wasDragged = false;
}

void PresetDropdownMenu::PresetListContent::mouseMove(const juce::MouseEvent& e)
{
    int newHover = e.getPosition().y / rowHeight;
    if (newHover < 0 || newHover >= static_cast<int>(rows.size()))
        newHover = -1;
    if (newHover != hoveredRow)
    {
        hoveredRow = newHover;
        repaint();
    }
}

void PresetDropdownMenu::PresetListContent::mouseExit(const juce::MouseEvent&)
{
    if (hoveredRow != -1)
    {
        hoveredRow = -1;
        repaint();
    }
}

// ============================================================================
// PresetDropdownMenu
// ============================================================================

PresetDropdownMenu::PresetDropdownMenu(PresetManager& presetMgr, CustomPresetLookAndFeel& laf)
    : presetManager(presetMgr)
    , customLF(laf)
{
    // Pick up scaled dimensions from LookAndFeel
    kRowHeight = laf.menuRowHeight;
    menuFontSize = laf.menuFontSize;
    listContent.rowHeight = kRowHeight;
    listContent.fontSize = menuFontSize;

    // Set up action buttons
    for (auto* button : {&saveButton, &saveAsButton, &deleteButton, &randomButton})
    {
        addAndMakeVisible(button);
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        button->setLookAndFeel(&customLF);
    }

    saveButton.onClick   = [this]() { if (onAction) onAction(1); dismiss(); };  // kSaveID
    saveAsButton.onClick = [this]() { if (onAction) onAction(2); dismiss(); };  // kSaveAsID
    deleteButton.onClick = [this]() { if (onAction) onAction(3); dismiss(); };  // kDeleteID
    randomButton.onClick = [this]() { if (onAction) onAction(4); dismiss(); };  // kRandomID

    // Set up viewport for scrollable preset list
    viewport.setViewedComponent(&listContent, false); // false = don't own
    viewport.setScrollBarsShown(true, false);  // vertical only
    viewport.setScrollBarThickness(10);
    viewport.setLookAndFeel(&customLF);
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(viewport);

    // Preset row click handler
    listContent.onRowClicked = [this](int rowIndex)
    {
        if (rowIndex >= 0 && rowIndex < static_cast<int>(listContent.rows.size()))
        {
            auto& row = listContent.rows[rowIndex];
            if (onPresetSelected)
                onPresetSelected(row.name, row.category);
            dismiss();
        }
    };

    buildContent();
}

PresetDropdownMenu::~PresetDropdownMenu()
{
    viewport.setLookAndFeel(nullptr);
    for (auto* button : {&saveButton, &saveAsButton, &deleteButton, &randomButton})
        button->setLookAndFeel(nullptr);
}

void PresetDropdownMenu::buildContent()
{
    folders.clear();
    listContent.rows.clear();

    auto presets = presetManager.getAllPresetsWithCategories();

    // Group by category
    std::map<juce::String, std::vector<PresetManager::PresetItem>> categorizedPresets;
    for (const auto& preset : presets)
        categorizedPresets[preset.category].push_back(preset);

    auto currentName = presetManager.getCurrentPresetName();
    auto currentCat = presetManager.getCurrentPresetCategory();
    bool isModified = presetManager.isCurrentPresetModified();

    // Build folder list (non-empty categories)
    for (const auto& categoryPair : categorizedPresets)
    {
        if (categoryPair.first.isEmpty())
            continue; // Root presets handled separately

        FolderInfo folder;
        folder.name = categoryPair.first;
        folder.presets = categoryPair.second;
        folders.push_back(std::move(folder));
    }

    // Build root-level preset rows
    auto rootIt = categorizedPresets.find("");
    if (rootIt != categorizedPresets.end())
    {
        for (const auto& preset : rootIt->second)
        {
            PresetListContent::Row row;
            row.name = preset.name;
            row.category = preset.category;
            row.isSelected = (preset.name == currentName && preset.category == currentCat);

            // Build display name
            row.displayName = preset.isFactory
                ? "[Factory] " + preset.name
                : preset.name;

            // Append asterisk if this is the current preset and it's modified
            if (row.isSelected && isModified)
                row.displayName += " *";

            listContent.rows.push_back(std::move(row));
        }
    }

    // Set list content size
    int contentHeight = static_cast<int>(listContent.rows.size()) * kRowHeight;
    listContent.setSize(customLF.menuWidth, juce::jmax(contentHeight, 1));
}

int PresetDropdownMenu::computeDesiredHeight() const
{
    int h = actionBarHeight;

    // Folders section
    if (!folders.empty())
    {
        h += kSepHeight;
        h += static_cast<int>(folders.size()) * kRowHeight;
    }

    // Separator before presets
    if (!listContent.rows.empty())
        h += kSepHeight;

    // Preset list (capped at max visible rows)
    int presetRows = juce::jmin(static_cast<int>(listContent.rows.size()), kMaxPresetRows);
    h += presetRows * kRowHeight;

    return h;
}

void PresetDropdownMenu::show(juce::Component* parent, juce::Rectangle<int> anchorBounds)
{
    if (parent == nullptr) return;

    // Match action bar height to the preset menu bar height
    actionBarHeight = anchorBounds.getHeight();

    int w = anchorBounds.getWidth();
    int desiredH = computeDesiredHeight();
    int maxH = parent->getHeight() - anchorBounds.getBottom() - 5;
    int h = juce::jmin(desiredH, maxH);

    setBounds(anchorBounds.getX(), anchorBounds.getBottom() - 2, w, h);
    parent->addAndMakeVisible(this);
    enterModalState(false, nullptr, false);
    toFront(true);
}

void PresetDropdownMenu::dismiss()
{
    exitModalState(0);
    setVisible(false);
    if (auto* p = getParentComponent())
        p->removeChildComponent(this);

    if (onDismiss)
    {
        auto cb = onDismiss;
        juce::MessageManager::callAsync(cb);
    }
}

void PresetDropdownMenu::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background (rounded corners)
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(bounds.toFloat(), 6.0f);

    // Border (rounded corners)
    g.setColour(juce::Colour(0xff595e5f));
    g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 6.0f, 2.0f);

    // Draw folder rows
    for (int i = 0; i < static_cast<int>(folders.size()); ++i)
    {
        if (i >= static_cast<int>(folderRowBounds.size()))
            break;

        auto rowBounds = folderRowBounds[i];

        // Selected (open submenu) or hover highlight
        if (i == openFolderIndex)
        {
            g.setColour(juce::Colour(0xff505050));  // Same as selected preset
            g.fillRect(rowBounds);
        }
        else if (i == hoveredFolderIndex)
        {
            g.setColour(juce::Colour(0xff404040));
            g.fillRect(rowBounds);
        }

        // Folder name
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(menuFontSize));
        auto textArea = rowBounds.reduced(12, 0);

        // Chevron-right on the right side
        auto arrowArea = textArea.removeFromRight(20);
        g.drawText(folders[i].name, textArea, juce::Justification::centredLeft);

        // Draw chevron-right path
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        juce::Path chevron;
        auto cx = arrowArea.getCentreX();
        auto cy = static_cast<float>(arrowArea.getCentreY());
        float sz = 4.0f;
        chevron.startNewSubPath(static_cast<float>(cx) - sz * 0.5f, cy - sz);
        chevron.lineTo(static_cast<float>(cx) + sz * 0.5f, cy);
        chevron.lineTo(static_cast<float>(cx) - sz * 0.5f, cy + sz);
        g.strokePath(chevron, juce::PathStrokeType(1.5f));
    }

    // Draw separators (2px, matching border color)
    g.setColour(juce::Colour(0xff595e5f));

    // Vertical separators between action buttons
    g.fillRect(saveButton.getRight(), saveButton.getY(), 2, saveButton.getHeight());
    g.fillRect(saveAsButton.getRight(), saveAsButton.getY(), 2, saveAsButton.getHeight());
    g.fillRect(deleteButton.getRight(), deleteButton.getY(), 2, deleteButton.getHeight());

    // Horizontal separator after action bar
    int sepY = actionBarHeight;
    g.fillRect(2, sepY, bounds.getWidth() - 4, 2);

    // Separator between folders and presets
    if (!folders.empty() && !listContent.rows.empty())
    {
        sepY = actionBarHeight + kSepHeight + static_cast<int>(folders.size()) * kRowHeight;
        g.fillRect(2, sepY, bounds.getWidth() - 4, 2);
    }
}

void PresetDropdownMenu::resized()
{
    auto bounds = getLocalBounds().reduced(2); // Inset for 2px border

    // Action bar at top (buttons with 2px gaps for separator lines)
    actionBarArea = bounds.removeFromTop(actionBarHeight - 1);
    {
        int aw = actionBarArea.getWidth();
        int gapW = 2;
        int btnW = (aw - 3 * gapW) / 4;
        int x = actionBarArea.getX();
        int y = actionBarArea.getY();
        int h = actionBarArea.getHeight();
        saveButton.setBounds(x, y, btnW, h);
        saveAsButton.setBounds(x + btnW + gapW, y, btnW, h);
        deleteButton.setBounds(x + 2 * (btnW + gapW), y, btnW, h);
        randomButton.setBounds(x + 3 * (btnW + gapW), y, aw - 3 * (btnW + gapW), h);
    }

    // Separator space
    bounds.removeFromTop(kSepHeight);

    // Folder rows
    folderRowBounds.clear();
    for (int i = 0; i < static_cast<int>(folders.size()); ++i)
    {
        folderRowBounds.push_back(bounds.removeFromTop(kRowHeight));
    }

    // Separator space
    if (!folders.empty() && !listContent.rows.empty())
        bounds.removeFromTop(kSepHeight);

    // Viewport takes remaining space
    viewport.setBounds(bounds);

    // Update list content width to match viewport (excluding scrollbar if visible)
    int contentWidth = viewport.getMaximumVisibleWidth();
    int contentHeight = static_cast<int>(listContent.rows.size()) * kRowHeight;
    listContent.setSize(contentWidth, juce::jmax(contentHeight, 1));

    // Scroll to make selected preset visible
    for (int i = 0; i < static_cast<int>(listContent.rows.size()); ++i)
    {
        if (listContent.rows[i].isSelected)
        {
            int rowTop = i * kRowHeight;
            int viewH = viewport.getViewHeight();
            if (rowTop < viewport.getViewPositionY() || rowTop + kRowHeight > viewport.getViewPositionY() + viewH)
            {
                // Center the selected row in the viewport
                int scrollY = juce::jmax(0, rowTop - viewH / 2 + kRowHeight / 2);
                viewport.setViewPosition(0, scrollY);
            }
            break;
        }
    }
}

void PresetDropdownMenu::mouseDown(const juce::MouseEvent& e)
{
    auto pos = e.getPosition();

    // Check if clicked on a folder row
    for (int i = 0; i < static_cast<int>(folderRowBounds.size()); ++i)
    {
        if (folderRowBounds[i].contains(pos))
        {
            handleFolderClick(i);
            return;
        }
    }
}

void PresetDropdownMenu::mouseMove(const juce::MouseEvent& e)
{
    int oldHover = hoveredFolderIndex;
    hoveredFolderIndex = -1;

    auto pos = e.getPosition();
    for (int i = 0; i < static_cast<int>(folderRowBounds.size()); ++i)
    {
        if (folderRowBounds[i].contains(pos))
        {
            hoveredFolderIndex = i;
            break;
        }
    }

    if (hoveredFolderIndex != oldHover)
        repaint();
}

void PresetDropdownMenu::mouseExit(const juce::MouseEvent&)
{
    if (hoveredFolderIndex != -1)
    {
        hoveredFolderIndex = -1;
        repaint();
    }
}

void PresetDropdownMenu::inputAttemptWhenModal()
{
    dismiss();
}

void PresetDropdownMenu::handleFolderClick(int index)
{
    if (index < 0 || index >= static_cast<int>(folders.size()))
        return;

    auto& folder = folders[index];

    auto currentName = presetManager.getCurrentPresetName();
    auto currentCat = presetManager.getCurrentPresetCategory();
    bool isModified = presetManager.isCurrentPresetModified();

    std::vector<SubfolderPopup::Row> items;
    for (const auto& preset : folder.presets)
    {
        SubfolderPopup::Row row;
        row.name = preset.name;
        row.category = preset.category;
        row.isSelected = (preset.name == currentName && preset.category == currentCat);
        row.displayName = preset.name;
        if (row.isSelected && isModified)
            row.displayName += " *";
        items.push_back(std::move(row));
    }

    // Highlight the clicked folder while submenu is open
    openFolderIndex = index;
    repaint();

    // Show subfolder popup to the right of the dropdown
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr) return;

    auto rowBounds = folderRowBounds[index];
    auto posInTopLevel = topLevel->getLocalPoint(this, juce::Point<int>(getWidth(), rowBounds.getY()));

    subfolderPopup = std::make_unique<SubfolderPopup>();

    juce::Component::SafePointer<PresetDropdownMenu> safeThis(this);
    subfolderPopup->onRowClicked = [safeThis, items](int rowIdx)
    {
        if (safeThis == nullptr) return;
        if (rowIdx >= 0 && rowIdx < static_cast<int>(items.size()))
        {
            if (safeThis->onPresetSelected)
                safeThis->onPresetSelected(items[rowIdx].name, items[rowIdx].category);
            safeThis->dismiss();
        }
    };
    subfolderPopup->onDismissed = [safeThis]()
    {
        if (safeThis == nullptr) return;
        safeThis->openFolderIndex = -1;
        safeThis->repaint();
        safeThis->subfolderPopup.reset();
    };
    subfolderPopup->show(topLevel, posInTopLevel, getWidth(), std::move(items), &customLF);
}

// ============================================================================
// PresetPanel
// ============================================================================

PresetPanel::PresetPanel(PresetManager& presetMgr,
                         juce::AudioProcessorValueTreeState& apvts)
    : presetManager(presetMgr)
    , valueTreeState(apvts)
{
    // No child components — everything is custom painted
    setOpaque(false);

    setupParameterListeners();
    updatePresetDisplay();
}

PresetPanel::~PresetPanel()
{
    removeParameterListeners();

    if (presetDropdown != nullptr)
        presetDropdown->dismiss();

    if (fileChooser != nullptr)
        fileChooser.reset();
}

void PresetPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background — dark gray matching knob interior
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Border — thin gray outline
    g.setColour(juce::Colour(0xff595e5f));
    g.drawRoundedRectangle(bounds.reduced(borderThickness * 0.5f), cornerRadius, borderThickness);

    // Draw triangles as paths instead of unicode (fixes iPad rendering issues)
    g.setColour(juce::Colour(0xff595e5f));

    // Left chevron (stroked path, same size as dropdown folder chevrons)
    {
        float cx = leftArrowZone.toFloat().getCentreX();
        float cy = leftArrowZone.toFloat().getCentreY();
        float sz = 4.0f;
        juce::Path chevron;
        chevron.startNewSubPath(cx + sz * 0.5f, cy - sz);
        chevron.lineTo(cx - sz * 0.5f, cy);
        chevron.lineTo(cx + sz * 0.5f, cy + sz);
        g.strokePath(chevron, juce::PathStrokeType(1.5f));
    }

    // Right chevron (stroked path, same size as dropdown folder chevrons)
    {
        float cx = rightArrowZone.toFloat().getCentreX();
        float cy = rightArrowZone.toFloat().getCentreY();
        float sz = 4.0f;
        juce::Path chevron;
        chevron.startNewSubPath(cx - sz * 0.5f, cy - sz);
        chevron.lineTo(cx + sz * 0.5f, cy);
        chevron.lineTo(cx - sz * 0.5f, cy + sz);
        g.strokePath(chevron, juce::PathStrokeType(1.5f));
    }

    // Preset name centered in the middle zone (14pt to match dropdown list)
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f));
    g.drawText(displayedPresetName, centerZone, juce::Justification::centred);
}

void PresetPanel::resized()
{
    auto bounds = getLocalBounds();

    leftArrowZone = bounds.removeFromLeft(arrowZoneWidth);
    rightArrowZone = bounds.removeFromRight(arrowZoneWidth);
    centerZone = bounds;

    // Update LookAndFeel menu width to match our total width
    customLookAndFeel.menuWidth = getWidth();
}

void PresetPanel::mouseDown(const juce::MouseEvent& event)
{
    auto pos = event.getPosition();

    if (leftArrowZone.contains(pos))
    {
        loadPreviousPreset();
    }
    else if (rightArrowZone.contains(pos))
    {
        loadNextPreset();
    }
    else if (centerZone.contains(pos))
    {
        showPresetMenu();
    }
}

void PresetPanel::parameterChanged(const juce::String& /*parameterID*/, float /*newValue*/)
{
    // Mark preset as modified when any parameter changes
    // Only if we're not currently loading a preset
    if (!isLoadingPreset)
    {
        presetManager.markPresetAsModified();

        // Update display on message thread (parameterChanged may be called from audio thread)
        juce::Component::SafePointer<PresetPanel> safeThis(this);
        juce::MessageManager::callAsync([safeThis]()
        {
            if (safeThis != nullptr)
                safeThis->updatePresetDisplay();
        });
    }
}

void PresetPanel::updatePresetList()
{
    updatePresetDisplay();
}

// ============================================================================
// Menu
// ============================================================================

void PresetPanel::showPresetMenu()
{
    // Toggle dropdown if already open
    if (presetDropdown != nullptr)
    {
        presetDropdown->dismiss();
        return;
    }

    // Rescan presets before showing menu
    presetManager.rescanUserPresets();

    // Position dropdown below this component in top-level coordinate space
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr)
        return;

    auto boundsInTopLevel = topLevel->getLocalArea(this, getLocalBounds());

    // Scale dropdown font/row to match the contentWrapper transform on iPhone
    float scale = (getWidth() > 0) ? boundsInTopLevel.getWidth() / (float)getWidth() : 1.0f;
    if (scale > 0.99f) scale = 1.0f; // Avoid tiny rounding on desktop
    customLookAndFeel.menuFontSize = 14.0f * scale;
    customLookAndFeel.menuRowHeight = juce::jmax(14, (int)(25 * scale));

    presetDropdown = std::make_unique<PresetDropdownMenu>(presetManager, customLookAndFeel);

    // Action callback (Save/SaveAs/Delete/Random)
    presetDropdown->onAction = [this](int actionId) { handleMenuResult(actionId); };

    // Preset selection callback
    presetDropdown->onPresetSelected = [this](const juce::String& name, const juce::String& category)
    {
        juce::ScopedValueSetter<bool> svs(isLoadingPreset, true);
        presetManager.loadPreset(name, category);
        updatePresetDisplay();
    };

    // Cleanup callback when dropdown is dismissed
    presetDropdown->onDismiss = [this]() { presetDropdown.reset(); };

    presetDropdown->show(topLevel, boundsInTopLevel);
}

void PresetPanel::handleMenuResult(int result)
{
    switch (result)
    {
        case kSaveID:
            saveCurrentPreset();
            break;
        case kSaveAsID:
            showSaveAsDialog();
            break;
        case kDeleteID:
            deleteCurrentPreset();
            break;
        case kRandomID:
            randomizePreset();
            break;
        default:
            break;
    }
}

// ============================================================================
// Preset operations
// ============================================================================

void PresetPanel::loadPreviousPreset()
{
    juce::ScopedValueSetter<bool> svs(isLoadingPreset, true);

    if (presetManager.loadPreviousPreset())
    {
        updatePresetDisplay();
    }
}

void PresetPanel::loadNextPreset()
{
    juce::ScopedValueSetter<bool> svs(isLoadingPreset, true);

    if (presetManager.loadNextPreset())
    {
        updatePresetDisplay();
    }
}

void PresetPanel::saveCurrentPreset()
{
    auto currentName = presetManager.getCurrentPresetName();

    if (currentName.isEmpty())
    {
        showSaveAsDialog();
        return;
    }

    // Check if it's a factory preset
    auto factoryNames = presetManager.getFactoryPresetNames();
    if (factoryNames.contains(currentName))
    {
        // Can't overwrite factory presets
        showSaveAsDialog();
        return;
    }

    // Get the current preset's file path to preserve subfolder location
    auto currentFile = presetManager.getCurrentPresetFile();
    if (currentFile != juce::File())
    {
        // Save to the same location (preserves subfolder)
        presetManager.savePresetAs(currentFile);
    }
    else
    {
        // Fallback to old behavior if file path not available
        presetManager.savePreset(currentName);
    }

    updatePresetDisplay();
}

void PresetPanel::showSaveAsDialog()
{
    #if JUCE_IOS
        // iOS: Custom Save-As dialog with folder selector, rendered on top-level editor
        auto suggestedName = presetManager.getCurrentPresetName();
        if (suggestedName.isEmpty())
            suggestedName = "NewPreset";

        auto* topLevel = getTopLevelComponent();
        if (topLevel == nullptr)
            return;

        auto userPresetDir = presetManager.getUserPresetDirectory();
        if (!userPresetDir.exists())
            userPresetDir.createDirectory();

        presetNameDialog = std::make_unique<PresetNameDialog>(suggestedName, userPresetDir, &customLookAndFeel);
        juce::Component::SafePointer<PresetPanel> safeThis(this);

        presetNameDialog->onSave = [safeThis, userPresetDir](const juce::String& subfolder, const juce::String& presetName)
        {
            juce::MessageManager::callAsync([safeThis, userPresetDir, subfolder, presetName]()
            {
                if (safeThis == nullptr) return;
                auto& self = *safeThis;

                if (auto* top = self.getTopLevelComponent())
                    top->removeChildComponent(self.presetNameDialog.get());
                self.presetNameDialog.reset();

                auto finalName = presetName.trim();
                if (finalName.isEmpty())
                    finalName = "NewPreset";

                auto targetDir = subfolder.isEmpty()
                    ? userPresetDir
                    : userPresetDir.getChildFile(subfolder);

                if (!targetDir.exists())
                    targetDir.createDirectory();

                auto presetFile = targetDir.getChildFile(finalName + ".xml");
                int counter = 1;
                while (presetFile.exists())
                {
                    presetFile = targetDir.getChildFile(finalName + "_" + juce::String(counter) + ".xml");
                    counter++;
                }

                self.presetManager.savePresetAs(presetFile);
                self.updatePresetDisplay();
            });
        };

        presetNameDialog->onCancel = [safeThis]()
        {
            juce::MessageManager::callAsync([safeThis]()
            {
                if (safeThis == nullptr) return;
                auto& self = *safeThis;

                if (auto* top = self.getTopLevelComponent())
                    top->removeChildComponent(self.presetNameDialog.get());
                self.presetNameDialog.reset();
            });
        };

        topLevel->addAndMakeVisible(presetNameDialog.get());
        {
            int dlgW = 320, dlgH = 134;
            int x = (topLevel->getWidth() - dlgW) / 2;
            int y = topLevel->getHeight() / 6;  // Upper area to avoid iOS keyboard
            presetNameDialog->setBounds(x, y, dlgW, dlgH);
        }
        presetNameDialog->toFront(true);
        presetNameDialog->nameEditor.grabKeyboardFocus();
    #else
        // macOS/Desktop: Use native file chooser dialog
        auto userPresetDir = presetManager.getUserPresetDirectory();
        auto suggestedName = presetManager.getCurrentPresetName();
        if (suggestedName.isEmpty())
            suggestedName = "NewPreset";

        if (!userPresetDir.exists())
        {
            auto result = userPresetDir.getParentDirectory().createDirectory();
            if (result.wasOk())
                result = userPresetDir.createDirectory();

            if (!result.wasOk())
            {
                return;
            }
        }

        fileChooser = std::make_unique<juce::FileChooser>(
            "Save Preset As",
            userPresetDir.getChildFile(suggestedName + ".xml"),
            "*.xml");

        auto flags = juce::FileBrowserComponent::saveMode |
                     juce::FileBrowserComponent::warnAboutOverwriting |
                     juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();
            if (file != juce::File{})
            {
                if (!file.hasFileExtension(".xml"))
                    file = file.withFileExtension(".xml");

                presetManager.savePresetAs(file);
                updatePresetDisplay();
            }

            fileChooser.reset();
        });
    #endif
}

void PresetPanel::deleteCurrentPreset()
{
    auto currentName = presetManager.getCurrentPresetName();
    auto currentCategory = presetManager.getCurrentPresetCategory();

    if (currentName.isEmpty())
        return;

    // Check if it's a factory preset
    auto factoryNames = presetManager.getFactoryPresetNames();
    if (factoryNames.contains(currentName))
    {
    #if JUCE_IOS
        return;
    #else
        juce::NativeMessageBox::showMessageBoxAsync(
            juce::MessageBoxIconType::InfoIcon,
            "Cannot Delete",
            "Factory presets cannot be deleted.");
        return;
    #endif
    }

#if JUCE_IOS
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr) return;

    confirmDialog = std::make_unique<ConfirmDialog>(
        "Delete \"" + currentName + "\"?", "Delete");

    juce::Component::SafePointer<PresetPanel> safeThis(this);

    confirmDialog->onConfirm = [safeThis, currentName, currentCategory]()
    {
        juce::MessageManager::callAsync([safeThis, currentName, currentCategory]()
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;

            if (auto* top = self.getTopLevelComponent())
                top->removeChildComponent(self.confirmDialog.get());
            self.confirmDialog.reset();

            if (self.presetManager.deletePreset(currentName, currentCategory))
                self.updatePresetDisplay();
        });
    };

    confirmDialog->onCancel = [safeThis]()
    {
        juce::MessageManager::callAsync([safeThis]()
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;

            if (auto* top = self.getTopLevelComponent())
                top->removeChildComponent(self.confirmDialog.get());
            self.confirmDialog.reset();
        });
    };

    topLevel->addAndMakeVisible(confirmDialog.get());
    {
        int dlgW = 300, dlgH = 100;
        int x = (topLevel->getWidth() - dlgW) / 2;
        int y = topLevel->getHeight() / 6;
        confirmDialog->setBounds(x, y, dlgW, dlgH);
    }
    confirmDialog->toFront(true);
#else
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Delete Preset")
        .withMessage("Are you sure you want to delete \"" + currentName + "\"?")
        .withButton("Delete")
        .withButton("Cancel");

    juce::NativeMessageBox::showAsync(options, [this, currentName, currentCategory](int result)
    {
        if (result == 0) // Delete button (first button = index 0)
        {
            if (presetManager.deletePreset(currentName, currentCategory))
            {
                updatePresetDisplay();
            }
        }
    });
#endif
}

void PresetPanel::randomizePreset()
{
    presetManager.randomizeFromPresets();
    updatePresetDisplay();
}

// ============================================================================
// Display & parameter listeners
// ============================================================================

void PresetPanel::updatePresetDisplay()
{
    auto name = presetManager.getCurrentPresetName();

    if (presetManager.isCurrentPresetModified() && !name.isEmpty())
        name += " *";

    if (name.isEmpty())
        name = "Untitled";

    displayedPresetName = name;
    repaint();
}

void PresetPanel::setupParameterListeners()
{
    for (auto* param : valueTreeState.processor.getParameters())
    {
        if (auto* paramWithID = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
        {
            parameterIDs.add(paramWithID->paramID);
            valueTreeState.addParameterListener(paramWithID->paramID, this);
        }
    }
}

void PresetPanel::removeParameterListeners()
{
    for (const auto& paramID : parameterIDs)
    {
        valueTreeState.removeParameterListener(paramID, this);
    }
}
