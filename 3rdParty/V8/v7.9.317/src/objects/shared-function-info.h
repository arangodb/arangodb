// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_H_

#include <memory>

#include "src/codegen/bailout-reason.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/function-kind.h"
#include "src/objects/function-syntax-kind.h"
#include "src/objects/objects.h"
#include "src/objects/script.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/struct.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "torque-generated/field-offsets-tq.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {

namespace tracing {
class TracedValue;
}

namespace internal {

class AsmWasmData;
class BytecodeArray;
class CoverageInfo;
class DebugInfo;
class IsCompiledScope;
class WasmCapiFunctionData;
class WasmExportedFunctionData;
class WasmJSFunctionData;

// Data collected by the pre-parser storing information about scopes and inner
// functions.
//
// PreparseData Layout:
// +-------------------------------+
// | data_length | children_length |
// +-------------------------------+
// | Scope Byte Data ...           |
// | ...                           |
// +-------------------------------+
// | [Padding]                     |
// +-------------------------------+
// | Inner PreparseData 1          |
// +-------------------------------+
// | ...                           |
// +-------------------------------+
// | Inner PreparseData N          |
// +-------------------------------+
class PreparseData
    : public TorqueGeneratedPreparseData<PreparseData, HeapObject> {
 public:
  inline int inner_start_offset() const;
  inline ObjectSlot inner_data_start() const;

  inline byte get(int index) const;
  inline void set(int index, byte value);
  inline void copy_in(int index, const byte* buffer, int length);

  inline PreparseData get_child(int index) const;
  inline void set_child(int index, PreparseData value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Clear uninitialized padding space.
  inline void clear_padding();

  DECL_PRINTER(PreparseData)
  DECL_VERIFIER(PreparseData)

  static const int kDataStartOffset = kSize;

  class BodyDescriptor;

  static int InnerOffset(int data_length) {
    return RoundUp(kDataStartOffset + data_length * kByteSize, kTaggedSize);
  }

  static int SizeFor(int data_length, int children_length) {
    return InnerOffset(data_length) + children_length * kTaggedSize;
  }

  TQ_OBJECT_CONSTRUCTORS(PreparseData)

 private:
  inline Object get_child_raw(int index) const;
};

// Abstract class representing extra data for an uncompiled function, which is
// not stored in the SharedFunctionInfo.
class UncompiledData
    : public TorqueGeneratedUncompiledData<UncompiledData, HeapObject> {
 public:
  inline static void Initialize(
      UncompiledData data, String inferred_name, int start_position,
      int end_position,
      std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
          gc_notify_updated_slot =
              [](HeapObject object, ObjectSlot slot, HeapObject target) {});

  using BodyDescriptor =
      FixedBodyDescriptor<kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                          kHeaderSize>;

  TQ_OBJECT_CONSTRUCTORS(UncompiledData)
};

// Class representing data for an uncompiled function that does not have any
// data from the pre-parser, either because it's a leaf function or because the
// pre-parser bailed out.
class UncompiledDataWithoutPreparseData
    : public TorqueGeneratedUncompiledDataWithoutPreparseData<
          UncompiledDataWithoutPreparseData, UncompiledData> {
 public:
  DECL_PRINTER(UncompiledDataWithoutPreparseData)

  // No extra fields compared to UncompiledData.
  using BodyDescriptor = UncompiledData::BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(UncompiledDataWithoutPreparseData)
};

// Class representing data for an uncompiled function that has pre-parsed scope
// data.
class UncompiledDataWithPreparseData
    : public TorqueGeneratedUncompiledDataWithPreparseData<
          UncompiledDataWithPreparseData, UncompiledData> {
 public:
  DECL_PRINTER(UncompiledDataWithPreparseData)

  inline static void Initialize(
      UncompiledDataWithPreparseData data, String inferred_name,
      int start_position, int end_position, PreparseData scope_data,
      std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
          gc_notify_updated_slot =
              [](HeapObject object, ObjectSlot slot, HeapObject target) {});

  using BodyDescriptor = SubclassBodyDescriptor<
      UncompiledData::BodyDescriptor,
      FixedBodyDescriptor<kStartOfStrongFieldsOffset, kEndOfStrongFieldsOffset,
                          kSize>>;

  TQ_OBJECT_CONSTRUCTORS(UncompiledDataWithPreparseData)
};

class InterpreterData : public Struct {
 public:
  DECL_ACCESSORS(bytecode_array, BytecodeArray)
  DECL_ACCESSORS(interpreter_trampoline, Code)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_INTERPRETER_DATA_FIELDS)

  DECL_CAST(InterpreterData)
  DECL_PRINTER(InterpreterData)
  DECL_VERIFIER(InterpreterData)

  OBJECT_CONSTRUCTORS(InterpreterData, Struct);
};

// SharedFunctionInfo describes the JSFunction information that can be
// shared by multiple instances of the function.
class SharedFunctionInfo : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE

  V8_EXPORT_PRIVATE static constexpr Object const kNoSharedNameSentinel =
      Smi::kZero;

  // [name]: Returns shared name if it exists or an empty string otherwise.
  inline String Name() const;
  inline void SetName(String name);

  // Get the code object which represents the execution of this function.
  V8_EXPORT_PRIVATE Code GetCode() const;

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode abstract_code();

  // Tells whether or not this shared function info is interpreted.
  //
  // Note: function->IsInterpreted() does not necessarily return the same value
  // as function->shared()->IsInterpreted() because the closure might have been
  // optimized.
  inline bool IsInterpreted() const;

  // Set up the link between shared function info and the script. The shared
  // function info is added to the list on the script.
  V8_EXPORT_PRIVATE static void SetScript(
      Handle<SharedFunctionInfo> shared, Handle<HeapObject> script_object,
      int function_literal_id, bool reset_preparsed_scope_data = true);

  // Layout description of the optimized code map.
  static const int kEntriesStart = 0;
  static const int kContextOffset = 0;
  static const int kCachedCodeOffset = 1;
  static const int kEntryLength = 2;
  static const int kInitialLength = kEntriesStart + kEntryLength;

  static const int kNotFound = -1;

  // [scope_info]: Scope info.
  DECL_ACCESSORS(scope_info, ScopeInfo)

  // Set scope_info without moving the existing name onto the ScopeInfo.
  inline void set_raw_scope_info(ScopeInfo scope_info,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // End position of this function in the script source.
  V8_EXPORT_PRIVATE int EndPosition() const;

  // Start position of this function in the script source.
  V8_EXPORT_PRIVATE int StartPosition() const;

  // Set the start and end position of this function in the script source.
  // Updates the scope info if available.
  V8_EXPORT_PRIVATE void SetPosition(int start_position, int end_position);

  // [outer scope info | feedback metadata] Shared storage for outer scope info
  // (on uncompiled functions) and feedback metadata (on compiled functions).
  DECL_ACCESSORS(raw_outer_scope_info_or_feedback_metadata, HeapObject)

  // Get the outer scope info whether this function is compiled or not.
  inline bool HasOuterScopeInfo() const;
  inline ScopeInfo GetOuterScopeInfo() const;

  // [feedback metadata] Metadata template for feedback vectors of instances of
  // this function.
  inline bool HasFeedbackMetadata() const;
  DECL_ACCESSORS(feedback_metadata, FeedbackMetadata)

  // Returns if this function has been compiled yet. Note: with bytecode
  // flushing, any GC after this call is made could cause the function
  // to become uncompiled. If you need to ensure the function remains compiled
  // for some period of time, use IsCompiledScope instead.
  inline bool is_compiled() const;

  // Returns an IsCompiledScope which reports whether the function is compiled,
  // and if compiled, will avoid the function becoming uncompiled while it is
  // held.
  inline IsCompiledScope is_compiled_scope() const;

  // [length]: The function length - usually the number of declared parameters.
  // Use up to 2^16-2 parameters (16 bits of values, where one is reserved for
  // kDontAdaptArgumentsSentinel). The value is only reliable when the function
  // has been compiled.
  inline uint16_t length() const;
  inline void set_length(int value);

  // [internal formal parameter count]: The declared number of parameters.
  // For subclass constructors, also includes new.target.
  // The size of function's frame is internal_formal_parameter_count + 1.
  DECL_UINT16_ACCESSORS(internal_formal_parameter_count)

  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [expected_nof_properties]: Expected number of properties for the
  // function. The value is only reliable when the function has been compiled.
  DECL_UINT16_ACCESSORS(expected_nof_properties)

  // [function_literal_id] - uniquely identifies the FunctionLiteral this
  // SharedFunctionInfo represents within its script, or -1 if this
  // SharedFunctionInfo object doesn't correspond to a parsed FunctionLiteral.
  DECL_INT32_ACCESSORS(function_literal_id)

#if V8_SFI_HAS_UNIQUE_ID
  // [unique_id] - For --trace-maps purposes, an identifier that's persistent
  // even if the GC moves this SharedFunctionInfo.
  DECL_INT_ACCESSORS(unique_id)
#endif

  // [function data]: This field holds some additional data for function.
  // Currently it has one of:
  //  - a FunctionTemplateInfo to make benefit the API [IsApiFunction()].
  //  - a BytecodeArray for the interpreter [HasBytecodeArray()].
  //  - a InterpreterData with the BytecodeArray and a copy of the
  //    interpreter trampoline [HasInterpreterData()]
  //  - an AsmWasmData with Asm->Wasm conversion [HasAsmWasmData()].
  //  - a Smi containing the builtin id [HasBuiltinId()]
  //  - a UncompiledDataWithoutPreparseData for lazy compilation
  //    [HasUncompiledDataWithoutPreparseData()]
  //  - a UncompiledDataWithPreparseData for lazy compilation
  //    [HasUncompiledDataWithPreparseData()]
  //  - a WasmExportedFunctionData for Wasm [HasWasmExportedFunctionData()]
  DECL_ACCESSORS(function_data, Object)

  inline bool IsApiFunction() const;
  inline bool is_class_constructor() const;
  inline FunctionTemplateInfo get_api_func_data();
  inline void set_api_func_data(FunctionTemplateInfo data);
  inline bool HasBytecodeArray() const;
  inline BytecodeArray GetBytecodeArray() const;
  inline void set_bytecode_array(BytecodeArray bytecode);
  inline Code InterpreterTrampoline() const;
  inline bool HasInterpreterData() const;
  inline InterpreterData interpreter_data() const;
  inline void set_interpreter_data(InterpreterData interpreter_data);
  inline BytecodeArray GetDebugBytecodeArray() const;
  inline void SetDebugBytecodeArray(BytecodeArray bytecode);
  inline bool HasAsmWasmData() const;
  inline AsmWasmData asm_wasm_data() const;
  inline void set_asm_wasm_data(AsmWasmData data);

  // builtin_id corresponds to the auto-generated Builtins::Name id.
  inline bool HasBuiltinId() const;
  inline int builtin_id() const;
  inline void set_builtin_id(int builtin_id);
  inline bool HasUncompiledData() const;
  inline UncompiledData uncompiled_data() const;
  inline void set_uncompiled_data(UncompiledData data);
  inline bool HasUncompiledDataWithPreparseData() const;
  inline UncompiledDataWithPreparseData uncompiled_data_with_preparse_data()
      const;
  inline void set_uncompiled_data_with_preparse_data(
      UncompiledDataWithPreparseData data);
  inline bool HasUncompiledDataWithoutPreparseData() const;
  inline bool HasWasmExportedFunctionData() const;
  WasmExportedFunctionData wasm_exported_function_data() const;
  inline bool HasWasmJSFunctionData() const;
  WasmJSFunctionData wasm_js_function_data() const;
  inline bool HasWasmCapiFunctionData() const;
  WasmCapiFunctionData wasm_capi_function_data() const;

  // Clear out pre-parsed scope data from UncompiledDataWithPreparseData,
  // turning it into UncompiledDataWithoutPreparseData.
  inline void ClearPreparseData();

  // The inferred_name is inferred from variable or property assignment of this
  // function. It is used to facilitate debugging and profiling of JavaScript
  // code written in OO style, where almost all functions are anonymous but are
  // assigned to object properties.
  inline bool HasInferredName();
  inline String inferred_name();

  // Break infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  V8_EXPORT_PRIVATE bool HasBreakInfo() const;
  bool BreakAtEntry() const;

  // Coverage infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  bool HasCoverageInfo() const;
  CoverageInfo GetCoverageInfo() const;

  // The function's name if it is non-empty, otherwise the inferred name.
  String DebugName();

  // Used for flags such as --turbo-filter.
  bool PassesFilter(const char* raw_filter);

  // [script_or_debug_info]: One of:
  //  - Script from which the function originates.
  //  - a DebugInfo which holds the actual script [HasDebugInfo()].
  DECL_ACCESSORS(script_or_debug_info, HeapObject)

  inline HeapObject script() const;
  inline void set_script(HeapObject script);

  // The function is subject to debugging if a debug info is attached.
  inline bool HasDebugInfo() const;
  inline DebugInfo GetDebugInfo() const;
  inline void SetDebugInfo(DebugInfo debug_info);

  // The offset of the 'function' token in the script source relative to the
  // start position. Can return kFunctionTokenOutOfRange if offset doesn't
  // fit in 16 bits.
  DECL_UINT16_ACCESSORS(raw_function_token_offset)

  // The position of the 'function' token in the script source. Can return
  // kNoSourcePosition if raw_function_token_offset() returns
  // kFunctionTokenOutOfRange.
  inline int function_token_position() const;

  // Returns true if the function has shared name.
  inline bool HasSharedName() const;

  // [flags] Bit field containing various flags about the function.
  DECL_INT32_ACCESSORS(flags)

  // Is this function a top-level function (scripts, evals).
  DECL_BOOLEAN_ACCESSORS(is_toplevel)

  // Indicates if this function can be lazy compiled.
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation)

  // Indicates the language mode.
  inline LanguageMode language_mode() const;
  inline void set_language_mode(LanguageMode language_mode);

  // How the function appears in source text.
  DECL_PRIMITIVE_ACCESSORS(syntax_kind, FunctionSyntaxKind)

  // Indicates whether the source is implicitly wrapped in a function.
  inline bool is_wrapped() const;

  // True if the function has any duplicated parameter names.
  DECL_BOOLEAN_ACCESSORS(has_duplicate_parameters)

  // Indicates whether the function is a native function.
  // These needs special treatment in .call and .apply since
  // null passed as the receiver should not be translated to the
  // global object.
  DECL_BOOLEAN_ACCESSORS(native)

  // Indicates that asm->wasm conversion failed and should not be re-attempted.
  DECL_BOOLEAN_ACCESSORS(is_asm_wasm_broken)

  // Indicates that the function was created by the Function function.
  // Though it's anonymous, toString should treat it as if it had the name
  // "anonymous".  We don't set the name itself so that the system does not
  // see a binding for it.
  DECL_BOOLEAN_ACCESSORS(name_should_print_as_anonymous)

  // Indicates that the function represented by the shared function info was
  // classed as an immediately invoked function execution (IIFE) function and
  // is only executed once.
  DECL_BOOLEAN_ACCESSORS(is_oneshot_iife)

  // Whether or not the number of expected properties may change.
  DECL_BOOLEAN_ACCESSORS(are_properties_final)

  // Indicates that the function represented by the shared function info
  // cannot observe the actual parameters passed at a call site, which
  // means the function doesn't use the arguments object, doesn't use
  // rest parameters, and is also in strict mode (meaning that there's
  // no way to get to the actual arguments via the non-standard "arguments"
  // accessor on sloppy mode functions). This can be used to speed up calls
  // to this function even in the presence of arguments mismatch.
  // See http://bit.ly/v8-faster-calls-with-arguments-mismatch for more
  // information on this.
  DECL_BOOLEAN_ACCESSORS(is_safe_to_skip_arguments_adaptor)

  // Indicates that the function has been reported for binary code coverage.
  DECL_BOOLEAN_ACCESSORS(has_reported_binary_coverage)

  // Indicates that the private name lookups inside the function skips the
  // closest outer class scope.
  DECL_BOOLEAN_ACCESSORS(private_name_lookup_skips_outer_class)

  inline FunctionKind kind() const;

  // Defines the index in a native context of closure's map instantiated using
  // this shared function info.
  DECL_INT_ACCESSORS(function_map_index)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  // Recalculates the |map_index| value after modifications of this shared info.
  inline void UpdateFunctionMapIndex();

  // Indicates whether optimizations have been disabled for this shared function
  // info. If we cannot optimize the function we disable optimization to avoid
  // spending time attempting to optimize it again.
  inline bool optimization_disabled() const;

  // The reason why optimization was disabled.
  inline BailoutReason disable_optimization_reason() const;

  // Disable (further) attempted optimization of all functions sharing this
  // shared function info.
  void DisableOptimization(BailoutReason reason);

  // This class constructor needs to call out to an instance fields
  // initializer. This flag is set when creating the
  // SharedFunctionInfo as a reminder to emit the initializer call
  // when generating code later.
  DECL_BOOLEAN_ACCESSORS(requires_instance_members_initializer)

  // [source code]: Source code for the function.
  bool HasSourceCode() const;
  static Handle<Object> GetSourceCode(Handle<SharedFunctionInfo> shared);
  static Handle<Object> GetSourceCodeHarmony(Handle<SharedFunctionInfo> shared);

  // Tells whether this function should be subject to debugging, e.g. for
  // - scope inspection
  // - internal break points
  // - coverage and type profile
  // - error stack trace
  inline bool IsSubjectToDebugging();

  // Whether this function is defined in user-provided JavaScript code.
  inline bool IsUserJavaScript();

  // True if one can flush compiled code from this function, in such a way that
  // it can later be re-compiled.
  inline bool CanDiscardCompiled() const;

  // Flush compiled data from this function, setting it back to CompileLazy and
  // clearing any compiled metadata.
  V8_EXPORT_PRIVATE static void DiscardCompiled(
      Isolate* isolate, Handle<SharedFunctionInfo> shared_info);

  // Discard the compiled metadata. If called during GC then
  // |gc_notify_updated_slot| should be used to record any slot updates.
  void DiscardCompiledMetadata(
      Isolate* isolate,
      std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
          gc_notify_updated_slot =
              [](HeapObject object, ObjectSlot slot, HeapObject target) {});

  // Returns true if the function has old bytecode that could be flushed. This
  // function shouldn't access any flags as it is used by concurrent marker.
  // Hence it takes the mode as an argument.
  inline bool ShouldFlushBytecode(BytecodeFlushMode mode);

  // Check whether or not this function is inlineable.
  bool IsInlineable();

  // Source size of this function.
  int SourceSize();

  // Returns `false` if formal parameters include rest parameters, optional
  // parameters, or destructuring parameters.
  // TODO(caitp): make this a flag set during parsing
  inline bool has_simple_parameters();

  // Initialize a SharedFunctionInfo from a parsed function literal.
  static void InitFromFunctionLiteral(Handle<SharedFunctionInfo> shared_info,
                                      FunctionLiteral* lit, bool is_toplevel);

  // Updates the expected number of properties based on estimate from parser.
  void UpdateExpectedNofPropertiesFromEstimate(FunctionLiteral* literal);
  void UpdateAndFinalizeExpectedNofPropertiesFromEstimate(
      FunctionLiteral* literal);

  // Sets the FunctionTokenOffset field based on the given token position and
  // start position.
  void SetFunctionTokenPosition(int function_token_position,
                                int start_position);

  static void EnsureSourcePositionsAvailable(
      Isolate* isolate, Handle<SharedFunctionInfo> shared_info);

  bool AreSourcePositionsAvailable() const;

  // Hash based on function literal id and script id.
  V8_EXPORT_PRIVATE uint32_t Hash();

  inline bool construct_as_builtin() const;

  // Determines and sets the ConstructAsBuiltinBit in |flags|, based on the
  // |function_data|. Must be called when creating the SFI after other fields
  // are initialized. The ConstructAsBuiltinBit determines whether
  // JSBuiltinsConstructStub or JSConstructStubGeneric should be called to
  // construct this function.
  inline void CalculateConstructAsBuiltin();

  // Dispatched behavior.
  DECL_PRINTER(SharedFunctionInfo)
  DECL_VERIFIER(SharedFunctionInfo)
#ifdef OBJECT_PRINT
  void PrintSourceCode(std::ostream& os);
#endif

  // Returns the SharedFunctionInfo in a format tracing can support.
  std::unique_ptr<v8::tracing::TracedValue> ToTracedValue(
      FunctionLiteral* literal);

  // The tracing scope for SharedFunctionInfo objects.
  static const char* kTraceScope;

  // Returns the unique TraceID for this SharedFunctionInfo (within the
  // kTraceScope, works only for functions that have a Script and start/end
  // position).
  uint64_t TraceID(FunctionLiteral* literal = nullptr) const;

  // Returns the unique trace ID reference for this SharedFunctionInfo
  // (based on the |TraceID()| above).
  std::unique_ptr<v8::tracing::TracedValue> TraceIDRef() const;

  // Iterate over all shared function infos in a given script.
  class ScriptIterator {
   public:
    V8_EXPORT_PRIVATE ScriptIterator(Isolate* isolate, Script script);
    explicit ScriptIterator(Handle<WeakFixedArray> shared_function_infos);
    V8_EXPORT_PRIVATE SharedFunctionInfo Next();
    int CurrentIndex() const { return index_ - 1; }

    // Reset the iterator to run on |script|.
    void Reset(Isolate* isolate, Script script);

   private:
    Handle<WeakFixedArray> shared_function_infos_;
    int index_;
    DISALLOW_COPY_AND_ASSIGN(ScriptIterator);
  };

  DECL_CAST(SharedFunctionInfo)

  // Constants.
  static const uint16_t kDontAdaptArgumentsSentinel = static_cast<uint16_t>(-1);

  static const int kMaximumFunctionTokenOffset = kMaxUInt16 - 1;
  static const uint16_t kFunctionTokenOutOfRange = static_cast<uint16_t>(-1);
  STATIC_ASSERT(kMaximumFunctionTokenOffset + 1 == kFunctionTokenOutOfRange);

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                TORQUE_GENERATED_SHARED_FUNCTION_INFO_FIELDS)

  static const int kAlignedSize = POINTER_SIZE_ALIGN(kSize);

  class BodyDescriptor;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)                               \
  /* Have FunctionKind first to make it cheaper to access */ \
  V(FunctionKindBits, FunctionKind, 5, _)                    \
  V(IsNativeBit, bool, 1, _)                                 \
  V(IsStrictBit, bool, 1, _)                                 \
  V(FunctionSyntaxKindBits, FunctionSyntaxKind, 3, _)        \
  V(IsClassConstructorBit, bool, 1, _)                       \
  V(HasDuplicateParametersBit, bool, 1, _)                   \
  V(AllowLazyCompilationBit, bool, 1, _)                     \
  V(NeedsHomeObjectBit, bool, 1, _)                          \
  V(IsAsmWasmBrokenBit, bool, 1, _)                          \
  V(FunctionMapIndexBits, int, 5, _)                         \
  V(DisabledOptimizationReasonBits, BailoutReason, 4, _)     \
  V(RequiresInstanceMembersInitializer, bool, 1, _)          \
  V(ConstructAsBuiltinBit, bool, 1, _)                       \
  V(NameShouldPrintAsAnonymousBit, bool, 1, _)               \
  V(HasReportedBinaryCoverageBit, bool, 1, _)                \
  V(IsTopLevelBit, bool, 1, _)                               \
  V(IsOneshotIIFEOrPropertiesAreFinalBit, bool, 1, _)        \
  V(IsSafeToSkipArgumentsAdaptorBit, bool, 1, _)             \
  V(PrivateNameLookupSkipsOuterClassBit, bool, 1, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  // Bailout reasons must fit in the DisabledOptimizationReason bitfield.
  STATIC_ASSERT(BailoutReason::kLastErrorMessage <=
                DisabledOptimizationReasonBits::kMax);

  STATIC_ASSERT(kLastFunctionKind <= FunctionKindBits::kMax);
  STATIC_ASSERT(FunctionSyntaxKind::kLastFunctionSyntaxKind <=
                FunctionSyntaxKindBits::kMax);

  // Indicates that this function uses a super property (or an eval that may
  // use a super property).
  // This is needed to set up the [[HomeObject]] on the function instance.
  inline bool needs_home_object() const;

 private:
  // [name_or_scope_info]: Function name string, kNoSharedNameSentinel or
  // ScopeInfo.
  DECL_ACCESSORS(name_or_scope_info, Object)

  // [outer scope info] The outer scope info, needed to lazily parse this
  // function.
  DECL_ACCESSORS(outer_scope_info, HeapObject)

  // [is_oneshot_iife_or_properties_are_final]: This bit is used to track
  // two mutually exclusive cases. Either this SharedFunctionInfo is
  // a oneshot_iife or we have finished parsing its properties. These cases
  // are mutually exclusive because the properties final bit is only used by
  // class constructors to handle lazily parsed properties and class
  // constructors can never be oneshot iifes.
  DECL_BOOLEAN_ACCESSORS(is_oneshot_iife_or_properties_are_final)

  inline void set_kind(FunctionKind kind);

  inline void set_needs_home_object(bool value);

  inline uint16_t get_property_estimate_from_literal(FunctionLiteral* literal);

  friend class Factory;
  friend class V8HeapExplorer;
  FRIEND_TEST(PreParserTest, LazyFunctionLength);

  OBJECT_CONSTRUCTORS(SharedFunctionInfo, HeapObject);
};

// Printing support.
struct SourceCodeOf {
  explicit SourceCodeOf(SharedFunctionInfo v, int max = -1)
      : value(v), max_length(max) {}
  const SharedFunctionInfo value;
  int max_length;
};

// IsCompiledScope enables a caller to check if a function is compiled, and
// ensure it remains compiled (i.e., doesn't have it's bytecode flushed) while
// the scope is retained.
class IsCompiledScope {
 public:
  inline IsCompiledScope(const SharedFunctionInfo shared, Isolate* isolate);
  inline IsCompiledScope() : retain_bytecode_(), is_compiled_(false) {}

  inline bool is_compiled() const { return is_compiled_; }

 private:
  MaybeHandle<BytecodeArray> retain_bytecode_;
  bool is_compiled_;
};

std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SHARED_FUNCTION_INFO_H_
