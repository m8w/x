#import <Cocoa/Cocoa.h>
#include "FilePicker.h"

std::string pickVideoFile() {
    __block std::string result;

    // Must run on the main thread
    dispatch_sync(dispatch_get_main_queue(), ^{
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        panel.title                = @"Choose a video file";
        panel.canChooseFiles       = YES;
        panel.canChooseDirectories = NO;
        panel.allowsMultipleSelection = NO;
        panel.allowedContentTypes  = @[
            [UTType typeWithIdentifier:@"public.movie"],
            [UTType typeWithIdentifier:@"public.mpeg-4"],
            [UTType typeWithIdentifier:@"com.apple.quicktime-movie"],
            [UTType typeWithIdentifier:@"public.avi"],
            [UTType typeWithIdentifier:@"org.matroska.mkv"],
        ];

        if ([panel runModal] == NSModalResponseOK) {
            NSURL* url = panel.URL;
            result = url.fileSystemRepresentation;
        }
    });

    return result;
}
