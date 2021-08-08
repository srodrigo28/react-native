/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <React/RCTDefines.h>

@class RCTBridge;
@protocol RCTBridgeMethod;
@class RCTModuleRegistry;
@class RCTViewRegistry;
@class RCTBundleManager;
@class RCTCallableJSModules;

/**
 * The type of a block that is capable of sending a response to a bridged
 * operation. Use this for returning callback methods to JS.
 */
typedef void (^RCTResponseSenderBlock)(NSArray *response);

/**
 * The type of a block that is capable of sending an error response to a
 * bridged operation. Use this for returning error information to JS.
 */
typedef void (^RCTResponseErrorBlock)(NSError *error);

/**
 * Block that bridge modules use to resolve the JS promise waiting for a result.
 * Nil results are supported and are converted to JS's undefined value.
 */
typedef void (^RCTPromiseResolveBlock)(id result);

/**
 * Block that bridge modules use to reject the JS promise waiting for a result.
 * The error may be nil but it is preferable to pass an NSError object for more
 * precise error messages.
 */
typedef void (^RCTPromiseRejectBlock)(NSString *code, NSString *message, NSError *error);

/**
 * This constant can be returned from +methodQueue to force module
 * methods to be called on the JavaScript thread. This can have serious
 * implications for performance, so only use this if you're sure it's what
 * you need.
 *
 * NOTE: RCTJSThread is not a real libdispatch queue
 */
RCT_EXTERN dispatch_queue_t RCTJSThread;

RCT_EXTERN_C_BEGIN

typedef struct RCTMethodInfo {
  const char *const jsName;
  const char *const objcName;
  const BOOL isSync;
} RCTMethodInfo;

RCT_EXTERN_C_END

/**
 * Provides the interface needed to register a bridge module.
 */
@protocol RCTBridgeModule <NSObject>

/**
 * Place this macro in your class implementation to automatically register
 * your module with the bridge when it loads. The optional js_name argument
 * will be used as the JS module name. If omitted, the JS module name will
 * match the Objective-C class name.
 */
#define RCT_EXPORT_MODULE(js_name)          \
  RCT_EXTERN void RCTRegisterModule(Class); \
  +(NSString *)moduleName                   \
  {                                         \
    return @ #js_name;                      \
  }                                         \
  +(void)load                               \
  {                                         \
    RCTRegisterModule(self);                \
  }

/**
 * Same as RCT_EXPORT_MODULE, but uses __attribute__((constructor)) for module
 * registration. Useful for registering swift classes that forbids use of load
 * Used in RCT_EXTERN_REMAP_MODULE
 */
#define RCT_EXPORT_MODULE_NO_LOAD(js_name, objc_name)                           \
  RCT_EXTERN void RCTRegisterModule(Class);                                     \
  +(NSString *)moduleName                                                       \
  {                                                                             \
    return @ #js_name;                                                          \
  }                                                                             \
  __attribute__((constructor)) static void RCT_CONCAT(initialize_, objc_name)() \
  {                                                                             \
    RCTRegisterModule([objc_name class]);                                       \
  }

/**
 * To improve startup performance users may want to generate their module lists
 * at build time and hook the delegate to merge with the runtime list. This
 * macro takes the place of the above for those cases by omitting the +load
 * generation.
 *
 */
#define RCT_EXPORT_PRE_REGISTERED_MODULE(js_name) \
  +(NSString *)moduleName                         \
  {                                               \
    return @ #js_name;                            \
  }

// Implemented by RCT_EXPORT_MODULE
+ (NSString *)moduleName;

@optional

/**
 * A reference to the RCTModuleRegistry. Useful for modules that require access
 * to other NativeModules. To implement this in your module, just add `@synthesize
 * moduleRegistry = _moduleRegistry;`. If using Swift, add
 * `@objc var moduleRegistry: RCTModuleRegistry!` to your module.
 */
@property (nonatomic, weak, readonly) RCTModuleRegistry *moduleRegistry;

/**
 * A reference to the RCTViewRegistry. Useful for modules that query UIViews,
 * given a react tag. This API is deprecated, and only exists to help migrate
 * NativeModules to Venice.
 *
 * To implement this in your module, just add `@synthesize
 * viewRegistry_DEPRECATED = _viewRegistry_DEPRECATED;`. If using Swift, add
 * `@objc var viewRegistry_DEPRECATED: RCTViewRegistry!` to your module.
 */
@property (nonatomic, weak, readwrite) RCTViewRegistry *viewRegistry_DEPRECATED;

/**
 * A reference to the RCTBundleManager. Useful for modules that need to read
 * or write to the app's bundle URL.
 *
 * To implement this in your module, just add `@synthesize bundleManager =
 * _bundleManager;`. If using Swift, add `@objc var bundleManager:
 * RCTBundleManager!` to your module.
 */
@property (nonatomic, weak, readwrite) RCTBundleManager *bundleManager;

/**
 * A reference to an RCTCallableJSModules. Useful for modules that need to
 * call into methods on JavaScript modules registered as callable with
 * React Native.
 *
 * To implement this in your module, just add `@synthesize callableJSModules =
 * _callableJSModules;`. If using Swift, add `@objc var callableJSModules:
 * RCTCallableJSModules!` to your module.
 */
@property (nonatomic, weak, readwrite) RCTCallableJSModules *callableJSModules;

/**
 * A reference to the RCTBridge. Useful for modules that require access
 * to bridge features, such as sending events or making JS calls. This
 * will be set automatically by the bridge when it initializes the module.
 * To implement this in your module, just add `@synthesize bridge = _bridge;`
 * If using Swift, add `@objc var bridge: RCTBridge!` to your module.
 */
@property (nonatomic, weak, readonly) RCTBridge *bridge;

/**
 * The queue that will be used to call all exported methods. If omitted, this
 * will call on a default background queue, which is avoids blocking the main
 * thread.
 *
 * If the methods in your module need to interact with UIKit methods, they will
 * probably need to call those on the main thread, as most of UIKit is main-
 * thread-only. You can tell React Native to call your module methods on the
 * main thread by returning a reference to the main queue, like this:
 *
 * - (dispatch_queue_t)methodQueue
 * {
 *   return dispatch_get_main_queue();
 * }
 *
 * If you don't want to specify the queue yourself, but you need to use it
 * inside your class (e.g. if you have internal methods that need to dispatch
 * onto that queue), you can just add `@synthesize methodQueue = _methodQueue;`
 * and the bridge will populate the methodQueue property for you automatically
 * when it initializes the module.
 */
@property (nonatomic, strong, readonly) dispatch_queue_t methodQueue;

/**
 * Wrap the parameter line of your method implementation with this macro to
 * expose it to JS. By default the exposed method will match the first part of
 * the Objective-C method selector name (up to the first colon). Use
 * RCT_REMAP_METHOD to specify the JS name of the method.
 *
 * For example, in ModuleName.m:
 *
 * - (void)doSomething:(NSString *)aString withA:(NSInteger)a andB:(NSInteger)b
 * { ... }
 *
 * becomes
 *
 * RCT_EXPORT_METHOD(doSomething:(NSString *)aString
 *                   withA:(NSInteger)a
 *                   andB:(NSInteger)b)
 * { ... }
 *
 * and is exposed to JavaScript as `NativeModules.ModuleName.doSomething`.
 *
 * ## Promises
 *
 * Bridge modules can also define methods that are exported to JavaScript as
 * methods that return a Promise, and are compatible with JS async functions.
 *
 * Declare the last two parameters of your native method to be a resolver block
 * and a rejecter block. The resolver block must precede the rejecter block.
 *
 * For example:
 *
 * RCT_EXPORT_METHOD(doSomethingAsync:(NSString *)aString
 *                           resolver:(RCTPromiseResolveBlock)resolve
 *                           rejecter:(RCTPromiseRejectBlock)reject
 * { ... }
 *
 * Calling `NativeModules.ModuleName.doSomethingAsync(aString)` from
 * JavaScript will return a promise that is resolved or rejected when your
 * native method implementation calls the respective block.
 *
 */
#define RCT_EXPORT_METHOD(method) RCT_REMAP_METHOD(, method)

/**
 * Same as RCT_EXPORT_METHOD but the method is called from JS
 * synchronously **on the JS thread**, possibly returning a result.
 *
 * WARNING: in the vast majority of cases, you should use RCT_EXPORT_METHOD which
 * allows your native module methods to be called asynchronously: calling
 * methods synchronously can have strong performance penalties and introduce
 * threading-related bugs to your native modules.
 *
 * The return type must be of object type (id) and should be serializable
 * to JSON. This means that the hook can only return nil or JSON values
 * (e.g. NSNumber, NSString, NSArray, NSDictionary).
 *
 * Calling these methods when running under the websocket executor
 * is currently not supported.
 */
#define RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(method) RCT_EXPORT_SYNCHRONOUS_TYPED_METHOD(id, method)

#define RCT_EXPORT_SYNCHRONOUS_TYPED_METHOD(returnType, method) \
  RCT_REMAP_BLOCKING_SYNCHRONOUS_METHOD(, returnType, method)

/**
 * Similar to RCT_EXPORT_METHOD but lets you set the JS name of the exported
 * method. Example usage:
 *
 * RCT_REMAP_METHOD(executeQueryWithParameters,
 *   executeQuery:(NSString *)query parameters:(NSDictionary *)parameters)
 * { ... }
 */
#define RCT_REMAP_METHOD(js_name, method)       \
  _RCT_EXTERN_REMAP_METHOD(js_name, method, NO) \
  -(void)method RCT_DYNAMIC;

/**
 * Similar to RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD but lets you set
 * the JS name of the exported method. Example usage:
 *
 * RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(executeQueryWithParameters,
 *   executeQuery:(NSString *)query parameters:(NSDictionary *)parameters)
 * { ... }
 */
#define RCT_REMAP_BLOCKING_SYNCHRONOUS_METHOD(js_name, returnType, method) \
  _RCT_EXTERN_REMAP_METHOD(js_name, method, YES)                           \
  -(returnType)method RCT_DYNAMIC;

/**
 * Use this macro in a private Objective-C implementation file to automatically
 * register an external module with the bridge when it loads. This allows you to
 * register Swift or private Objective-C classes with the bridge.
 *
 * For example if one wanted to export a Swift class to the bridge:
 *
 * MyModule.swift:
 *
 *   @objc(MyModule) class MyModule: NSObject {
 *
 *     @objc func doSomething(string: String! withFoo a: Int, bar b: Int) { ... }
 *
 *   }
 *
 * MyModuleExport.m:
 *
 *   #import <React/RCTBridgeModule.h>
 *
 *   @interface RCT_EXTERN_MODULE(MyModule, NSObject)
 *
 *   RCT_EXTERN_METHOD(doSomething:(NSString *)string withFoo:(NSInteger)a bar:(NSInteger)b)
 *
 *   @end
 *
 * This will now expose MyModule and the method to JavaScript via
 * `NativeModules.MyModule.doSomething`
 */
#define RCT_EXTERN_MODULE(objc_name, objc_supername) RCT_EXTERN_REMAP_MODULE(, objc_name, objc_supername)

/**
 * Like RCT_EXTERN_MODULE, but allows setting a custom JavaScript name.
 */
#define RCT_EXTERN_REMAP_MODULE(js_name, objc_name, objc_supername) \
  objc_name:                                                        \
  objc_supername @                                                  \
  end @interface objc_name(RCTExternModule)<RCTBridgeModule>        \
  @end                                                              \
  @implementation objc_name (RCTExternModule)                       \
  RCT_EXPORT_MODULE_NO_LOAD(js_name, objc_name)

/**
 * Use this macro in accordance with RCT_EXTERN_MODULE to export methods
 * of an external module.
 */
#define RCT_EXTERN_METHOD(method) _RCT_EXTERN_REMAP_METHOD(, method, NO)

/**
 * Use this macro in accordance with RCT_EXTERN_MODULE to export methods
 * of an external module that should be invoked synchronously.
 */
#define RCT_EXTERN__BLOCKING_SYNCHRONOUS_METHOD(method) _RCT_EXTERN_REMAP_METHOD(, method, YES)

/**
 * Like RCT_EXTERN_REMAP_METHOD, but allows setting a custom JavaScript name
 * and also whether this method is synchronous.
 */
#define _RCT_EXTERN_REMAP_METHOD(js_name, method, is_blocking_synchronous_method)                            \
  +(const RCTMethodInfo *)RCT_CONCAT(__rct_export__, RCT_CONCAT(js_name, RCT_CONCAT(__LINE__, __COUNTER__))) \
  {                                                                                                          \
    static RCTMethodInfo config = {#js_name, #method, is_blocking_synchronous_method};                       \
    return &config;                                                                                          \
  }

/**
 * Most modules can be used from any thread. All of the modules exported non-sync method will be called on its
 * methodQueue, and the module will be constructed lazily when its first invoked. Some modules have main need to access
 * information that's main queue only (e.g. most UIKit classes). Since we don't want to dispatch synchronously to the
 * main thread to this safely, we construct these modules and export their constants ahead-of-time.
 *
 * Note that when set to false, the module constructor will be called from any thread.
 *
 * This requirement is currently inferred by checking if the module has a custom initializer or if there's exported
 * constants. In the future, we'll stop automatically inferring this and instead only rely on this method.
 */
+ (BOOL)requiresMainQueueSetup;

/**
 * Injects methods into JS.  Entries in this array are used in addition to any
 * methods defined using the macros above.  This method is called only once,
 * before registration.
 */
- (NSArray<id<RCTBridgeMethod>> *)methodsToExport;

/**
 * Injects constants into JS. These constants are made accessible via NativeModules.ModuleName.X. It is only called once
 * for the lifetime of the bridge, so it is not suitable for returning dynamic values, but may be used for long-lived
 * values such as session keys, that are regenerated only as part of a reload of the entire React application.
 *
 * If you implement this method and do not implement `requiresMainQueueSetup`, you will trigger deprecated logic
 * that eagerly initializes your module on bridge startup. In the future, this behaviour will be changed to default
 * to initializing lazily, and even modules with constants will be initialized lazily.
 */
- (NSDictionary *)constantsToExport;

/**
 * Notifies the module that a batch of JS method invocations has just completed.
 */
- (void)batchDidComplete;

/**
 * Notifies the module that the active batch of JS method invocations has been
 * partially flushed.
 *
 * This occurs before -batchDidComplete, and more frequently.
 */
- (void)partialBatchDidFlush;

@end

/**
 * A protocol that allows TurboModules to do lookup on other TurboModules.
 * Calling these methods may cause a module to be synchronously instantiated.
 */
@protocol RCTTurboModuleRegistry <NSObject>
- (id)moduleForName:(const char *)moduleName;

/**
 * Rationale:
 * When TurboModules lookup other modules by name, we first check the TurboModule
 * registry to see if a TurboModule exists with the respective name. In this case,
 * we don't want a RedBox to be raised if the TurboModule isn't found.
 *
 * This method is deprecated and will be deleted after the migration from
 * TurboModules to TurboModules is complete.
 */
- (id)moduleForName:(const char *)moduleName warnOnLookupFailure:(BOOL)warnOnLookupFailure;
- (BOOL)moduleIsInitialized:(const char *)moduleName;

- (NSArray<NSString *> *)eagerInitModuleNames;
- (NSArray<NSString *> *)eagerInitMainQueueModuleNames;
@end

/**
 * Experimental.
 * A protocol to declare that a class supports TurboModule.
 * This may be removed in the future.
 * See RCTTurboModule.h for actual signature.
 */
@protocol RCTTurboModule;

/**
 * A class that allows NativeModules and TurboModules to look up one another.
 */
@interface RCTModuleRegistry : NSObject
- (void)setBridge:(RCTBridge *)bridge;
- (void)setTurboModuleRegistry:(id<RCTTurboModuleRegistry>)turboModuleRegistry;

- (id)moduleForName:(const char *)moduleName;
- (id)moduleForName:(const char *)moduleName lazilyLoadIfNecessary:(BOOL)lazilyLoad;
@end

typedef void (^RCTBridgelessBundleURLSetter)(NSURL *bundleURL);
typedef NSURL * (^RCTBridgelessBundleURLGetter)();

/**
 * A class that allows NativeModules/TurboModules to read/write the bundleURL, with or without the bridge.
 */
@interface RCTBundleManager : NSObject
- (void)setBridge:(RCTBridge *)bridge;
- (void)setBridgelessBundleURLGetter:(RCTBridgelessBundleURLGetter)getter
                           andSetter:(RCTBridgelessBundleURLSetter)setter
                    andDefaultGetter:(RCTBridgelessBundleURLGetter)defaultGetter;
- (void)resetBundleURL;
@property NSURL *bundleURL;
@end

typedef UIView * (^RCTBridgelessComponentViewProvider)(NSNumber *);

/**
 * A class that allows NativeModules to query for views, given React Tags.
 */
@interface RCTViewRegistry : NSObject
- (void)setBridge:(RCTBridge *)bridge;
- (void)setBridgelessComponentViewProvider:(RCTBridgelessComponentViewProvider)bridgelessComponentViewProvider;

- (UIView *)viewForReactTag:(NSNumber *)reactTag;
@end

typedef void (^RCTBridgelessJSModuleMethodInvoker)(
    NSString *moduleName,
    NSString *methodName,
    NSArray *args,
    dispatch_block_t onComplete);

/**
 * A class that allows NativeModules to call methods on JavaScript modules registered
 * as callable with React Native.
 */
@interface RCTCallableJSModules : NSObject
- (void)setBridge:(RCTBridge *)bridge;
- (void)setBridgelessJSModuleMethodInvoker:(RCTBridgelessJSModuleMethodInvoker)bridgelessJSModuleMethodInvoker;

- (void)invokeModule:(NSString *)moduleName method:(NSString *)methodName withArgs:(NSArray *)args;
- (void)invokeModule:(NSString *)moduleName
              method:(NSString *)methodName
            withArgs:(NSArray *)args
          onComplete:(dispatch_block_t)onComplete;
@end