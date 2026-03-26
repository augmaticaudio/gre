#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PresetManager.h"
#include <map>

// Custom LookAndFeel for preset UI styling (buttons, subfolder popups, scrollbar)
class CustomPresetLookAndFeel : public juce::LookAndFeel_V4
{
public:
    int menuWidth = 200;      // Set dynamically to match preset box width
    float menuFontSize = 14.0f; // Scaled for iPhone dropdown (matches design-space font)
    int menuRowHeight = 25;     // Scaled row height for dropdown items

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(juce::Colour(0xff101010));
        // Border to distinguish subfolder popup from app background
        g.setColour(juce::Colour(0xff595e5f));
        g.drawRect(0, 0, width, height, 2);
    }


    void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                          bool isSeparator, bool /*isActive*/, bool isHighlighted,
                          bool isTicked, bool hasSubMenu, const juce::String& text,
                          const juce::String& /*shortcutKeyText*/,
                          const juce::Drawable* /*icon*/, const juce::Colour* /*textColour*/) override
    {
        // Handle separator with horizontal line
        if (isSeparator)
        {
            g.setColour(juce::Colour(0xff101010));
            g.fillRect(area);
            g.setColour(juce::Colour(0xff555555));
            auto lineY = area.getCentreY();
            g.drawLine(static_cast<float>(area.getX()), static_cast<float>(lineY),
                      static_cast<float>(area.getRight()), static_cast<float>(lineY),
                      1.0f);
            return;
        }

        // Regular menu item — use fillRect(area) instead of fillAll to preserve border
        if (isHighlighted)
        {
            g.setColour(juce::Colour(0xff404040));
            g.fillRect(area);
        }
        else
        {
            g.setColour(juce::Colour(0xff101010));
            g.fillRect(area);
        }

        g.setColour(juce::Colours::white);

        auto textArea = area;

        // Draw tick mark if ticked (current preset indicator)
        if (isTicked)
        {
            auto tickArea = textArea.removeFromLeft(24);
            g.setFont(juce::Font(menuFontSize));
            g.drawText(juce::String::charToString((juce::juce_wchar)0x2713), // checkmark
                       tickArea.reduced(4, 0), juce::Justification::centredRight);
        }
        else
        {
            textArea.removeFromLeft(24); // Indent to align with ticked items
        }

        // Reserve space for submenu arrow — chevron-right (> shape)
        if (hasSubMenu)
        {
            auto arrowArea = textArea.removeFromRight(20);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            juce::Path chevron;
            auto cx = arrowArea.getCentreX();
            auto cy = static_cast<float>(arrowArea.getCentreY());
            float sz = 4.0f;
            chevron.startNewSubPath(cx - sz * 0.5f, cy - sz);
            chevron.lineTo(cx + sz * 0.5f, cy);
            chevron.lineTo(cx - sz * 0.5f, cy + sz);
            g.strokePath(chevron, juce::PathStrokeType(1.5f));
            g.setColour(juce::Colours::white);
        }

        // Draw text
        g.setFont(juce::Font(menuFontSize));
        g.drawText(text, textArea.reduced(4, 0), juce::Justification::centredLeft);
    }

    int getPopupMenuBorderSize() override
    {
        return 2;  // Inset items so 2px border drawn in drawPopupMenuBackground stays visible
    }

    void getIdealPopupMenuItemSize(const juce::String& /*text*/,
                                   bool isSeparator,
                                   int /*standardMenuItemHeight*/,
                                   int& idealWidth,
                                   int& idealHeight) override
    {
        idealWidth = menuWidth;
        idealHeight = isSeparator ? 8 : menuRowHeight;
    }

    // Flat button background: frame only, no hover fill change
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& /*backgroundColour*/,
                              bool /*isMouseOverButton*/, bool /*isButtonDown*/) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xff101010));
        g.fillRect(bounds);
    }

    // Match button text font size to popup menu item font
    juce::Font getTextButtonFont(juce::TextButton&, int) override
    {
        return juce::Font(menuFontSize);
    }

    // Scrollbar styling for preset list viewport
    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical,
                       int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override
    {
        // Subtle track
        g.setColour(juce::Colour(0xff101010));
        g.fillRect(x, y, width, height);

        if (thumbSize <= 0) return;

        // Thumb
        auto alpha = isMouseDown ? 0.9f : isMouseOver ? 0.7f : 0.5f;
        g.setColour(juce::Colours::white.withAlpha(alpha));
        if (isScrollbarVertical)
        {
            auto thumbBounds = juce::Rectangle<float>(
                static_cast<float>(x) + 2.0f,
                static_cast<float>(thumbStartPosition) + 1.0f,
                static_cast<float>(width) - 4.0f,
                static_cast<float>(thumbSize) - 2.0f);
            g.fillRoundedRectangle(thumbBounds, (static_cast<float>(width) - 4.0f) * 0.5f);
        }
    }

    int getDefaultScrollbarWidth() override { return 10; }

    bool areScrollbarButtonsVisible() override { return false; }

    // Scroll indicator (kept for subfolder popups that may scroll)
    void drawPopupMenuUpDownArrow(juce::Graphics& g, int width, int height, bool isScrollUpArrow) override
    {
        auto colour = juce::Colour(0xff404040);
        g.setGradientFill(juce::ColourGradient(colour, 0, isScrollUpArrow ? 0.0f : (float)height,
                                                colour.withAlpha(0.0f), 0, isScrollUpArrow ? (float)height : 0.0f,
                                                false));
        g.fillRect(0, 0, width, height);

        auto hw = (float)width * 0.5f;
        auto arrowW = (float)height * 0.3f;
        auto y1 = (float)height * (isScrollUpArrow ? 0.6f : 0.3f);
        auto y2 = (float)height * (isScrollUpArrow ? 0.3f : 0.6f);

        juce::Path p;
        p.addTriangle(hw - arrowW, y1, hw + arrowW, y1, hw, y2);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillPath(p);
    }

    // iOS AUv3: PopupMenu must render inside plugin editor, not as a separate native window
    juce::Component* getParentComponentForMenuOptions(const juce::PopupMenu::Options& options) override
    {
        if (auto* target = options.getTargetComponent())
            return target->getTopLevelComponent();
        return LookAndFeel_V4::getParentComponentForMenuOptions(options);
    }
};

/**
 * Custom popup for subfolder contents (replaces JUCE PopupMenu for subfolders).
 * Shows a scrollable list of items with guaranteed 2px border on all sides.
 * Used by both PresetDropdownMenu and MappingDropdownMenu.
 */
class SubfolderPopup : public juce::Component
{
public:
    struct Row
    {
        juce::String name;
        juce::String category;
        juce::String displayName;
        bool isSelected = false;
    };

    std::function<void(int rowIndex)> onRowClicked;
    std::function<void()> onDismissed;

    SubfolderPopup()
    {
        viewport.setViewedComponent(&listContent, false);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(10);
        viewport.setScrollOnDragMode(juce::Viewport::ScrollOnDragMode::all);
        addAndMakeVisible(viewport);

        listContent.onClicked = [this](int idx)
        {
            if (onRowClicked)
                onRowClicked(idx);
            dismiss();
        };
    }

    ~SubfolderPopup() override
    {
        viewport.setLookAndFeel(nullptr);
    }

    void show(juce::Component* parent, juce::Point<int> position,
              int width, std::vector<Row> items, CustomPresetLookAndFeel* laf)
    {
        if (parent == nullptr) return;

        rows = std::move(items);
        listContent.rowsPtr = &rows;

        // Pick up scaled font/row sizes from LookAndFeel
        int rh = kRowHeight;
        if (laf != nullptr)
        {
            viewport.setLookAndFeel(laf);
            rh = laf->menuRowHeight;
            listContent.fontSize = laf->menuFontSize;
            listContent.rowH = rh;
        }

        int rowCount = static_cast<int>(rows.size());
        int maxVisible = juce::jmin(rowCount, kMaxVisibleRows);
        int contentH = rowCount * rh;
        int viewH = maxVisible * rh;
        int totalH = viewH + 4; // 2px border top + bottom

        listContent.setSize(width - 4, juce::jmax(contentH, 1));

        int x = position.x;
        int y = position.y;

        // Ensure it fits within parent bounds
        if (x + width > parent->getWidth())
            x = position.x - width - width; // flip to left side
        if (y + totalH > parent->getHeight())
            y = parent->getHeight() - totalH - 5;
        if (y < 0) y = 0;

        setBounds(x, y, width, totalH);
        parent->addAndMakeVisible(this);
        enterModalState(false, nullptr, false);
        toFront(true);

        // Scroll to selected item
        for (int i = 0; i < rowCount; ++i)
        {
            if (rows[i].isSelected)
            {
                int rowTop = i * rh;
                int scrollY = juce::jmax(0, rowTop - viewH / 2 + rh / 2);
                viewport.setViewPosition(0, scrollY);
                break;
            }
        }
    }

    void dismiss()
    {
        exitModalState(0);
        setVisible(false);
        if (auto* p = getParentComponent())
            p->removeChildComponent(this);
        if (onDismissed)
        {
            auto cb = onDismissed;
            juce::MessageManager::callAsync(cb);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        g.setColour(juce::Colour(0xff101010));
        g.fillRoundedRectangle(bounds.toFloat(), 6.0f);
        g.setColour(juce::Colour(0xff595e5f));
        g.drawRoundedRectangle(bounds.toFloat().reduced(1.0f), 6.0f, 2.0f);
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds().reduced(2));
        // Update content width to match viewport's visible width (accounts for scrollbar)
        listContent.setSize(viewport.getMaximumVisibleWidth(), listContent.getHeight());
    }

    void inputAttemptWhenModal() override { dismiss(); }

private:
    static constexpr int kRowHeight = 25;
    static constexpr int kMaxVisibleRows = 12;

    std::vector<Row> rows;

    class ListContent : public juce::Component
    {
    public:
        const std::vector<Row>* rowsPtr = nullptr;
        int hoveredRow = -1;
        float fontSize = 14.0f;
        int rowH = 25;
        std::function<void(int)> onClicked;

        void paint(juce::Graphics& g) override
        {
            if (rowsPtr == nullptr) return;
            auto& r = *rowsPtr;
            for (int i = 0; i < static_cast<int>(r.size()); ++i)
            {
                auto rowBounds = juce::Rectangle<int>(0, i * rowH, getWidth(), rowH);
                if (r[i].isSelected)
                    g.setColour(juce::Colour(0xff505050));
                else if (i == hoveredRow)
                    g.setColour(juce::Colour(0xff404040));
                else
                    g.setColour(juce::Colour(0xff101010));
                g.fillRect(rowBounds);

                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(fontSize));
                g.drawText(r[i].displayName, rowBounds.reduced(12, 0),
                           juce::Justification::centredLeft);
            }
        }

        void mouseDown(const juce::MouseEvent& e) override
        {
            mouseDownPos = e.getPosition();
            wasDragged = false;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!wasDragged && e.getPosition().getDistanceFrom(mouseDownPos) > 8.0f)
                wasDragged = true;
        }

        void mouseUp(const juce::MouseEvent& e) override
        {
            if (!wasDragged && rowsPtr != nullptr)
            {
                int row = (rowH > 0) ? e.getPosition().y / rowH : -1;
                if (row >= 0 && row < static_cast<int>(rowsPtr->size()))
                    if (onClicked) onClicked(row);
            }
            wasDragged = false;
        }

        void mouseMove(const juce::MouseEvent& e) override
        {
            int newHover = (rowsPtr != nullptr && rowH > 0) ? e.getPosition().y / rowH : -1;
            if (rowsPtr == nullptr || newHover < 0 || newHover >= static_cast<int>(rowsPtr->size()))
                newHover = -1;
            if (newHover != hoveredRow) { hoveredRow = newHover; repaint(); }
        }

        void mouseExit(const juce::MouseEvent&) override
        {
            if (hoveredRow != -1) { hoveredRow = -1; repaint(); }
        }

    private:
        juce::Point<int> mouseDownPos;
        bool wasDragged = false;
    };

    juce::Viewport viewport;
    ListContent listContent;
};

/**
 * Custom dropdown menu replacing PopupMenu for preset selection.
 * Features: drag-to-scroll with scrollbar, current preset highlighting,
 * folder rows with chevron-right, action buttons with frame-only styling.
 */
class PresetDropdownMenu : public juce::Component
{
public:
    PresetDropdownMenu(PresetManager& presetMgr, CustomPresetLookAndFeel& laf);
    ~PresetDropdownMenu() override;

    void show(juce::Component* parent, juce::Rectangle<int> anchorBounds);
    void dismiss();

    // Callbacks
    std::function<void(int)> onAction;  // Save/SaveAs/Delete/Random action IDs
    std::function<void(const juce::String& name, const juce::String& category)> onPresetSelected;
    std::function<void()> onDismiss;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void inputAttemptWhenModal() override;

private:
    PresetManager& presetManager;
    CustomPresetLookAndFeel& customLF;

    // Action buttons
    juce::TextButton saveButton{"Save"};
    juce::TextButton saveAsButton{"Save As"};
    juce::TextButton deleteButton{"Delete"};
    juce::TextButton randomButton{"Random"};

    // Folder data
    struct FolderInfo
    {
        juce::String name;
        std::vector<PresetManager::PresetItem> presets;
    };
    std::vector<FolderInfo> folders;
    std::vector<juce::Rectangle<int>> folderRowBounds;
    int hoveredFolderIndex = -1;
    int openFolderIndex = -1;  // Folder whose submenu is currently open

    // Scrollable preset list content
    class PresetListContent : public juce::Component
    {
    public:
        struct Row
        {
            juce::String name;
            juce::String displayName;
            juce::String category;
            bool isSelected = false;
        };

        std::vector<Row> rows;
        int rowHeight = 25;
        float fontSize = 14.0f;
        int hoveredRow = -1;
        std::function<void(int)> onRowClicked;

        void paint(juce::Graphics& g) override;
        void mouseDown(const juce::MouseEvent& e) override;
        void mouseDrag(const juce::MouseEvent& e) override;
        void mouseUp(const juce::MouseEvent& e) override;
        void mouseMove(const juce::MouseEvent& e) override;
        void mouseExit(const juce::MouseEvent& e) override;

    private:
        juce::Point<int> mouseDownPos;
        bool wasDragged = false;
    };

    juce::Viewport viewport;
    PresetListContent listContent;

    // Layout constants
    int kRowHeight = 25;  // Scaled at runtime via LookAndFeel
    static constexpr int kSepHeight = 2;
    static constexpr int kMaxPresetRows = 12;
    float menuFontSize = 14.0f;  // Scaled at runtime via LookAndFeel

    // Dynamic action bar height (set from anchor bounds in show())
    int actionBarHeight = 34;

    // Computed layout areas
    juce::Rectangle<int> actionBarArea;
    juce::Rectangle<int> folderArea;

    void buildContent();
    void handleFolderClick(int index);
    int computeDesiredHeight() const;

    // Custom subfolder popup (replaces JUCE PopupMenu)
    std::unique_ptr<SubfolderPopup> subfolderPopup;
};

/**
 * @brief Compact preset box with integrated navigation arrows and dropdown menu
 *
 * Displays as a rounded rectangle with:
 * - Left arrow (<) for previous preset
 * - Center: preset name (click opens custom dropdown)
 * - Right arrow (>) for next preset
 *
 * Dropdown contains: action buttons (Save, Save As, Delete, Random),
 * folder rows with chevron-right for submenus, scrollable root-level presets
 * with drag-to-scroll and visible scrollbar.
 */
class PresetPanel : public juce::Component,
                    public juce::AudioProcessorValueTreeState::Listener
{
public:
    PresetPanel(PresetManager& presetMgr,
                juce::AudioProcessorValueTreeState& apvts);

    ~PresetPanel() override;

    // Component interface
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    // AudioProcessorValueTreeState::Listener interface
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Refresh the preset display (called externally if needed)
    void updatePresetList();

    // Set scaled font size (called from editor resized() to match R1 header font)
    void setScaledFontSize(float size) { scaledFontSize = size; repaint(); }

private:
    // References
    PresetManager& presetManager;
    juce::AudioProcessorValueTreeState& valueTreeState;

    // State tracking
    bool isLoadingPreset = false;
    juce::StringArray parameterIDs;
    juce::String displayedPresetName;

    // File chooser (must be kept alive during async operations)
    std::unique_ptr<juce::FileChooser> fileChooser;

    #if JUCE_IOS
    // iOS: Save-As dialog with folder selector and name input
    class PresetNameDialog : public juce::Component
    {
    public:
        juce::ComboBox folderSelector;
        juce::TextEditor nameEditor;
        juce::TextEditor newFolderEditor;
        juce::TextButton okButton{"Save"};
        juce::TextButton cancelButton{"Cancel"};
        std::function<void(const juce::String& subfolder, const juce::String& name)> onSave;
        std::function<void()> onCancel;

        PresetNameDialog(const juce::String& initialName, const juce::File& presetsDir, juce::LookAndFeel* lf = nullptr)
        {
            addAndMakeVisible(folderSelector);
            folderSelector.addItem("Select folder", 1);
            int itemId = 2;
            for (const auto& child : presetsDir.findChildFiles(juce::File::findDirectories, false))
            {
                folderSelector.addItem(child.getFileName(), itemId);
                folderNames.add(child.getFileName());
                itemId++;
            }
            folderSelector.addItem("+ New Folder...", kNewFolderID);
            folderSelector.setSelectedId(1);
            folderSelector.onChange = [this]()
            {
                bool showNewFolder = (folderSelector.getSelectedId() == kNewFolderID);
                newFolderEditor.setVisible(showNewFolder);
                if (showNewFolder)
                    newFolderEditor.grabKeyboardFocus();
                setSize(320, showNewFolder ? 172 : 134);
                resized();
            };

            if (lf != nullptr)
                folderSelector.setLookAndFeel(lf);

            // Style ComboBox to match dark theme
            folderSelector.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff1a1a1a));
            folderSelector.setColour(juce::ComboBox::textColourId, juce::Colours::white);
            folderSelector.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff595e5f));
            folderSelector.setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff595e5f));

            addChildComponent(newFolderEditor);
            newFolderEditor.setTextToShowWhenEmpty("Folder name...", juce::Colours::grey);
            newFolderEditor.setFont(juce::Font(16.0f));
            newFolderEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
            newFolderEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            newFolderEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff595e5f));
            newFolderEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff707070));
            newFolderEditor.setJustification(juce::Justification::centredLeft);

            addAndMakeVisible(nameEditor);
            nameEditor.setText(initialName);
            nameEditor.setFont(juce::Font(14.0f));
            nameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff1a1a1a));
            nameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white);
            nameEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff595e5f));
            nameEditor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff707070));
            nameEditor.setJustification(juce::Justification::centredLeft);

            addAndMakeVisible(okButton);
            okButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            okButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            okButton.onClick = [this]()
            {
                if (onSave)
                {
                    juce::String subfolder;
                    int selectedId = folderSelector.getSelectedId();
                    if (selectedId == kNewFolderID)
                        subfolder = newFolderEditor.getText().trim();
                    else if (selectedId > 1)
                        subfolder = folderNames[selectedId - 2];
                    onSave(subfolder, nameEditor.getText());
                }
            };

            addAndMakeVisible(cancelButton);
            cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            cancelButton.onClick = [this]() { if (onCancel) onCancel(); };

            setSize(320, 134);
        }

        ~PresetNameDialog() override
        {
            folderSelector.setLookAndFeel(nullptr);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            folderSelector.setBounds(bounds.removeFromTop(32));
            bounds.removeFromTop(6);
            if (newFolderEditor.isVisible())
            {
                newFolderEditor.setBounds(bounds.removeFromTop(32));
                bounds.removeFromTop(6);
            }
            nameEditor.setBounds(bounds.removeFromTop(32));
            bounds.removeFromTop(8);
            auto buttonArea = bounds.removeFromTop(36);
            cancelButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2 - 5));
            buttonArea.removeFromLeft(10);
            okButton.setBounds(buttonArea);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colour(0xff2d2d2d));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(juce::Colour(0xff595e5f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);
        }

    private:
        static constexpr int kNewFolderID = 9999;
        juce::StringArray folderNames;
    };

    std::unique_ptr<PresetNameDialog> presetNameDialog;

    // iOS: Simple confirmation dialog
    class ConfirmDialog : public juce::Component
    {
    public:
        juce::TextButton confirmButton;
        juce::TextButton cancelButton{"Cancel"};
        std::function<void()> onConfirm;
        std::function<void()> onCancel;

        ConfirmDialog(const juce::String& message, const juce::String& confirmText)
            : confirmButton(confirmText)
        {
            messageText = message;
            addAndMakeVisible(confirmButton);
            confirmButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            confirmButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            confirmButton.onClick = [this]() { if (onConfirm) onConfirm(); };
            addAndMakeVisible(cancelButton);
            cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            cancelButton.onClick = [this]() { if (onCancel) onCancel(); };
            setSize(300, 100);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            bounds.removeFromTop(40);
            bounds.removeFromTop(8);
            auto buttonArea = bounds.removeFromTop(36);
            cancelButton.setBounds(buttonArea.removeFromLeft(buttonArea.getWidth() / 2 - 5));
            buttonArea.removeFromLeft(10);
            confirmButton.setBounds(buttonArea);
        }

        void paint(juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();
            g.setColour(juce::Colour(0xff2d2d2d));
            g.fillRoundedRectangle(bounds, 6.0f);
            g.setColour(juce::Colour(0xff595e5f));
            g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 2.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(15.0f));
            g.drawText(messageText, getLocalBounds().reduced(10).removeFromTop(40),
                       juce::Justification::centred);
        }

    private:
        juce::String messageText;
    };

    std::unique_ptr<ConfirmDialog> confirmDialog;
    #endif

    // Menu actions
    void showPresetMenu();
    void handleMenuResult(int result);

    // Preset operations
    void loadPreviousPreset();
    void loadNextPreset();
    void saveCurrentPreset();
    void showSaveAsDialog();
    void deleteCurrentPreset();
    void randomizePreset();

    // Display
    void updatePresetDisplay();
    void setupParameterListeners();
    void removeParameterListeners();

    // Hit zones (computed in resized())
    juce::Rectangle<int> leftArrowZone;
    juce::Rectangle<int> rightArrowZone;
    juce::Rectangle<int> centerZone;

    // Menu item ID constants (used by action callbacks)
    enum MenuItemIDs
    {
        kSaveID = 1,
        kSaveAsID = 2,
        kDeleteID = 3,
        kRandomID = 4
    };

    // Style constants
    static constexpr int arrowZoneWidth = 30;
    static constexpr float cornerRadius = 6.0f;
    static constexpr float borderThickness = 2.0f;
    float scaledFontSize = 14.0f;  // Updated externally to match R1 header font

    // Custom LookAndFeel for popup menu styling
    CustomPresetLookAndFeel customLookAndFeel;

    // Custom dropdown menu (replaces PopupMenu)
    std::unique_ptr<PresetDropdownMenu> presetDropdown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetPanel)
};
