// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_INTERFACE_H_
#define V8_DEBUG_DEBUG_INTERFACE_H_

#include "include/v8-inspector.h"
#include "include/v8-util.h"
#include "include/v8.h"

#include "src/debug/interface-types.h"
#include "src/globals.h"

namespace v8 {

namespace internal {
struct CoverageBlock;
struct CoverageFunction;
struct CoverageScript;
struct TypeProfileEntry;
struct TypeProfileScript;
class Coverage;
class PostponeInterruptsScope;
class Script;
class TypeProfile;
}  // namespace internal

namespace debug {

void SetContextId(Local<Context> context, int id);
int GetContextId(Local<Context> context);

void SetInspector(Isolate* isolate, v8_inspector::V8Inspector*);
v8_inspector::V8Inspector* GetInspector(Isolate* isolate);

// Schedule a debugger break to happen when function is called inside given
// isolate.
void SetBreakOnNextFunctionCall(Isolate* isolate);

// Remove scheduled debugger break in given isolate if it has not
// happened yet.
void ClearBreakOnNextFunctionCall(Isolate* isolate);

/**
 * Returns array of internal properties specific to the value type. Result has
 * the following format: [<name>, <value>,...,<name>, <value>]. Result array
 * will be allocated in the current context.
 */
MaybeLocal<Array> GetInternalProperties(Isolate* isolate, Local<Value> value);

enum ExceptionBreakState {
  NoBreakOnException = 0,
  BreakOnUncaughtException = 1,
  BreakOnAnyException = 2
};

/**
 * Defines if VM will pause on exceptions or not.
 * If BreakOnAnyExceptions is set then VM will pause on caught and uncaught
 * exception, if BreakOnUncaughtException is set then VM will pause only on
 * uncaught exception, otherwise VM won't stop on any exception.
 */
void ChangeBreakOnException(Isolate* isolate, ExceptionBreakState state);

void RemoveBreakpoint(Isolate* isolate, BreakpointId id);
void SetBreakPointsActive(Isolate* isolate, bool is_active);

enum StepAction {
  StepOut = 0,   // Step out of the current function.
  StepNext = 1,  // Step to the next statement in the current function.
  StepIn = 2     // Step into new functions invoked or the next statement
                 // in the current function.
};

void PrepareStep(Isolate* isolate, StepAction action);
void ClearStepping(Isolate* isolate);
void BreakRightNow(Isolate* isolate);

bool AllFramesOnStackAreBlackboxed(Isolate* isolate);

class Script;

struct LiveEditResult {
  enum Status {
    OK,
    COMPILE_ERROR,
    BLOCKED_BY_RUNNING_GENERATOR,
    BLOCKED_BY_FUNCTION_ABOVE_BREAK_FRAME,
    BLOCKED_BY_FUNCTION_BELOW_NON_DROPPABLE_FRAME,
    BLOCKED_BY_ACTIVE_FUNCTION,
    BLOCKED_BY_NEW_TARGET_IN_RESTART_FRAME,
    FRAME_RESTART_IS_NOT_SUPPORTED
  };
  Status status = OK;
  bool stack_changed = false;
  // Available only for OK.
  v8::Local<v8::debug::Script> script;
  // Fields below are available only for COMPILE_ERROR.
  v8::Local<v8::String> message;
  int line_number = -1;
  int column_number = -1;
};

/**
 * Native wrapper around v8::internal::Script object.
 */
class V8_EXPORT_PRIVATE Script {
 public:
  v8::Isolate* GetIsolate() const;

  ScriptOriginOptions OriginOptions() const;
  bool WasCompiled() const;
  bool IsEmbedded() const;
  int Id() const;
  int LineOffset() const;
  int ColumnOffset() const;
  std::vector<int> LineEnds() const;
  MaybeLocal<String> Name() const;
  MaybeLocal<String> SourceURL() const;
  MaybeLocal<String> SourceMappingURL() const;
  Maybe<int> ContextId() const;
  MaybeLocal<String> Source() const;
  bool IsWasm() const;
  bool IsModule() const;
  bool GetPossibleBreakpoints(
      const debug::Location& start, const debug::Location& end,
      bool restrict_to_function,
      std::vector<debug::BreakLocation>* locations) const;
  int GetSourceOffset(const debug::Location& location) const;
  v8::debug::Location GetSourceLocation(int offset) const;
  bool SetScriptSource(v8::Local<v8::String> newSource, bool preview,
                       LiveEditResult* result) const;
  bool SetBreakpoint(v8::Local<v8::String> condition, debug::Location* location,
                     BreakpointId* id) const;
};

// Specialization for wasm Scripts.
class WasmScript : public Script {
 public:
  static WasmScript* Cast(Script* script);

  int NumFunctions() const;
  int NumImportedFunctions() const;

  std::pair<int, int> GetFunctionRange(int function_index) const;

  debug::WasmDisassembly DisassembleFunction(int function_index) const;
  uint32_t GetFunctionHash(int function_index);
};

void GetLoadedScripts(Isolate* isolate, PersistentValueVector<Script>& scripts);

MaybeLocal<UnboundScript> CompileInspectorScript(Isolate* isolate,
                                                 Local<String> source);

enum ExceptionType { kException, kPromiseRejection };

class DebugDelegate {
 public:
  virtual ~DebugDelegate() = default;
  virtual void ScriptCompiled(v8::Local<Script> script, bool is_live_edited,
                              bool has_compile_error) {}
  // |inspector_break_points_hit| contains id of breakpoints installed with
  // debug::Script::SetBreakpoint API.
  virtual void BreakProgramRequested(
      v8::Local<v8::Context> paused_context,
      const std::vector<debug::BreakpointId>& inspector_break_points_hit) {}
  virtual void ExceptionThrown(v8::Local<v8::Context> paused_context,
                               v8::Local<v8::Value> exception,
                               v8::Local<v8::Value> promise, bool is_uncaught,
                               ExceptionType exception_type) {}
  virtual bool IsFunctionBlackboxed(v8::Local<debug::Script> script,
                                    const debug::Location& start,
                                    const debug::Location& end) {
    return false;
  }
};

void SetDebugDelegate(Isolate* isolate, DebugDelegate* listener);

class AsyncEventDelegate {
 public:
  virtual ~AsyncEventDelegate() = default;
  virtual void AsyncEventOccurred(debug::DebugAsyncActionType type, int id,
                                  bool is_blackboxed) = 0;
};

void SetAsyncEventDelegate(Isolate* isolate, AsyncEventDelegate* delegate);

void ResetBlackboxedStateCache(Isolate* isolate,
                               v8::Local<debug::Script> script);

int EstimatedValueSize(Isolate* isolate, v8::Local<v8::Value> value);

enum Builtin {
  kObjectKeys,
  kObjectGetPrototypeOf,
  kObjectGetOwnPropertyDescriptor,
  kObjectGetOwnPropertyNames,
  kObjectGetOwnPropertySymbols,
};

Local<Function> GetBuiltin(Isolate* isolate, Builtin builtin);

V8_EXPORT_PRIVATE void SetConsoleDelegate(Isolate* isolate,
                                          ConsoleDelegate* delegate);

int GetStackFrameId(v8::Local<v8::StackFrame> frame);

v8::Local<v8::StackTrace> GetDetailedStackTrace(Isolate* isolate,
                                                v8::Local<v8::Object> error);

/**
 * Native wrapper around v8::internal::JSGeneratorObject object.
 */
class GeneratorObject {
 public:
  v8::MaybeLocal<debug::Script> Script();
  v8::Local<v8::Function> Function();
  debug::Location SuspendedLocation();
  bool IsSuspended();

  static v8::Local<debug::GeneratorObject> Cast(v8::Local<v8::Value> value);
};

/*
 * Provide API layer between inspector and code coverage.
 */
class V8_EXPORT_PRIVATE Coverage {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Coverage);

  enum Mode {
    // Make use of existing information in feedback vectors on the heap.
    // Only return a yes/no result. Optimization and GC are not affected.
    // Collecting best effort coverage does not reset counters.
    kBestEffort,
    // Disable optimization and prevent feedback vectors from being garbage
    // collected in order to preserve precise invocation counts. Collecting
    // precise count coverage resets counters to get incremental updates.
    kPreciseCount,
    // We are only interested in a yes/no result for the function. Optimization
    // and GC can be allowed once a function has been invoked. Collecting
    // precise binary coverage resets counters for incremental updates.
    kPreciseBinary,
    // Similar to the precise coverage modes but provides coverage at a
    // lower granularity. Design doc: goo.gl/lA2swZ.
    kBlockCount,
    kBlockBinary,
  };

  // Forward declarations.
  class ScriptData;
  class FunctionData;

  class V8_EXPORT_PRIVATE BlockData {
   public:
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(BlockData);

    int StartOffset() const;
    int EndOffset() const;
    uint32_t Count() const;

   private:
    explicit BlockData(i::CoverageBlock* block,
                       std::shared_ptr<i::Coverage> coverage)
        : block_(block), coverage_(std::move(coverage)) {}

    i::CoverageBlock* block_;
    std::shared_ptr<i::Coverage> coverage_;

    friend class v8::debug::Coverage::FunctionData;
  };

  class V8_EXPORT_PRIVATE FunctionData {
   public:
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(FunctionData);

    int StartOffset() const;
    int EndOffset() const;
    uint32_t Count() const;
    MaybeLocal<String> Name() const;
    size_t BlockCount() const;
    bool HasBlockCoverage() const;
    BlockData GetBlockData(size_t i) const;

   private:
    explicit FunctionData(i::CoverageFunction* function,
                          std::shared_ptr<i::Coverage> coverage)
        : function_(function), coverage_(std::move(coverage)) {}

    i::CoverageFunction* function_;
    std::shared_ptr<i::Coverage> coverage_;

    friend class v8::debug::Coverage::ScriptData;
  };

  class V8_EXPORT_PRIVATE ScriptData {
   public:
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ScriptData);

    Local<debug::Script> GetScript() const;
    size_t FunctionCount() const;
    FunctionData GetFunctionData(size_t i) const;

   private:
    explicit ScriptData(size_t index, std::shared_ptr<i::Coverage> c);

    i::CoverageScript* script_;
    std::shared_ptr<i::Coverage> coverage_;

    friend class v8::debug::Coverage;
  };

  static Coverage CollectPrecise(Isolate* isolate);
  static Coverage CollectBestEffort(Isolate* isolate);

  static void SelectMode(Isolate* isolate, Mode mode);

  size_t ScriptCount() const;
  ScriptData GetScriptData(size_t i) const;
  bool IsEmpty() const { return coverage_ == nullptr; }

 private:
  explicit Coverage(std::shared_ptr<i::Coverage> coverage)
      : coverage_(std::move(coverage)) {}
  std::shared_ptr<i::Coverage> coverage_;
};

/*
 * Provide API layer between inspector and type profile.
 */
class V8_EXPORT_PRIVATE TypeProfile {
 public:
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(TypeProfile);

  enum Mode {
    kNone,
    kCollect,
  };
  class ScriptData;  // Forward declaration.

  class V8_EXPORT_PRIVATE Entry {
   public:
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(Entry);

    int SourcePosition() const;
    std::vector<MaybeLocal<String>> Types() const;

   private:
    explicit Entry(const i::TypeProfileEntry* entry,
                   std::shared_ptr<i::TypeProfile> type_profile)
        : entry_(entry), type_profile_(std::move(type_profile)) {}

    const i::TypeProfileEntry* entry_;
    std::shared_ptr<i::TypeProfile> type_profile_;

    friend class v8::debug::TypeProfile::ScriptData;
  };

  class V8_EXPORT_PRIVATE ScriptData {
   public:
    MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(ScriptData);

    Local<debug::Script> GetScript() const;
    std::vector<Entry> Entries() const;

   private:
    explicit ScriptData(size_t index,
                        std::shared_ptr<i::TypeProfile> type_profile);

    i::TypeProfileScript* script_;
    std::shared_ptr<i::TypeProfile> type_profile_;

    friend class v8::debug::TypeProfile;
  };

  static TypeProfile Collect(Isolate* isolate);

  static void SelectMode(Isolate* isolate, Mode mode);

  size_t ScriptCount() const;
  ScriptData GetScriptData(size_t i) const;

 private:
  explicit TypeProfile(std::shared_ptr<i::TypeProfile> type_profile)
      : type_profile_(std::move(type_profile)) {}

  std::shared_ptr<i::TypeProfile> type_profile_;
};

class ScopeIterator {
 public:
  static std::unique_ptr<ScopeIterator> CreateForFunction(
      v8::Isolate* isolate, v8::Local<v8::Function> func);
  static std::unique_ptr<ScopeIterator> CreateForGeneratorObject(
      v8::Isolate* isolate, v8::Local<v8::Object> generator);

  ScopeIterator() = default;
  virtual ~ScopeIterator() = default;

  enum ScopeType {
    ScopeTypeGlobal = 0,
    ScopeTypeLocal,
    ScopeTypeWith,
    ScopeTypeClosure,
    ScopeTypeCatch,
    ScopeTypeBlock,
    ScopeTypeScript,
    ScopeTypeEval,
    ScopeTypeModule
  };

  virtual bool Done() = 0;
  virtual void Advance() = 0;
  virtual ScopeType GetType() = 0;
  virtual v8::Local<v8::Object> GetObject() = 0;
  virtual v8::Local<v8::Value> GetFunctionDebugName() = 0;
  virtual int GetScriptId() = 0;
  virtual bool HasLocationInfo() = 0;
  virtual debug::Location GetStartLocation() = 0;
  virtual debug::Location GetEndLocation() = 0;

  virtual bool SetVariableValue(v8::Local<v8::String> name,
                                v8::Local<v8::Value> value) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopeIterator);
};

class StackTraceIterator {
 public:
  static std::unique_ptr<StackTraceIterator> Create(Isolate* isolate,
                                                    int index = 0);
  StackTraceIterator() = default;
  virtual ~StackTraceIterator() = default;

  virtual bool Done() const = 0;
  virtual void Advance() = 0;

  virtual int GetContextId() const = 0;
  virtual v8::MaybeLocal<v8::Value> GetReceiver() const = 0;
  virtual v8::Local<v8::Value> GetReturnValue() const = 0;
  virtual v8::Local<v8::String> GetFunctionDebugName() const = 0;
  virtual v8::Local<v8::debug::Script> GetScript() const = 0;
  virtual debug::Location GetSourceLocation() const = 0;
  virtual v8::Local<v8::Function> GetFunction() const = 0;
  virtual std::unique_ptr<ScopeIterator> GetScopeIterator() const = 0;

  virtual bool Restart() = 0;
  virtual v8::MaybeLocal<v8::Value> Evaluate(v8::Local<v8::String> source,
                                             bool throw_on_side_effect) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(StackTraceIterator);
};

class QueryObjectPredicate {
 public:
  virtual ~QueryObjectPredicate() = default;
  virtual bool Filter(v8::Local<v8::Object> object) = 0;
};

void QueryObjects(v8::Local<v8::Context> context,
                  QueryObjectPredicate* predicate,
                  v8::PersistentValueVector<v8::Object>* objects);

void GlobalLexicalScopeNames(v8::Local<v8::Context> context,
                             v8::PersistentValueVector<v8::String>* names);

void SetReturnValue(v8::Isolate* isolate, v8::Local<v8::Value> value);

enum class NativeAccessorType {
  None = 0,
  HasGetter = 1 << 0,
  HasSetter = 1 << 1,
  IsBuiltin = 1 << 2
};

int GetNativeAccessorDescriptor(v8::Local<v8::Context> context,
                                v8::Local<v8::Object> object,
                                v8::Local<v8::Name> name);

int64_t GetNextRandomInt64(v8::Isolate* isolate);

v8::MaybeLocal<v8::Value> EvaluateGlobal(v8::Isolate* isolate,
                                         v8::Local<v8::String> source,
                                         bool throw_on_side_effect);

int GetDebuggingId(v8::Local<v8::Function> function);

bool SetFunctionBreakpoint(v8::Local<v8::Function> function,
                           v8::Local<v8::String> condition, BreakpointId* id);

v8::Platform* GetCurrentPlatform();

class PostponeInterruptsScope {
 public:
  explicit PostponeInterruptsScope(v8::Isolate* isolate);
  ~PostponeInterruptsScope();

 private:
  std::unique_ptr<i::PostponeInterruptsScope> scope_;
};

class WeakMap : public v8::Object {
 public:
  V8_WARN_UNUSED_RESULT v8::MaybeLocal<v8::Value> Get(
      v8::Local<v8::Context> context, v8::Local<v8::Value> key);
  V8_WARN_UNUSED_RESULT v8::MaybeLocal<WeakMap> Set(
      v8::Local<v8::Context> context, v8::Local<v8::Value> key,
      v8::Local<v8::Value> value);

  static Local<WeakMap> New(v8::Isolate* isolate);
  V8_INLINE static WeakMap* Cast(Value* obj);

 private:
  WeakMap();
};
}  // namespace debug
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_INTERFACE_H_
