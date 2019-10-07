// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSPECTOR_V8_DEBUGGER_H_
#define V8_INSPECTOR_V8_DEBUGGER_H_

#include <list>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "src/base/macros.h"
#include "src/debug/debug-interface.h"
#include "src/inspector/protocol/Debugger.h"
#include "src/inspector/protocol/Forward.h"
#include "src/inspector/protocol/Runtime.h"
#include "src/inspector/v8-debugger-script.h"
#include "src/inspector/wasm-translation.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

class AsyncStackTrace;
class StackFrame;
class V8Debugger;
class V8DebuggerAgentImpl;
class V8InspectorImpl;
class V8StackTraceImpl;
struct V8StackTraceId;

using protocol::Response;
using ScheduleStepIntoAsyncCallback =
    protocol::Debugger::Backend::ScheduleStepIntoAsyncCallback;
using TerminateExecutionCallback =
    protocol::Runtime::Backend::TerminateExecutionCallback;

class V8Debugger : public v8::debug::DebugDelegate,
                   public v8::debug::AsyncEventDelegate {
 public:
  V8Debugger(v8::Isolate*, V8InspectorImpl*);
  ~V8Debugger() override;

  bool enabled() const;
  v8::Isolate* isolate() const { return m_isolate; }

  void setBreakpointsActive(bool);

  v8::debug::ExceptionBreakState getPauseOnExceptionsState();
  void setPauseOnExceptionsState(v8::debug::ExceptionBreakState);
  bool canBreakProgram();
  void breakProgram(int targetContextGroupId);
  void interruptAndBreak(int targetContextGroupId);
  void continueProgram(int targetContextGroupId);
  void breakProgramOnAssert(int targetContextGroupId);

  void setPauseOnNextCall(bool, int targetContextGroupId);
  void stepIntoStatement(int targetContextGroupId, bool breakOnAsyncCall);
  void stepOverStatement(int targetContextGroupId);
  void stepOutOfFunction(int targetContextGroupId);
  void scheduleStepIntoAsync(
      std::unique_ptr<ScheduleStepIntoAsyncCallback> callback,
      int targetContextGroupId);
  void pauseOnAsyncCall(int targetContextGroupId, uintptr_t task,
                        const String16& debuggerId);

  void terminateExecution(std::unique_ptr<TerminateExecutionCallback> callback);

  Response continueToLocation(int targetContextGroupId,
                              V8DebuggerScript* script,
                              std::unique_ptr<protocol::Debugger::Location>,
                              const String16& targetCallFramess);

  // Each script inherits debug data from v8::Context where it has been
  // compiled.
  // Only scripts whose debug data matches |contextGroupId| will be reported.
  // Passing 0 will result in reporting all scripts.
  void getCompiledScripts(int contextGroupId,
                          std::vector<std::unique_ptr<V8DebuggerScript>>&);
  void enable();
  void disable();

  bool isPaused() const { return m_pausedContextGroupId; }
  bool isPausedInContextGroup(int contextGroupId) const;

  int maxAsyncCallChainDepth() { return m_maxAsyncCallStackDepth; }
  void setAsyncCallStackDepth(V8DebuggerAgentImpl*, int);

  std::shared_ptr<AsyncStackTrace> currentAsyncParent();
  V8StackTraceId currentExternalParent();

  std::shared_ptr<StackFrame> symbolize(v8::Local<v8::StackFrame> v8Frame);

  std::unique_ptr<V8StackTraceImpl> createStackTrace(v8::Local<v8::StackTrace>);
  std::unique_ptr<V8StackTraceImpl> captureStackTrace(bool fullStack);

  v8::MaybeLocal<v8::Array> internalProperties(v8::Local<v8::Context>,
                                               v8::Local<v8::Value>);

  v8::Local<v8::Array> queryObjects(v8::Local<v8::Context> context,
                                    v8::Local<v8::Object> prototype);

  void asyncTaskScheduled(const StringView& taskName, void* task,
                          bool recurring);
  void asyncTaskCanceled(void* task);
  void asyncTaskStarted(void* task);
  void asyncTaskFinished(void* task);
  void allAsyncTasksCanceled();

  V8StackTraceId storeCurrentStackTrace(const StringView& description);
  void externalAsyncTaskStarted(const V8StackTraceId& parent);
  void externalAsyncTaskFinished(const V8StackTraceId& parent);

  uintptr_t storeStackTrace(std::shared_ptr<AsyncStackTrace> stack);

  void muteScriptParsedEvents();
  void unmuteScriptParsedEvents();

  V8InspectorImpl* inspector() { return m_inspector; }

  WasmTranslation* wasmTranslation() { return &m_wasmTranslation; }

  void setMaxAsyncTaskStacksForTest(int limit);
  void dumpAsyncTaskStacksStateForTest();

  v8_inspector::V8StackTraceId scheduledAsyncCall() {
    return m_scheduledAsyncCall;
  }

  std::pair<int64_t, int64_t> debuggerIdFor(int contextGroupId);
  std::pair<int64_t, int64_t> debuggerIdFor(
      const String16& serializedDebuggerId);
  std::shared_ptr<AsyncStackTrace> stackTraceFor(int contextGroupId,
                                                 const V8StackTraceId& id);

 private:
  void clearContinueToLocation();
  bool shouldContinueToCurrentLocation();

  static size_t nearHeapLimitCallback(void* data, size_t current_heap_limit,
                                      size_t initial_heap_limit);
  static void terminateExecutionCompletedCallback(v8::Isolate* isolate);

  void handleProgramBreak(
      v8::Local<v8::Context> pausedContext, v8::Local<v8::Value> exception,
      const std::vector<v8::debug::BreakpointId>& hitBreakpoints,
      v8::debug::ExceptionType exception_type = v8::debug::kException,
      bool isUncaught = false);

  enum ScopeTargetKind {
    FUNCTION,
    GENERATOR,
  };
  v8::MaybeLocal<v8::Value> getTargetScopes(v8::Local<v8::Context>,
                                            v8::Local<v8::Value>,
                                            ScopeTargetKind);

  v8::MaybeLocal<v8::Value> functionScopes(v8::Local<v8::Context>,
                                           v8::Local<v8::Function>);
  v8::MaybeLocal<v8::Value> generatorScopes(v8::Local<v8::Context>,
                                            v8::Local<v8::Value>);

  void asyncTaskScheduledForStack(const String16& taskName, void* task,
                                  bool recurring);
  void asyncTaskCanceledForStack(void* task);
  void asyncTaskStartedForStack(void* task);
  void asyncTaskFinishedForStack(void* task);

  void asyncTaskCandidateForStepping(void* task, bool isLocal);
  void asyncTaskStartedForStepping(void* task);
  void asyncTaskFinishedForStepping(void* task);
  void asyncTaskCanceledForStepping(void* task);

  // v8::debug::DebugEventListener implementation.
  void AsyncEventOccurred(v8::debug::DebugAsyncActionType type, int id,
                          bool isBlackboxed) override;
  void ScriptCompiled(v8::Local<v8::debug::Script> script, bool is_live_edited,
                      bool has_compile_error) override;
  void BreakProgramRequested(
      v8::Local<v8::Context> paused_context,
      const std::vector<v8::debug::BreakpointId>& break_points_hit) override;
  void ExceptionThrown(v8::Local<v8::Context> paused_context,
                       v8::Local<v8::Value> exception,
                       v8::Local<v8::Value> promise, bool is_uncaught,
                       v8::debug::ExceptionType exception_type) override;
  bool IsFunctionBlackboxed(v8::Local<v8::debug::Script> script,
                            const v8::debug::Location& start,
                            const v8::debug::Location& end) override;

  int currentContextGroupId();
  bool asyncStepOutOfFunction(int targetContextGroupId, bool onlyAtReturn);

  v8::MaybeLocal<v8::Uint32> stableObjectId(v8::Local<v8::Context>,
                                            v8::Local<v8::Value>);

  v8::Isolate* m_isolate;
  V8InspectorImpl* m_inspector;
  int m_enableCount;
  int m_breakpointsActiveCount = 0;
  int m_ignoreScriptParsedEventsCounter;
  size_t m_originalHeapLimit = 0;
  bool m_scheduledOOMBreak = false;
  bool m_scheduledAssertBreak = false;
  int m_targetContextGroupId = 0;
  int m_pausedContextGroupId = 0;
  int m_continueToLocationBreakpointId;
  String16 m_continueToLocationTargetCallFrames;
  std::unique_ptr<V8StackTraceImpl> m_continueToLocationStack;

  using AsyncTaskToStackTrace =
      std::unordered_map<void*, std::weak_ptr<AsyncStackTrace>>;
  AsyncTaskToStackTrace m_asyncTaskStacks;
  std::unordered_set<void*> m_recurringTasks;

  int m_maxAsyncCallStacks;
  int m_maxAsyncCallStackDepth;

  std::vector<void*> m_currentTasks;
  std::vector<std::shared_ptr<AsyncStackTrace>> m_currentAsyncParent;
  std::vector<V8StackTraceId> m_currentExternalParent;

  void collectOldAsyncStacksIfNeeded();
  int m_asyncStacksCount = 0;
  // V8Debugger owns all the async stacks, while most of the other references
  // are weak, which allows to collect some stacks when there are too many.
  std::list<std::shared_ptr<AsyncStackTrace>> m_allAsyncStacks;
  std::unordered_map<int, std::weak_ptr<StackFrame>> m_framesCache;

  std::unordered_map<V8DebuggerAgentImpl*, int> m_maxAsyncCallStackDepthMap;
  void* m_taskWithScheduledBreak = nullptr;
  String16 m_taskWithScheduledBreakDebuggerId;

  std::unique_ptr<ScheduleStepIntoAsyncCallback> m_stepIntoAsyncCallback;
  bool m_breakRequested = false;

  v8::debug::ExceptionBreakState m_pauseOnExceptionsState;
  bool m_pauseOnAsyncCall = false;
  v8_inspector::V8StackTraceId m_scheduledAsyncCall;

  using StackTraceIdToStackTrace =
      std::unordered_map<uintptr_t, std::weak_ptr<AsyncStackTrace>>;
  StackTraceIdToStackTrace m_storedStackTraces;
  uintptr_t m_lastStackTraceId = 0;

  std::unordered_map<int, std::pair<int64_t, int64_t>>
      m_contextGroupIdToDebuggerId;
  std::unordered_map<String16, std::pair<int64_t, int64_t>>
      m_serializedDebuggerIdToDebuggerId;

  std::unique_ptr<TerminateExecutionCallback> m_terminateExecutionCallback;

  uint32_t m_lastStableObjectId = 0;
  v8::Global<v8::debug::WeakMap> m_stableObjectId;

  WasmTranslation m_wasmTranslation;

  DISALLOW_COPY_AND_ASSIGN(V8Debugger);
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8_DEBUGGER_H_
