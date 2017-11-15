/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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

#ifndef V8_INSPECTOR_V8INSPECTORIMPL_H_
#define V8_INSPECTOR_V8INSPECTORIMPL_H_

#include <vector>

#include "src/base/macros.h"
#include "src/inspector/protocol/Protocol.h"

#include "include/v8-debug.h"
#include "include/v8-inspector.h"

namespace v8_inspector {

class InspectedContext;
class V8ConsoleMessageStorage;
class V8Debugger;
class V8DebuggerAgentImpl;
class V8InspectorSessionImpl;
class V8ProfilerAgentImpl;
class V8RuntimeAgentImpl;
class V8StackTraceImpl;

class V8InspectorImpl : public V8Inspector {
 public:
  V8InspectorImpl(v8::Isolate*, V8InspectorClient*);
  ~V8InspectorImpl() override;

  v8::Isolate* isolate() const { return m_isolate; }
  V8InspectorClient* client() { return m_client; }
  V8Debugger* debugger() { return m_debugger.get(); }
  int contextGroupId(v8::Local<v8::Context>);
  int contextGroupId(int contextId);

  v8::MaybeLocal<v8::Value> runCompiledScript(v8::Local<v8::Context>,
                                              v8::Local<v8::Script>);
  v8::MaybeLocal<v8::Value> callFunction(v8::Local<v8::Function>,
                                         v8::Local<v8::Context>,
                                         v8::Local<v8::Value> receiver,
                                         int argc, v8::Local<v8::Value> info[]);
  v8::MaybeLocal<v8::Value> compileAndRunInternalScript(v8::Local<v8::Context>,
                                                        v8::Local<v8::String>);
  v8::MaybeLocal<v8::Value> callInternalFunction(v8::Local<v8::Function>,
                                                 v8::Local<v8::Context>,
                                                 v8::Local<v8::Value> receiver,
                                                 int argc,
                                                 v8::Local<v8::Value> info[]);
  v8::MaybeLocal<v8::Script> compileScript(v8::Local<v8::Context>,
                                           const String16& code,
                                           const String16& fileName);
  v8::Local<v8::Context> regexContext();

  // V8Inspector implementation.
  std::unique_ptr<V8InspectorSession> connect(int contextGroupId,
                                              V8Inspector::Channel*,
                                              const StringView& state) override;
  void contextCreated(const V8ContextInfo&) override;
  void contextDestroyed(v8::Local<v8::Context>) override;
  void resetContextGroup(int contextGroupId) override;
  void willExecuteScript(v8::Local<v8::Context>, int scriptId) override;
  void didExecuteScript(v8::Local<v8::Context>) override;
  void idleStarted() override;
  void idleFinished() override;
  unsigned exceptionThrown(v8::Local<v8::Context>, const StringView& message,
                           v8::Local<v8::Value> exception,
                           const StringView& detailedMessage,
                           const StringView& url, unsigned lineNumber,
                           unsigned columnNumber, std::unique_ptr<V8StackTrace>,
                           int scriptId) override;
  void exceptionRevoked(v8::Local<v8::Context>, unsigned exceptionId,
                        const StringView& message) override;
  std::unique_ptr<V8StackTrace> createStackTrace(
      v8::Local<v8::StackTrace>) override;
  std::unique_ptr<V8StackTrace> captureStackTrace(bool fullStack) override;
  void asyncTaskScheduled(const StringView& taskName, void* task,
                          bool recurring) override;
  void asyncTaskCanceled(void* task) override;
  void asyncTaskStarted(void* task) override;
  void asyncTaskFinished(void* task) override;
  void allAsyncTasksCanceled() override;

  unsigned nextExceptionId() { return ++m_lastExceptionId; }
  void enableStackCapturingIfNeeded();
  void disableStackCapturingIfNeeded();
  void muteExceptions(int contextGroupId);
  void unmuteExceptions(int contextGroupId);
  V8ConsoleMessageStorage* ensureConsoleMessageStorage(int contextGroupId);
  bool hasConsoleMessageStorage(int contextGroupId);
  using ContextByIdMap =
      protocol::HashMap<int, std::unique_ptr<InspectedContext>>;
  void discardInspectedContext(int contextGroupId, int contextId);
  const ContextByIdMap* contextGroup(int contextGroupId);
  void disconnect(V8InspectorSessionImpl*);
  V8InspectorSessionImpl* sessionForContextGroup(int contextGroupId);
  InspectedContext* getContext(int groupId, int contextId) const;
  V8DebuggerAgentImpl* enabledDebuggerAgentForGroup(int contextGroupId);
  V8RuntimeAgentImpl* enabledRuntimeAgentForGroup(int contextGroupId);
  V8ProfilerAgentImpl* enabledProfilerAgentForGroup(int contextGroupId);

 private:
  v8::MaybeLocal<v8::Value> callFunction(
      v8::Local<v8::Function>, v8::Local<v8::Context>,
      v8::Local<v8::Value> receiver, int argc, v8::Local<v8::Value> info[],
      v8::MicrotasksScope::Type runMicrotasks);

  v8::Isolate* m_isolate;
  V8InspectorClient* m_client;
  std::unique_ptr<V8Debugger> m_debugger;
  v8::Global<v8::Context> m_regexContext;
  int m_capturingStackTracesCount;
  unsigned m_lastExceptionId;
  int m_lastContextId;

  using MuteExceptionsMap = protocol::HashMap<int, int>;
  MuteExceptionsMap m_muteExceptionsMap;

  using ContextsByGroupMap =
      protocol::HashMap<int, std::unique_ptr<ContextByIdMap>>;
  ContextsByGroupMap m_contexts;

  using SessionMap = protocol::HashMap<int, V8InspectorSessionImpl*>;
  SessionMap m_sessions;

  using ConsoleStorageMap =
      protocol::HashMap<int, std::unique_ptr<V8ConsoleMessageStorage>>;
  ConsoleStorageMap m_consoleStorageMap;

  protocol::HashMap<int, int> m_contextIdToGroupIdMap;

  DISALLOW_COPY_AND_ASSIGN(V8InspectorImpl);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8INSPECTORIMPL_H_
