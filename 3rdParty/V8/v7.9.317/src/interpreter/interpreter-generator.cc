// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-generator.h"

#include <array>
#include <tuple>

#include "src/builtins/builtins-arguments-gen.h"
#include "src/builtins/builtins-constructor-gen.h"
#include "src/builtins/builtins-iterator-gen.h"
#include "src/codegen/code-factory.h"
#include "src/debug/debug.h"
#include "src/ic/accessor-assembler.h"
#include "src/ic/binary-op-assembler.h"
#include "src/ic/ic.h"
#include "src/interpreter/bytecode-flags.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/interpreter/interpreter-intrinsics-generator.h"
#include "src/objects/cell.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/oddball.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/source-text-module.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {
namespace interpreter {

namespace {

using compiler::Node;
using Label = CodeStubAssembler::Label;

#define IGNITION_HANDLER(Name, BaseAssembler)                         \
  class Name##Assembler : public BaseAssembler {                      \
   public:                                                            \
    explicit Name##Assembler(compiler::CodeAssemblerState* state,     \
                             Bytecode bytecode, OperandScale scale)   \
        : BaseAssembler(state, bytecode, scale) {}                    \
    static void Generate(compiler::CodeAssemblerState* state,         \
                         OperandScale scale);                         \
                                                                      \
   private:                                                           \
    void GenerateImpl();                                              \
    DISALLOW_COPY_AND_ASSIGN(Name##Assembler);                        \
  };                                                                  \
  void Name##Assembler::Generate(compiler::CodeAssemblerState* state, \
                                 OperandScale scale) {                \
    Name##Assembler assembler(state, Bytecode::k##Name, scale);       \
    state->SetInitialDebugInformation(#Name, __FILE__, __LINE__);     \
    assembler.GenerateImpl();                                         \
  }                                                                   \
  void Name##Assembler::GenerateImpl()

// LdaZero
//
// Load literal '0' into the accumulator.
IGNITION_HANDLER(LdaZero, InterpreterAssembler) {
  TNode<Number> zero_value = NumberConstant(0.0);
  SetAccumulator(zero_value);
  Dispatch();
}

// LdaSmi <imm>
//
// Load an integer literal into the accumulator as a Smi.
IGNITION_HANDLER(LdaSmi, InterpreterAssembler) {
  TNode<Smi> smi_int = BytecodeOperandImmSmi(0);
  SetAccumulator(smi_int);
  Dispatch();
}

// LdaConstant <idx>
//
// Load constant literal at |idx| in the constant pool into the accumulator.
IGNITION_HANDLER(LdaConstant, InterpreterAssembler) {
  TNode<Object> constant = LoadConstantPoolEntryAtOperandIndex(0);
  SetAccumulator(constant);
  Dispatch();
}

// LdaUndefined
//
// Load Undefined into the accumulator.
IGNITION_HANDLER(LdaUndefined, InterpreterAssembler) {
  SetAccumulator(UndefinedConstant());
  Dispatch();
}

// LdaNull
//
// Load Null into the accumulator.
IGNITION_HANDLER(LdaNull, InterpreterAssembler) {
  SetAccumulator(NullConstant());
  Dispatch();
}

// LdaTheHole
//
// Load TheHole into the accumulator.
IGNITION_HANDLER(LdaTheHole, InterpreterAssembler) {
  SetAccumulator(TheHoleConstant());
  Dispatch();
}

// LdaTrue
//
// Load True into the accumulator.
IGNITION_HANDLER(LdaTrue, InterpreterAssembler) {
  SetAccumulator(TrueConstant());
  Dispatch();
}

// LdaFalse
//
// Load False into the accumulator.
IGNITION_HANDLER(LdaFalse, InterpreterAssembler) {
  SetAccumulator(FalseConstant());
  Dispatch();
}

// Ldar <src>
//
// Load accumulator with value from register <src>.
IGNITION_HANDLER(Ldar, InterpreterAssembler) {
  TNode<Object> value = LoadRegisterAtOperandIndex(0);
  SetAccumulator(value);
  Dispatch();
}

// Star <dst>
//
// Store accumulator to register <dst>.
IGNITION_HANDLER(Star, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  StoreRegisterAtOperandIndex(accumulator, 0);
  Dispatch();
}

// Mov <src> <dst>
//
// Stores the value of register <src> to register <dst>.
IGNITION_HANDLER(Mov, InterpreterAssembler) {
  TNode<Object> src_value = LoadRegisterAtOperandIndex(0);
  StoreRegisterAtOperandIndex(src_value, 1);
  Dispatch();
}

class InterpreterLoadGlobalAssembler : public InterpreterAssembler {
 public:
  InterpreterLoadGlobalAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                 OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void LdaGlobal(int slot_operand_index, int name_operand_index,
                 TypeofMode typeof_mode) {
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

    AccessorAssembler accessor_asm(state());
    ExitPoint exit_point(this, [=](Node* result) {
      SetAccumulator(result);
      Dispatch();
    });

    LazyNode<Smi> lazy_smi_slot = [=] {
      return SmiTag(Signed(BytecodeOperandIdx(slot_operand_index)));
    };

    LazyNode<UintPtrT> lazy_slot = [=] {
      return BytecodeOperandIdx(slot_operand_index);
    };

    LazyNode<Context> lazy_context = [=] { return GetContext(); };

    LazyNode<Name> lazy_name = [=] {
      TNode<Name> name =
          CAST(LoadConstantPoolEntryAtOperandIndex(name_operand_index));
      return name;
    };

    accessor_asm.LoadGlobalIC(maybe_feedback_vector, lazy_smi_slot, lazy_slot,
                              lazy_context, lazy_name, typeof_mode,
                              &exit_point);
  }
};

// LdaGlobal <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> outside of a typeof.
IGNITION_HANDLER(LdaGlobal, InterpreterLoadGlobalAssembler) {
  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  LdaGlobal(kSlotOperandIndex, kNameOperandIndex, NOT_INSIDE_TYPEOF);
}

// LdaGlobalInsideTypeof <name_index> <slot>
//
// Load the global with name in constant pool entry <name_index> into the
// accumulator using FeedBackVector slot <slot> inside of a typeof.
IGNITION_HANDLER(LdaGlobalInsideTypeof, InterpreterLoadGlobalAssembler) {
  static const int kNameOperandIndex = 0;
  static const int kSlotOperandIndex = 1;

  LdaGlobal(kSlotOperandIndex, kNameOperandIndex, INSIDE_TYPEOF);
}

// StaGlobal <name_index> <slot>
//
// Store the value in the accumulator into the global with name in constant pool
// entry <name_index> using FeedBackVector slot <slot>.
IGNITION_HANDLER(StaGlobal, InterpreterAssembler) {
  TNode<Context> context = GetContext();

  // Store the global via the StoreGlobalIC.
  TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(1));
  TNode<Smi> smi_slot = SmiTag(raw_slot);
  TNode<HeapObject> maybe_vector = LoadFeedbackVector();

  Label no_feedback(this, Label::kDeferred), end(this);
  GotoIf(IsUndefined(maybe_vector), &no_feedback);

  CallBuiltin(Builtins::kStoreGlobalIC, context, name, value, smi_slot,
              maybe_vector);
  Goto(&end);

  Bind(&no_feedback);
  CallRuntime(Runtime::kStoreGlobalICNoFeedback_Miss, context, value, name);
  Goto(&end);

  Bind(&end);
  Dispatch();
}

// LdaContextSlot <context> <slot_index> <depth>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain starting at |context| into the accumulator.
IGNITION_HANDLER(LdaContextSlot, InterpreterAssembler) {
  TNode<Context> context = CAST(LoadRegisterAtOperandIndex(0));
  TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(1));
  TNode<Uint32T> depth = BytecodeOperandUImm(2);
  TNode<Context> slot_context = GetContextAtDepth(context, depth);
  TNode<Object> result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// LdaImmutableContextSlot <context> <slot_index> <depth>
//
// Load the object in |slot_index| of the context at |depth| in the context
// chain starting at |context| into the accumulator.
IGNITION_HANDLER(LdaImmutableContextSlot, InterpreterAssembler) {
  TNode<Context> context = CAST(LoadRegisterAtOperandIndex(0));
  TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(1));
  TNode<Uint32T> depth = BytecodeOperandUImm(2);
  TNode<Context> slot_context = GetContextAtDepth(context, depth);
  TNode<Object> result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// LdaCurrentContextSlot <slot_index>
//
// Load the object in |slot_index| of the current context into the accumulator.
IGNITION_HANDLER(LdaCurrentContextSlot, InterpreterAssembler) {
  TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(0));
  TNode<Context> slot_context = GetContext();
  TNode<Object> result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// LdaImmutableCurrentContextSlot <slot_index>
//
// Load the object in |slot_index| of the current context into the accumulator.
IGNITION_HANDLER(LdaImmutableCurrentContextSlot, InterpreterAssembler) {
  TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(0));
  TNode<Context> slot_context = GetContext();
  TNode<Object> result = LoadContextElement(slot_context, slot_index);
  SetAccumulator(result);
  Dispatch();
}

// StaContextSlot <context> <slot_index> <depth>
//
// Stores the object in the accumulator into |slot_index| of the context at
// |depth| in the context chain starting at |context|.
IGNITION_HANDLER(StaContextSlot, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<Context> context = CAST(LoadRegisterAtOperandIndex(0));
  TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(1));
  TNode<Uint32T> depth = BytecodeOperandUImm(2);
  TNode<Context> slot_context = GetContextAtDepth(context, depth);
  StoreContextElement(slot_context, slot_index, value);
  Dispatch();
}

// StaCurrentContextSlot <slot_index>
//
// Stores the object in the accumulator into |slot_index| of the current
// context.
IGNITION_HANDLER(StaCurrentContextSlot, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(0));
  TNode<Context> slot_context = GetContext();
  StoreContextElement(slot_context, slot_index, value);
  Dispatch();
}

// LdaLookupSlot <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
IGNITION_HANDLER(LdaLookupSlot, InterpreterAssembler) {
  TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
  TNode<Context> context = GetContext();
  TNode<Object> result = CallRuntime(Runtime::kLoadLookupSlot, context, name);
  SetAccumulator(result);
  Dispatch();
}

// LdaLookupSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
IGNITION_HANDLER(LdaLookupSlotInsideTypeof, InterpreterAssembler) {
  TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
  TNode<Context> context = GetContext();
  TNode<Object> result =
      CallRuntime(Runtime::kLoadLookupSlotInsideTypeof, context, name);
  SetAccumulator(result);
  Dispatch();
}

class InterpreterLookupContextSlotAssembler : public InterpreterAssembler {
 public:
  InterpreterLookupContextSlotAssembler(CodeAssemblerState* state,
                                        Bytecode bytecode,
                                        OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void LookupContextSlot(Runtime::FunctionId function_id) {
    TNode<Context> context = GetContext();
    TNode<IntPtrT> slot_index = Signed(BytecodeOperandIdx(1));
    TNode<Uint32T> depth = BytecodeOperandUImm(2);

    Label slowpath(this, Label::kDeferred);

    // Check for context extensions to allow the fast path.
    GotoIfHasContextExtensionUpToDepth(context, depth, &slowpath);

    // Fast path does a normal load context.
    {
      TNode<Context> slot_context = GetContextAtDepth(context, depth);
      TNode<Object> result = LoadContextElement(slot_context, slot_index);
      SetAccumulator(result);
      Dispatch();
    }

    // Slow path when we have to call out to the runtime.
    BIND(&slowpath);
    {
      TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
      TNode<Object> result = CallRuntime(function_id, context, name);
      SetAccumulator(result);
      Dispatch();
    }
  }
};

// LdaLookupContextSlot <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
IGNITION_HANDLER(LdaLookupContextSlot, InterpreterLookupContextSlotAssembler) {
  LookupContextSlot(Runtime::kLoadLookupSlot);
}

// LdaLookupContextSlotInsideTypeof <name_index>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
IGNITION_HANDLER(LdaLookupContextSlotInsideTypeof,
                 InterpreterLookupContextSlotAssembler) {
  LookupContextSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

class InterpreterLookupGlobalAssembler : public InterpreterLoadGlobalAssembler {
 public:
  InterpreterLookupGlobalAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                   OperandScale operand_scale)
      : InterpreterLoadGlobalAssembler(state, bytecode, operand_scale) {}

  void LookupGlobalSlot(Runtime::FunctionId function_id) {
    TNode<Context> context = GetContext();
    TNode<Uint32T> depth = BytecodeOperandUImm(2);

    Label slowpath(this, Label::kDeferred);

    // Check for context extensions to allow the fast path
    GotoIfHasContextExtensionUpToDepth(context, depth, &slowpath);

    // Fast path does a normal load global
    {
      static const int kNameOperandIndex = 0;
      static const int kSlotOperandIndex = 1;

      TypeofMode typeof_mode =
          function_id == Runtime::kLoadLookupSlotInsideTypeof
              ? INSIDE_TYPEOF
              : NOT_INSIDE_TYPEOF;

      LdaGlobal(kSlotOperandIndex, kNameOperandIndex, typeof_mode);
    }

    // Slow path when we have to call out to the runtime
    BIND(&slowpath);
    {
      TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
      TNode<Object> result = CallRuntime(function_id, context, name);
      SetAccumulator(result);
      Dispatch();
    }
  }
};

// LdaLookupGlobalSlot <name_index> <feedback_slot> <depth>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically.
IGNITION_HANDLER(LdaLookupGlobalSlot, InterpreterLookupGlobalAssembler) {
  LookupGlobalSlot(Runtime::kLoadLookupSlot);
}

// LdaLookupGlobalSlotInsideTypeof <name_index> <feedback_slot> <depth>
//
// Lookup the object with the name in constant pool entry |name_index|
// dynamically without causing a NoReferenceError.
IGNITION_HANDLER(LdaLookupGlobalSlotInsideTypeof,
                 InterpreterLookupGlobalAssembler) {
  LookupGlobalSlot(Runtime::kLoadLookupSlotInsideTypeof);
}

// StaLookupSlot <name_index> <flags>
//
// Store the object in accumulator to the object with the name in constant
// pool entry |name_index|.
IGNITION_HANDLER(StaLookupSlot, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
  TNode<Uint32T> bytecode_flags = BytecodeOperandFlag(1);
  TNode<Context> context = GetContext();
  TVARIABLE(Object, var_result);

  Label sloppy(this), strict(this), end(this);
  DCHECK_EQ(0, LanguageMode::kSloppy);
  DCHECK_EQ(1, LanguageMode::kStrict);
  DCHECK_EQ(0, static_cast<int>(LookupHoistingMode::kNormal));
  DCHECK_EQ(1, static_cast<int>(LookupHoistingMode::kLegacySloppy));
  Branch(IsSetWord32<StoreLookupSlotFlags::LanguageModeBit>(bytecode_flags),
         &strict, &sloppy);

  BIND(&strict);
  {
    CSA_ASSERT(this, IsClearWord32<StoreLookupSlotFlags::LookupHoistingModeBit>(
                         bytecode_flags));
    var_result =
        CallRuntime(Runtime::kStoreLookupSlot_Strict, context, name, value);
    Goto(&end);
  }

  BIND(&sloppy);
  {
    Label hoisting(this), ordinary(this);
    Branch(IsSetWord32<StoreLookupSlotFlags::LookupHoistingModeBit>(
               bytecode_flags),
           &hoisting, &ordinary);

    BIND(&hoisting);
    {
      var_result = CallRuntime(Runtime::kStoreLookupSlot_SloppyHoisting,
                               context, name, value);
      Goto(&end);
    }

    BIND(&ordinary);
    {
      var_result =
          CallRuntime(Runtime::kStoreLookupSlot_Sloppy, context, name, value);
      Goto(&end);
    }
  }

  BIND(&end);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// LdaNamedProperty <object> <name_index> <slot>
//
// Calls the LoadIC at FeedBackVector slot <slot> for <object> and the name at
// constant pool entry <name_index>.
IGNITION_HANDLER(LdaNamedProperty, InterpreterAssembler) {
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<UintPtrT> feedback_slot = BytecodeOperandIdx(2);

  // Load receiver.
  TNode<Object> recv = LoadRegisterAtOperandIndex(0);

  // Load the name and context lazily.
  LazyNode<Smi> lazy_smi_slot = [=] { return SmiTag(Signed(feedback_slot)); };
  LazyNode<Name> lazy_name = [=] {
    return CAST(LoadConstantPoolEntryAtOperandIndex(1));
  };
  LazyNode<Context> lazy_context = [=] { return GetContext(); };

  Label done(this);
  TVARIABLE(Object, var_result);
  ExitPoint exit_point(this, &done, &var_result);

  AccessorAssembler::LazyLoadICParameters params(
      lazy_context, recv, lazy_name, lazy_smi_slot, feedback_vector);
  AccessorAssembler accessor_asm(state());
  accessor_asm.LoadIC_BytecodeHandler(&params, &exit_point);

  BIND(&done);
  {
    SetAccumulator(var_result.value());
    Dispatch();
  }
}

// LdaPropertyNofeedback <object> <slot>
//
// Calls the GetProperty builtin for <object> and the key in the accumulator.
IGNITION_HANDLER(LdaNamedPropertyNoFeedback, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(1));
  TNode<Context> context = GetContext();
  TNode<Object> result =
      CallBuiltin(Builtins::kGetProperty, context, object, name);
  SetAccumulator(result);
  Dispatch();
}

// KeyedLoadIC <object> <slot>
//
// Calls the KeyedLoadIC at FeedBackVector slot <slot> for <object> and the key
// in the accumulator.
IGNITION_HANDLER(LdaKeyedProperty, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Object> name = GetAccumulator();
  TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(1));
  TNode<Smi> smi_slot = SmiTag(raw_slot);
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  TVARIABLE(Object, var_result);
  var_result = CallBuiltin(Builtins::kKeyedLoadIC, context, object, name,
                           smi_slot, feedback_vector);
  SetAccumulator(var_result.value());
  Dispatch();
}

class InterpreterStoreNamedPropertyAssembler : public InterpreterAssembler {
 public:
  InterpreterStoreNamedPropertyAssembler(CodeAssemblerState* state,
                                         Bytecode bytecode,
                                         OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void StaNamedProperty(Callable ic, NamedPropertyType property_type) {
    TNode<Code> code_target = HeapConstant(ic.code());
    TNode<Object> object = LoadRegisterAtOperandIndex(0);
    TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(1));
    TNode<Object> value = GetAccumulator();
    TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(2));
    TNode<Smi> smi_slot = SmiTag(raw_slot);
    TNode<HeapObject> maybe_vector = LoadFeedbackVector();
    TNode<Context> context = GetContext();

    TVARIABLE(Object, var_result);
    var_result = CallStub(ic.descriptor(), code_target, context, object, name,
                          value, smi_slot, maybe_vector);
    // To avoid special logic in the deoptimizer to re-materialize the value in
    // the accumulator, we overwrite the accumulator after the IC call. It
    // doesn't really matter what we write to the accumulator here, since we
    // restore to the correct value on the outside. Storing the result means we
    // don't need to keep unnecessary state alive across the callstub.
    SetAccumulator(var_result.value());
    Dispatch();
  }
};

// StaNamedProperty <object> <name_index> <slot>
//
// Calls the StoreIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
IGNITION_HANDLER(StaNamedProperty, InterpreterStoreNamedPropertyAssembler) {
  Callable ic = Builtins::CallableFor(isolate(), Builtins::kStoreIC);
  StaNamedProperty(ic, NamedPropertyType::kNotOwn);
}

// StaNamedOwnProperty <object> <name_index> <slot>
//
// Calls the StoreOwnIC at FeedBackVector slot <slot> for <object> and
// the name in constant pool entry <name_index> with the value in the
// accumulator.
IGNITION_HANDLER(StaNamedOwnProperty, InterpreterStoreNamedPropertyAssembler) {
  Callable ic = CodeFactory::StoreOwnICInOptimizedCode(isolate());
  StaNamedProperty(ic, NamedPropertyType::kOwn);
}

// StaNamedPropertyNoFeedback <object> <name_index>
//
// Calls the SetPropertyBuiltin for <object> and the name in constant pool entry
// <name_index> with the value in the accumulator.
IGNITION_HANDLER(StaNamedPropertyNoFeedback,
                 InterpreterStoreNamedPropertyAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(1));
  TNode<Object> value = GetAccumulator();
  TNode<Context> context = GetContext();

  TNode<Object> result =
      CallRuntime(Runtime::kSetNamedProperty, context, object, name, value);
  SetAccumulator(result);
  Dispatch();
}

// StaKeyedProperty <object> <key> <slot>
//
// Calls the KeyedStoreIC at FeedbackVector slot <slot> for <object> and
// the key <key> with the value in the accumulator.
IGNITION_HANDLER(StaKeyedProperty, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Object> name = LoadRegisterAtOperandIndex(1);
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(2));
  TNode<Smi> smi_slot = SmiTag(raw_slot);
  TNode<HeapObject> maybe_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  TVARIABLE(Object, var_result);
  var_result = CallBuiltin(Builtins::kKeyedStoreIC, context, object, name,
                           value, smi_slot, maybe_vector);
  // To avoid special logic in the deoptimizer to re-materialize the value in
  // the accumulator, we overwrite the accumulator after the IC call. It
  // doesn't really matter what we write to the accumulator here, since we
  // restore to the correct value on the outside. Storing the result means we
  // don't need to keep unnecessary state alive across the callstub.
  SetAccumulator(var_result.value());
  Dispatch();
}

// StaInArrayLiteral <array> <index> <slot>
//
// Calls the StoreInArrayLiteralIC at FeedbackVector slot <slot> for <array> and
// the key <index> with the value in the accumulator.
IGNITION_HANDLER(StaInArrayLiteral, InterpreterAssembler) {
  TNode<Object> array = LoadRegisterAtOperandIndex(0);
  TNode<Object> index = LoadRegisterAtOperandIndex(1);
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(2));
  TNode<Smi> smi_slot = SmiTag(raw_slot);
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  TVARIABLE(Object, var_result);
  var_result = CallBuiltin(Builtins::kStoreInArrayLiteralIC, context, array,
                           index, value, smi_slot, feedback_vector);
  // To avoid special logic in the deoptimizer to re-materialize the value in
  // the accumulator, we overwrite the accumulator after the IC call. It
  // doesn't really matter what we write to the accumulator here, since we
  // restore to the correct value on the outside. Storing the result means we
  // don't need to keep unnecessary state alive across the callstub.
  SetAccumulator(var_result.value());
  Dispatch();
}

// StaDataPropertyInLiteral <object> <name> <flags>
//
// Define a property <name> with value from the accumulator in <object>.
// Property attributes and whether set_function_name are stored in
// DataPropertyInLiteralFlags <flags>.
//
// This definition is not observable and is used only for definitions
// in object or class literals.
IGNITION_HANDLER(StaDataPropertyInLiteral, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Object> name = LoadRegisterAtOperandIndex(1);
  TNode<Object> value = GetAccumulator();
  TNode<Smi> flags =
      SmiFromInt32(UncheckedCast<Int32T>(BytecodeOperandFlag(2)));
  TNode<Smi> vector_index = BytecodeOperandIdxSmi(3);

  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  CallRuntime(Runtime::kDefineDataPropertyInLiteral, context, object, name,
              value, flags, feedback_vector, vector_index);
  Dispatch();
}

IGNITION_HANDLER(CollectTypeProfile, InterpreterAssembler) {
  TNode<Smi> position = BytecodeOperandImmSmi(0);
  TNode<Object> value = GetAccumulator();

  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  CallRuntime(Runtime::kCollectTypeProfile, context, position, value,
              feedback_vector);
  Dispatch();
}

// LdaModuleVariable <cell_index> <depth>
//
// Load the contents of a module variable into the accumulator.  The variable is
// identified by <cell_index>.  <depth> is the depth of the current context
// relative to the module context.
IGNITION_HANDLER(LdaModuleVariable, InterpreterAssembler) {
  TNode<IntPtrT> cell_index = BytecodeOperandImmIntPtr(0);
  TNode<Uint32T> depth = BytecodeOperandUImm(1);

  TNode<Context> module_context = GetContextAtDepth(GetContext(), depth);
  TNode<SourceTextModule> module =
      CAST(LoadContextElement(module_context, Context::EXTENSION_INDEX));

  Label if_export(this), if_import(this), end(this);
  Branch(IntPtrGreaterThan(cell_index, IntPtrConstant(0)), &if_export,
         &if_import);

  BIND(&if_export);
  {
    TNode<FixedArray> regular_exports = LoadObjectField<FixedArray>(
        module, SourceTextModule::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    TNode<IntPtrT> export_index = IntPtrSub(cell_index, IntPtrConstant(1));
    TNode<Cell> cell =
        CAST(LoadFixedArrayElement(regular_exports, export_index));
    SetAccumulator(LoadObjectField(cell, Cell::kValueOffset));
    Goto(&end);
  }

  BIND(&if_import);
  {
    TNode<FixedArray> regular_imports = LoadObjectField<FixedArray>(
        module, SourceTextModule::kRegularImportsOffset);
    // The actual array index is (-cell_index - 1).
    TNode<IntPtrT> import_index = IntPtrSub(IntPtrConstant(-1), cell_index);
    TNode<Cell> cell =
        CAST(LoadFixedArrayElement(regular_imports, import_index));
    SetAccumulator(LoadObjectField(cell, Cell::kValueOffset));
    Goto(&end);
  }

  BIND(&end);
  Dispatch();
}

// StaModuleVariable <cell_index> <depth>
//
// Store accumulator to the module variable identified by <cell_index>.
// <depth> is the depth of the current context relative to the module context.
IGNITION_HANDLER(StaModuleVariable, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> cell_index = BytecodeOperandImmIntPtr(0);
  TNode<Uint32T> depth = BytecodeOperandUImm(1);

  TNode<Context> module_context = GetContextAtDepth(GetContext(), depth);
  TNode<SourceTextModule> module =
      CAST(LoadContextElement(module_context, Context::EXTENSION_INDEX));

  Label if_export(this), if_import(this), end(this);
  Branch(IntPtrGreaterThan(cell_index, IntPtrConstant(0)), &if_export,
         &if_import);

  BIND(&if_export);
  {
    TNode<FixedArray> regular_exports = LoadObjectField<FixedArray>(
        module, SourceTextModule::kRegularExportsOffset);
    // The actual array index is (cell_index - 1).
    TNode<IntPtrT> export_index = IntPtrSub(cell_index, IntPtrConstant(1));
    TNode<Object> cell = LoadFixedArrayElement(regular_exports, export_index);
    StoreObjectField(cell, Cell::kValueOffset, value);
    Goto(&end);
  }

  BIND(&if_import);
  {
    // Not supported (probably never).
    Abort(AbortReason::kUnsupportedModuleOperation);
    Goto(&end);
  }

  BIND(&end);
  Dispatch();
}

// PushContext <context>
//
// Saves the current context in <context>, and pushes the accumulator as the
// new current context.
IGNITION_HANDLER(PushContext, InterpreterAssembler) {
  TNode<Context> new_context = CAST(GetAccumulator());
  TNode<Context> old_context = GetContext();
  StoreRegisterAtOperandIndex(old_context, 0);
  SetContext(new_context);
  Dispatch();
}

// PopContext <context>
//
// Pops the current context and sets <context> as the new context.
IGNITION_HANDLER(PopContext, InterpreterAssembler) {
  TNode<Context> context = CAST(LoadRegisterAtOperandIndex(0));
  SetContext(context);
  Dispatch();
}

class InterpreterBinaryOpAssembler : public InterpreterAssembler {
 public:
  InterpreterBinaryOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                               OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  using BinaryOpGenerator = TNode<Object> (BinaryOpAssembler::*)(
      TNode<Context> context, TNode<Object> left, TNode<Object> right,
      TNode<UintPtrT> slot, TNode<HeapObject> maybe_feedback_vector,
      bool rhs_known_smi);

  void BinaryOpWithFeedback(BinaryOpGenerator generator) {
    TNode<Object> lhs = LoadRegisterAtOperandIndex(0);
    TNode<Object> rhs = GetAccumulator();
    TNode<Context> context = GetContext();
    TNode<UintPtrT> slot_index = BytecodeOperandIdx(1);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

    BinaryOpAssembler binop_asm(state());
    TNode<Object> result = (binop_asm.*generator)(context, lhs, rhs, slot_index,
                                                  maybe_feedback_vector, false);
    SetAccumulator(result);
    Dispatch();
  }

  void BinaryOpSmiWithFeedback(BinaryOpGenerator generator) {
    TNode<Object> lhs = GetAccumulator();
    TNode<Smi> rhs = BytecodeOperandImmSmi(0);
    TNode<Context> context = GetContext();
    TNode<UintPtrT> slot_index = BytecodeOperandIdx(1);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

    BinaryOpAssembler binop_asm(state());
    TNode<Object> result = (binop_asm.*generator)(context, lhs, rhs, slot_index,
                                                  maybe_feedback_vector, true);
    SetAccumulator(result);
    Dispatch();
  }
};

// Add <src>
//
// Add register <src> to accumulator.
IGNITION_HANDLER(Add, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_AddWithFeedback);
}

// Sub <src>
//
// Subtract register <src> from accumulator.
IGNITION_HANDLER(Sub, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_SubtractWithFeedback);
}

// Mul <src>
//
// Multiply accumulator by register <src>.
IGNITION_HANDLER(Mul, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_MultiplyWithFeedback);
}

// Div <src>
//
// Divide register <src> by accumulator.
IGNITION_HANDLER(Div, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_DivideWithFeedback);
}

// Mod <src>
//
// Modulo register <src> by accumulator.
IGNITION_HANDLER(Mod, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_ModulusWithFeedback);
}

// Exp <src>
//
// Exponentiate register <src> (base) with accumulator (exponent).
IGNITION_HANDLER(Exp, InterpreterBinaryOpAssembler) {
  BinaryOpWithFeedback(&BinaryOpAssembler::Generate_ExponentiateWithFeedback);
}

// AddSmi <imm>
//
// Adds an immediate value <imm> to the value in the accumulator.
IGNITION_HANDLER(AddSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_AddWithFeedback);
}

// SubSmi <imm>
//
// Subtracts an immediate value <imm> from the value in the accumulator.
IGNITION_HANDLER(SubSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_SubtractWithFeedback);
}

// MulSmi <imm>
//
// Multiplies an immediate value <imm> to the value in the accumulator.
IGNITION_HANDLER(MulSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_MultiplyWithFeedback);
}

// DivSmi <imm>
//
// Divides the value in the accumulator by immediate value <imm>.
IGNITION_HANDLER(DivSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_DivideWithFeedback);
}

// ModSmi <imm>
//
// Modulo accumulator by immediate value <imm>.
IGNITION_HANDLER(ModSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(&BinaryOpAssembler::Generate_ModulusWithFeedback);
}

// ExpSmi <imm>
//
// Exponentiate accumulator (base) with immediate value <imm> (exponent).
IGNITION_HANDLER(ExpSmi, InterpreterBinaryOpAssembler) {
  BinaryOpSmiWithFeedback(
      &BinaryOpAssembler::Generate_ExponentiateWithFeedback);
}

class InterpreterBitwiseBinaryOpAssembler : public InterpreterAssembler {
 public:
  InterpreterBitwiseBinaryOpAssembler(CodeAssemblerState* state,
                                      Bytecode bytecode,
                                      OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void BitwiseBinaryOpWithFeedback(Operation bitwise_op) {
    TNode<Object> left = LoadRegisterAtOperandIndex(0);
    TNode<Object> right = GetAccumulator();
    TNode<Context> context = GetContext();
    TNode<UintPtrT> slot_index = BytecodeOperandIdx(1);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

    TVARIABLE(Smi, var_left_feedback);
    TVARIABLE(Smi, var_right_feedback);
    TVARIABLE(Word32T, var_left_word32);
    TVARIABLE(Word32T, var_right_word32);
    TVARIABLE(Object, var_left_bigint, left);
    TVARIABLE(Object, var_right_bigint);
    Label if_left_number(this), do_number_op(this);
    Label if_left_bigint(this), do_bigint_op(this);

    TaggedToWord32OrBigIntWithFeedback(context, left, &if_left_number,
                                       &var_left_word32, &if_left_bigint,
                                       &var_left_bigint, &var_left_feedback);
    BIND(&if_left_number);
    TaggedToWord32OrBigIntWithFeedback(context, right, &do_number_op,
                                       &var_right_word32, &do_bigint_op,
                                       &var_right_bigint, &var_right_feedback);
    BIND(&do_number_op);
    TNode<Number> result = BitwiseOp(var_left_word32.value(),
                                     var_right_word32.value(), bitwise_op);
    TNode<Smi> result_type = SelectSmiConstant(
        TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
        BinaryOperationFeedback::kNumber);
    TNode<Smi> input_feedback =
        SmiOr(var_left_feedback.value(), var_right_feedback.value());
    UpdateFeedback(SmiOr(result_type, input_feedback), maybe_feedback_vector,
                   slot_index);
    SetAccumulator(result);
    Dispatch();

    // BigInt cases.
    BIND(&if_left_bigint);
    TaggedToNumericWithFeedback(context, right, &do_bigint_op,
                                &var_right_bigint, &var_right_feedback);

    BIND(&do_bigint_op);
    SetAccumulator(
        CallRuntime(Runtime::kBigIntBinaryOp, context, var_left_bigint.value(),
                    var_right_bigint.value(), SmiConstant(bitwise_op)));
    UpdateFeedback(SmiOr(var_left_feedback.value(), var_right_feedback.value()),
                   maybe_feedback_vector, slot_index);
    Dispatch();
  }

  void BitwiseBinaryOpWithSmi(Operation bitwise_op) {
    TNode<Object> left = GetAccumulator();
    TNode<Smi> right = BytecodeOperandImmSmi(0);
    TNode<UintPtrT> slot_index = BytecodeOperandIdx(1);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
    TNode<Context> context = GetContext();

    TVARIABLE(Smi, var_left_feedback);
    TVARIABLE(Word32T, var_left_word32);
    // TODO(v8:6949): var_left_bigint should be BigInt, but before that we need
    // to clean up TaggedToWord32OrBigIntWithFeedback and related methods.
    TVARIABLE(Object, var_left_bigint);
    Label do_smi_op(this), if_bigint_mix(this);

    TaggedToWord32OrBigIntWithFeedback(context, left, &do_smi_op,
                                       &var_left_word32, &if_bigint_mix,
                                       &var_left_bigint, &var_left_feedback);
    BIND(&do_smi_op);
    TNode<Number> result =
        BitwiseOp(var_left_word32.value(), SmiToInt32(right), bitwise_op);
    TNode<Smi> result_type = SelectSmiConstant(
        TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
        BinaryOperationFeedback::kNumber);
    UpdateFeedback(SmiOr(result_type, var_left_feedback.value()),
                   maybe_feedback_vector, slot_index);
    SetAccumulator(result);
    Dispatch();

    BIND(&if_bigint_mix);
    UpdateFeedback(var_left_feedback.value(), maybe_feedback_vector,
                   slot_index);
    ThrowTypeError(context, MessageTemplate::kBigIntMixedTypes);
  }
};

// BitwiseOr <src>
//
// BitwiseOr register <src> to accumulator.
IGNITION_HANDLER(BitwiseOr, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kBitwiseOr);
}

// BitwiseXor <src>
//
// BitwiseXor register <src> to accumulator.
IGNITION_HANDLER(BitwiseXor, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kBitwiseXor);
}

// BitwiseAnd <src>
//
// BitwiseAnd register <src> to accumulator.
IGNITION_HANDLER(BitwiseAnd, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kBitwiseAnd);
}

// ShiftLeft <src>
//
// Left shifts register <src> by the count specified in the accumulator.
// Register <src> is converted to an int32 and the accumulator to uint32
// before the operation. 5 lsb bits from the accumulator are used as count
// i.e. <src> << (accumulator & 0x1F).
IGNITION_HANDLER(ShiftLeft, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kShiftLeft);
}

// ShiftRight <src>
//
// Right shifts register <src> by the count specified in the accumulator.
// Result is sign extended. Register <src> is converted to an int32 and the
// accumulator to uint32 before the operation. 5 lsb bits from the accumulator
// are used as count i.e. <src> >> (accumulator & 0x1F).
IGNITION_HANDLER(ShiftRight, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kShiftRight);
}

// ShiftRightLogical <src>
//
// Right Shifts register <src> by the count specified in the accumulator.
// Result is zero-filled. The accumulator and register <src> are converted to
// uint32 before the operation 5 lsb bits from the accumulator are used as
// count i.e. <src> << (accumulator & 0x1F).
IGNITION_HANDLER(ShiftRightLogical, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithFeedback(Operation::kShiftRightLogical);
}

// BitwiseOrSmi <imm>
//
// BitwiseOrSmi accumulator with <imm>.
IGNITION_HANDLER(BitwiseOrSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kBitwiseOr);
}

// BitwiseXorSmi <imm>
//
// BitwiseXorSmi accumulator with <imm>.
IGNITION_HANDLER(BitwiseXorSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kBitwiseXor);
}

// BitwiseAndSmi <imm>
//
// BitwiseAndSmi accumulator with <imm>.
IGNITION_HANDLER(BitwiseAndSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kBitwiseAnd);
}

// BitwiseNot <feedback_slot>
//
// Perform bitwise-not on the accumulator.
IGNITION_HANDLER(BitwiseNot, InterpreterAssembler) {
  TNode<Object> operand = GetAccumulator();
  TNode<UintPtrT> slot_index = BytecodeOperandIdx(0);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  TVARIABLE(Word32T, var_word32);
  TVARIABLE(Smi, var_feedback);
  // TODO(v8:6949): var_bigint should be BigInt, but before that we need to
  // clean up TaggedToWord32OrBigIntWithFeedback and related methods.
  TVARIABLE(Object, var_bigint);
  Label if_number(this), if_bigint(this);
  TaggedToWord32OrBigIntWithFeedback(context, operand, &if_number, &var_word32,
                                     &if_bigint, &var_bigint, &var_feedback);

  // Number case.
  BIND(&if_number);
  TNode<Number> result =
      ChangeInt32ToTagged(Signed(Word32BitwiseNot(var_word32.value())));
  TNode<Smi> result_type = SelectSmiConstant(
      TaggedIsSmi(result), BinaryOperationFeedback::kSignedSmall,
      BinaryOperationFeedback::kNumber);
  UpdateFeedback(SmiOr(result_type, var_feedback.value()),
                 maybe_feedback_vector, slot_index);
  SetAccumulator(result);
  Dispatch();

  // BigInt case.
  BIND(&if_bigint);
  UpdateFeedback(SmiConstant(BinaryOperationFeedback::kBigInt),
                 maybe_feedback_vector, slot_index);
  SetAccumulator(CallRuntime(Runtime::kBigIntUnaryOp, context,
                             var_bigint.value(),
                             SmiConstant(Operation::kBitwiseNot)));
  Dispatch();
}

// ShiftLeftSmi <imm>
//
// Left shifts accumulator by the count specified in <imm>.
// The accumulator is converted to an int32 before the operation. The 5
// lsb bits from <imm> are used as count i.e. <src> << (<imm> & 0x1F).
IGNITION_HANDLER(ShiftLeftSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kShiftLeft);
}

// ShiftRightSmi <imm>
//
// Right shifts accumulator by the count specified in <imm>. Result is sign
// extended. The accumulator is converted to an int32 before the operation. The
// 5 lsb bits from <imm> are used as count i.e. <src> >> (<imm> & 0x1F).
IGNITION_HANDLER(ShiftRightSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kShiftRight);
}

// ShiftRightLogicalSmi <imm>
//
// Right shifts accumulator by the count specified in <imm>. Result is zero
// extended. The accumulator is converted to an int32 before the operation. The
// 5 lsb bits from <imm> are used as count i.e. <src> >>> (<imm> & 0x1F).
IGNITION_HANDLER(ShiftRightLogicalSmi, InterpreterBitwiseBinaryOpAssembler) {
  BitwiseBinaryOpWithSmi(Operation::kShiftRightLogical);
}

class UnaryNumericOpAssembler : public InterpreterAssembler {
 public:
  UnaryNumericOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                          OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  virtual ~UnaryNumericOpAssembler() = default;

  // Must return a tagged value.
  virtual TNode<Number> SmiOp(TNode<Smi> smi_value,
                              TVariable<Smi>* var_feedback, Label* do_float_op,
                              TVariable<Float64T>* var_float) = 0;
  // Must return a Float64 value.
  virtual TNode<Float64T> FloatOp(TNode<Float64T> float_value) = 0;
  // Must return a tagged value.
  virtual TNode<HeapObject> BigIntOp(TNode<HeapObject> bigint_value) = 0;

  void UnaryOpWithFeedback() {
    TVARIABLE(Object, var_value, GetAccumulator());
    TVARIABLE(Object, var_result);
    TVARIABLE(Float64T, var_float_value);
    TVARIABLE(Smi, var_feedback, SmiConstant(BinaryOperationFeedback::kNone));
    Label start(this, {&var_value, &var_feedback}), end(this);
    Label do_float_op(this, &var_float_value);
    Goto(&start);
    // We might have to try again after ToNumeric conversion.
    BIND(&start);
    {
      Label if_smi(this), if_heapnumber(this), if_oddball(this);
      Label if_bigint(this, Label::kDeferred);
      Label if_other(this, Label::kDeferred);
      TNode<Object> value = var_value.value();
      GotoIf(TaggedIsSmi(value), &if_smi);

      TNode<HeapObject> value_heap_object = CAST(value);
      TNode<Map> map = LoadMap(value_heap_object);
      GotoIf(IsHeapNumberMap(map), &if_heapnumber);
      TNode<Uint16T> instance_type = LoadMapInstanceType(map);
      GotoIf(IsBigIntInstanceType(instance_type), &if_bigint);
      Branch(InstanceTypeEqual(instance_type, ODDBALL_TYPE), &if_oddball,
             &if_other);

      BIND(&if_smi);
      {
        var_result =
            SmiOp(CAST(value), &var_feedback, &do_float_op, &var_float_value);
        Goto(&end);
      }

      BIND(&if_heapnumber);
      {
        var_float_value = LoadHeapNumberValue(value_heap_object);
        Goto(&do_float_op);
      }

      BIND(&if_bigint);
      {
        var_result = BigIntOp(value_heap_object);
        CombineFeedback(&var_feedback, BinaryOperationFeedback::kBigInt);
        Goto(&end);
      }

      BIND(&if_oddball);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_feedback.value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));
        OverwriteFeedback(&var_feedback,
                          BinaryOperationFeedback::kNumberOrOddball);
        var_value =
            LoadObjectField(value_heap_object, Oddball::kToNumberOffset);
        Goto(&start);
      }

      BIND(&if_other);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(this, SmiEqual(var_feedback.value(),
                                  SmiConstant(BinaryOperationFeedback::kNone)));
        OverwriteFeedback(&var_feedback, BinaryOperationFeedback::kAny);
        var_value = CallBuiltin(Builtins::kNonNumberToNumeric, GetContext(),
                                value_heap_object);
        Goto(&start);
      }
    }

    BIND(&do_float_op);
    {
      CombineFeedback(&var_feedback, BinaryOperationFeedback::kNumber);
      var_result =
          AllocateHeapNumberWithValue(FloatOp(var_float_value.value()));
      Goto(&end);
    }

    BIND(&end);
    TNode<UintPtrT> slot_index = BytecodeOperandIdx(0);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
    UpdateFeedback(var_feedback.value(), maybe_feedback_vector, slot_index);
    SetAccumulator(var_result.value());
    Dispatch();
  }
};

class NegateAssemblerImpl : public UnaryNumericOpAssembler {
 public:
  explicit NegateAssemblerImpl(CodeAssemblerState* state, Bytecode bytecode,
                               OperandScale operand_scale)
      : UnaryNumericOpAssembler(state, bytecode, operand_scale) {}

  TNode<Number> SmiOp(TNode<Smi> smi_value, TVariable<Smi>* var_feedback,
                      Label* do_float_op,
                      TVariable<Float64T>* var_float) override {
    TVARIABLE(Number, var_result);
    Label if_zero(this), if_min_smi(this), end(this);
    // Return -0 if operand is 0.
    GotoIf(SmiEqual(smi_value, SmiConstant(0)), &if_zero);

    // Special-case the minimum Smi to avoid overflow.
    GotoIf(SmiEqual(smi_value, SmiConstant(Smi::kMinValue)), &if_min_smi);

    // Else simply subtract operand from 0.
    CombineFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
    var_result = SmiSub(SmiConstant(0), smi_value);
    Goto(&end);

    BIND(&if_zero);
    CombineFeedback(var_feedback, BinaryOperationFeedback::kNumber);
    var_result = MinusZeroConstant();
    Goto(&end);

    BIND(&if_min_smi);
    *var_float = SmiToFloat64(smi_value);
    Goto(do_float_op);

    BIND(&end);
    return var_result.value();
  }

  TNode<Float64T> FloatOp(TNode<Float64T> float_value) override {
    return Float64Neg(float_value);
  }

  TNode<HeapObject> BigIntOp(TNode<HeapObject> bigint_value) override {
    return CAST(CallRuntime(Runtime::kBigIntUnaryOp, GetContext(), bigint_value,
                            SmiConstant(Operation::kNegate)));
  }
};

// Negate <feedback_slot>
//
// Perform arithmetic negation on the accumulator.
IGNITION_HANDLER(Negate, NegateAssemblerImpl) { UnaryOpWithFeedback(); }

// ToName <dst>
//
// Convert the object referenced by the accumulator to a name.
IGNITION_HANDLER(ToName, InterpreterAssembler) {
  TNode<Object> object = GetAccumulator();
  TNode<Context> context = GetContext();
  TNode<Object> result = CallBuiltin(Builtins::kToName, context, object);
  StoreRegisterAtOperandIndex(result, 0);
  Dispatch();
}

// ToNumber <slot>
//
// Convert the object referenced by the accumulator to a number.
IGNITION_HANDLER(ToNumber, InterpreterAssembler) {
  ToNumberOrNumeric(Object::Conversion::kToNumber);
}

// ToNumeric <slot>
//
// Convert the object referenced by the accumulator to a numeric.
IGNITION_HANDLER(ToNumeric, InterpreterAssembler) {
  ToNumberOrNumeric(Object::Conversion::kToNumeric);
}

// ToObject <dst>
//
// Convert the object referenced by the accumulator to a JSReceiver.
IGNITION_HANDLER(ToObject, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<Context> context = GetContext();
  TNode<Object> result = CallBuiltin(Builtins::kToObject, context, accumulator);
  StoreRegisterAtOperandIndex(result, 0);
  Dispatch();
}

// ToString
//
// Convert the accumulator to a String.
IGNITION_HANDLER(ToString, InterpreterAssembler) {
  SetAccumulator(ToString_Inline(GetContext(), GetAccumulator()));
  Dispatch();
}

class IncDecAssembler : public UnaryNumericOpAssembler {
 public:
  explicit IncDecAssembler(CodeAssemblerState* state, Bytecode bytecode,
                           OperandScale operand_scale)
      : UnaryNumericOpAssembler(state, bytecode, operand_scale) {}

  Operation op() {
    DCHECK(op_ == Operation::kIncrement || op_ == Operation::kDecrement);
    return op_;
  }

  TNode<Number> SmiOp(TNode<Smi> value, TVariable<Smi>* var_feedback,
                      Label* do_float_op,
                      TVariable<Float64T>* var_float) override {
    TNode<Smi> one = SmiConstant(1);
    Label if_overflow(this), if_notoverflow(this);
    TNode<Smi> result = op() == Operation::kIncrement
                            ? TrySmiAdd(value, one, &if_overflow)
                            : TrySmiSub(value, one, &if_overflow);
    Goto(&if_notoverflow);

    BIND(&if_overflow);
    {
      *var_float = SmiToFloat64(value);
      Goto(do_float_op);
    }

    BIND(&if_notoverflow);
    CombineFeedback(var_feedback, BinaryOperationFeedback::kSignedSmall);
    return result;
  }

  TNode<Float64T> FloatOp(TNode<Float64T> float_value) override {
    return op() == Operation::kIncrement
               ? Float64Add(float_value, Float64Constant(1.0))
               : Float64Sub(float_value, Float64Constant(1.0));
  }

  TNode<HeapObject> BigIntOp(TNode<HeapObject> bigint_value) override {
    return CAST(CallRuntime(Runtime::kBigIntUnaryOp, GetContext(), bigint_value,
                            SmiConstant(op())));
  }

  void IncWithFeedback() {
    op_ = Operation::kIncrement;
    UnaryOpWithFeedback();
  }

  void DecWithFeedback() {
    op_ = Operation::kDecrement;
    UnaryOpWithFeedback();
  }

 private:
  Operation op_ = Operation::kEqual;  // Dummy initialization.
};

// Inc
//
// Increments value in the accumulator by one.
IGNITION_HANDLER(Inc, IncDecAssembler) { IncWithFeedback(); }

// Dec
//
// Decrements value in the accumulator by one.
IGNITION_HANDLER(Dec, IncDecAssembler) { DecWithFeedback(); }

// ToBooleanLogicalNot
//
// Perform logical-not on the accumulator, first casting the
// accumulator to a boolean value if required.
IGNITION_HANDLER(ToBooleanLogicalNot, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TVARIABLE(Oddball, result);
  Label if_true(this), if_false(this), end(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  {
    result = FalseConstant();
    Goto(&end);
  }
  BIND(&if_false);
  {
    result = TrueConstant();
    Goto(&end);
  }
  BIND(&end);
  SetAccumulator(result.value());
  Dispatch();
}

// LogicalNot
//
// Perform logical-not on the accumulator, which must already be a boolean
// value.
IGNITION_HANDLER(LogicalNot, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TVARIABLE(Oddball, result);
  Label if_true(this), if_false(this), end(this);
  TNode<Oddball> true_value = TrueConstant();
  TNode<Oddball> false_value = FalseConstant();
  Branch(TaggedEqual(value, true_value), &if_true, &if_false);
  BIND(&if_true);
  {
    result = false_value;
    Goto(&end);
  }
  BIND(&if_false);
  {
    CSA_ASSERT(this, TaggedEqual(value, false_value));
    result = true_value;
    Goto(&end);
  }
  BIND(&end);
  SetAccumulator(result.value());
  Dispatch();
}

// TypeOf
//
// Load the accumulator with the string representating type of the
// object in the accumulator.
IGNITION_HANDLER(TypeOf, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<String> result = Typeof(value);
  SetAccumulator(result);
  Dispatch();
}

// DeletePropertyStrict
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following strict mode semantics.
IGNITION_HANDLER(DeletePropertyStrict, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Object> key = GetAccumulator();
  TNode<Context> context = GetContext();
  TNode<Object> result =
      CallBuiltin(Builtins::kDeleteProperty, context, object, key,
                  SmiConstant(Smi::FromEnum(LanguageMode::kStrict)));
  SetAccumulator(result);
  Dispatch();
}

// DeletePropertySloppy
//
// Delete the property specified in the accumulator from the object
// referenced by the register operand following sloppy mode semantics.
IGNITION_HANDLER(DeletePropertySloppy, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Object> key = GetAccumulator();
  TNode<Context> context = GetContext();
  TNode<Object> result =
      CallBuiltin(Builtins::kDeleteProperty, context, object, key,
                  SmiConstant(Smi::FromEnum(LanguageMode::kSloppy)));
  SetAccumulator(result);
  Dispatch();
}

// GetSuperConstructor
//
// Get the super constructor from the object referenced by the accumulator.
// The result is stored in register |reg|.
IGNITION_HANDLER(GetSuperConstructor, InterpreterAssembler) {
  TNode<JSFunction> active_function = CAST(GetAccumulator());
  TNode<Context> context = GetContext();
  TNode<Object> result = GetSuperConstructor(context, active_function);
  StoreRegisterAtOperandIndex(result, 0);
  Dispatch();
}

class InterpreterJSCallAssembler : public InterpreterAssembler {
 public:
  InterpreterJSCallAssembler(CodeAssemblerState* state, Bytecode bytecode,
                             OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  // Generates code to perform a JS call that collects type feedback.
  void JSCall(ConvertReceiverMode receiver_mode) {
    TNode<Object> function = LoadRegisterAtOperandIndex(0);
    RegListNodePair args = GetRegisterListAtOperandIndex(1);
    TNode<UintPtrT> slot_id = BytecodeOperandIdx(3);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
    TNode<Context> context = GetContext();

    // Collect the {function} feedback.
    CollectCallFeedback(function, context, maybe_feedback_vector, slot_id);

    // Call the function and dispatch to the next handler.
    CallJSAndDispatch(function, context, args, receiver_mode);
  }

  // Generates code to perform a JS call without collecting feedback.
  void JSCallNoFeedback(ConvertReceiverMode receiver_mode) {
    TNode<Object> function = LoadRegisterAtOperandIndex(0);
    RegListNodePair args = GetRegisterListAtOperandIndex(1);
    TNode<Context> context = GetContext();

    // Call the function and dispatch to the next handler.
    CallJSAndDispatch(function, context, args, receiver_mode);
  }

  // Generates code to perform a JS call with a known number of arguments that
  // collects type feedback.
  void JSCallN(int arg_count, ConvertReceiverMode receiver_mode) {
    // Indices and counts of operands on the bytecode.
    const int kFirstArgumentOperandIndex = 1;
    const int kReceiverOperandCount =
        (receiver_mode == ConvertReceiverMode::kNullOrUndefined) ? 0 : 1;
    const int kRecieverAndArgOperandCount = kReceiverOperandCount + arg_count;
    const int kSlotOperandIndex =
        kFirstArgumentOperandIndex + kRecieverAndArgOperandCount;

    TNode<Object> function = LoadRegisterAtOperandIndex(0);
    TNode<UintPtrT> slot_id = BytecodeOperandIdx(kSlotOperandIndex);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
    TNode<Context> context = GetContext();

    // Collect the {function} feedback.
    CollectCallFeedback(function, context, maybe_feedback_vector, slot_id);

    switch (kRecieverAndArgOperandCount) {
      case 0:
        CallJSAndDispatch(function, context, Int32Constant(arg_count),
                          receiver_mode);
        break;
      case 1:
        CallJSAndDispatch(
            function, context, Int32Constant(arg_count), receiver_mode,
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex));
        break;
      case 2:
        CallJSAndDispatch(
            function, context, Int32Constant(arg_count), receiver_mode,
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex),
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex + 1));
        break;
      case 3:
        CallJSAndDispatch(
            function, context, Int32Constant(arg_count), receiver_mode,
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex),
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex + 1),
            LoadRegisterAtOperandIndex(kFirstArgumentOperandIndex + 2));
        break;
      default:
        UNREACHABLE();
    }
  }
};

// Call <callable> <receiver> <arg_count> <feedback_slot_id>
//
// Call a JSfunction or Callable in |callable| with the |receiver| and
// |arg_count| arguments in subsequent registers. Collect type feedback
// into |feedback_slot_id|
IGNITION_HANDLER(CallAnyReceiver, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kAny);
}

IGNITION_HANDLER(CallProperty, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallProperty0, InterpreterJSCallAssembler) {
  JSCallN(0, ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallProperty1, InterpreterJSCallAssembler) {
  JSCallN(1, ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallProperty2, InterpreterJSCallAssembler) {
  JSCallN(2, ConvertReceiverMode::kNotNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver, InterpreterJSCallAssembler) {
  JSCall(ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver0, InterpreterJSCallAssembler) {
  JSCallN(0, ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver1, InterpreterJSCallAssembler) {
  JSCallN(1, ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallUndefinedReceiver2, InterpreterJSCallAssembler) {
  JSCallN(2, ConvertReceiverMode::kNullOrUndefined);
}

IGNITION_HANDLER(CallNoFeedback, InterpreterJSCallAssembler) {
  JSCallNoFeedback(ConvertReceiverMode::kAny);
}

// CallRuntime <function_id> <first_arg> <arg_count>
//
// Call the runtime function |function_id| with the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers.
IGNITION_HANDLER(CallRuntime, InterpreterAssembler) {
  TNode<Uint32T> function_id = BytecodeOperandRuntimeId(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  TNode<Context> context = GetContext();
  Node* result = CallRuntimeN(function_id, context, args);
  SetAccumulator(result);
  Dispatch();
}

// InvokeIntrinsic <function_id> <first_arg> <arg_count>
//
// Implements the semantic equivalent of calling the runtime function
// |function_id| with the first argument in |first_arg| and |arg_count|
// arguments in subsequent registers.
IGNITION_HANDLER(InvokeIntrinsic, InterpreterAssembler) {
  TNode<Uint32T> function_id = BytecodeOperandIntrinsicId(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  TNode<Context> context = GetContext();
  TNode<Object> result =
      GenerateInvokeIntrinsic(this, function_id, context, args);
  SetAccumulator(result);
  Dispatch();
}

// CallRuntimeForPair <function_id> <first_arg> <arg_count> <first_return>
//
// Call the runtime function |function_id| which returns a pair, with the
// first argument in register |first_arg| and |arg_count| arguments in
// subsequent registers. Returns the result in <first_return> and
// <first_return + 1>
IGNITION_HANDLER(CallRuntimeForPair, InterpreterAssembler) {
  // Call the runtime function.
  TNode<Uint32T> function_id = BytecodeOperandRuntimeId(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  TNode<Context> context = GetContext();
  Node* result_pair = CallRuntimeN(function_id, context, args, 2);
  // Store the results in <first_return> and <first_return + 1>
  TNode<Object> result0 = CAST(Projection(0, result_pair));
  TNode<Object> result1 = CAST(Projection(1, result_pair));
  StoreRegisterPairAtOperandIndex(result0, result1, 3);
  Dispatch();
}

// CallJSRuntime <context_index> <receiver> <arg_count>
//
// Call the JS runtime function that has the |context_index| with the receiver
// in register |receiver| and |arg_count| arguments in subsequent registers.
IGNITION_HANDLER(CallJSRuntime, InterpreterAssembler) {
  TNode<IntPtrT> context_index = Signed(BytecodeOperandNativeContextIndex(0));
  RegListNodePair args = GetRegisterListAtOperandIndex(1);

  // Get the function to call from the native context.
  TNode<Context> context = GetContext();
  TNode<NativeContext> native_context = LoadNativeContext(context);
  TNode<Object> function = LoadContextElement(native_context, context_index);

  // Call the function.
  CallJSAndDispatch(function, context, args,
                    ConvertReceiverMode::kNullOrUndefined);
}

// CallWithSpread <callable> <first_arg> <arg_count>
//
// Call a JSfunction or Callable in |callable| with the receiver in
// |first_arg| and |arg_count - 1| arguments in subsequent registers. The
// final argument is always a spread.
//
IGNITION_HANDLER(CallWithSpread, InterpreterAssembler) {
  TNode<Object> callable = LoadRegisterAtOperandIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(3);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  // Call into Runtime function CallWithSpread which does everything.
  CallJSWithSpreadAndDispatch(callable, context, args, slot_id,
                              maybe_feedback_vector);
}

// ConstructWithSpread <first_arg> <arg_count>
//
// Call the constructor in |constructor| with the first argument in register
// |first_arg| and |arg_count| arguments in subsequent registers. The final
// argument is always a spread. The new.target is in the accumulator.
//
IGNITION_HANDLER(ConstructWithSpread, InterpreterAssembler) {
  TNode<Object> new_target = GetAccumulator();
  TNode<Object> constructor = LoadRegisterAtOperandIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(3);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();
  TNode<Object> result = ConstructWithSpread(
      constructor, context, new_target, args, slot_id, maybe_feedback_vector);
  SetAccumulator(result);
  Dispatch();
}

// Construct <constructor> <first_arg> <arg_count>
//
// Call operator construct with |constructor| and the first argument in
// register |first_arg| and |arg_count| arguments in subsequent
// registers. The new.target is in the accumulator.
//
IGNITION_HANDLER(Construct, InterpreterAssembler) {
  TNode<Object> new_target = GetAccumulator();
  TNode<Object> constructor = LoadRegisterAtOperandIndex(0);
  RegListNodePair args = GetRegisterListAtOperandIndex(1);
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(3);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();
  TNode<Object> result = Construct(constructor, context, new_target, args,
                                   slot_id, maybe_feedback_vector);
  SetAccumulator(result);
  Dispatch();
}

class InterpreterCompareOpAssembler : public InterpreterAssembler {
 public:
  InterpreterCompareOpAssembler(CodeAssemblerState* state, Bytecode bytecode,
                                OperandScale operand_scale)
      : InterpreterAssembler(state, bytecode, operand_scale) {}

  void CompareOpWithFeedback(Operation compare_op) {
    TNode<Object> lhs = LoadRegisterAtOperandIndex(0);
    TNode<Object> rhs = GetAccumulator();
    TNode<Context> context = GetContext();

    TVARIABLE(Smi, var_type_feedback);
    TNode<Oddball> result;
    switch (compare_op) {
      case Operation::kEqual:
        result = Equal(lhs, rhs, context, &var_type_feedback);
        break;
      case Operation::kStrictEqual:
        result = StrictEqual(lhs, rhs, &var_type_feedback);
        break;
      case Operation::kLessThan:
      case Operation::kGreaterThan:
      case Operation::kLessThanOrEqual:
      case Operation::kGreaterThanOrEqual:
        result = RelationalComparison(compare_op, lhs, rhs, context,
                                      &var_type_feedback);
        break;
      default:
        UNREACHABLE();
    }

    TNode<UintPtrT> slot_index = BytecodeOperandIdx(1);
    TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
    UpdateFeedback(var_type_feedback.value(), maybe_feedback_vector,
                   slot_index);
    SetAccumulator(result);
    Dispatch();
  }
};

// TestEqual <src>
//
// Test if the value in the <src> register equals the accumulator.
IGNITION_HANDLER(TestEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kEqual);
}

// TestEqualStrict <src>
//
// Test if the value in the <src> register is strictly equal to the accumulator.
IGNITION_HANDLER(TestEqualStrict, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kStrictEqual);
}

// TestLessThan <src>
//
// Test if the value in the <src> register is less than the accumulator.
IGNITION_HANDLER(TestLessThan, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kLessThan);
}

// TestGreaterThan <src>
//
// Test if the value in the <src> register is greater than the accumulator.
IGNITION_HANDLER(TestGreaterThan, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kGreaterThan);
}

// TestLessThanOrEqual <src>
//
// Test if the value in the <src> register is less than or equal to the
// accumulator.
IGNITION_HANDLER(TestLessThanOrEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kLessThanOrEqual);
}

// TestGreaterThanOrEqual <src>
//
// Test if the value in the <src> register is greater than or equal to the
// accumulator.
IGNITION_HANDLER(TestGreaterThanOrEqual, InterpreterCompareOpAssembler) {
  CompareOpWithFeedback(Operation::kGreaterThanOrEqual);
}

// TestReferenceEqual <src>
//
// Test if the value in the <src> register is equal to the accumulator
// by means of simple comparison. For SMIs and simple reference comparisons.
IGNITION_HANDLER(TestReferenceEqual, InterpreterAssembler) {
  TNode<Object> lhs = LoadRegisterAtOperandIndex(0);
  TNode<Object> rhs = GetAccumulator();
  TNode<Oddball> result = SelectBooleanConstant(TaggedEqual(lhs, rhs));
  SetAccumulator(result);
  Dispatch();
}

// TestIn <src> <feedback_slot>
//
// Test if the object referenced by the register operand is a property of the
// object referenced by the accumulator.
IGNITION_HANDLER(TestIn, InterpreterAssembler) {
  TNode<Object> name = LoadRegisterAtOperandIndex(0);
  TNode<Object> object = GetAccumulator();
  TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(1));
  TNode<Smi> smi_slot = SmiTag(raw_slot);
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  TVARIABLE(Object, var_result);
  var_result = CallBuiltin(Builtins::kKeyedHasIC, context, object, name,
                           smi_slot, feedback_vector);
  SetAccumulator(var_result.value());
  Dispatch();
}

// TestInstanceOf <src> <feedback_slot>
//
// Test if the object referenced by the <src> register is an an instance of type
// referenced by the accumulator.
IGNITION_HANDLER(TestInstanceOf, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<Object> callable = GetAccumulator();
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(1);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  Label feedback_done(this);
  GotoIf(IsUndefined(maybe_feedback_vector), &feedback_done);

  // Record feedback for the {callable} in the {feedback_vector}.
  CollectCallableFeedback(callable, context, CAST(maybe_feedback_vector),
                          slot_id);
  Goto(&feedback_done);

  BIND(&feedback_done);
  // Perform the actual instanceof operation.
  SetAccumulator(InstanceOf(object, callable, context));
  Dispatch();
}

// TestUndetectable
//
// Test if the value in the accumulator is undetectable (null, undefined or
// document.all).
IGNITION_HANDLER(TestUndetectable, InterpreterAssembler) {
  Label return_false(this), end(this);
  TNode<Object> object = GetAccumulator();

  // If the object is an Smi then return false.
  SetAccumulator(FalseConstant());
  GotoIf(TaggedIsSmi(object), &end);

  // If it is a HeapObject, load the map and check for undetectable bit.
  TNode<Oddball> result =
      SelectBooleanConstant(IsUndetectableMap(LoadMap(CAST(object))));
  SetAccumulator(result);
  Goto(&end);

  BIND(&end);
  Dispatch();
}

// TestNull
//
// Test if the value in accumulator is strictly equal to null.
IGNITION_HANDLER(TestNull, InterpreterAssembler) {
  TNode<Object> object = GetAccumulator();
  TNode<Oddball> result =
      SelectBooleanConstant(TaggedEqual(object, NullConstant()));
  SetAccumulator(result);
  Dispatch();
}

// TestUndefined
//
// Test if the value in the accumulator is strictly equal to undefined.
IGNITION_HANDLER(TestUndefined, InterpreterAssembler) {
  TNode<Object> object = GetAccumulator();
  TNode<Oddball> result =
      SelectBooleanConstant(TaggedEqual(object, UndefinedConstant()));
  SetAccumulator(result);
  Dispatch();
}

// TestTypeOf <literal_flag>
//
// Tests if the object in the <accumulator> is typeof the literal represented
// by |literal_flag|.
IGNITION_HANDLER(TestTypeOf, InterpreterAssembler) {
  TNode<Object> object = GetAccumulator();
  TNode<Uint32T> literal_flag = BytecodeOperandFlag(0);

#define MAKE_LABEL(name, lower_case) Label if_##lower_case(this);
  TYPEOF_LITERAL_LIST(MAKE_LABEL)
#undef MAKE_LABEL

#define LABEL_POINTER(name, lower_case) &if_##lower_case,
  Label* labels[] = {TYPEOF_LITERAL_LIST(LABEL_POINTER)};
#undef LABEL_POINTER

#define CASE(name, lower_case) \
  static_cast<int32_t>(TestTypeOfFlags::LiteralFlag::k##name),
  int32_t cases[] = {TYPEOF_LITERAL_LIST(CASE)};
#undef CASE

  Label if_true(this), if_false(this), end(this);

  // We juse use the final label as the default and properly CSA_ASSERT
  // that the {literal_flag} is valid here; this significantly improves
  // the generated code (compared to having a default label that aborts).
  unsigned const num_cases = arraysize(cases);
  CSA_ASSERT(this, Uint32LessThan(literal_flag, Int32Constant(num_cases)));
  Switch(literal_flag, labels[num_cases - 1], cases, labels, num_cases - 1);

  BIND(&if_number);
  {
    Comment("IfNumber");
    GotoIfNumber(object, &if_true);
    Goto(&if_false);
  }
  BIND(&if_string);
  {
    Comment("IfString");
    GotoIf(TaggedIsSmi(object), &if_false);
    Branch(IsString(CAST(object)), &if_true, &if_false);
  }
  BIND(&if_symbol);
  {
    Comment("IfSymbol");
    GotoIf(TaggedIsSmi(object), &if_false);
    Branch(IsSymbol(CAST(object)), &if_true, &if_false);
  }
  BIND(&if_boolean);
  {
    Comment("IfBoolean");
    GotoIf(TaggedEqual(object, TrueConstant()), &if_true);
    Branch(TaggedEqual(object, FalseConstant()), &if_true, &if_false);
  }
  BIND(&if_bigint);
  {
    Comment("IfBigInt");
    GotoIf(TaggedIsSmi(object), &if_false);
    Branch(IsBigInt(CAST(object)), &if_true, &if_false);
  }
  BIND(&if_undefined);
  {
    Comment("IfUndefined");
    GotoIf(TaggedIsSmi(object), &if_false);
    // Check it is not null and the map has the undetectable bit set.
    GotoIf(IsNull(object), &if_false);
    Branch(IsUndetectableMap(LoadMap(CAST(object))), &if_true, &if_false);
  }
  BIND(&if_function);
  {
    Comment("IfFunction");
    GotoIf(TaggedIsSmi(object), &if_false);
    // Check if callable bit is set and not undetectable.
    TNode<Int32T> map_bitfield = LoadMapBitField(LoadMap(CAST(object)));
    TNode<Int32T> callable_undetectable =
        Word32And(map_bitfield, Int32Constant(Map::IsUndetectableBit::kMask |
                                              Map::IsCallableBit::kMask));
    Branch(Word32Equal(callable_undetectable,
                       Int32Constant(Map::IsCallableBit::kMask)),
           &if_true, &if_false);
  }
  BIND(&if_object);
  {
    Comment("IfObject");
    GotoIf(TaggedIsSmi(object), &if_false);

    // If the object is null then return true.
    GotoIf(IsNull(object), &if_true);

    // Check if the object is a receiver type and is not undefined or callable.
    TNode<Map> map = LoadMap(CAST(object));
    GotoIfNot(IsJSReceiverMap(map), &if_false);
    TNode<Int32T> map_bitfield = LoadMapBitField(map);
    TNode<Int32T> callable_undetectable =
        Word32And(map_bitfield, Int32Constant(Map::IsUndetectableBit::kMask |
                                              Map::IsCallableBit::kMask));
    Branch(Word32Equal(callable_undetectable, Int32Constant(0)), &if_true,
           &if_false);
  }
  BIND(&if_other);
  {
    // Typeof doesn't return any other string value.
    Goto(&if_false);
  }

  BIND(&if_false);
  {
    SetAccumulator(FalseConstant());
    Goto(&end);
  }
  BIND(&if_true);
  {
    SetAccumulator(TrueConstant());
    Goto(&end);
  }
  BIND(&end);
  Dispatch();
}

// Jump <imm>
//
// Jump by the number of bytes represented by the immediate operand |imm|.
IGNITION_HANDLER(Jump, InterpreterAssembler) {
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  Jump(relative_jump);
}

// JumpConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool.
IGNITION_HANDLER(JumpConstant, InterpreterAssembler) {
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Jump(relative_jump);
}

// JumpIfTrue <imm>
//
// Jump by the number of bytes represented by an immediate operand if the
// accumulator contains true. This only works for boolean inputs, and
// will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfTrue, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  CSA_ASSERT(this, IsBoolean(CAST(accumulator)));
  JumpIfTaggedEqual(accumulator, TrueConstant(), relative_jump);
}

// JumpIfTrueConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the accumulator contains true. This only works for boolean inputs,
// and will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfTrueConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  CSA_ASSERT(this, IsBoolean(CAST(accumulator)));
  JumpIfTaggedEqual(accumulator, TrueConstant(), relative_jump);
}

// JumpIfFalse <imm>
//
// Jump by the number of bytes represented by an immediate operand if the
// accumulator contains false. This only works for boolean inputs, and
// will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfFalse, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  CSA_ASSERT(this, IsBoolean(CAST(accumulator)));
  JumpIfTaggedEqual(accumulator, FalseConstant(), relative_jump);
}

// JumpIfFalseConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the accumulator contains false. This only works for boolean inputs,
// and will misbehave if passed arbitrary input values.
IGNITION_HANDLER(JumpIfFalseConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  CSA_ASSERT(this, IsBoolean(CAST(accumulator)));
  JumpIfTaggedEqual(accumulator, FalseConstant(), relative_jump);
}

// JumpIfToBooleanTrue <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is true when the object is cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanTrue, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Jump(relative_jump);
  BIND(&if_false);
  Dispatch();
}

// JumpIfToBooleanTrueConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is true when the object is
// cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanTrueConstant, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Jump(relative_jump);
  BIND(&if_false);
  Dispatch();
}

// JumpIfToBooleanFalse <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is false when the object is cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanFalse, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Dispatch();
  BIND(&if_false);
  Jump(relative_jump);
}

// JumpIfToBooleanFalseConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is false when the object is
// cast to boolean.
IGNITION_HANDLER(JumpIfToBooleanFalseConstant, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Label if_true(this), if_false(this);
  BranchIfToBooleanIsTrue(value, &if_true, &if_false);
  BIND(&if_true);
  Dispatch();
  BIND(&if_false);
  Jump(relative_jump);
}

// JumpIfNull <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the null constant.
IGNITION_HANDLER(JumpIfNull, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  JumpIfTaggedEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the null constant.
IGNITION_HANDLER(JumpIfNullConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfTaggedEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfNotNull <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is not the null constant.
IGNITION_HANDLER(JumpIfNotNull, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  JumpIfTaggedNotEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfNotNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is not the null constant.
IGNITION_HANDLER(JumpIfNotNullConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfTaggedNotEqual(accumulator, NullConstant(), relative_jump);
}

// JumpIfUndefined <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the undefined constant.
IGNITION_HANDLER(JumpIfUndefined, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  JumpIfTaggedEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfUndefinedConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the undefined constant.
IGNITION_HANDLER(JumpIfUndefinedConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfTaggedEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfNotUndefined <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is not the undefined constant.
IGNITION_HANDLER(JumpIfNotUndefined, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  JumpIfTaggedNotEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfNotUndefinedConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is not the undefined
// constant.
IGNITION_HANDLER(JumpIfNotUndefinedConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  JumpIfTaggedNotEqual(accumulator, UndefinedConstant(), relative_jump);
}

// JumpIfUndefinedOrNull <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is the undefined constant or the null constant.
IGNITION_HANDLER(JumpIfUndefinedOrNull, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();

  Label do_jump(this);
  GotoIf(IsUndefined(accumulator), &do_jump);
  GotoIf(IsNull(accumulator), &do_jump);
  Dispatch();

  BIND(&do_jump);
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  Jump(relative_jump);
}

// JumpIfUndefinedOrNullConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is the undefined constant or
// the null constant.
IGNITION_HANDLER(JumpIfUndefinedOrNullConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();

  Label do_jump(this);
  GotoIf(IsUndefined(accumulator), &do_jump);
  GotoIf(IsNull(accumulator), &do_jump);
  Dispatch();

  BIND(&do_jump);
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);
  Jump(relative_jump);
}

// JumpIfJSReceiver <imm>
//
// Jump by the number of bytes represented by an immediate operand if the object
// referenced by the accumulator is a JSReceiver.
IGNITION_HANDLER(JumpIfJSReceiver, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));

  Label if_object(this), if_notobject(this, Label::kDeferred), if_notsmi(this);
  Branch(TaggedIsSmi(accumulator), &if_notobject, &if_notsmi);

  BIND(&if_notsmi);
  Branch(IsJSReceiver(CAST(accumulator)), &if_object, &if_notobject);
  BIND(&if_object);
  Jump(relative_jump);

  BIND(&if_notobject);
  Dispatch();
}

// JumpIfJSReceiverConstant <idx>
//
// Jump by the number of bytes in the Smi in the |idx| entry in the constant
// pool if the object referenced by the accumulator is a JSReceiver.
IGNITION_HANDLER(JumpIfJSReceiverConstant, InterpreterAssembler) {
  TNode<Object> accumulator = GetAccumulator();
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntryAtOperandIndex(0);

  Label if_object(this), if_notobject(this), if_notsmi(this);
  Branch(TaggedIsSmi(accumulator), &if_notobject, &if_notsmi);

  BIND(&if_notsmi);
  Branch(IsJSReceiver(CAST(accumulator)), &if_object, &if_notobject);

  BIND(&if_object);
  Jump(relative_jump);

  BIND(&if_notobject);
  Dispatch();
}

// JumpLoop <imm> <loop_depth>
//
// Jump by the number of bytes represented by the immediate operand |imm|. Also
// performs a loop nesting check and potentially triggers OSR in case the
// current OSR level matches (or exceeds) the specified |loop_depth|.
IGNITION_HANDLER(JumpLoop, InterpreterAssembler) {
  TNode<IntPtrT> relative_jump = Signed(BytecodeOperandUImmWord(0));
  TNode<Int32T> loop_depth = BytecodeOperandImm(1);
  TNode<Int8T> osr_level = LoadOsrNestingLevel();

  // Check if OSR points at the given {loop_depth} are armed by comparing it to
  // the current {osr_level} loaded from the header of the BytecodeArray.
  Label ok(this), osr_armed(this, Label::kDeferred);
  TNode<BoolT> condition = Int32GreaterThanOrEqual(loop_depth, osr_level);
  Branch(condition, &ok, &osr_armed);

  BIND(&ok);
  JumpBackward(relative_jump);

  BIND(&osr_armed);
  {
    Callable callable = CodeFactory::InterpreterOnStackReplacement(isolate());
    TNode<Code> target = HeapConstant(callable.code());
    TNode<Context> context = GetContext();
    CallStub(callable.descriptor(), target, context);
    JumpBackward(relative_jump);
  }
}

// SwitchOnSmiNoFeedback <table_start> <table_length> <case_value_base>
//
// Jump by the number of bytes defined by a Smi in a table in the constant pool,
// where the table starts at |table_start| and has |table_length| entries.
// The table is indexed by the accumulator, minus |case_value_base|. If the
// case_value falls outside of the table |table_length|, fall-through to the
// next bytecode.
IGNITION_HANDLER(SwitchOnSmiNoFeedback, InterpreterAssembler) {
  TNode<Object> acc = GetAccumulator();
  TNode<UintPtrT> table_start = BytecodeOperandIdx(0);
  TNode<UintPtrT> table_length = BytecodeOperandUImmWord(1);
  TNode<IntPtrT> case_value_base = BytecodeOperandImmIntPtr(2);

  Label fall_through(this);

  // The accumulator must be a Smi.
  // TODO(leszeks): Add a bytecode with type feedback that allows other
  // accumulator values.
  CSA_ASSERT(this, TaggedIsSmi(acc));

  TNode<IntPtrT> case_value = IntPtrSub(SmiUntag(CAST(acc)), case_value_base);
  GotoIf(IntPtrLessThan(case_value, IntPtrConstant(0)), &fall_through);
  GotoIf(IntPtrGreaterThanOrEqual(case_value, table_length), &fall_through);
  TNode<WordT> entry = IntPtrAdd(table_start, case_value);
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntry(entry);
  Jump(relative_jump);

  BIND(&fall_through);
  Dispatch();
}

// CreateRegExpLiteral <pattern_idx> <literal_idx> <flags>
//
// Creates a regular expression literal for literal index <literal_idx> with
// <flags> and the pattern in <pattern_idx>.
IGNITION_HANDLER(CreateRegExpLiteral, InterpreterAssembler) {
  TNode<Object> pattern = LoadConstantPoolEntryAtOperandIndex(0);
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(1);
  TNode<Smi> flags =
      SmiFromInt32(UncheckedCast<Int32T>(BytecodeOperandFlag(2)));
  TNode<Context> context = GetContext();

  TVARIABLE(JSRegExp, result);

  ConstructorBuiltinsAssembler constructor_assembler(state());
  result = constructor_assembler.EmitCreateRegExpLiteral(
      feedback_vector, slot_id, pattern, flags, context);
  SetAccumulator(result.value());
  Dispatch();
}

// CreateArrayLiteral <element_idx> <literal_idx> <flags>
//
// Creates an array literal for literal index <literal_idx> with
// CreateArrayLiteral flags <flags> and constant elements in <element_idx>.
IGNITION_HANDLER(CreateArrayLiteral, InterpreterAssembler) {
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(1);
  TNode<Context> context = GetContext();
  TNode<Uint32T> bytecode_flags = BytecodeOperandFlag(2);

  Label fast_shallow_clone(this), call_runtime(this, Label::kDeferred);
  // No feedback, so handle it as a slow case.
  GotoIf(IsUndefined(feedback_vector), &call_runtime);

  Branch(IsSetWord32<CreateArrayLiteralFlags::FastCloneSupportedBit>(
             bytecode_flags),
         &fast_shallow_clone, &call_runtime);

  BIND(&fast_shallow_clone);
  {
    ConstructorBuiltinsAssembler constructor_assembler(state());
    TNode<JSArray> result = constructor_assembler.EmitCreateShallowArrayLiteral(
        CAST(feedback_vector), slot_id, context, &call_runtime,
        TRACK_ALLOCATION_SITE);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&call_runtime);
  {
    TNode<UintPtrT> flags_raw =
        DecodeWordFromWord32<CreateArrayLiteralFlags::FlagsBits>(
            bytecode_flags);
    TNode<Smi> flags = SmiTag(Signed(flags_raw));
    TNode<Object> constant_elements = LoadConstantPoolEntryAtOperandIndex(0);
    TNode<Object> result =
        CallRuntime(Runtime::kCreateArrayLiteral, context, feedback_vector,
                    SmiTag(Signed(slot_id)), constant_elements, flags);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateEmptyArrayLiteral <literal_idx>
//
// Creates an empty JSArray literal for literal index <literal_idx>.
IGNITION_HANDLER(CreateEmptyArrayLiteral, InterpreterAssembler) {
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(0);
  TNode<Context> context = GetContext();

  Label no_feedback(this, Label::kDeferred), end(this);
  TVARIABLE(JSArray, result);
  GotoIf(IsUndefined(maybe_feedback_vector), &no_feedback);

  ConstructorBuiltinsAssembler constructor_assembler(state());
  result = constructor_assembler.EmitCreateEmptyArrayLiteral(
      CAST(maybe_feedback_vector), slot_id, context);
  Goto(&end);

  BIND(&no_feedback);
  {
    TNode<Map> array_map = LoadJSArrayElementsMap(GetInitialFastElementsKind(),
                                                  LoadNativeContext(context));
    result =
        AllocateJSArray(GetInitialFastElementsKind(), array_map, SmiConstant(0),
                        SmiConstant(0), {}, ParameterMode::SMI_PARAMETERS);
    Goto(&end);
  }

  BIND(&end);
  SetAccumulator(result.value());
  Dispatch();
}

// CreateArrayFromIterable
//
// Spread the given iterable from the accumulator into a new JSArray.
// TODO(neis): Turn this into an intrinsic when we're running out of bytecodes.
IGNITION_HANDLER(CreateArrayFromIterable, InterpreterAssembler) {
  TNode<Object> iterable = GetAccumulator();
  TNode<Context> context = GetContext();
  TNode<Object> result =
      CallBuiltin(Builtins::kIterableToListWithSymbolLookup, context, iterable);
  SetAccumulator(result);
  Dispatch();
}

// CreateObjectLiteral <element_idx> <literal_idx> <flags>
//
// Creates an object literal for literal index <literal_idx> with
// CreateObjectLiteralFlags <flags> and constant elements in <element_idx>.
IGNITION_HANDLER(CreateObjectLiteral, InterpreterAssembler) {
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<UintPtrT> slot_id = BytecodeOperandIdx(1);
  TNode<Uint32T> bytecode_flags = BytecodeOperandFlag(2);

  Label if_fast_clone(this), if_not_fast_clone(this, Label::kDeferred);
  // No feedback, so handle it as a slow case.
  GotoIf(IsUndefined(feedback_vector), &if_not_fast_clone);

  // Check if we can do a fast clone or have to call the runtime.
  Branch(IsSetWord32<CreateObjectLiteralFlags::FastCloneSupportedBit>(
             bytecode_flags),
         &if_fast_clone, &if_not_fast_clone);

  BIND(&if_fast_clone);
  {
    // If we can do a fast clone do the fast-path in CreateShallowObjectLiteral.
    ConstructorBuiltinsAssembler constructor_assembler(state());
    TNode<HeapObject> result =
        constructor_assembler.EmitCreateShallowObjectLiteral(
            CAST(feedback_vector), slot_id, &if_not_fast_clone);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_not_fast_clone);
  {
    // If we can't do a fast clone, call into the runtime.
    TNode<ObjectBoilerplateDescription> object_boilerplate_description =
        CAST(LoadConstantPoolEntryAtOperandIndex(0));
    TNode<Context> context = GetContext();

    TNode<UintPtrT> flags_raw =
        DecodeWordFromWord32<CreateObjectLiteralFlags::FlagsBits>(
            bytecode_flags);
    TNode<Smi> flags = SmiTag(Signed(flags_raw));

    TNode<Object> result = CallRuntime(Runtime::kCreateObjectLiteral, context,
                                       feedback_vector, SmiTag(Signed(slot_id)),
                                       object_boilerplate_description, flags);
    SetAccumulator(result);
    // TODO(klaasb) build a single dispatch once the call is inlined
    Dispatch();
  }
}

// CreateEmptyObjectLiteral
//
// Creates an empty JSObject literal.
IGNITION_HANDLER(CreateEmptyObjectLiteral, InterpreterAssembler) {
  TNode<Context> context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  TNode<JSObject> result =
      constructor_assembler.EmitCreateEmptyObjectLiteral(context);
  SetAccumulator(result);
  Dispatch();
}

// CloneObject <source_idx> <flags> <feedback_slot>
//
// Allocates a new JSObject with each enumerable own property copied from
// {source}, converting getters into data properties.
IGNITION_HANDLER(CloneObject, InterpreterAssembler) {
  TNode<Object> source = LoadRegisterAtOperandIndex(0);
  TNode<Uint32T> bytecode_flags = BytecodeOperandFlag(1);
  TNode<UintPtrT> raw_flags =
      DecodeWordFromWord32<CreateObjectLiteralFlags::FlagsBits>(bytecode_flags);
  TNode<Smi> smi_flags = SmiTag(Signed(raw_flags));
  TNode<IntPtrT> raw_slot = Signed(BytecodeOperandIdx(2));
  TNode<Smi> smi_slot = SmiTag(raw_slot);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<Context> context = GetContext();

  TVARIABLE(Object, var_result);
  var_result = CallBuiltin(Builtins::kCloneObjectIC, context, source, smi_flags,
                           smi_slot, maybe_feedback_vector);
  SetAccumulator(var_result.value());
  Dispatch();
}

// GetTemplateObject <descriptor_idx> <literal_idx>
//
// Creates the template to pass for tagged templates and returns it in the
// accumulator, creating and caching the site object on-demand as per the
// specification.
IGNITION_HANDLER(GetTemplateObject, InterpreterAssembler) {
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();
  TNode<UintPtrT> slot = BytecodeOperandIdx(1);

  Label call_runtime(this, Label::kDeferred);
  GotoIf(IsUndefined(maybe_feedback_vector), &call_runtime);

  TNode<Object> cached_value =
      CAST(LoadFeedbackVectorSlot(CAST(maybe_feedback_vector), slot));

  GotoIf(TaggedEqual(cached_value, SmiConstant(0)), &call_runtime);

  SetAccumulator(cached_value);
  Dispatch();

  BIND(&call_runtime);
  {
    TNode<Object> description = LoadConstantPoolEntryAtOperandIndex(0);
    TNode<Smi> slot_smi = SmiTag(Signed(slot));
    TNode<JSFunction> closure =
        CAST(LoadRegister(Register::function_closure()));
    TNode<SharedFunctionInfo> shared_info = LoadObjectField<SharedFunctionInfo>(
        closure, JSFunction::kSharedFunctionInfoOffset);
    TNode<Context> context = GetContext();
    TNode<Object> result = CallRuntime(Runtime::kGetTemplateObject, context,
                                       description, shared_info, slot_smi);

    Label end(this);
    GotoIf(IsUndefined(maybe_feedback_vector), &end);
    StoreFeedbackVectorSlot(CAST(maybe_feedback_vector), slot, result);
    Goto(&end);

    Bind(&end);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateClosure <index> <slot> <flags>
//
// Creates a new closure for SharedFunctionInfo at position |index| in the
// constant pool and with pretenuring controlled by |flags|.
IGNITION_HANDLER(CreateClosure, InterpreterAssembler) {
  TNode<Object> shared = LoadConstantPoolEntryAtOperandIndex(0);
  TNode<Uint32T> flags = BytecodeOperandFlag(2);
  TNode<Context> context = GetContext();
  TNode<UintPtrT> slot = BytecodeOperandIdx(1);

  Label if_undefined(this);
  TNode<ClosureFeedbackCellArray> feedback_cell_array =
      LoadClosureFeedbackArray(
          CAST(LoadRegister(Register::function_closure())));
  TNode<FeedbackCell> feedback_cell =
      CAST(LoadFixedArrayElement(feedback_cell_array, slot));

  Label if_fast(this), if_slow(this, Label::kDeferred);
  Branch(IsSetWord32<CreateClosureFlags::FastNewClosureBit>(flags), &if_fast,
         &if_slow);

  BIND(&if_fast);
  {
    TNode<Object> result =
        CallBuiltin(Builtins::kFastNewClosure, context, shared, feedback_cell);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_slow);
  {
    Label if_newspace(this), if_oldspace(this);
    Branch(IsSetWord32<CreateClosureFlags::PretenuredBit>(flags), &if_oldspace,
           &if_newspace);

    BIND(&if_newspace);
    {
      TNode<Object> result =
          CallRuntime(Runtime::kNewClosure, context, shared, feedback_cell);
      SetAccumulator(result);
      Dispatch();
    }

    BIND(&if_oldspace);
    {
      TNode<Object> result = CallRuntime(Runtime::kNewClosure_Tenured, context,
                                         shared, feedback_cell);
      SetAccumulator(result);
      Dispatch();
    }
  }
}

// CreateBlockContext <index>
//
// Creates a new block context with the scope info constant at |index|.
IGNITION_HANDLER(CreateBlockContext, InterpreterAssembler) {
  TNode<ScopeInfo> scope_info = CAST(LoadConstantPoolEntryAtOperandIndex(0));
  TNode<Context> context = GetContext();
  SetAccumulator(CallRuntime(Runtime::kPushBlockContext, context, scope_info));
  Dispatch();
}

// CreateCatchContext <exception> <scope_info_idx>
//
// Creates a new context for a catch block with the |exception| in a register
// and the ScopeInfo at |scope_info_idx|.
IGNITION_HANDLER(CreateCatchContext, InterpreterAssembler) {
  TNode<Object> exception = LoadRegisterAtOperandIndex(0);
  TNode<ScopeInfo> scope_info = CAST(LoadConstantPoolEntryAtOperandIndex(1));
  TNode<Context> context = GetContext();
  SetAccumulator(
      CallRuntime(Runtime::kPushCatchContext, context, exception, scope_info));
  Dispatch();
}

// CreateFunctionContext <scope_info_idx> <slots>
//
// Creates a new context with number of |slots| for the function closure.
IGNITION_HANDLER(CreateFunctionContext, InterpreterAssembler) {
  TNode<UintPtrT> scope_info_idx = BytecodeOperandIdx(0);
  TNode<ScopeInfo> scope_info = CAST(LoadConstantPoolEntry(scope_info_idx));
  TNode<Uint32T> slots = BytecodeOperandUImm(1);
  TNode<Context> context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  SetAccumulator(constructor_assembler.EmitFastNewFunctionContext(
      scope_info, slots, context, FUNCTION_SCOPE));
  Dispatch();
}

// CreateEvalContext <scope_info_idx> <slots>
//
// Creates a new context with number of |slots| for an eval closure.
IGNITION_HANDLER(CreateEvalContext, InterpreterAssembler) {
  TNode<UintPtrT> scope_info_idx = BytecodeOperandIdx(0);
  TNode<ScopeInfo> scope_info = CAST(LoadConstantPoolEntry(scope_info_idx));
  TNode<Uint32T> slots = BytecodeOperandUImm(1);
  TNode<Context> context = GetContext();
  ConstructorBuiltinsAssembler constructor_assembler(state());
  SetAccumulator(constructor_assembler.EmitFastNewFunctionContext(
      scope_info, slots, context, EVAL_SCOPE));
  Dispatch();
}

// CreateWithContext <register> <scope_info_idx>
//
// Creates a new context with the ScopeInfo at |scope_info_idx| for a
// with-statement with the object in |register|.
IGNITION_HANDLER(CreateWithContext, InterpreterAssembler) {
  TNode<Object> object = LoadRegisterAtOperandIndex(0);
  TNode<ScopeInfo> scope_info = CAST(LoadConstantPoolEntryAtOperandIndex(1));
  TNode<Context> context = GetContext();
  SetAccumulator(
      CallRuntime(Runtime::kPushWithContext, context, object, scope_info));
  Dispatch();
}

// CreateMappedArguments
//
// Creates a new mapped arguments object.
IGNITION_HANDLER(CreateMappedArguments, InterpreterAssembler) {
  TNode<JSFunction> closure = CAST(LoadRegister(Register::function_closure()));
  TNode<Context> context = GetContext();

  Label if_duplicate_parameters(this, Label::kDeferred);
  Label if_not_duplicate_parameters(this);

  // Check if function has duplicate parameters.
  // TODO(rmcilroy): Remove this check when FastNewSloppyArgumentsStub supports
  // duplicate parameters.
  TNode<SharedFunctionInfo> shared_info = LoadObjectField<SharedFunctionInfo>(
      closure, JSFunction::kSharedFunctionInfoOffset);
  TNode<Uint32T> flags =
      LoadObjectField<Uint32T>(shared_info, SharedFunctionInfo::kFlagsOffset);
  TNode<BoolT> has_duplicate_parameters =
      IsSetWord32<SharedFunctionInfo::HasDuplicateParametersBit>(flags);
  Branch(has_duplicate_parameters, &if_duplicate_parameters,
         &if_not_duplicate_parameters);

  BIND(&if_not_duplicate_parameters);
  {
    ArgumentsBuiltinsAssembler constructor_assembler(state());
    TNode<JSObject> result =
        constructor_assembler.EmitFastNewSloppyArguments(context, closure);
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_duplicate_parameters);
  {
    TNode<Object> result =
        CallRuntime(Runtime::kNewSloppyArguments_Generic, context, closure);
    SetAccumulator(result);
    Dispatch();
  }
}

// CreateUnmappedArguments
//
// Creates a new unmapped arguments object.
IGNITION_HANDLER(CreateUnmappedArguments, InterpreterAssembler) {
  TNode<Context> context = GetContext();
  TNode<JSFunction> closure = CAST(LoadRegister(Register::function_closure()));
  ArgumentsBuiltinsAssembler builtins_assembler(state());
  TNode<JSObject> result =
      builtins_assembler.EmitFastNewStrictArguments(context, closure);
  SetAccumulator(result);
  Dispatch();
}

// CreateRestParameter
//
// Creates a new rest parameter array.
IGNITION_HANDLER(CreateRestParameter, InterpreterAssembler) {
  TNode<JSFunction> closure = CAST(LoadRegister(Register::function_closure()));
  TNode<Context> context = GetContext();
  ArgumentsBuiltinsAssembler builtins_assembler(state());
  TNode<JSObject> result =
      builtins_assembler.EmitFastNewRestParameter(context, closure);
  SetAccumulator(result);
  Dispatch();
}

// StackCheck
//
// Performs a stack guard check.
IGNITION_HANDLER(StackCheck, InterpreterAssembler) {
  TNode<Context> context = GetContext();
  PerformStackCheck(context);
  Dispatch();
}

// SetPendingMessage
//
// Sets the pending message to the value in the accumulator, and returns the
// previous pending message in the accumulator.
IGNITION_HANDLER(SetPendingMessage, InterpreterAssembler) {
  TNode<ExternalReference> pending_message = ExternalConstant(
      ExternalReference::address_of_pending_message_obj(isolate()));
  TNode<HeapObject> previous_message = Load<HeapObject>(pending_message);
  TNode<Object> new_message = GetAccumulator();
  StoreFullTaggedNoWriteBarrier(pending_message, new_message);
  SetAccumulator(previous_message);
  Dispatch();
}

// Throw
//
// Throws the exception in the accumulator.
IGNITION_HANDLER(Throw, InterpreterAssembler) {
  TNode<Object> exception = GetAccumulator();
  TNode<Context> context = GetContext();
  CallRuntime(Runtime::kThrow, context, exception);
  // We shouldn't ever return from a throw.
  Abort(AbortReason::kUnexpectedReturnFromThrow);
  Unreachable();
}

// ReThrow
//
// Re-throws the exception in the accumulator.
IGNITION_HANDLER(ReThrow, InterpreterAssembler) {
  TNode<Object> exception = GetAccumulator();
  TNode<Context> context = GetContext();
  CallRuntime(Runtime::kReThrow, context, exception);
  // We shouldn't ever return from a throw.
  Abort(AbortReason::kUnexpectedReturnFromThrow);
  Unreachable();
}

// Abort <abort_reason>
//
// Aborts execution (via a call to the runtime function).
IGNITION_HANDLER(Abort, InterpreterAssembler) {
  TNode<UintPtrT> reason = BytecodeOperandIdx(0);
  CallRuntime(Runtime::kAbort, NoContextConstant(), SmiTag(Signed(reason)));
  Unreachable();
}

// Return
//
// Return the value in the accumulator.
IGNITION_HANDLER(Return, InterpreterAssembler) {
  UpdateInterruptBudgetOnReturn();
  TNode<Object> accumulator = GetAccumulator();
  Return(accumulator);
}

// ThrowReferenceErrorIfHole <variable_name>
//
// Throws an exception if the value in the accumulator is TheHole.
IGNITION_HANDLER(ThrowReferenceErrorIfHole, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();

  Label throw_error(this, Label::kDeferred);
  GotoIf(TaggedEqual(value, TheHoleConstant()), &throw_error);
  Dispatch();

  BIND(&throw_error);
  {
    TNode<Name> name = CAST(LoadConstantPoolEntryAtOperandIndex(0));
    CallRuntime(Runtime::kThrowAccessedUninitializedVariable, GetContext(),
                name);
    // We shouldn't ever return from a throw.
    Abort(AbortReason::kUnexpectedReturnFromThrow);
    Unreachable();
  }
}

// ThrowSuperNotCalledIfHole
//
// Throws an exception if the value in the accumulator is TheHole.
IGNITION_HANDLER(ThrowSuperNotCalledIfHole, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();

  Label throw_error(this, Label::kDeferred);
  GotoIf(TaggedEqual(value, TheHoleConstant()), &throw_error);
  Dispatch();

  BIND(&throw_error);
  {
    CallRuntime(Runtime::kThrowSuperNotCalled, GetContext());
    // We shouldn't ever return from a throw.
    Abort(AbortReason::kUnexpectedReturnFromThrow);
    Unreachable();
  }
}

// ThrowSuperAlreadyCalledIfNotHole
//
// Throws SuperAleradyCalled exception if the value in the accumulator is not
// TheHole.
IGNITION_HANDLER(ThrowSuperAlreadyCalledIfNotHole, InterpreterAssembler) {
  TNode<Object> value = GetAccumulator();

  Label throw_error(this, Label::kDeferred);
  GotoIf(TaggedNotEqual(value, TheHoleConstant()), &throw_error);
  Dispatch();

  BIND(&throw_error);
  {
    CallRuntime(Runtime::kThrowSuperAlreadyCalledError, GetContext());
    // We shouldn't ever return from a throw.
    Abort(AbortReason::kUnexpectedReturnFromThrow);
    Unreachable();
  }
}

// Debugger
//
// Call runtime to handle debugger statement.
IGNITION_HANDLER(Debugger, InterpreterAssembler) {
  TNode<Context> context = GetContext();
  CallStub(CodeFactory::HandleDebuggerStatement(isolate()), context);
  Dispatch();
}

// DebugBreak
//
// Call runtime to handle a debug break.
#define DEBUG_BREAK(Name, ...)                                               \
  IGNITION_HANDLER(Name, InterpreterAssembler) {                             \
    TNode<Context> context = GetContext();                                   \
    TNode<Object> accumulator = GetAccumulator();                            \
    TNode<Object> result_pair =                                              \
        CallRuntime(Runtime::kDebugBreakOnBytecode, context, accumulator);   \
    TNode<Object> return_value = CAST(Projection(0, result_pair));           \
    TNode<IntPtrT> original_bytecode = SmiUntag(Projection(1, result_pair)); \
    MaybeDropFrames(context);                                                \
    SetAccumulator(return_value);                                            \
    DispatchToBytecode(original_bytecode, BytecodeOffset());                 \
  }
DEBUG_BREAK_BYTECODE_LIST(DEBUG_BREAK)
#undef DEBUG_BREAK

// IncBlockCounter <slot>
//
// Increment the execution count for the given slot. Used for block code
// coverage.
IGNITION_HANDLER(IncBlockCounter, InterpreterAssembler) {
  TNode<Object> closure = LoadRegister(Register::function_closure());
  TNode<Smi> coverage_array_slot = BytecodeOperandIdxSmi(0);
  TNode<Context> context = GetContext();

  CallBuiltin(Builtins::kIncBlockCounter, context, closure,
              coverage_array_slot);

  Dispatch();
}

// ForInEnumerate <receiver>
//
// Enumerates the enumerable keys of the |receiver| and either returns the
// map of the |receiver| if it has a usable enum cache or a fixed array
// with the keys to enumerate in the accumulator.
IGNITION_HANDLER(ForInEnumerate, InterpreterAssembler) {
  TNode<HeapObject> receiver = CAST(LoadRegisterAtOperandIndex(0));
  TNode<Context> context = GetContext();

  Label if_empty(this), if_runtime(this, Label::kDeferred);
  TNode<Map> receiver_map = CheckEnumCache(receiver, &if_empty, &if_runtime);
  SetAccumulator(receiver_map);
  Dispatch();

  BIND(&if_empty);
  {
    TNode<FixedArray> result = EmptyFixedArrayConstant();
    SetAccumulator(result);
    Dispatch();
  }

  BIND(&if_runtime);
  {
    TNode<Object> result =
        CallRuntime(Runtime::kForInEnumerate, context, receiver);
    SetAccumulator(result);
    Dispatch();
  }
}

// ForInPrepare <cache_info_triple>
//
// Returns state for for..in loop execution based on the enumerator in
// the accumulator register, which is the result of calling ForInEnumerate
// on a JSReceiver object.
// The result is output in registers |cache_info_triple| to
// |cache_info_triple + 2|, with the registers holding cache_type, cache_array,
// and cache_length respectively.
IGNITION_HANDLER(ForInPrepare, InterpreterAssembler) {
  // The {enumerator} is either a Map or a FixedArray.
  TNode<HeapObject> enumerator = CAST(GetAccumulator());
  TNode<UintPtrT> vector_index = BytecodeOperandIdx(1);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

  // Check if we're using an enum cache.
  Label if_fast(this), if_slow(this);
  Branch(IsMap(enumerator), &if_fast, &if_slow);

  BIND(&if_fast);
  {
    // Load the enumeration length and cache from the {enumerator}.
    TNode<Map> map_enumerator = CAST(enumerator);
    TNode<WordT> enum_length = LoadMapEnumLength(map_enumerator);
    CSA_ASSERT(this, WordNotEqual(enum_length,
                                  IntPtrConstant(kInvalidEnumCacheSentinel)));
    TNode<DescriptorArray> descriptors = LoadMapDescriptors(map_enumerator);
    TNode<EnumCache> enum_cache = LoadObjectField<EnumCache>(
        descriptors, DescriptorArray::kEnumCacheOffset);
    TNode<FixedArray> enum_keys =
        LoadObjectField<FixedArray>(enum_cache, EnumCache::kKeysOffset);

    // Check if we have enum indices available.
    TNode<FixedArray> enum_indices =
        LoadObjectField<FixedArray>(enum_cache, EnumCache::kIndicesOffset);
    TNode<IntPtrT> enum_indices_length =
        LoadAndUntagFixedArrayBaseLength(enum_indices);
    TNode<Smi> feedback = SelectSmiConstant(
        IntPtrLessThanOrEqual(enum_length, enum_indices_length),
        ForInFeedback::kEnumCacheKeysAndIndices, ForInFeedback::kEnumCacheKeys);
    UpdateFeedback(feedback, maybe_feedback_vector, vector_index);

    // Construct the cache info triple.
    TNode<Map> cache_type = map_enumerator;
    TNode<FixedArray> cache_array = enum_keys;
    TNode<Smi> cache_length = SmiTag(Signed(enum_length));
    StoreRegisterTripleAtOperandIndex(cache_type, cache_array, cache_length, 0);
    Dispatch();
  }

  BIND(&if_slow);
  {
    // The {enumerator} is a FixedArray with all the keys to iterate.
    TNode<FixedArray> array_enumerator = CAST(enumerator);

    // Record the fact that we hit the for-in slow-path.
    UpdateFeedback(SmiConstant(ForInFeedback::kAny), maybe_feedback_vector,
                   vector_index);

    // Construct the cache info triple.
    TNode<FixedArray> cache_type = array_enumerator;
    TNode<FixedArray> cache_array = array_enumerator;
    TNode<Smi> cache_length = LoadFixedArrayBaseLength(array_enumerator);
    StoreRegisterTripleAtOperandIndex(cache_type, cache_array, cache_length, 0);
    Dispatch();
  }
}

// ForInNext <receiver> <index> <cache_info_pair>
//
// Returns the next enumerable property in the the accumulator.
IGNITION_HANDLER(ForInNext, InterpreterAssembler) {
  TNode<HeapObject> receiver = CAST(LoadRegisterAtOperandIndex(0));
  TNode<Object> index = LoadRegisterAtOperandIndex(1);
  TNode<Object> cache_type;
  TNode<Object> cache_array;
  std::tie(cache_type, cache_array) = LoadRegisterPairAtOperandIndex(2);
  TNode<UintPtrT> vector_index = BytecodeOperandIdx(3);
  TNode<HeapObject> maybe_feedback_vector = LoadFeedbackVector();

  // Load the next key from the enumeration array.
  TNode<Object> key = LoadFixedArrayElement(CAST(cache_array), index, 0,
                                            CodeStubAssembler::SMI_PARAMETERS);

  // Check if we can use the for-in fast path potentially using the enum cache.
  Label if_fast(this), if_slow(this, Label::kDeferred);
  TNode<Map> receiver_map = LoadMap(receiver);
  Branch(TaggedEqual(receiver_map, cache_type), &if_fast, &if_slow);
  BIND(&if_fast);
  {
    // Enum cache in use for {receiver}, the {key} is definitely valid.
    SetAccumulator(key);
    Dispatch();
  }
  BIND(&if_slow);
  {
    // Record the fact that we hit the for-in slow-path.
    UpdateFeedback(SmiConstant(ForInFeedback::kAny), maybe_feedback_vector,
                   vector_index);

    // Need to filter the {key} for the {receiver}.
    TNode<Context> context = GetContext();
    TNode<Object> result =
        CallBuiltin(Builtins::kForInFilter, context, key, receiver);
    SetAccumulator(result);
    Dispatch();
  }
}

// ForInContinue <index> <cache_length>
//
// Returns false if the end of the enumerable properties has been reached.
IGNITION_HANDLER(ForInContinue, InterpreterAssembler) {
  TNode<Object> index = LoadRegisterAtOperandIndex(0);
  TNode<Object> cache_length = LoadRegisterAtOperandIndex(1);

  // Check if {index} is at {cache_length} already.
  Label if_true(this), if_false(this), end(this);
  Branch(TaggedEqual(index, cache_length), &if_true, &if_false);
  BIND(&if_true);
  {
    SetAccumulator(FalseConstant());
    Goto(&end);
  }
  BIND(&if_false);
  {
    SetAccumulator(TrueConstant());
    Goto(&end);
  }
  BIND(&end);
  Dispatch();
}

// ForInStep <index>
//
// Increments the loop counter in register |index| and stores the result
// in the accumulator.
IGNITION_HANDLER(ForInStep, InterpreterAssembler) {
  TNode<Smi> index = CAST(LoadRegisterAtOperandIndex(0));
  TNode<Smi> one = SmiConstant(1);
  TNode<Smi> result = SmiAdd(index, one);
  SetAccumulator(result);
  Dispatch();
}

// GetIterator <object>
//
// Retrieves the object[Symbol.iterator] method, calls it and stores
// the result in the accumulator. If the result is not a JSReceiver, throws
// SymbolIteratorInvalid runtime exception.
IGNITION_HANDLER(GetIterator, InterpreterAssembler) {
  TNode<Object> receiver = LoadRegisterAtOperandIndex(0);
  TNode<Context> context = GetContext();
  TNode<HeapObject> feedback_vector = LoadFeedbackVector();
  TNode<IntPtrT> load_feedback_slot = Signed(BytecodeOperandIdx(1));
  TNode<IntPtrT> call_feedback_slot = Signed(BytecodeOperandIdx(2));
  TNode<Smi> load_slot_smi = SmiTag(load_feedback_slot);
  TNode<Smi> call_slot_smi = SmiTag(call_feedback_slot);

  TNode<Object> iterator =
      CallBuiltin(Builtins::kGetIteratorWithFeedback, context, receiver,
                  load_slot_smi, call_slot_smi, feedback_vector);
  SetAccumulator(iterator);
  Dispatch();
}

// Wide
//
// Prefix bytecode indicating next bytecode has wide (16-bit) operands.
IGNITION_HANDLER(Wide, InterpreterAssembler) {
  DispatchWide(OperandScale::kDouble);
}

// ExtraWide
//
// Prefix bytecode indicating next bytecode has extra-wide (32-bit) operands.
IGNITION_HANDLER(ExtraWide, InterpreterAssembler) {
  DispatchWide(OperandScale::kQuadruple);
}

// Illegal
//
// An invalid bytecode aborting execution if dispatched.
IGNITION_HANDLER(Illegal, InterpreterAssembler) {
  Abort(AbortReason::kInvalidBytecode);
  Unreachable();
}

// SuspendGenerator <generator> <first input register> <register count>
// <suspend_id>
//
// Stores the parameters and the register file in the generator. Also stores
// the current context, |suspend_id|, and the current bytecode offset
// (for debugging purposes) into the generator. Then, returns the value
// in the accumulator.
IGNITION_HANDLER(SuspendGenerator, InterpreterAssembler) {
  TNode<JSGeneratorObject> generator = CAST(LoadRegisterAtOperandIndex(0));
  TNode<FixedArray> array = CAST(LoadObjectField(
      generator, JSGeneratorObject::kParametersAndRegistersOffset));
  TNode<JSFunction> closure = CAST(LoadRegister(Register::function_closure()));
  TNode<Context> context = GetContext();
  RegListNodePair registers = GetRegisterListAtOperandIndex(1);
  TNode<Smi> suspend_id = BytecodeOperandUImmSmi(3);

  TNode<SharedFunctionInfo> shared =
      CAST(LoadObjectField(closure, JSFunction::kSharedFunctionInfoOffset));
  TNode<Int32T> formal_parameter_count = UncheckedCast<Int32T>(
      LoadObjectField(shared, SharedFunctionInfo::kFormalParameterCountOffset,
                      MachineType::Uint16()));

  ExportParametersAndRegisterFile(array, registers, formal_parameter_count);
  StoreObjectField(generator, JSGeneratorObject::kContextOffset, context);
  StoreObjectField(generator, JSGeneratorObject::kContinuationOffset,
                   suspend_id);

  // Store the bytecode offset in the [input_or_debug_pos] field, to be used by
  // the inspector.
  TNode<Smi> offset = SmiTag(BytecodeOffset());
  StoreObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset,
                   offset);

  UpdateInterruptBudgetOnReturn();
  Return(GetAccumulator());
}

// SwitchOnGeneratorState <generator> <table_start> <table_length>
//
// If |generator| is undefined, falls through. Otherwise, loads the
// generator's state (overwriting it with kGeneratorExecuting), sets the context
// to the generator's resume context, and performs state dispatch on the
// generator's state by looking up the generator state in a jump table in the
// constant pool, starting at |table_start|, and of length |table_length|.
IGNITION_HANDLER(SwitchOnGeneratorState, InterpreterAssembler) {
  TNode<Object> maybe_generator = LoadRegisterAtOperandIndex(0);

  Label fallthrough(this);
  GotoIf(TaggedEqual(maybe_generator, UndefinedConstant()), &fallthrough);

  TNode<JSGeneratorObject> generator = CAST(maybe_generator);

  TNode<Smi> state =
      CAST(LoadObjectField(generator, JSGeneratorObject::kContinuationOffset));
  TNode<Smi> new_state = SmiConstant(JSGeneratorObject::kGeneratorExecuting);
  StoreObjectField(generator, JSGeneratorObject::kContinuationOffset,
                   new_state);

  TNode<Context> context =
      CAST(LoadObjectField(generator, JSGeneratorObject::kContextOffset));
  SetContext(context);

  TNode<UintPtrT> table_start = BytecodeOperandIdx(1);
  // TODO(leszeks): table_length is only used for a CSA_ASSERT, we don't
  // actually need it otherwise.
  TNode<UintPtrT> table_length = BytecodeOperandUImmWord(2);

  // The state must be a Smi.
  CSA_ASSERT(this, TaggedIsSmi(state));

  TNode<IntPtrT> case_value = SmiUntag(state);

  CSA_ASSERT(this, IntPtrGreaterThanOrEqual(case_value, IntPtrConstant(0)));
  CSA_ASSERT(this, IntPtrLessThan(case_value, table_length));
  USE(table_length);

  TNode<WordT> entry = IntPtrAdd(table_start, case_value);
  TNode<IntPtrT> relative_jump = LoadAndUntagConstantPoolEntry(entry);
  Jump(relative_jump);

  BIND(&fallthrough);
  Dispatch();
}

// ResumeGenerator <generator> <first output register> <register count>
//
// Imports the register file stored in the generator and marks the generator
// state as executing.
IGNITION_HANDLER(ResumeGenerator, InterpreterAssembler) {
  TNode<JSGeneratorObject> generator = CAST(LoadRegisterAtOperandIndex(0));
  TNode<JSFunction> closure = CAST(LoadRegister(Register::function_closure()));
  RegListNodePair registers = GetRegisterListAtOperandIndex(1);

  TNode<SharedFunctionInfo> shared =
      CAST(LoadObjectField(closure, JSFunction::kSharedFunctionInfoOffset));
  TNode<Int32T> formal_parameter_count = UncheckedCast<Int32T>(
      LoadObjectField(shared, SharedFunctionInfo::kFormalParameterCountOffset,
                      MachineType::Uint16()));

  ImportRegisterFile(
      CAST(LoadObjectField(generator,
                           JSGeneratorObject::kParametersAndRegistersOffset)),
      registers, formal_parameter_count);

  // Return the generator's input_or_debug_pos in the accumulator.
  SetAccumulator(
      LoadObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset));

  Dispatch();
}

#undef IGNITION_HANDLER

}  // namespace

Handle<Code> GenerateBytecodeHandler(Isolate* isolate, const char* debug_name,
                                     Bytecode bytecode,
                                     OperandScale operand_scale,
                                     int builtin_index,
                                     const AssemblerOptions& options) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  compiler::CodeAssemblerState state(
      isolate, &zone, InterpreterDispatchDescriptor{}, Code::BYTECODE_HANDLER,
      debug_name,
      FLAG_untrusted_code_mitigations
          ? PoisoningMitigationLevel::kPoisonCriticalOnly
          : PoisoningMitigationLevel::kDontPoison,
      builtin_index);

  switch (bytecode) {
#define CALL_GENERATOR(Name, ...)                     \
  case Bytecode::k##Name:                             \
    Name##Assembler::Generate(&state, operand_scale); \
    break;
    BYTECODE_LIST(CALL_GENERATOR);
#undef CALL_GENERATOR
  }

  Handle<Code> code = compiler::CodeAssembler::GenerateCode(&state, options);

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_trace_ignition_codegen) {
    StdoutStream os;
    code->Disassemble(Bytecodes::ToString(bytecode), os, isolate);
    os << std::flush;
  }
#endif  // ENABLE_DISASSEMBLER

  return code;
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
