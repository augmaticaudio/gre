#include "MidiMappingPanel.h"
#include <map>

// ============================================================================
// MappingDropdownMenu::MappingListContent
// ============================================================================

void MappingDropdownMenu::MappingListContent::paint(juce::Graphics& g)
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
        g.setFont(juce::Font(14.0f));
        g.drawText(rows[i].displayName, rowBounds.reduced(12, 0),
                   juce::Justification::centredLeft);
    }
}

void MappingDropdownMenu::MappingListContent::mouseDown(const juce::MouseEvent& e)
{
    mouseDownPos = e.getPosition();
    wasDragged = false;
}

void MappingDropdownMenu::MappingListContent::mouseDrag(const juce::MouseEvent& e)
{
    if (!wasDragged)
    {
        auto dist = e.getPosition().getDistanceFrom(mouseDownPos);
        if (dist > 8.0f)
            wasDragged = true;
    }
}

void MappingDropdownMenu::MappingListContent::mouseUp(const juce::MouseEvent& e)
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

void MappingDropdownMenu::MappingListContent::mouseMove(const juce::MouseEvent& e)
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

void MappingDropdownMenu::MappingListContent::mouseExit(const juce::MouseEvent&)
{
    if (hoveredRow != -1)
    {
        hoveredRow = -1;
        repaint();
    }
}

// ============================================================================
// MappingDropdownMenu
// ============================================================================

MappingDropdownMenu::MappingDropdownMenu(MidiMappingManager& mappingMgr, CustomPresetLookAndFeel& laf)
    : mappingManager(mappingMgr)
    , customLF(laf)
{
    // Set up action buttons (3 buttons — no Random)
    for (auto* button : {&saveButton, &saveAsButton, &deleteButton})
    {
        addAndMakeVisible(button);
        button->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        button->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        button->setLookAndFeel(&customLF);
    }

    saveButton.onClick   = [this]() { if (onAction) onAction(1); dismiss(); };  // kSaveID
    saveAsButton.onClick = [this]() { if (onAction) onAction(2); dismiss(); };  // kSaveAsID
    deleteButton.onClick = [this]() { if (onAction) onAction(3); dismiss(); };  // kDeleteID

    // Set up viewport for scrollable mapping list
    viewport.setViewedComponent(&listContent, false); // false = don't own
    viewport.setScrollBarsShown(true, false);  // vertical only
    viewport.setScrollBarThickness(10);
    viewport.setLookAndFeel(&customLF);
    viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
    addAndMakeVisible(viewport);

    // Mapping row click handler
    listContent.onRowClicked = [this](int rowIndex)
    {
        if (rowIndex >= 0 && rowIndex < static_cast<int>(listContent.rows.size()))
        {
            auto& row = listContent.rows[rowIndex];
            if (onMappingSelected)
                onMappingSelected(row.name, row.category);
            dismiss();
        }
    };

    buildContent();
}

MappingDropdownMenu::~MappingDropdownMenu()
{
    viewport.setLookAndFeel(nullptr);
    for (auto* button : {&saveButton, &saveAsButton, &deleteButton})
        button->setLookAndFeel(nullptr);
}

void MappingDropdownMenu::buildContent()
{
    folders.clear();
    listContent.rows.clear();

    auto mappings = mappingManager.getAllMappingsWithCategories();

    // Group by category
    std::map<juce::String, std::vector<MidiMappingManager::MappingItem>> categorized;
    for (const auto& mapping : mappings)
        categorized[mapping.category].push_back(mapping);

    auto currentName = mappingManager.getCurrentMappingName();
    auto currentCat = mappingManager.getCurrentMappingCategory();

    // Build folder list (non-empty categories)
    for (const auto& categoryPair : categorized)
    {
        if (categoryPair.first.isEmpty())
            continue; // Root mappings handled separately

        FolderInfo folder;
        folder.name = categoryPair.first;
        folder.mappings = categoryPair.second;
        folders.push_back(std::move(folder));
    }

    // Build root-level mapping rows
    auto rootIt = categorized.find("");
    if (rootIt != categorized.end())
    {
        for (const auto& mapping : rootIt->second)
        {
            MappingListContent::Row row;
            row.name = mapping.name;
            row.category = mapping.category;
            row.isSelected = (mapping.name == currentName && mapping.category == currentCat);
            row.displayName = mapping.name;

            listContent.rows.push_back(std::move(row));
        }
    }

    // Set list content size
    int contentHeight = static_cast<int>(listContent.rows.size()) * kRowHeight;
    listContent.setSize(customLF.menuWidth, juce::jmax(contentHeight, 1));
}

int MappingDropdownMenu::computeDesiredHeight() const
{
    int h = actionBarHeight;

    // Folders section
    if (!folders.empty())
    {
        h += kSepHeight;
        h += static_cast<int>(folders.size()) * kRowHeight;
    }

    // Separator before mappings
    if (!listContent.rows.empty())
        h += kSepHeight;

    // Mapping list (capped at max visible rows)
    int mappingRows = juce::jmin(static_cast<int>(listContent.rows.size()), kMaxMappingRows);
    h += mappingRows * kRowHeight;

    return h;
}

void MappingDropdownMenu::show(juce::Component* parent, juce::Rectangle<int> anchorBounds)
{
    if (parent == nullptr) return;

    // Match action bar height to the mapping menu bar height
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

void MappingDropdownMenu::dismiss()
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

void MappingDropdownMenu::paint(juce::Graphics& g)
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
            g.setColour(juce::Colour(0xff505050));  // Same as selected mapping
            g.fillRect(rowBounds);
        }
        else if (i == hoveredFolderIndex)
        {
            g.setColour(juce::Colour(0xff404040));
            g.fillRect(rowBounds);
        }

        // Folder name
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(14.0f));
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

    // Horizontal separator after action bar
    int sepY = actionBarHeight;
    g.fillRect(2, sepY, bounds.getWidth() - 4, 2);

    // Separator between folders and mappings
    if (!folders.empty() && !listContent.rows.empty())
    {
        sepY = actionBarHeight + kSepHeight + static_cast<int>(folders.size()) * kRowHeight;
        g.fillRect(2, sepY, bounds.getWidth() - 4, 2);
    }
}

void MappingDropdownMenu::resized()
{
    auto bounds = getLocalBounds().reduced(2); // Inset for 2px border

    // Action bar at top (3 buttons with 2px gaps for separator lines)
    actionBarArea = bounds.removeFromTop(actionBarHeight - 1);
    {
        int aw = actionBarArea.getWidth();
        int gapW = 2;
        int btnW = (aw - 2 * gapW) / 3;
        int x = actionBarArea.getX();
        int y = actionBarArea.getY();
        int h = actionBarArea.getHeight();
        saveButton.setBounds(x, y, btnW, h);
        saveAsButton.setBounds(x + btnW + gapW, y, btnW, h);
        deleteButton.setBounds(x + 2 * (btnW + gapW), y, aw - 2 * (btnW + gapW), h);
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

    // Scroll to make selected mapping visible
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

void MappingDropdownMenu::mouseDown(const juce::MouseEvent& e)
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

void MappingDropdownMenu::mouseMove(const juce::MouseEvent& e)
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

void MappingDropdownMenu::mouseExit(const juce::MouseEvent&)
{
    if (hoveredFolderIndex != -1)
    {
        hoveredFolderIndex = -1;
        repaint();
    }
}

void MappingDropdownMenu::inputAttemptWhenModal()
{
    dismiss();
}

void MappingDropdownMenu::handleFolderClick(int index)
{
    if (index < 0 || index >= static_cast<int>(folders.size()))
        return;

    auto& folder = folders[index];

    auto currentName = mappingManager.getCurrentMappingName();
    auto currentCat = mappingManager.getCurrentMappingCategory();

    std::vector<SubfolderPopup::Row> items;
    for (const auto& mapping : folder.mappings)
    {
        SubfolderPopup::Row row;
        row.name = mapping.name;
        row.category = mapping.category;
        row.isSelected = (mapping.name == currentName && mapping.category == currentCat);
        row.displayName = mapping.name;
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

    juce::Component::SafePointer<MappingDropdownMenu> safeThis(this);
    subfolderPopup->onRowClicked = [safeThis, items](int rowIdx)
    {
        if (safeThis == nullptr) return;
        if (rowIdx >= 0 && rowIdx < static_cast<int>(items.size()))
        {
            if (safeThis->onMappingSelected)
                safeThis->onMappingSelected(items[rowIdx].name, items[rowIdx].category);
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
// MappingPanel
// ============================================================================

MappingPanel::MappingPanel(MidiMappingManager& mappingMgr)
    : mappingManager(mappingMgr)
{
    setOpaque(false);
    updateMappingDisplay();
}

MappingPanel::~MappingPanel()
{
    if (mappingDropdown != nullptr)
        mappingDropdown->dismiss();

    if (fileChooser != nullptr)
        fileChooser.reset();
}

void MappingPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background — matches plugin background
    g.setColour(juce::Colour(0xff101010));
    g.fillRoundedRectangle(bounds, cornerRadius);

    // Border — thin gray outline
    g.setColour(juce::Colour(0xff595e5f));
    g.drawRoundedRectangle(bounds.reduced(borderThickness * 0.5f), cornerRadius, borderThickness);

    // Mapping name centered (no arrows - click anywhere to open menu)
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f));
    g.drawText(displayedMappingName, bounds, juce::Justification::centred);
}

void MappingPanel::resized()
{
    auto bounds = getLocalBounds();

    leftArrowZone = bounds.removeFromLeft(arrowZoneWidth);
    rightArrowZone = bounds.removeFromRight(arrowZoneWidth);
    centerZone = bounds;

    // Update LookAndFeel menu width to match our total width
    customLookAndFeel.menuWidth = getWidth();
}

void MappingPanel::mouseDown(const juce::MouseEvent& /*event*/)
{
    // Click anywhere to show mapping menu (no arrow navigation)
    showMappingMenu();
}

void MappingPanel::updateMappingDisplay()
{
    auto name = mappingManager.getCurrentMappingName();

    if (name.isEmpty())
        name = "Select...";

    displayedMappingName = name;
    repaint();
}

// ============================================================================
// Menu
// ============================================================================

void MappingPanel::showMappingMenu()
{
    // Toggle dropdown if already open
    if (mappingDropdown != nullptr)
    {
        mappingDropdown->dismiss();
        return;
    }

    // Rescan mappings before showing menu
    mappingManager.rescanUserMappings();

    mappingDropdown = std::make_unique<MappingDropdownMenu>(mappingManager, customLookAndFeel);

    // Action callback (Save/SaveAs/Delete)
    mappingDropdown->onAction = [this](int actionId) { handleMenuResult(actionId); };

    // Mapping selection callback
    mappingDropdown->onMappingSelected = [this](const juce::String& name, const juce::String& /*category*/)
    {
        mappingManager.loadMapping(name);
        updateMappingDisplay();
        if (onMappingLoaded)
            onMappingLoaded();
    };

    // Cleanup callback when dropdown is dismissed
    mappingDropdown->onDismiss = [this]() { mappingDropdown.reset(); };

    // Position dropdown below this component in top-level coordinate space
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr)
    {
        mappingDropdown.reset();
        return;
    }

    auto boundsInTopLevel = topLevel->getLocalArea(this, getLocalBounds());
    mappingDropdown->show(topLevel, boundsInTopLevel);
}

void MappingPanel::handleMenuResult(int result)
{
    switch (result)
    {
        case kSaveID:
            saveCurrentMapping();
            break;
        case kSaveAsID:
            showSaveAsDialog();
            break;
        case kDeleteID:
            deleteCurrentMapping();
            break;
        default:
            break;
    }
}

// ============================================================================
// Mapping operations
// ============================================================================

void MappingPanel::loadPreviousMapping()
{
    if (mappingManager.loadPreviousMapping())
    {
        updateMappingDisplay();
        if (onMappingLoaded)
            onMappingLoaded();
    }
}

void MappingPanel::loadNextMapping()
{
    if (mappingManager.loadNextMapping())
    {
        updateMappingDisplay();
        if (onMappingLoaded)
            onMappingLoaded();
    }
}

void MappingPanel::saveCurrentMapping()
{
    auto currentName = mappingManager.getCurrentMappingName();

    if (currentName.isEmpty())
    {
        showSaveAsDialog();
        return;
    }

    mappingManager.saveMapping(currentName);
    updateMappingDisplay();
}

void MappingPanel::showSaveAsDialog()
{
#if JUCE_IOS
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr) return;

    auto userDir = mappingManager.getUserMappingDirectory();
    if (!userDir.exists())
        userDir.createDirectory();

    auto suggestedName = mappingManager.getCurrentMappingName();
    if (suggestedName.isEmpty()) suggestedName = "NewMapping";

    mappingNameDialog = std::make_unique<MappingNameDialog>(suggestedName, userDir, &customLookAndFeel);

    juce::Component::SafePointer<MappingPanel> safeThis(this);

    mappingNameDialog->onSave = [safeThis, userDir](const juce::String& subfolder, const juce::String& name)
    {
        juce::MessageManager::callAsync([safeThis, userDir, subfolder, name]()
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;

            if (auto* top = self.getTopLevelComponent())
                top->removeChildComponent(self.mappingNameDialog.get());
            self.mappingNameDialog.reset();

            auto finalName = name.trim();
            if (finalName.isEmpty()) finalName = "NewMapping";

            auto targetDir = subfolder.isEmpty() ? userDir : userDir.getChildFile(subfolder);
            if (!targetDir.exists())
                targetDir.createDirectory();

            auto file = targetDir.getChildFile(finalName + ".xml");
            self.mappingManager.saveMappingAs(file);
            self.updateMappingDisplay();
        });
    };

    mappingNameDialog->onCancel = [safeThis]()
    {
        juce::MessageManager::callAsync([safeThis]()
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;

            if (auto* top = self.getTopLevelComponent())
                top->removeChildComponent(self.mappingNameDialog.get());
            self.mappingNameDialog.reset();
        });
    };

    topLevel->addAndMakeVisible(mappingNameDialog.get());
    {
        int dlgW = 320, dlgH = 140;
        int x = (topLevel->getWidth() - dlgW) / 2;
        int y = topLevel->getHeight() / 6;  // Upper area to avoid iOS keyboard
        mappingNameDialog->setBounds(x, y, dlgW, dlgH);
    }
    mappingNameDialog->toFront(true);
    mappingNameDialog->nameEditor.grabKeyboardFocus();
#else
    auto userDir = mappingManager.getUserMappingDirectory();
    auto suggestedName = mappingManager.getCurrentMappingName();
    if (suggestedName.isEmpty())
        suggestedName = "NewMapping";

    if (!userDir.exists())
        userDir.createDirectory();

    fileChooser = std::make_unique<juce::FileChooser>(
        "Save MIDI Mapping As",
        userDir.getChildFile(suggestedName + ".xml"),
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

            mappingManager.saveMappingAs(file);
            updateMappingDisplay();
        }
        fileChooser.reset();
    });
#endif
}

void MappingPanel::deleteCurrentMapping()
{
    auto currentName = mappingManager.getCurrentMappingName();
    if (currentName.isEmpty())
        return;

#if JUCE_IOS
    auto* topLevel = getTopLevelComponent();
    if (topLevel == nullptr) return;

    confirmDialog = std::make_unique<ConfirmDialog>(
        "Delete \"" + currentName + "\"?", "Delete");

    juce::Component::SafePointer<MappingPanel> safeThis(this);

    confirmDialog->onConfirm = [safeThis, currentName]()
    {
        juce::MessageManager::callAsync([safeThis, currentName]()
        {
            if (safeThis == nullptr) return;
            auto& self = *safeThis;

            if (auto* top = self.getTopLevelComponent())
                top->removeChildComponent(self.confirmDialog.get());
            self.confirmDialog.reset();

            self.mappingManager.deleteMapping(currentName);
            self.updateMappingDisplay();
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
        .withTitle("Delete MIDI Mapping")
        .withMessage("Are you sure you want to delete \"" + currentName + "\"?")
        .withButton("Delete")
        .withButton("Cancel");

    juce::NativeMessageBox::showAsync(options, [this, currentName](int result)
    {
        if (result == 0)  // Delete button
        {
            mappingManager.deleteMapping(currentName);
            updateMappingDisplay();
        }
    });
#endif
}
