#include "clipboard.h"

#import <Cocoa/Cocoa.h>
#include <string>

namespace coconut::clipboard {

std::string platformReadText() {
  @try {
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    NSString* str = [pb stringForType:NSPasteboardTypeString];
    return str ? std::string([str UTF8String]) : std::string();
  } @catch (NSException*) {
    return {};
  }
}

bool platformWriteText(const std::string& text) {
  @try {
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    NSString* str = [NSString stringWithUTF8String:text.c_str()];
    if (!str) return false;
    [pb clearContents];
    return [pb setString:str forType:NSPasteboardTypeString];
  } @catch (NSException*) {
    return false;
  }
}

} // namespace coconut::clipboard