// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8CONSOLE_H_
#define V8_INSPECTOR_V8CONSOLE_H_

#include "src/base/macros.h"

#include "include/v8.h"

namespace v8_inspector {

class InspectedContext;

// Console API
// https://console.spec.whatwg.org/#console-interface
class V8Console {
 public:
  static v8::Local<v8::Object> createConsole(InspectedContext*,
                                             bool hasMemoryAttribute);
  static void clearInspectedContextIfNeeded(v8::Local<v8::Context>,
                                            v8::Local<v8::Object> console);
  static v8::Local<v8::Object> createCommandLineAPI(InspectedContext*);

  class CommandLineAPIScope {
   public:
    CommandLineAPIScope(v8::Local<v8::Context>,
                        v8::Local<v8::Object> commandLineAPI,
                        v8::Local<v8::Object> global);
    ~CommandLineAPIScope();

   private:
    static void accessorGetterCallback(
        v8::Local<v8::Name>, const v8::PropertyCallbackInfo<v8::Value>&);
    static void accessorSetterCallback(v8::Local<v8::Name>,
                                       v8::Local<v8::Value>,
                                       const v8::PropertyCallbackInfo<void>&);

    v8::Local<v8::Context> m_context;
    v8::Local<v8::Object> m_commandLineAPI;
    v8::Local<v8::Object> m_global;
    v8::Local<v8::Set> m_installedMethods;
    bool m_cleanup;

    DISALLOW_COPY_AND_ASSIGN(CommandLineAPIScope);
  };

 private:
  static void debugCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void errorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void infoCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void logCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void warnCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void dirCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void dirxmlCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void tableCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void traceCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void groupCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void groupCollapsedCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void groupEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void clearCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void countCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void assertCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void markTimelineCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void profileCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void profileEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void timelineCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void timelineEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void timeCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void timeEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void timeStampCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  // TODO(foolip): There is no spec for the Memory Info API, see blink-dev:
  // https://groups.google.com/a/chromium.org/d/msg/blink-dev/g5YRCGpC9vs/b4OJz71NmPwJ
  static void memoryGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void memorySetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);

  // CommandLineAPI
  static void keysCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void valuesCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void debugFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void undebugFunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void monitorFunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void unmonitorFunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void lastEvaluationResultCallback(
      const v8::FunctionCallbackInfo<v8::Value>&);
  static void inspectCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void copyCallback(const v8::FunctionCallbackInfo<v8::Value>&);
  static void inspectedObject(const v8::FunctionCallbackInfo<v8::Value>&,
                              unsigned num);
  static void inspectedObject0(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 0);
  }
  static void inspectedObject1(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 1);
  }
  static void inspectedObject2(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 2);
  }
  static void inspectedObject3(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 3);
  }
  static void inspectedObject4(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    inspectedObject(info, 4);
  }
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8CONSOLE_H_
