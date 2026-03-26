#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "../MidiMappingManager.h"
#include "PresetPanel.h"  // For CustomPresetLookAndFeel (shared styling)
#include <map>

/**
 * Custom dropdown menu for MIDI mapping selection.
 * Mirrors PresetDropdownMenu design: drag-to-scroll with scrollbar,
 * current mapping highlighting, folder rows with chevron-right,
 * action buttons (Save, Save As, Delete) with frame-only styling.
 */
class MappingDropdownMenu : public juce::Component
{
public:
    MappingDropdownMenu(MidiMappingManager& mappingMgr, CustomPresetLookAndFeel& laf);
    ~MappingDropdownMenu() override;

    void show(juce::Component* parent, juce::Rectangle<int> anchorBounds);
    void dismiss();

    // Callbacks
    std::function<void(int)> onAction;  // Save/SaveAs/Delete action IDs
    std::function<void(const juce::String& name, const juce::String& category)> onMappingSelected;
    std::function<void()> onDismiss;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void inputAttemptWhenModal() override;

private:
    MidiMappingManager& mappingManager;
    CustomPresetLookAndFeel& customLF;

    // Action buttons (3 — no Random for mappings)
    juce::TextButton saveButton{"Save"};
    juce::TextButton saveAsButton{"Save As"};
    juce::TextButton deleteButton{"Delete"};

    // Folder data
    struct FolderInfo
    {
        juce::String name;
        std::vector<MidiMappingManager::MappingItem> mappings;
    };
    std::vector<FolderInfo> folders;
    std::vector<juce::Rectangle<int>> folderRowBounds;
    int hoveredFolderIndex = -1;
    int openFolderIndex = -1;  // Folder whose submenu is currently open

    // Scrollable mapping list content
    class MappingListContent : public juce::Component
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
    MappingListContent listContent;

    // Layout constants (matching PresetDropdownMenu)
    static constexpr int kRowHeight = 25;
    static constexpr int kSepHeight = 2;
    static constexpr int kMaxMappingRows = 12;

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
 * Compact mapping box with integrated navigation arrows and dropdown menu.
 * Mirrors PresetPanel design:
 * - Left arrow (<) for previous mapping
 * - Center: mapping name (click opens custom dropdown)
 * - Right arrow (>) for next mapping
 *
 * Dropdown contains: action buttons (Save, Save As, Delete),
 * folder rows with chevron-right for submenus, scrollable root-level
 * mappings with drag-to-scroll and visible scrollbar.
 */
class MappingPanel : public juce::Component
{
public:
    MappingPanel(MidiMappingManager& mappingMgr);
    ~MappingPanel() override;

    // Component interface
    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

    // Refresh the mapping display (called externally if needed)
    void updateMappingDisplay();

    // Callback fired after a mapping is loaded (used by MIDITabComponent to refresh displays)
    std::function<void()> onMappingLoaded;

private:
    // References
    MidiMappingManager& mappingManager;

    // File chooser (must be kept alive during async operations)
    std::unique_ptr<juce::FileChooser> fileChooser;

    // Display state
    juce::String displayedMappingName;

#if JUCE_IOS
    // iOS: Save-As dialog with folder selector (native FileChooser broken in AUv3)
    class MappingNameDialog : public juce::Component
    {
    public:
        juce::ComboBox folderSelector;
        juce::TextEditor nameEditor;
        juce::TextEditor newFolderEditor;
        juce::TextButton okButton{"Save"};
        juce::TextButton cancelButton{"Cancel"};
        std::function<void(const juce::String& subfolder, const juce::String& name)> onSave;
        std::function<void()> onCancel;

        MappingNameDialog(const juce::String& initialName, const juce::File& mappingsDir,
                          juce::LookAndFeel* lf = nullptr)
        {
            addAndMakeVisible(folderSelector);
            folderSelector.addItem("Select folder", 1);
            int itemId = 2;
            for (const auto& child : mappingsDir.findChildFiles(juce::File::findDirectories, false))
            {
                folderSelector.addItem(child.getFileName(), itemId);
                folderNames.add(child.getFileName());
                itemId++;
            }
            folderSelector.addItem("+ New Folder...", kNewFolderID);
            folderSelector.setSelectedId(1);
            folderSelector.onChange = [this]()
            {
                bool showNew = (folderSelector.getSelectedId() == kNewFolderID);
                newFolderEditor.setVisible(showNew);
                if (showNew) newFolderEditor.grabKeyboardFocus();
                setSize(320, showNew ? 178 : 140);
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
                    int sel = folderSelector.getSelectedId();
                    if (sel == kNewFolderID)
                        subfolder = newFolderEditor.getText().trim();
                    else if (sel > 1)
                        subfolder = folderNames[sel - 2];
                    onSave(subfolder, nameEditor.getText());
                }
            };

            addAndMakeVisible(cancelButton);
            cancelButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff2d2d2d));
            cancelButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            cancelButton.onClick = [this]() { if (onCancel) onCancel(); };

            setSize(320, 140);
        }

        ~MappingNameDialog() override
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

    std::unique_ptr<MappingNameDialog> mappingNameDialog;

    // iOS: Simple confirmation dialog (NativeMessageBox broken in AUv3)
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
    void showMappingMenu();
    void handleMenuResult(int result);

    // Mapping operations
    void loadPreviousMapping();
    void loadNextMapping();
    void saveCurrentMapping();
    void showSaveAsDialog();
    void deleteCurrentMapping();

    // Hit zones (computed in resized())
    juce::Rectangle<int> leftArrowZone;
    juce::Rectangle<int> rightArrowZone;
    juce::Rectangle<int> centerZone;

    // Menu item ID constants
    enum MenuItemIDs
    {
        kSaveID = 1,
        kSaveAsID = 2,
        kDeleteID = 3
    };

    // Style constants (matching PresetPanel)
    static constexpr int arrowZoneWidth = 30;
    static constexpr float cornerRadius = 6.0f;
    static constexpr float borderThickness = 2.0f;

    // Custom LookAndFeel for dropdown styling
    CustomPresetLookAndFeel customLookAndFeel;

    // Custom dropdown menu (replaces PopupMenu)
    std::unique_ptr<MappingDropdownMenu> mappingDropdown;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingPanel)
};
