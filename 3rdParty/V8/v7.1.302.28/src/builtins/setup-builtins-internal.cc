// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/setup-isolate.h"

#include "src/assembler-inl.h"
#include "src/builtins/builtins.h"
#include "src/code-events.h"
#include "src/compiler/code-assembler.h"
#include "src/handles-inl.h"
#include "src/interface-descriptors.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-generator.h"
#include "src/interpreter/interpreter.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

// Forward declarations for C++ builtins.
#define FORWARD_DECLARE(Name) \
  Object* Builtin_##Name(int argc, Object** args, Isolate* isolate);
BUILTIN_LIST_C(FORWARD_DECLARE)
#undef FORWARD_DECLARE

namespace {

void PostBuildProfileAndTracing(Isolate* isolate, Code* code,
                                const char* name) {
  PROFILE(isolate, CodeCreateEvent(CodeEventListener::BUILTIN_TAG,
                                   AbstractCode::cast(code), name));
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_builtin_code) {
    code->PrintBuiltinCode(isolate, name);
  }
#endif
}

AssemblerOptions BuiltinAssemblerOptions(Isolate* isolate,
                                         int32_t builtin_index) {
  AssemblerOptions options = AssemblerOptions::Default(isolate);
  CHECK(!options.isolate_independent_code);
  CHECK(!options.use_pc_relative_calls_and_jumps);

  if (!isolate->ShouldLoadConstantsFromRootList() ||
      !Builtins::IsIsolateIndependent(builtin_index)) {
    return options;
  }

  const base::AddressRegion& code_range =
      isolate->heap()->memory_allocator()->code_range();
  bool pc_relative_calls_fit_in_code_range =
      !code_range.is_empty() &&
      code_range.size() <= kMaxPCRelativeCodeRangeInMB * MB;

  options.isolate_independent_code = true;
  options.use_pc_relative_calls_and_jumps = pc_relative_calls_fit_in_code_range;

  return options;
}

typedef void (*MacroAssemblerGenerator)(MacroAssembler*);
typedef void (*CodeAssemblerGenerator)(compiler::CodeAssemblerState*);

Handle<Code> BuildPlaceholder(Isolate* isolate, int32_t builtin_index) {
  HandleScope scope(isolate);
  const size_t buffer_size = 1 * KB;
  byte buffer[buffer_size];  // NOLINT(runtime/arrays)
  MacroAssembler masm(isolate, buffer, buffer_size, CodeObjectRequired::kYes);
  DCHECK(!masm.has_frame());
  {
    FrameScope scope(&masm, StackFrame::NONE);
    // The contents of placeholder don't matter, as long as they don't create
    // embedded constants or external references.
    masm.Move(kJavaScriptCallCodeStartRegister, Smi::kZero);
    masm.Call(kJavaScriptCallCodeStartRegister);
  }
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::BUILTIN, masm.CodeObject(), builtin_index);
  return scope.CloseAndEscape(code);
}

Code* BuildWithMacroAssembler(Isolate* isolate, int32_t builtin_index,
                              MacroAssemblerGenerator generator,
                              const char* s_name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  const size_t buffer_size = 32 * KB;
  byte buffer[buffer_size];  // NOLINT(runtime/arrays)

  MacroAssembler masm(isolate, BuiltinAssemblerOptions(isolate, builtin_index),
                      buffer, buffer_size, CodeObjectRequired::kYes);
  masm.set_builtin_index(builtin_index);
  DCHECK(!masm.has_frame());
  generator(&masm);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::BUILTIN, masm.CodeObject(), builtin_index);
  PostBuildProfileAndTracing(isolate, *code, s_name);
  return *code;
}

Code* BuildAdaptor(Isolate* isolate, int32_t builtin_index,
                   Address builtin_address,
                   Builtins::ExitFrameType exit_frame_type, const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  const size_t buffer_size = 32 * KB;
  byte buffer[buffer_size];  // NOLINT(runtime/arrays)
  MacroAssembler masm(isolate, BuiltinAssemblerOptions(isolate, builtin_index),
                      buffer, buffer_size, CodeObjectRequired::kYes);
  masm.set_builtin_index(builtin_index);
  DCHECK(!masm.has_frame());
  Builtins::Generate_Adaptor(&masm, builtin_address, exit_frame_type);
  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  Handle<Code> code = isolate->factory()->NewCode(
      desc, Code::BUILTIN, masm.CodeObject(), builtin_index);
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}

// Builder for builtins implemented in TurboFan with JS linkage.
Code* BuildWithCodeStubAssemblerJS(Isolate* isolate, int32_t builtin_index,
                                   CodeAssemblerGenerator generator, int argc,
                                   const char* name) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);

  SegmentSize segment_size = isolate->serializer_enabled()
                                 ? SegmentSize::kLarge
                                 : SegmentSize::kDefault;
  Zone zone(isolate->allocator(), ZONE_NAME, segment_size);
  const int argc_with_recv =
      (argc == SharedFunctionInfo::kDontAdaptArgumentsSentinel) ? 0 : argc + 1;
  compiler::CodeAssemblerState state(
      isolate, &zone, argc_with_recv, Code::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, builtin_index);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin_index));
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}

// Builder for builtins implemented in TurboFan with CallStub linkage.
Code* BuildWithCodeStubAssemblerCS(Isolate* isolate, int32_t builtin_index,
                                   CodeAssemblerGenerator generator,
                                   CallDescriptors::Key interface_descriptor,
                                   const char* name, int result_size) {
  HandleScope scope(isolate);
  // Canonicalize handles, so that we can share constant pool entries pointing
  // to code targets without dereferencing their handles.
  CanonicalHandleScope canonical(isolate);
  SegmentSize segment_size = isolate->serializer_enabled()
                                 ? SegmentSize::kLarge
                                 : SegmentSize::kDefault;
  Zone zone(isolate->allocator(), ZONE_NAME, segment_size);
  // The interface descriptor with given key must be initialized at this point
  // and this construction just queries the details from the descriptors table.
  CallInterfaceDescriptor descriptor(interface_descriptor);
  // Ensure descriptor is already initialized.
  DCHECK_EQ(result_size, descriptor.GetReturnCount());
  DCHECK_LE(0, descriptor.GetRegisterParameterCount());
  compiler::CodeAssemblerState state(
      isolate, &zone, descriptor, Code::BUILTIN, name,
      PoisoningMitigationLevel::kDontPoison, 0, builtin_index);
  generator(&state);
  Handle<Code> code = compiler::CodeAssembler::GenerateCode(
      &state, BuiltinAssemblerOptions(isolate, builtin_index));
  PostBuildProfileAndTracing(isolate, *code, name);
  return *code;
}

}  // anonymous namespace

// static
void SetupIsolateDelegate::AddBuiltin(Builtins* builtins, int index,
                                      Code* code) {
  DCHECK_EQ(index, code->builtin_index());
  builtins->set_builtin(index, code);
}

// static
void SetupIsolateDelegate::PopulateWithPlaceholders(Isolate* isolate) {
  // Fill the builtins list with placeholders. References to these placeholder
  // builtins are eventually replaced by the actual builtins. This is to
  // support circular references between builtins.
  Builtins* builtins = isolate->builtins();
  HandleScope scope(isolate);
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Handle<Code> placeholder = BuildPlaceholder(isolate, i);
    AddBuiltin(builtins, i, *placeholder);
  }
}

// static
void SetupIsolateDelegate::ReplacePlaceholders(Isolate* isolate) {
  // Replace references from all code objects to placeholders.
  Builtins* builtins = isolate->builtins();
  DisallowHeapAllocation no_gc;
  CodeSpaceMemoryModificationScope modification_scope(isolate->heap());
  static const int kRelocMask =
      RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
      RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
      RelocInfo::ModeMask(RelocInfo::RELATIVE_CODE_TARGET);
  HeapIterator iterator(isolate->heap());
  while (HeapObject* obj = iterator.next()) {
    if (!obj->IsCode()) continue;
    Code* code = Code::cast(obj);
    bool flush_icache = false;
    for (RelocIterator it(code, kRelocMask); !it.done(); it.next()) {
      RelocInfo* rinfo = it.rinfo();
      if (RelocInfo::IsCodeTargetMode(rinfo->rmode())) {
        Code* target = Code::GetCodeFromTargetAddress(rinfo->target_address());
        DCHECK_IMPLIES(RelocInfo::IsRelativeCodeTarget(rinfo->rmode()),
                       Builtins::IsIsolateIndependent(target->builtin_index()));
        if (!target->is_builtin()) continue;
        Code* new_target = builtins->builtin(target->builtin_index());
        rinfo->set_target_address(new_target->raw_instruction_start(),
                                  UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
      } else {
        DCHECK(RelocInfo::IsEmbeddedObject(rinfo->rmode()));
        Object* object = rinfo->target_object();
        if (!object->IsCode()) continue;
        Code* target = Code::cast(object);
        if (!target->is_builtin()) continue;
        Code* new_target = builtins->builtin(target->builtin_index());
        rinfo->set_target_object(isolate->heap(), new_target,
                                 UPDATE_WRITE_BARRIER, SKIP_ICACHE_FLUSH);
      }
      flush_icache = true;
    }
    if (flush_icache) {
      Assembler::FlushICache(code->raw_instruction_start(),
                             code->raw_instruction_size());
    }
  }
}

namespace {

Code* GenerateBytecodeHandler(Isolate* isolate, int builtin_index,
                              const char* name,
                              interpreter::OperandScale operand_scale,
                              interpreter::Bytecode bytecode) {
  DCHECK(interpreter::Bytecodes::BytecodeHasHandler(bytecode, operand_scale));

  Handle<Code> code = interpreter::GenerateBytecodeHandler(
      isolate, bytecode, operand_scale, builtin_index,
      BuiltinAssemblerOptions(isolate, builtin_index));

  PostBuildProfileAndTracing(isolate, *code, name);

  return *code;
}

Code* GenerateLazyBytecodeHandler(Isolate* isolate, int builtin_index,
                                  const char* name,
                                  interpreter::OperandScale operand_scale) {
  Handle<Code> code = interpreter::GenerateDeserializeLazyHandler(
      isolate, operand_scale, builtin_index,
      BuiltinAssemblerOptions(isolate, builtin_index));

  PostBuildProfileAndTracing(isolate, *code, name);

  return *code;
}

}  // namespace

// static
void SetupIsolateDelegate::SetupBuiltinsInternal(Isolate* isolate) {
  Builtins* builtins = isolate->builtins();
  DCHECK(!builtins->initialized_);

  PopulateWithPlaceholders(isolate);

  // Create a scope for the handles in the builtins.
  HandleScope scope(isolate);

  int index = 0;
  Code* code;
#define BUILD_CPP(Name)                                              \
  code = BuildAdaptor(isolate, index, FUNCTION_ADDR(Builtin_##Name), \
                      Builtins::BUILTIN_EXIT, #Name);                \
  AddBuiltin(builtins, index++, code);
#define BUILD_API(Name)                                              \
  code = BuildAdaptor(isolate, index, FUNCTION_ADDR(Builtin_##Name), \
                      Builtins::EXIT, #Name);                        \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFJ(Name, Argc, ...)                              \
  code = BuildWithCodeStubAssemblerJS(                          \
      isolate, index, &Builtins::Generate_##Name, Argc, #Name); \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFC(Name, InterfaceDescriptor, result_size)        \
  code = BuildWithCodeStubAssemblerCS(                           \
      isolate, index, &Builtins::Generate_##Name,                \
      CallDescriptors::InterfaceDescriptor, #Name, result_size); \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFS(Name, ...)                                                   \
  /* Return size for generic TF builtins (stub linkage) is always 1. */        \
  code =                                                                       \
      BuildWithCodeStubAssemblerCS(isolate, index, &Builtins::Generate_##Name, \
                                   CallDescriptors::Name, #Name, 1);           \
  AddBuiltin(builtins, index++, code);
#define BUILD_TFH(Name, InterfaceDescriptor)               \
  /* Return size for IC builtins/handlers is always 1. */  \
  code = BuildWithCodeStubAssemblerCS(                     \
      isolate, index, &Builtins::Generate_##Name,          \
      CallDescriptors::InterfaceDescriptor, #Name, 1);     \
  AddBuiltin(builtins, index++, code);

#define BUILD_BCH(Name, OperandScale, Bytecode)                         \
  code = GenerateBytecodeHandler(isolate, index, Builtins::name(index), \
                                 OperandScale, Bytecode);               \
  AddBuiltin(builtins, index++, code);

#define BUILD_DLH(Name, OperandScale)                                       \
  code = GenerateLazyBytecodeHandler(isolate, index, Builtins::name(index), \
                                     OperandScale);                         \
  AddBuiltin(builtins, index++, code);

#define BUILD_ASM(Name)                                                     \
  code = BuildWithMacroAssembler(isolate, index, Builtins::Generate_##Name, \
                                 #Name);                                    \
  AddBuiltin(builtins, index++, code);

  BUILTIN_LIST(BUILD_CPP, BUILD_API, BUILD_TFJ, BUILD_TFC, BUILD_TFS, BUILD_TFH,
               BUILD_BCH, BUILD_DLH, BUILD_ASM);

#undef BUILD_CPP
#undef BUILD_API
#undef BUILD_TFJ
#undef BUILD_TFC
#undef BUILD_TFS
#undef BUILD_TFH
#undef BUILD_BCH
#undef BUILD_DLH
#undef BUILD_ASM
  CHECK_EQ(Builtins::builtin_count, index);

  ReplacePlaceholders(isolate);

#define SET_PROMISE_REJECTION_PREDICTION(Name) \
  builtins->builtin(Builtins::k##Name)->set_is_promise_rejection(true);

  BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(SET_PROMISE_REJECTION_PREDICTION)
#undef SET_PROMISE_REJECTION_PREDICTION

#define SET_EXCEPTION_CAUGHT_PREDICTION(Name) \
  builtins->builtin(Builtins::k##Name)->set_is_exception_caught(true);

  BUILTIN_EXCEPTION_CAUGHT_PREDICTION_LIST(SET_EXCEPTION_CAUGHT_PREDICTION)
#undef SET_EXCEPTION_CAUGHT_PREDICTION

  builtins->MarkInitialized();
}

}  // namespace internal
}  // namespace v8
