// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DESCRIPTORS_H_
#define V8_BUILTINS_BUILTINS_DESCRIPTORS_H_

#include "src/builtins/builtins.h"
#include "src/codegen/interface-descriptors.h"
#include "src/compiler/code-assembler.h"
#include "src/objects/shared-function-info.h"

namespace v8 {
namespace internal {

// Define interface descriptors for builtins with JS linkage.
#define DEFINE_TFJ_INTERFACE_DESCRIPTOR(Name, Argc, ...)                \
  struct Builtin_##Name##_InterfaceDescriptor {                         \
    enum ParameterIndices {                                             \
      kJSTarget = compiler::CodeAssembler::kTargetParameterIndex,       \
      ##__VA_ARGS__,                                                    \
      kJSNewTarget,                                                     \
      kJSActualArgumentsCount,                                          \
      kContext,                                                         \
      kParameterCount,                                                  \
    };                                                                  \
    static_assert((Argc) == static_cast<uint16_t>(kParameterCount - 4), \
                  "Inconsistent set of arguments");                     \
    static_assert(kJSTarget == -1, "Unexpected kJSTarget index value"); \
  };

// Define interface descriptors for builtins with StubCall linkage.
#define DEFINE_TFC_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  using Builtin_##Name##_InterfaceDescriptor = InterfaceDescriptor##Descriptor;

#define DEFINE_TFS_INTERFACE_DESCRIPTOR(Name, ...) \
  using Builtin_##Name##_InterfaceDescriptor = Name##Descriptor;

// Define interface descriptors for IC handlers/dispatchers.
#define DEFINE_TFH_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  using Builtin_##Name##_InterfaceDescriptor = InterfaceDescriptor##Descriptor;

#define DEFINE_ASM_INTERFACE_DESCRIPTOR(Name, InterfaceDescriptor) \
  using Builtin_##Name##_InterfaceDescriptor = InterfaceDescriptor##Descriptor;

BUILTIN_LIST(IGNORE_BUILTIN, DEFINE_TFJ_INTERFACE_DESCRIPTOR,
             DEFINE_TFC_INTERFACE_DESCRIPTOR, DEFINE_TFS_INTERFACE_DESCRIPTOR,
             DEFINE_TFH_INTERFACE_DESCRIPTOR, IGNORE_BUILTIN,
             DEFINE_ASM_INTERFACE_DESCRIPTOR)

#undef DEFINE_TFJ_INTERFACE_DESCRIPTOR
#undef DEFINE_TFC_INTERFACE_DESCRIPTOR
#undef DEFINE_TFS_INTERFACE_DESCRIPTOR
#undef DEFINE_TFH_INTERFACE_DESCRIPTOR
#undef DEFINE_ASM_INTERFACE_DESCRIPTOR

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DESCRIPTORS_H_
