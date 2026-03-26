#include "DocumentsBookmarkManager.h"

#if JUCE_IOS

#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

static NSString* const kBookmarkKey = @"documentsBookmark";
// REDACTED

// ============================================================================
// DocumentPickerDelegate — handles UIDocumentPickerViewController callbacks
// ============================================================================
@interface DocumentPickerDelegate : NSObject <UIDocumentPickerDelegate>
@property (nonatomic, copy) void (^completionHandler)(NSURL* _Nullable url);
@end

@implementation DocumentPickerDelegate

- (void)documentPicker:(UIDocumentPickerViewController*)controller
    didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    if (self.completionHandler && urls.count > 0)
        self.completionHandler(urls.firstObject);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
    if (self.completionHandler)
        self.completionHandler(nil);
}

@end

// ============================================================================
// Pimpl implementation — hides all Objective-C types from the C++ header
// ============================================================================
class DocumentsBookmarkManager::Impl
{
public:
    Impl()
    {
        resolveStoredBookmark();
    }

    ~Impl()
    {
        if (accessingResource && resolvedURL != nil)
            [resolvedURL stopAccessingSecurityScopedResource];
    }

    bool hasValidBookmark() const
    {
        return resolvedURL != nil;
    }

    juce::File getResolvedDocumentsRoot() const
    {
        // Standalone app: direct Documents access (no bookmark needed)
        if (!juce::SystemStats::isRunningInAppExtensionSandbox())
            return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

        // AUv3: use bookmark-resolved URL if available
        if (resolvedURL != nil)
            return juce::File(juce::String([resolvedURL.path UTF8String]));

        // AUv3 without bookmark: empty (caller falls back to App Group)
        return {};
    }

    void requestBookmark(juce::Component* parentComponent,
                         std::function<void(bool)> callback)
    {
        if (parentComponent == nullptr || parentComponent->getPeer() == nullptr)
        {
            if (callback) callback(false);
            return;
        }

        // Get the UIViewController from the JUCE component
        UIView* view = (__bridge UIView*) parentComponent->getPeer()->getNativeHandle();
        UIViewController* vc = findViewController(view);

        if (vc == nil)
        {
            if (callback) callback(false);
            return;
        }

        // Create folder picker for the Documents directory
        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc]
                initForOpeningContentTypes:@[UTTypeFolder]];

        picker.allowsMultipleSelection = NO;

        // Pre-navigate to the standalone app's Documents directory
        // This is the "On My iPad / Augmatic GRE" folder
        {
            // The standalone app's Documents directory is at a known container path
            // We can suggest it via directoryURL, but the user may navigate elsewhere
            NSString* docsPath = [NSSearchPathForDirectoriesInDomains(
                NSDocumentDirectory, NSUserDomainMask, YES) firstObject];
            if (docsPath != nil)
                picker.directoryURL = [NSURL fileURLWithPath:docsPath];
        }

        // Retain the delegate for the duration of the picker
        DocumentPickerDelegate* delegate = [[DocumentPickerDelegate alloc] init];
        pickerDelegate = delegate;  // prevent ARC deallocation

        auto* implPtr = this;
        delegate.completionHandler = ^(NSURL* _Nullable url) {
            if (url != nil)
            {
                // Begin accessing the security-scoped resource
                [url startAccessingSecurityScopedResource];

                // Create bookmark data
                NSError* error = nil;
                NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationMinimalBookmark
                                     includingResourceValuesForKeys:nil
                                                      relativeToURL:nil
                                                              error:&error];

                if (bookmarkData != nil)
                {
                    // Store in shared UserDefaults (accessible by both standalone and AUv3)
                    NSUserDefaults* shared = [[NSUserDefaults alloc] initWithSuiteName:kAppGroupID];
                    if (shared != nil)
                        [shared setObject:bookmarkData forKey:kBookmarkKey];

                    // Update internal state
                    implPtr->resolvedURL = url;
                    implPtr->accessingResource = YES;

                    if (callback) callback(true);
                }
                else
                {
                    [url stopAccessingSecurityScopedResource];
                    DBG("Bookmark creation failed: " + juce::String([[error localizedDescription] UTF8String]));
                    if (callback) callback(false);
                }
            }
            else
            {
                // User cancelled
                if (callback) callback(false);
            }

            // Release delegate reference
            implPtr->pickerDelegate = nil;
        };

        picker.delegate = delegate;
        [vc presentViewController:picker animated:YES completion:nil];
    }

private:
    NSURL* resolvedURL = nil;
    BOOL accessingResource = NO;
    DocumentPickerDelegate* pickerDelegate = nil;  // prevent ARC deallocation

    void resolveStoredBookmark()
    {
        NSUserDefaults* shared = [[NSUserDefaults alloc] initWithSuiteName:kAppGroupID];
        if (shared == nil)
        {
            DBG("App Group NSUserDefaults unavailable — check entitlements for " + juce::String([kAppGroupID UTF8String]));
            return;
        }

        NSData* bookmarkData = [shared dataForKey:kBookmarkKey];

        if (bookmarkData == nil)
            return;

        BOOL isStale = NO;
        NSError* error = nil;
        NSURL* url = [NSURL URLByResolvingBookmarkData:bookmarkData
                                               options:0
                                         relativeToURL:nil
                                   bookmarkDataIsStale:&isStale
                                                 error:&error];

        if (url == nil)
        {
            DBG("Bookmark resolution failed: " + juce::String([[error localizedDescription] UTF8String]));
            // Clear stale bookmark so user can re-grant access
            [shared removeObjectForKey:kBookmarkKey];
            return;
        }

        if (isStale)
        {
            // Bookmark is stale — regenerate immediately
            NSData* fresh = [url bookmarkDataWithOptions:NSURLBookmarkCreationMinimalBookmark
                           includingResourceValuesForKeys:nil
                                            relativeToURL:nil
                                                    error:nil];
            if (fresh != nil)
            {
                [shared setObject:fresh forKey:kBookmarkKey];
            }
        }

        // Start accessing the resource
        accessingResource = [url startAccessingSecurityScopedResource];
        if (accessingResource)
            resolvedURL = url;
    }

    static UIViewController* findViewController(UIView* view)
    {
        UIResponder* responder = view;
        while (responder != nil)
        {
            if ([responder isKindOfClass:[UIViewController class]])
                return (UIViewController*)responder;
            responder = [responder nextResponder];
        }
        return nil;
    }
};

// ============================================================================
// DocumentsBookmarkManager — public C++ API delegates to Impl
// ============================================================================

DocumentsBookmarkManager::DocumentsBookmarkManager()
    : pimpl(std::make_unique<Impl>()) {}

DocumentsBookmarkManager::~DocumentsBookmarkManager() = default;

bool DocumentsBookmarkManager::hasValidBookmark() const
{
    return pimpl->hasValidBookmark();
}

void DocumentsBookmarkManager::requestBookmark(juce::Component* parentComponent,
                                                std::function<void(bool)> callback)
{
    pimpl->requestBookmark(parentComponent, std::move(callback));
}

juce::File DocumentsBookmarkManager::getResolvedDocumentsRoot() const
{
    return pimpl->getResolvedDocumentsRoot();
}

#else // !JUCE_IOS

// ============================================================================
// Stub implementation for non-iOS platforms (macOS, Windows, Linux)
// ============================================================================

class DocumentsBookmarkManager::Impl {};

DocumentsBookmarkManager::DocumentsBookmarkManager() : pimpl(nullptr) {}
DocumentsBookmarkManager::~DocumentsBookmarkManager() = default;

bool DocumentsBookmarkManager::hasValidBookmark() const { return false; }

void DocumentsBookmarkManager::requestBookmark(juce::Component*, std::function<void(bool)> cb)
{
    if (cb) cb(false);
}

juce::File DocumentsBookmarkManager::getResolvedDocumentsRoot() const { return {}; }

#endif // JUCE_IOS
