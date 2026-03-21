#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "FilePicker.h"

std::string pickVideoFile() {
    // We are already on the main thread (GLFW render loop).
    // Call NSOpenPanel directly — no dispatch_sync needed.
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    panel.title                   = @"Choose a video file";
    panel.canChooseFiles          = YES;
    panel.canChooseDirectories    = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedContentTypes     = @[
        UTTypeMovie,
        UTTypeMPEG4Movie,
        UTTypeQuickTimeMovie,
        [UTType typeWithFilenameExtension:@"mkv"],
        [UTType typeWithFilenameExtension:@"avi"],
        [UTType typeWithFilenameExtension:@"webm"],
    ];

    if ([panel runModal] == NSModalResponseOK) {
        return std::string(panel.URL.fileSystemRepresentation);
    }
    return "";
}
