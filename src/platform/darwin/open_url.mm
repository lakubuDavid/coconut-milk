#include "open_url.h"

#import <Cocoa/Cocoa.h>
#include <string>

namespace coconut::open_url {

bool platformOpenUrl(const std::string& url) {
  if (url.empty()) return false;

  @try {
    NSString* nsurl = [NSString stringWithUTF8String:url.c_str()];
    if (!nsurl) return false;

    NSURL* parsed = [NSURL URLWithString:nsurl];
    if (!parsed) return false;

    return [[NSWorkspace sharedWorkspace] openURL:parsed];
  } @catch (NSException*) {
    return false;
  }
}

} // namespace coconut::open_url