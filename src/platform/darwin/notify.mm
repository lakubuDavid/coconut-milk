#include "notify.h"

#import <Cocoa/Cocoa.h>
#include <string>

namespace coconut::notify {

bool platformNotify(const std::string& title, const std::string& body) {
  if (title.empty() && body.empty()) return false;

  @try {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    NSUserNotification* note = [[NSUserNotification alloc] init];
    if (!title.empty())
      note.title = [NSString stringWithUTF8String:title.c_str()];
    if (!body.empty())
      note.informativeText = [NSString stringWithUTF8String:body.c_str()];
    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:note];
#pragma clang diagnostic pop
    return true;
  } @catch (NSException*) {
    return false;
  }
}

} // namespace coconut::notify