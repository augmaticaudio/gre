#include "NativeTitleBar_mac.h"

#if JUCE_MAC
#import <Cocoa/Cocoa.h>
#import <objc/runtime.h>

// ─── Custom About Panel Helper ───────────────────────────────────────────────
// Manages a custom About panel shown when the user clicks "About" in the app menu.
// The panel is independent of JUCE's window system — no JUCE peers are affected.

@interface AugmaticAboutHelper : NSObject
@property (nonatomic, strong) NSPanel* aboutPanel;
+ (instancetype)shared;
- (void)showAboutPanel;
- (void)closePanel:(id)sender;
- (void)openWebsite:(id)sender;
@end

@implementation AugmaticAboutHelper

+ (instancetype)shared
{
    static AugmaticAboutHelper* instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{ instance = [[self alloc] init]; });
    return instance;
}

- (void)showAboutPanel
{
    // If panel already visible, bring to front
    if (self.aboutPanel && [self.aboutPanel isVisible])
    {
        [self.aboutPanel makeKeyAndOrderFront:nil];
        return;
    }

    CGFloat w = 320;
    CGFloat h = 420;

    NSPanel* panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, w, h)
                                                styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable)
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    panel.title = @"About Augmatic GRE";
    panel.releasedWhenClosed = NO;

    NSView* cv = panel.contentView;
    CGFloat y = h - 20;

    // App icon (64x64, centered)
    CGFloat iconSize = 64;
    y -= iconSize;
    NSImageView* iconView = [[NSImageView alloc] initWithFrame:
        NSMakeRect((w - iconSize) / 2, y, iconSize, iconSize)];
    iconView.image = [NSApp applicationIconImage];
    iconView.imageScaling = NSImageScaleProportionallyUpOrDown;
    [cv addSubview:iconView];

    // "Augmatic GRE"
    y -= 54;
    NSTextField* nameLabel = [NSTextField labelWithString:@"Augmatic GRE"];
    nameLabel.frame = NSMakeRect(0, y, w, 24);
    nameLabel.alignment = NSTextAlignmentCenter;
    nameLabel.font = [NSFont boldSystemFontOfSize:16];
    [cv addSubview:nameLabel];

    // "Version: X.X.XXX"
    y -= 48;
    NSString* version = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:@"CFBundleShortVersionString"];
    if (version == nil) version = @"";
    NSTextField* versionLabel = [NSTextField labelWithString:
        [NSString stringWithFormat:@"Version: %@", version]];
    versionLabel.frame = NSMakeRect(0, y, w, 18);
    versionLabel.alignment = NSTextAlignmentCenter;
    versionLabel.font = [NSFont systemFontOfSize:12];
    versionLabel.textColor = [NSColor secondaryLabelColor];
    [cv addSubview:versionLabel];

    // Credit lines
    y -= 24;
    NSArray<NSString*>* lines = @[
// REDACTED
        @"100% Vibe Coded",
        @"100% Open Source",
        @"",
        @"Based on the Mutable Instruments",
        @"Grids Eurorack module"
    ];

    for (NSString* line in lines)
    {
        if (line.length == 0) { y -= 8; continue; }
        NSTextField* label = [NSTextField labelWithString:line];
        label.frame = NSMakeRect(0, y, w, 16);
        label.alignment = NSTextAlignmentCenter;
        label.font = [NSFont systemFontOfSize:12];
        [cv addSubview:label];
        y -= 20;
    }

    // Website link (purple, clickable)
    y -= 12;
    NSButton* linkButton = [NSButton buttonWithTitle:@"augmaticaudio.com/gre"
                                              target:nil
                                              action:nil];
    linkButton.bordered = NO;
    NSDictionary* linkAttrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:12],
        NSForegroundColorAttributeName: [NSColor colorWithRed:160.0/255.0
                                                        green:100.0/255.0
                                                         blue:220.0/255.0
                                                        alpha:1.0],
        NSUnderlineStyleAttributeName: @(NSUnderlineStyleSingle)
    };
    linkButton.attributedTitle = [[NSAttributedString alloc]
        initWithString:@"augmaticaudio.com/gre" attributes:linkAttrs];
    linkButton.frame = NSMakeRect(0, y, w, 18);
    linkButton.target = self;
    linkButton.action = @selector(openWebsite:);
    [cv addSubview:linkButton];

    // OK button (centered at bottom)
    NSButton* okButton = [NSButton buttonWithTitle:@"OK"
                                            target:self
                                            action:@selector(closePanel:)];
    okButton.frame = NSMakeRect((w - 80) / 2, 16, 80, 30);
    okButton.keyEquivalent = @"\r";
    [cv addSubview:okButton];

    self.aboutPanel = panel;
    [panel center];
    [panel makeKeyAndOrderFront:nil];
}

- (void)closePanel:(id)sender
{
    [self.aboutPanel close];
    self.aboutPanel = nil;
}

- (void)openWebsite:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:
        [NSURL URLWithString:@"https://augmaticaudio.com/gre/"]];
}

@end

// ─── Reapply native window styling (used after menu modification) ────────────

static void reapplyNativeWindowStyling()
{
    for (NSWindow* window in [NSApp windows])
    {
        // Skip our own About panel and any utility windows
        if ([window isKindOfClass:[NSPanel class]])
            continue;

        window.titlebarAppearsTransparent = YES;
        window.titleVisibility = NSWindowTitleHidden;
        window.backgroundColor = [NSColor colorWithRed:16.0/255.0
                                                 green:16.0/255.0
                                                  blue:16.0/255.0
                                                 alpha:1.0];
    }
}

// ─── Deferred About menu item injection ──────────────────────────────────────
// Adds the "About Augmatic GRE" item to the existing app menu AFTER JUCE has
// fully initialized (1s delay from +load). If the menu modification triggers
// JUCE's peer recreation, the native window styling is reapplied immediately
// and again after a short delay to catch async recreation.

static void addAboutMenuItemAndReapplyStyling()
{
    NSMenu* mainMenu = [NSApp mainMenu];
    if (mainMenu == nil || mainMenu.numberOfItems == 0)
        return;

    NSMenu* appMenu = [[mainMenu itemAtIndex:0] submenu];
    if (appMenu == nil)
        return;

    // Don't add if About already exists
    for (NSMenuItem* item in appMenu.itemArray)
        if ([item.title localizedCaseInsensitiveContainsString:@"About"])
            return;

    // Add About item — action uses responder chain → NSApp → swizzled method
    NSMenuItem* aboutItem = [[NSMenuItem alloc]
        initWithTitle:@"About Augmatic GRE"
        action:@selector(orderFrontStandardAboutPanel:)
        keyEquivalent:@""];
    [appMenu insertItem:aboutItem atIndex:0];
    [appMenu insertItem:[NSMenuItem separatorItem] atIndex:1];

    // Reapply native styling immediately (in case menu change triggered peer recreation)
    reapplyNativeWindowStyling();

    // Reapply again after short delays to catch async peer recreation
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.1 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{ reapplyNativeWindowStyling(); });
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.5 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{ reapplyNativeWindowStyling(); });
}

// ─── Method Swizzle + Deferred Menu Setup ────────────────────────────────────
// +load runs at class load time, before main() — before JUCE initializes.
// 1. Swizzles orderFrontStandardAboutPanel: to show our custom About panel
// 2. Schedules deferred menu item addition (1s) to avoid interfering with
//    JUCE's window initialization

@implementation NSApplication (AugmaticGREAbout)

+ (void)load
{
    // Only activate in our standalone app — not in DAW hosts loading the AUv3
    NSString* bundleId = [[NSBundle mainBundle] bundleIdentifier];
// REDACTED
        return;

    // Swizzle the About panel action
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        Method original = class_getInstanceMethod(self,
            @selector(orderFrontStandardAboutPanel:));
        Method custom = class_getInstanceMethod(self,
            @selector(augmaticGRE_orderFrontStandardAboutPanel:));
        method_exchangeImplementations(original, custom);
    });

    // Defer menu item addition until JUCE has fully initialized and the window is settled
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
            addAboutMenuItemAndReapplyStyling();
        });
}

- (void)augmaticGRE_orderFrontStandardAboutPanel:(id)sender
{
    [[AugmaticAboutHelper shared] showAboutPanel];
}

@end

// ─── Public Functions ────────────────────────────────────────────────────────

void applyNativeTitleBarStyle(juce::Component* topLevelComponent)
{
    if (auto* peer = topLevelComponent->getPeer())
    {
        NSView* nsView = (__bridge NSView*)peer->getNativeHandle();
        NSWindow* nsWindow = [nsView window];

        if (nsWindow == nil)
            return;

        // Transparent title bar — blends with window background color
        nsWindow.titlebarAppearsTransparent = YES;

        // Hide title text (app name visible in Dock)
        nsWindow.titleVisibility = NSWindowTitleHidden;

        // Match app background color (0xff101010 = RGB 16,16,16)
        nsWindow.backgroundColor = [NSColor colorWithRed:16.0/255.0
                                                   green:16.0/255.0
                                                    blue:16.0/255.0
                                                   alpha:1.0];
    }
}

#endif
