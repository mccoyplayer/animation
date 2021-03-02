#include "REAIOSLogger.h"
#include "Common/cpp/hidden_headers/Logger.h"
#import <Foundation/Foundation.h>

namespace reanimated {

std::unique_ptr<LoggerInterface> Logger::instance = std::unique_ptr<REAIOSLogger>(new REAIOSLogger());

void REAIOSLogger::log(const char* str) {
  NSLog(@"%@", [NSString stringWithCString:str encoding:[NSString defaultCStringEncoding]]);
}

void REAIOSLogger::log(double d) {
  NSLog(@"%lf", d);
}

void REAIOSLogger::log(int i) {
   NSLog(@"%i", i);
}

void REAIOSLogger::log(bool b) {
  const char* str = (b)? "true" : "false";
  NSLog(@"%@", [NSString stringWithCString:str encoding:[NSString defaultCStringEncoding]]);
}

}
