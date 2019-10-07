/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8_INSPECTOR_V8_RUNTIME_AGENT_IMPL_H_
#define V8_INSPECTOR_V8_RUNTIME_AGENT_IMPL_H_

#include <unordered_map>

#include "src/base/macros.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"

#include "include/v8.h"

namespace v8_inspector {

class InjectedScript;
class InspectedContext;
class RemoteObjectIdBase;
class V8ConsoleMessage;
class V8InspectorImpl;
class V8InspectorSessionImpl;

using protocol::Response;
using protocol::Maybe;

class V8RuntimeAgentImpl : public protocol::Runtime::Backend {
 public:
  V8RuntimeAgentImpl(V8InspectorSessionImpl*, protocol::FrontendChannel*,
                     protocol::DictionaryValue* state);
  ~V8RuntimeAgentImpl() override;
  void restore();

  // Part of the protocol.
  Response enable() override;
  Response disable() override;
  void evaluate(const String16& expression, Maybe<String16> objectGroup,
                Maybe<bool> includeCommandLineAPI, Maybe<bool> silent,
                Maybe<int> executionContextId, Maybe<bool> returnByValue,
                Maybe<bool> generatePreview, Maybe<bool> userGesture,
                Maybe<bool> awaitPromise, Maybe<bool> throwOnSideEffect,
                Maybe<double> timeout,
                std::unique_ptr<EvaluateCallback>) override;
  void awaitPromise(const String16& promiseObjectId, Maybe<bool> returnByValue,
                    Maybe<bool> generatePreview,
                    std::unique_ptr<AwaitPromiseCallback>) override;
  void callFunctionOn(
      const String16& expression, Maybe<String16> objectId,
      Maybe<protocol::Array<protocol::Runtime::CallArgument>> optionalArguments,
      Maybe<bool> silent, Maybe<bool> returnByValue,
      Maybe<bool> generatePreview, Maybe<bool> userGesture,
      Maybe<bool> awaitPromise, Maybe<int> executionContextId,
      Maybe<String16> objectGroup,
      std::unique_ptr<CallFunctionOnCallback>) override;
  Response releaseObject(const String16& objectId) override;
  Response getProperties(
      const String16& objectId, Maybe<bool> ownProperties,
      Maybe<bool> accessorPropertiesOnly, Maybe<bool> generatePreview,
      std::unique_ptr<protocol::Array<protocol::Runtime::PropertyDescriptor>>*
          result,
      Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>*
          internalProperties,
      Maybe<protocol::Runtime::ExceptionDetails>*) override;
  Response releaseObjectGroup(const String16& objectGroup) override;
  Response runIfWaitingForDebugger() override;
  Response setCustomObjectFormatterEnabled(bool) override;
  Response setMaxCallStackSizeToCapture(int) override;
  Response discardConsoleEntries() override;
  Response compileScript(const String16& expression, const String16& sourceURL,
                         bool persistScript, Maybe<int> executionContextId,
                         Maybe<String16>*,
                         Maybe<protocol::Runtime::ExceptionDetails>*) override;
  void runScript(const String16&, Maybe<int> executionContextId,
                 Maybe<String16> objectGroup, Maybe<bool> silent,
                 Maybe<bool> includeCommandLineAPI, Maybe<bool> returnByValue,
                 Maybe<bool> generatePreview, Maybe<bool> awaitPromise,
                 std::unique_ptr<RunScriptCallback>) override;
  Response queryObjects(
      const String16& prototypeObjectId, Maybe<String16> objectGroup,
      std::unique_ptr<protocol::Runtime::RemoteObject>* objects) override;
  Response globalLexicalScopeNames(
      Maybe<int> executionContextId,
      std::unique_ptr<protocol::Array<String16>>* outNames) override;
  Response getIsolateId(String16* outIsolateId) override;
  Response getHeapUsage(double* out_usedSize, double* out_totalSize) override;
  void terminateExecution(
      std::unique_ptr<TerminateExecutionCallback> callback) override;

  Response addBinding(const String16& name,
                      Maybe<int> executionContextId) override;
  Response removeBinding(const String16& name) override;
  void addBindings(InspectedContext* context);

  void reset();
  void reportExecutionContextCreated(InspectedContext*);
  void reportExecutionContextDestroyed(InspectedContext*);
  void inspect(std::unique_ptr<protocol::Runtime::RemoteObject> objectToInspect,
               std::unique_ptr<protocol::DictionaryValue> hints);
  void messageAdded(V8ConsoleMessage*);
  bool enabled() const { return m_enabled; }

 private:
  bool reportMessage(V8ConsoleMessage*, bool generatePreview);

  static void bindingCallback(const v8::FunctionCallbackInfo<v8::Value>& args);
  void bindingCalled(const String16& name, const String16& payload,
                     int executionContextId);
  void addBinding(InspectedContext* context, const String16& name);

  V8InspectorSessionImpl* m_session;
  protocol::DictionaryValue* m_state;
  protocol::Runtime::Frontend m_frontend;
  V8InspectorImpl* m_inspector;
  bool m_enabled;
  std::unordered_map<String16, std::unique_ptr<v8::Global<v8::Script>>>
      m_compiledScripts;

  DISALLOW_COPY_AND_ASSIGN(V8RuntimeAgentImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_RUNTIME_AGENT_IMPL_H_
