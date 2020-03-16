/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef V8_INSPECTOR_INJECTED_SCRIPT_H_
#define V8_INSPECTOR_INJECTED_SCRIPT_H_

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "src/base/macros.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-debugger.h"

#include "include/v8.h"

namespace v8_inspector {

class RemoteObjectId;
class V8InspectorImpl;
class V8InspectorSessionImpl;
class ValueMirror;
enum class WrapMode;

using protocol::Maybe;
using protocol::Response;

class EvaluateCallback {
 public:
  virtual void sendSuccess(
      std::unique_ptr<protocol::Runtime::RemoteObject> result,
      protocol::Maybe<protocol::Runtime::ExceptionDetails>
          exceptionDetails) = 0;
  virtual void sendFailure(const protocol::DispatchResponse& response) = 0;
  virtual ~EvaluateCallback() = default;
};

class InjectedScript final {
 public:
  InjectedScript(InspectedContext*, int sessionId);
  ~InjectedScript();

  InspectedContext* context() const { return m_context; }

  Response getProperties(
      v8::Local<v8::Object>, const String16& groupName, bool ownProperties,
      bool accessorPropertiesOnly, WrapMode wrapMode,
      std::unique_ptr<protocol::Array<protocol::Runtime::PropertyDescriptor>>*
          result,
      Maybe<protocol::Runtime::ExceptionDetails>*);

  Response getInternalAndPrivateProperties(
      v8::Local<v8::Value>, const String16& groupName,
      std::unique_ptr<
          protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>*
          internalProperties,
      std::unique_ptr<
          protocol::Array<protocol::Runtime::PrivatePropertyDescriptor>>*
          privateProperties);

  void releaseObject(const String16& objectId);

  Response wrapObject(v8::Local<v8::Value>, const String16& groupName,
                      WrapMode wrapMode,
                      std::unique_ptr<protocol::Runtime::RemoteObject>* result);
  Response wrapObject(v8::Local<v8::Value>, const String16& groupName,
                      WrapMode wrapMode,
                      v8::MaybeLocal<v8::Value> customPreviewConfig,
                      int maxCustomPreviewDepth,
                      std::unique_ptr<protocol::Runtime::RemoteObject>* result);
  Response wrapObjectMirror(
      const ValueMirror& mirror, const String16& groupName, WrapMode wrapMode,
      v8::MaybeLocal<v8::Value> customPreviewConfig, int maxCustomPreviewDepth,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result);
  std::unique_ptr<protocol::Runtime::RemoteObject> wrapTable(
      v8::Local<v8::Object> table, v8::MaybeLocal<v8::Array> columns);

  void addPromiseCallback(V8InspectorSessionImpl* session,
                          v8::MaybeLocal<v8::Value> value,
                          const String16& objectGroup, WrapMode wrapMode,
                          std::unique_ptr<EvaluateCallback> callback);

  Response findObject(const RemoteObjectId&, v8::Local<v8::Value>*) const;
  String16 objectGroupName(const RemoteObjectId&) const;
  void releaseObjectGroup(const String16&);
  void setCustomObjectFormatterEnabled(bool);
  Response resolveCallArgument(protocol::Runtime::CallArgument*,
                               v8::Local<v8::Value>* result);

  Response createExceptionDetails(
      const v8::TryCatch&, const String16& groupName,
      Maybe<protocol::Runtime::ExceptionDetails>* result);
  Response wrapEvaluateResult(
      v8::MaybeLocal<v8::Value> maybeResultValue, const v8::TryCatch&,
      const String16& objectGroup, WrapMode wrapMode,
      std::unique_ptr<protocol::Runtime::RemoteObject>* result,
      Maybe<protocol::Runtime::ExceptionDetails>*);
  v8::Local<v8::Value> lastEvaluationResult() const;
  void setLastEvaluationResult(v8::Local<v8::Value> result);

  class Scope {
   public:
    Response initialize();
    void installCommandLineAPI();
    void ignoreExceptionsAndMuteConsole();
    void pretendUserGesture();
    void allowCodeGenerationFromStrings();
    v8::Local<v8::Context> context() const { return m_context; }
    InjectedScript* injectedScript() const { return m_injectedScript; }
    const v8::TryCatch& tryCatch() const { return m_tryCatch; }
    V8InspectorImpl* inspector() const { return m_inspector; }

   protected:
    explicit Scope(V8InspectorSessionImpl*);
    virtual ~Scope();
    virtual Response findInjectedScript(V8InspectorSessionImpl*) = 0;

    V8InspectorImpl* m_inspector;
    InjectedScript* m_injectedScript;

   private:
    void cleanup();
    v8::debug::ExceptionBreakState setPauseOnExceptionsState(
        v8::debug::ExceptionBreakState);

    v8::HandleScope m_handleScope;
    v8::TryCatch m_tryCatch;
    v8::Local<v8::Context> m_context;
    std::unique_ptr<V8Console::CommandLineAPIScope> m_commandLineAPIScope;
    bool m_ignoreExceptionsAndMuteConsole;
    v8::debug::ExceptionBreakState m_previousPauseOnExceptionsState;
    bool m_userGesture;
    bool m_allowEval;
    int m_contextGroupId;
    int m_sessionId;
  };

  class ContextScope : public Scope {
   public:
    ContextScope(V8InspectorSessionImpl*, int executionContextId);
    ~ContextScope() override;

   private:
    Response findInjectedScript(V8InspectorSessionImpl*) override;
    int m_executionContextId;

    DISALLOW_COPY_AND_ASSIGN(ContextScope);
  };

  class ObjectScope : public Scope {
   public:
    ObjectScope(V8InspectorSessionImpl*, const String16& remoteObjectId);
    ~ObjectScope() override;
    const String16& objectGroupName() const { return m_objectGroupName; }
    v8::Local<v8::Value> object() const { return m_object; }

   private:
    Response findInjectedScript(V8InspectorSessionImpl*) override;
    String16 m_remoteObjectId;
    String16 m_objectGroupName;
    v8::Local<v8::Value> m_object;

    DISALLOW_COPY_AND_ASSIGN(ObjectScope);
  };

  class CallFrameScope : public Scope {
   public:
    CallFrameScope(V8InspectorSessionImpl*, const String16& remoteCallFrameId);
    ~CallFrameScope() override;
    size_t frameOrdinal() const { return m_frameOrdinal; }

   private:
    Response findInjectedScript(V8InspectorSessionImpl*) override;
    String16 m_remoteCallFrameId;
    size_t m_frameOrdinal;

    DISALLOW_COPY_AND_ASSIGN(CallFrameScope);
  };
  String16 bindObject(v8::Local<v8::Value>, const String16& groupName);

 private:
  v8::Local<v8::Object> commandLineAPI();
  void unbindObject(int id);

  static Response bindRemoteObjectIfNeeded(
      int sessionId, v8::Local<v8::Context> context, v8::Local<v8::Value>,
      const String16& groupName, protocol::Runtime::RemoteObject* remoteObject);

  class ProtocolPromiseHandler;
  void discardEvaluateCallbacks();
  std::unique_ptr<EvaluateCallback> takeEvaluateCallback(
      EvaluateCallback* callback);
  Response addExceptionToDetails(
      v8::Local<v8::Value> exception,
      protocol::Runtime::ExceptionDetails* exceptionDetails,
      const String16& objectGroup);

  InspectedContext* m_context;
  int m_sessionId;
  v8::Global<v8::Value> m_lastEvaluationResult;
  v8::Global<v8::Object> m_commandLineAPI;
  int m_lastBoundObjectId = 1;
  std::unordered_map<int, v8::Global<v8::Value>> m_idToWrappedObject;
  std::unordered_map<int, String16> m_idToObjectGroupName;
  std::unordered_map<String16, std::vector<int>> m_nameToObjectGroup;
  std::unordered_set<EvaluateCallback*> m_evaluateCallbacks;
  bool m_customPreviewEnabled = false;

  DISALLOW_COPY_AND_ASSIGN(InjectedScript);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_INJECTED_SCRIPT_H_
