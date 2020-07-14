#import "NativeProxy.h"
#include <folly/json.h>
#import <React/RCTFollyConvert.h>
#import <React/RCTUIManager.h>
#import "IOSScheduler.h"
#import "IOSErrorHandler.h"
#import <jsi/JSCRuntime.h>
#import "RuntimeDecorator.h"
#import "REAModule.h"
#import "REANodesManager.h"

namespace reanimated {

using namespace facebook;
using namespace react;

// COPIED FROM RCTTurboModule.mm
static id convertJSIValueToObjCObject(jsi::Runtime &runtime, const jsi::Value &value);

static NSString *convertJSIStringToNSString(jsi::Runtime &runtime, const jsi::String &value)
{
  return [NSString stringWithUTF8String:value.utf8(runtime).c_str()];
}

static NSDictionary *convertJSIObjectToNSDictionary(jsi::Runtime &runtime, const jsi::Object &value)
{
  jsi::Array propertyNames = value.getPropertyNames(runtime);
  size_t size = propertyNames.size(runtime);
  NSMutableDictionary *result = [NSMutableDictionary new];
  for (size_t i = 0; i < size; i++) {
    jsi::String name = propertyNames.getValueAtIndex(runtime, i).getString(runtime);
    NSString *k = convertJSIStringToNSString(runtime, name);
    id v = convertJSIValueToObjCObject(runtime, value.getProperty(runtime, name));
    if (v) {
      result[k] = v;
    }
  }
  return [result copy];
}

static NSArray *
convertJSIArrayToNSArray(jsi::Runtime &runtime, const jsi::Array &value)
{
  size_t size = value.size(runtime);
  NSMutableArray *result = [NSMutableArray new];
  for (size_t i = 0; i < size; i++) {
    // Insert kCFNull when it's `undefined` value to preserve the indices.
    [result
        addObject:convertJSIValueToObjCObject(runtime, value.getValueAtIndex(runtime, i)) ?: (id)kCFNull];
  }
  return [result copy];
}

static id convertJSIValueToObjCObject(jsi::Runtime &runtime, const jsi::Value &value)
{
  if (value.isUndefined() || value.isNull()) {
    return nil;
  }
  if (value.isBool()) {
    return @(value.getBool());
  }
  if (value.isNumber()) {
    return @(value.getNumber());
  }
  if (value.isString()) {
    return convertJSIStringToNSString(runtime, value.getString(runtime));
  }
  if (value.isObject()) {
    jsi::Object o = value.getObject(runtime);
    if (o.isArray(runtime)) {
      return convertJSIArrayToNSArray(runtime, o.getArray(runtime));
    }
    return convertJSIObjectToNSDictionary(runtime, o);
  }

  throw std::runtime_error("Unsupported jsi::jsi::Value kind");
}

std::shared_ptr<NativeReanimatedModule> createReanimatedModule(std::shared_ptr<CallInvoker> jsInvoker) {

  RCTBridge *bridge;
  if ([[UIApplication sharedApplication].delegate respondsToSelector:@selector(bridge)]) {
    bridge = [[UIApplication sharedApplication].delegate performSelector:@selector(bridge) withObject:[UIApplication sharedApplication].delegate];
  }
  REAModule *reanimatedModule = [bridge moduleForClass:[REAModule class]];

  auto propUpdater = [reanimatedModule](jsi::Runtime &rt, int viewTag, const jsi::Object &props) -> void {
    NSDictionary *propsDict = convertJSIObjectToNSDictionary(rt, props);
    [reanimatedModule.nodesManager updateProps:propsDict ofViewWithTag:[NSNumber numberWithInt:viewTag] viewName:@"RCTView"];
  };

  auto requestRender = [reanimatedModule](std::function<void(double)> onRender) {
    [reanimatedModule.nodesManager postOnAnimation:^(CADisplayLink *displayLink) {
      onRender(displayLink.timestamp * 1000.0);
    }];
  };

  std::shared_ptr<Scheduler> scheduler(new IOSScheduler(jsInvoker));
  std::unique_ptr<jsi::Runtime> animatedRuntime = facebook::jsc::makeJSCRuntime();
  std::shared_ptr<ErrorHandler> errorHandler = std::make_shared<IOSErrorHandler>(scheduler);

  std::shared_ptr<NativeReanimatedModule> module(new NativeReanimatedModule(jsInvoker,
                                                                            scheduler,
                                                                            std::move(animatedRuntime),
                                                                            requestRender,
                                                                            propUpdater,
                                                                            errorHandler));

  [reanimatedModule.nodesManager registerEventHandler:^(NSString *eventName, id<RCTEvent> event) {
    std::string eventNameString([eventName UTF8String]);
    std::string eventAsString = folly::toJson(convertIdToFollyDynamic([event arguments][2]));

    eventAsString = "{ NativeMap:"  + eventAsString + "}";
    module->onEvent(eventNameString, eventAsString);
  }];

  return module;
}

}

