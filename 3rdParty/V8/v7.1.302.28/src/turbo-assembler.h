// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TURBO_ASSEMBLER_H_
#define V8_TURBO_ASSEMBLER_H_

#include "src/assembler-arch.h"
#include "src/base/template-utils.h"
#include "src/heap/heap.h"

namespace v8 {
namespace internal {

// Common base class for platform-specific TurboAssemblers containing
// platform-independent bits.
class V8_EXPORT_PRIVATE TurboAssemblerBase : public Assembler {
 public:
  Isolate* isolate() const {
    DCHECK(!options().v8_agnostic_code);
    return isolate_;
  }

  Handle<HeapObject> CodeObject() const {
    DCHECK(!code_object_.is_null());
    return code_object_;
  }

  bool root_array_available() const { return root_array_available_; }
  void set_root_array_available(bool v) { root_array_available_ = v; }

  bool trap_on_abort() const { return trap_on_abort_; }

  bool should_abort_hard() const { return hard_abort_; }
  void set_abort_hard(bool v) { hard_abort_ = v; }

  void set_builtin_index(int i) { maybe_builtin_index_ = i; }

  void set_has_frame(bool v) { has_frame_ = v; }
  bool has_frame() const { return has_frame_; }

  // Loads the given constant or external reference without embedding its direct
  // pointer. The produced code is isolate-independent.
  void IndirectLoadConstant(Register destination, Handle<HeapObject> object);
  void IndirectLoadExternalReference(Register destination,
                                     ExternalReference reference);

  virtual void LoadFromConstantsTable(Register destination,
                                      int constant_index) = 0;

  virtual void LoadRootRegisterOffset(Register destination,
                                      intptr_t offset) = 0;
  virtual void LoadRootRelative(Register destination, int32_t offset) = 0;

  virtual void LoadRoot(Register destination, RootIndex index) = 0;

  static int32_t RootRegisterOffset(RootIndex root_index);
  static int32_t RootRegisterOffsetForExternalReferenceIndex(
      int reference_index);

  static int32_t RootRegisterOffsetForBuiltinIndex(int builtin_index);

  static intptr_t RootRegisterOffsetForExternalReference(
      Isolate* isolate, const ExternalReference& reference);

  // An address is addressable through kRootRegister if it is located within
  // isolate->root_register_addressable_region().
  static bool IsAddressableThroughRootRegister(
      Isolate* isolate, const ExternalReference& reference);

 protected:
  TurboAssemblerBase(const AssemblerOptions& options, void* buffer,
                     int buffer_size)
      : TurboAssemblerBase(nullptr, options.EnableV8AgnosticCode(), buffer,
                           buffer_size, CodeObjectRequired::kNo) {}

  TurboAssemblerBase(Isolate* isolate, const AssemblerOptions& options,
                     void* buffer, int buffer_size,
                     CodeObjectRequired create_code_object);

  void RecordCommentForOffHeapTrampoline(int builtin_index);

  Isolate* const isolate_ = nullptr;

  // This handle will be patched with the code object on installation.
  Handle<HeapObject> code_object_;

  // Whether kRootRegister has been initialized.
  bool root_array_available_ = true;

  // Immediately trap instead of calling {Abort} when debug code fails.
  bool trap_on_abort_ = FLAG_trap_on_abort;

  // Emit a C call to abort instead of a runtime call.
  bool hard_abort_ = false;

  // May be set while generating builtins.
  int maybe_builtin_index_ = Builtins::kNoBuiltinId;

  bool has_frame_ = false;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TurboAssemblerBase);
};

// Avoids emitting calls to the {Builtins::kAbort} builtin when emitting debug
// code during the lifetime of this scope object. For disabling debug code
// entirely use the {DontEmitDebugCodeScope} instead.
class HardAbortScope {
 public:
  explicit HardAbortScope(TurboAssemblerBase* assembler)
      : assembler_(assembler), old_value_(assembler->should_abort_hard()) {
    assembler_->set_abort_hard(true);
  }
  ~HardAbortScope() { assembler_->set_abort_hard(old_value_); }

 private:
  TurboAssemblerBase* assembler_;
  bool old_value_;
};

// Helper stubs can be called in different ways depending on where the target
// code is located and how the call sequence is expected to look like:
//  - JavaScript: Call on-heap {Code} object via {RelocInfo::CODE_TARGET}.
//  - WebAssembly: Call native {WasmCode} stub via {RelocInfo::WASM_STUB_CALL}.
enum class StubCallMode { kCallOnHeapBuiltin, kCallWasmRuntimeStub };

#ifdef DEBUG
template <typename RegType, typename... RegTypes,
          // All arguments must be either Register or DoubleRegister.
          typename = typename std::enable_if<
              base::is_same<Register, RegType, RegTypes...>::value ||
              base::is_same<DoubleRegister, RegType, RegTypes...>::value>::type>
inline bool AreAliased(RegType first_reg, RegTypes... regs) {
  int num_different_regs = NumRegs(RegType::ListOf(first_reg, regs...));
  int num_given_regs = sizeof...(regs) + 1;
  return num_different_regs < num_given_regs;
}
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_TURBO_ASSEMBLER_H_
