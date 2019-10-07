/*
 * Copyright (c) 2010-2011 Google Inc. All rights reserved.
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

#include "src/inspector/v8-inspector-impl.h"

#include <vector>

#include "src/base/platform/mutex.h"
#include "src/inspector/inspected-context.h"
#include "src/inspector/string-util.h"
#include "src/inspector/v8-console-agent-impl.h"
#include "src/inspector/v8-console-message.h"
#include "src/inspector/v8-console.h"
#include "src/inspector/v8-debugger-agent-impl.h"
#include "src/inspector/v8-debugger.h"
#include "src/inspector/v8-inspector-session-impl.h"
#include "src/inspector/v8-profiler-agent-impl.h"
#include "src/inspector/v8-runtime-agent-impl.h"
#include "src/inspector/v8-stack-trace-impl.h"

#include "include/v8-platform.h"

namespace v8_inspector {

std::unique_ptr<V8Inspector> V8Inspector::create(v8::Isolate* isolate,
                                                 V8InspectorClient* client) {
  return std::unique_ptr<V8Inspector>(new V8InspectorImpl(isolate, client));
}

V8InspectorImpl::V8InspectorImpl(v8::Isolate* isolate,
                                 V8InspectorClient* client)
    : m_isolate(isolate),
      m_client(client),
      m_debugger(new V8Debugger(isolate, this)),
      m_capturingStackTracesCount(0),
      m_lastExceptionId(0),
      m_lastContextId(0),
      m_isolateId(v8::debug::GetNextRandomInt64(m_isolate)) {
  v8::debug::SetInspector(m_isolate, this);
  v8::debug::SetConsoleDelegate(m_isolate, console());
}

V8InspectorImpl::~V8InspectorImpl() {
  v8::debug::SetInspector(m_isolate, nullptr);
  v8::debug::SetConsoleDelegate(m_isolate, nullptr);
}

int V8InspectorImpl::contextGroupId(v8::Local<v8::Context> context) const {
  return contextGroupId(InspectedContext::contextId(context));
}

int V8InspectorImpl::contextGroupId(int contextId) const {
  auto it = m_contextIdToGroupIdMap.find(contextId);
  return it != m_contextIdToGroupIdMap.end() ? it->second : 0;
}

v8::MaybeLocal<v8::Value> V8InspectorImpl::compileAndRunInternalScript(
    v8::Local<v8::Context> context, v8::Local<v8::String> source) {
  v8::Local<v8::UnboundScript> unboundScript;
  if (!v8::debug::CompileInspectorScript(m_isolate, source)
           .ToLocal(&unboundScript))
    return v8::MaybeLocal<v8::Value>();
  v8::MicrotasksScope microtasksScope(m_isolate,
                                      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope contextScope(context);
  v8::Isolate::SafeForTerminationScope allowTermination(m_isolate);
  return unboundScript->BindToCurrentContext()->Run(context);
}

v8::MaybeLocal<v8::Script> V8InspectorImpl::compileScript(
    v8::Local<v8::Context> context, const String16& code,
    const String16& fileName) {
  v8::ScriptOrigin origin(
      toV8String(m_isolate, fileName), v8::Integer::New(m_isolate, 0),
      v8::Integer::New(m_isolate, 0),
      v8::False(m_isolate),                                         // sharable
      v8::Local<v8::Integer>(), toV8String(m_isolate, String16()),  // sourceMap
      v8::True(m_isolate));  // opaqueresource
  v8::ScriptCompiler::Source source(toV8String(m_isolate, code), origin);
  return v8::ScriptCompiler::Compile(context, &source,
                                     v8::ScriptCompiler::kNoCompileOptions);
}

void V8InspectorImpl::enableStackCapturingIfNeeded() {
  if (!m_capturingStackTracesCount)
    V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(m_isolate,
                                                                true);
  ++m_capturingStackTracesCount;
}

void V8InspectorImpl::disableStackCapturingIfNeeded() {
  if (!(--m_capturingStackTracesCount))
    V8StackTraceImpl::setCaptureStackTraceForUncaughtExceptions(m_isolate,
                                                                false);
}

void V8InspectorImpl::muteExceptions(int contextGroupId) {
  m_muteExceptionsMap[contextGroupId]++;
}

void V8InspectorImpl::unmuteExceptions(int contextGroupId) {
  m_muteExceptionsMap[contextGroupId]--;
}

V8ConsoleMessageStorage* V8InspectorImpl::ensureConsoleMessageStorage(
    int contextGroupId) {
  ConsoleStorageMap::iterator storageIt =
      m_consoleStorageMap.find(contextGroupId);
  if (storageIt == m_consoleStorageMap.end())
    storageIt = m_consoleStorageMap
                    .insert(std::make_pair(
                        contextGroupId,
                        std::unique_ptr<V8ConsoleMessageStorage>(
                            new V8ConsoleMessageStorage(this, contextGroupId))))
                    .first;
  return storageIt->second.get();
}

bool V8InspectorImpl::hasConsoleMessageStorage(int contextGroupId) {
  ConsoleStorageMap::iterator storageIt =
      m_consoleStorageMap.find(contextGroupId);
  return storageIt != m_consoleStorageMap.end();
}

std::unique_ptr<V8StackTrace> V8InspectorImpl::createStackTrace(
    v8::Local<v8::StackTrace> stackTrace) {
  return m_debugger->createStackTrace(stackTrace);
}

std::unique_ptr<V8InspectorSession> V8InspectorImpl::connect(
    int contextGroupId, V8Inspector::Channel* channel,
    const StringView& state) {
  int sessionId = ++m_lastSessionId;
  std::unique_ptr<V8InspectorSessionImpl> session =
      V8InspectorSessionImpl::create(this, contextGroupId, sessionId, channel,
                                     state);
  m_sessions[contextGroupId][sessionId] = session.get();
  return std::move(session);
}

void V8InspectorImpl::disconnect(V8InspectorSessionImpl* session) {
  auto& map = m_sessions[session->contextGroupId()];
  map.erase(session->sessionId());
  if (map.empty()) m_sessions.erase(session->contextGroupId());
}

InspectedContext* V8InspectorImpl::getContext(int groupId,
                                              int contextId) const {
  if (!groupId || !contextId) return nullptr;

  ContextsByGroupMap::const_iterator contextGroupIt = m_contexts.find(groupId);
  if (contextGroupIt == m_contexts.end()) return nullptr;

  ContextByIdMap::iterator contextIt = contextGroupIt->second->find(contextId);
  if (contextIt == contextGroupIt->second->end()) return nullptr;

  return contextIt->second.get();
}

InspectedContext* V8InspectorImpl::getContext(int contextId) const {
  return getContext(contextGroupId(contextId), contextId);
}

v8::MaybeLocal<v8::Context> V8InspectorImpl::contextById(
    int groupId, v8::Maybe<int> contextId) {
  if (contextId.IsNothing()) {
    v8::Local<v8::Context> context =
        client()->ensureDefaultContextInGroup(groupId);
    return context.IsEmpty() ? v8::MaybeLocal<v8::Context>() : context;
  }
  InspectedContext* context = getContext(contextId.FromJust());
  return context ? context->context() : v8::MaybeLocal<v8::Context>();
}

void V8InspectorImpl::contextCreated(const V8ContextInfo& info) {
  int contextId = ++m_lastContextId;
  InspectedContext* context = new InspectedContext(this, info, contextId);
  m_contextIdToGroupIdMap[contextId] = info.contextGroupId;

  ContextsByGroupMap::iterator contextIt = m_contexts.find(info.contextGroupId);
  if (contextIt == m_contexts.end())
    contextIt = m_contexts
                    .insert(std::make_pair(
                        info.contextGroupId,
                        std::unique_ptr<ContextByIdMap>(new ContextByIdMap())))
                    .first;
  const auto& contextById = contextIt->second;

  DCHECK(contextById->find(contextId) == contextById->cend());
  (*contextById)[contextId].reset(context);
  forEachSession(
      info.contextGroupId, [&context](V8InspectorSessionImpl* session) {
        session->runtimeAgent()->addBindings(context);
        session->runtimeAgent()->reportExecutionContextCreated(context);
      });
}

void V8InspectorImpl::contextDestroyed(v8::Local<v8::Context> context) {
  int contextId = InspectedContext::contextId(context);
  int groupId = contextGroupId(context);
  contextCollected(groupId, contextId);
}

void V8InspectorImpl::contextCollected(int groupId, int contextId) {
  m_contextIdToGroupIdMap.erase(contextId);

  ConsoleStorageMap::iterator storageIt = m_consoleStorageMap.find(groupId);
  if (storageIt != m_consoleStorageMap.end())
    storageIt->second->contextDestroyed(contextId);

  InspectedContext* inspectedContext = getContext(groupId, contextId);
  if (!inspectedContext) return;

  forEachSession(groupId, [&inspectedContext](V8InspectorSessionImpl* session) {
    session->runtimeAgent()->reportExecutionContextDestroyed(inspectedContext);
  });
  discardInspectedContext(groupId, contextId);
}

void V8InspectorImpl::resetContextGroup(int contextGroupId) {
  m_consoleStorageMap.erase(contextGroupId);
  m_muteExceptionsMap.erase(contextGroupId);
  forEachSession(contextGroupId,
                 [](V8InspectorSessionImpl* session) { session->reset(); });
  m_contexts.erase(contextGroupId);
  m_debugger->wasmTranslation()->Clear();
}

void V8InspectorImpl::idleStarted() { m_isolate->SetIdle(true); }

void V8InspectorImpl::idleFinished() { m_isolate->SetIdle(false); }

unsigned V8InspectorImpl::exceptionThrown(
    v8::Local<v8::Context> context, const StringView& message,
    v8::Local<v8::Value> exception, const StringView& detailedMessage,
    const StringView& url, unsigned lineNumber, unsigned columnNumber,
    std::unique_ptr<V8StackTrace> stackTrace, int scriptId) {
  int groupId = contextGroupId(context);
  if (!groupId || m_muteExceptionsMap[groupId]) return 0;
  std::unique_ptr<V8StackTraceImpl> stackTraceImpl(
      static_cast<V8StackTraceImpl*>(stackTrace.release()));
  unsigned exceptionId = nextExceptionId();
  std::unique_ptr<V8ConsoleMessage> consoleMessage =
      V8ConsoleMessage::createForException(
          m_client->currentTimeMS(), toString16(detailedMessage),
          toString16(url), lineNumber, columnNumber, std::move(stackTraceImpl),
          scriptId, m_isolate, toString16(message),
          InspectedContext::contextId(context), exception, exceptionId);
  ensureConsoleMessageStorage(groupId)->addMessage(std::move(consoleMessage));
  return exceptionId;
}

void V8InspectorImpl::exceptionRevoked(v8::Local<v8::Context> context,
                                       unsigned exceptionId,
                                       const StringView& message) {
  int groupId = contextGroupId(context);
  if (!groupId) return;

  std::unique_ptr<V8ConsoleMessage> consoleMessage =
      V8ConsoleMessage::createForRevokedException(
          m_client->currentTimeMS(), toString16(message), exceptionId);
  ensureConsoleMessageStorage(groupId)->addMessage(std::move(consoleMessage));
}

std::unique_ptr<V8StackTrace> V8InspectorImpl::captureStackTrace(
    bool fullStack) {
  return m_debugger->captureStackTrace(fullStack);
}

V8StackTraceId V8InspectorImpl::storeCurrentStackTrace(
    const StringView& description) {
  return m_debugger->storeCurrentStackTrace(description);
}

void V8InspectorImpl::externalAsyncTaskStarted(const V8StackTraceId& parent) {
  m_debugger->externalAsyncTaskStarted(parent);
}

void V8InspectorImpl::externalAsyncTaskFinished(const V8StackTraceId& parent) {
  m_debugger->externalAsyncTaskFinished(parent);
}

void V8InspectorImpl::asyncTaskScheduled(const StringView& taskName, void* task,
                                         bool recurring) {
  if (!task) return;
  m_debugger->asyncTaskScheduled(taskName, task, recurring);
}

void V8InspectorImpl::asyncTaskCanceled(void* task) {
  if (!task) return;
  m_debugger->asyncTaskCanceled(task);
}

void V8InspectorImpl::asyncTaskStarted(void* task) {
  if (!task) return;
  m_debugger->asyncTaskStarted(task);
}

void V8InspectorImpl::asyncTaskFinished(void* task) {
  if (!task) return;
  m_debugger->asyncTaskFinished(task);
}

void V8InspectorImpl::allAsyncTasksCanceled() {
  m_debugger->allAsyncTasksCanceled();
}

v8::Local<v8::Context> V8InspectorImpl::regexContext() {
  if (m_regexContext.IsEmpty())
    m_regexContext.Reset(m_isolate, v8::Context::New(m_isolate));
  return m_regexContext.Get(m_isolate);
}

void V8InspectorImpl::discardInspectedContext(int contextGroupId,
                                              int contextId) {
  if (!getContext(contextGroupId, contextId)) return;
  m_contexts[contextGroupId]->erase(contextId);
  if (m_contexts[contextGroupId]->empty()) m_contexts.erase(contextGroupId);
}

V8InspectorSessionImpl* V8InspectorImpl::sessionById(int contextGroupId,
                                                     int sessionId) {
  auto it = m_sessions.find(contextGroupId);
  if (it == m_sessions.end()) return nullptr;
  auto it2 = it->second.find(sessionId);
  return it2 == it->second.end() ? nullptr : it2->second;
}

V8Console* V8InspectorImpl::console() {
  if (!m_console) m_console.reset(new V8Console(this));
  return m_console.get();
}

void V8InspectorImpl::forEachContext(
    int contextGroupId,
    const std::function<void(InspectedContext*)>& callback) {
  auto it = m_contexts.find(contextGroupId);
  if (it == m_contexts.end()) return;
  std::vector<int> ids;
  ids.reserve(it->second->size());
  for (auto& contextIt : *(it->second)) ids.push_back(contextIt.first);

  // Retrieve by ids each time since |callback| may destroy some contexts.
  for (auto& contextId : ids) {
    it = m_contexts.find(contextGroupId);
    if (it == m_contexts.end()) continue;
    auto contextIt = it->second->find(contextId);
    if (contextIt != it->second->end()) callback(contextIt->second.get());
  }
}

void V8InspectorImpl::forEachSession(
    int contextGroupId,
    const std::function<void(V8InspectorSessionImpl*)>& callback) {
  auto it = m_sessions.find(contextGroupId);
  if (it == m_sessions.end()) return;
  std::vector<int> ids;
  ids.reserve(it->second.size());
  for (auto& sessionIt : it->second) ids.push_back(sessionIt.first);

  // Retrieve by ids each time since |callback| may destroy some contexts.
  for (auto& sessionId : ids) {
    it = m_sessions.find(contextGroupId);
    if (it == m_sessions.end()) continue;
    auto sessionIt = it->second.find(sessionId);
    if (sessionIt != it->second.end()) callback(sessionIt->second);
  }
}

V8InspectorImpl::EvaluateScope::EvaluateScope(v8::Isolate* isolate)
    : m_isolate(isolate), m_safeForTerminationScope(isolate) {}

struct V8InspectorImpl::EvaluateScope::CancelToken {
  v8::base::Mutex m_mutex;
  bool m_canceled = false;
};

V8InspectorImpl::EvaluateScope::~EvaluateScope() {
  if (m_cancelToken) {
    v8::base::LockGuard<v8::base::Mutex> lock(&m_cancelToken->m_mutex);
    m_cancelToken->m_canceled = true;
    m_isolate->CancelTerminateExecution();
  }
}

class V8InspectorImpl::EvaluateScope::TerminateTask : public v8::Task {
 public:
  TerminateTask(v8::Isolate* isolate, std::shared_ptr<CancelToken> token)
      : m_isolate(isolate), m_token(std::move(token)) {}

  void Run() override {
    // CancelToken contains m_canceled bool which may be changed from main
    // thread, so lock mutex first.
    v8::base::LockGuard<v8::base::Mutex> lock(&m_token->m_mutex);
    if (m_token->m_canceled) return;
    m_isolate->TerminateExecution();
  }

 private:
  v8::Isolate* m_isolate;
  std::shared_ptr<CancelToken> m_token;
};

protocol::Response V8InspectorImpl::EvaluateScope::setTimeout(double timeout) {
  if (m_isolate->IsExecutionTerminating()) {
    return protocol::Response::Error("Execution was terminated");
  }
  m_cancelToken.reset(new CancelToken());
  v8::debug::GetCurrentPlatform()->CallDelayedOnWorkerThread(
      v8::base::make_unique<TerminateTask>(m_isolate, m_cancelToken), timeout);
  return protocol::Response::OK();
}

}  // namespace v8_inspector
