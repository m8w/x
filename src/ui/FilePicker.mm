#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#include "FilePicker.h"

std::string pickVideoFile() {
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

std::string pickSaveFile(const std::string& suggestedName) {
    NSSavePanel* panel = [NSSavePanel savePanel];
    panel.title               = @"Save recording as";
    panel.allowedContentTypes = @[ UTTypeMPEG4Movie ];
    panel.canCreateDirectories = YES;

    // Pre-fill the filename from the suggestion (basename only)
    NSString* suggested = [NSString stringWithUTF8String:suggestedName.c_str()];
    NSString* basename  = suggested.lastPathComponent;
    // Strip .mp4 extension — NSSavePanel adds it from allowedContentTypes
    if ([basename.pathExtension.lowercaseString isEqualToString:@"mp4"])
        basename = [basename stringByDeletingPathExtension];
    panel.nameFieldStringValue = basename;

    // Pre-navigate to the directory part of the suggestion if it exists
    NSString* dir = suggested.stringByDeletingLastPathComponent;
    if (dir.length > 0)
        panel.directoryURL = [NSURL fileURLWithPath:dir];

    if ([panel runModal] == NSModalResponseOK) {
        return std::string(panel.URL.fileSystemRepresentation);
    }
    return "";
}
