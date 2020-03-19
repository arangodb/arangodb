// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_CONSTANTS_H_
#define V8_TORQUE_CONSTANTS_H_

#include <cstring>
#include <string>

#include "src/base/flags.h"

namespace v8 {
namespace internal {
namespace torque {

static const char* const CONSTEXPR_TYPE_PREFIX = "constexpr ";
static const char* const NEVER_TYPE_STRING = "never";
static const char* const CONSTEXPR_BOOL_TYPE_STRING = "constexpr bool";
static const char* const CONSTEXPR_INTPTR_TYPE_STRING = "constexpr intptr";
static const char* const CONSTEXPR_INSTANCE_TYPE_TYPE_STRING =
    "constexpr InstanceType";
static const char* const BOOL_TYPE_STRING = "bool";
static const char* const VOID_TYPE_STRING = "void";
static const char* const ARGUMENTS_TYPE_STRING = "Arguments";
static const char* const CONTEXT_TYPE_STRING = "Context";
static const char* const JS_FUNCTION_TYPE_STRING = "JSFunction";
static const char* const MAP_TYPE_STRING = "Map";
static const char* const OBJECT_TYPE_STRING = "Object";
static const char* const HEAP_OBJECT_TYPE_STRING = "HeapObject";
static const char* const JSANY_TYPE_STRING = "JSAny";
static const char* const JSOBJECT_TYPE_STRING = "JSObject";
static const char* const SMI_TYPE_STRING = "Smi";
static const char* const TAGGED_TYPE_STRING = "Tagged";
static const char* const UNINITIALIZED_TYPE_STRING = "Uninitialized";
static const char* const RAWPTR_TYPE_STRING = "RawPtr";
static const char* const CONST_STRING_TYPE_STRING = "constexpr string";
static const char* const STRING_TYPE_STRING = "String";
static const char* const NUMBER_TYPE_STRING = "Number";
static const char* const BUILTIN_POINTER_TYPE_STRING = "BuiltinPtr";
static const char* const INTPTR_TYPE_STRING = "intptr";
static const char* const UINTPTR_TYPE_STRING = "uintptr";
static const char* const INT32_TYPE_STRING = "int32";
static const char* const UINT32_TYPE_STRING = "uint32";
static const char* const INT16_TYPE_STRING = "int16";
static const char* const UINT16_TYPE_STRING = "uint16";
static const char* const INT8_TYPE_STRING = "int8";
static const char* const UINT8_TYPE_STRING = "uint8";
static const char* const FLOAT64_TYPE_STRING = "float64";
static const char* const CONST_INT31_TYPE_STRING = "constexpr int31";
static const char* const CONST_INT32_TYPE_STRING = "constexpr int32";
static const char* const CONST_FLOAT64_TYPE_STRING = "constexpr float64";
static const char* const TORQUE_INTERNAL_NAMESPACE_STRING = "torque_internal";
static const char* const REFERENCE_TYPE_STRING = "Reference";
static const char* const SLICE_TYPE_STRING = "Slice";
static const char* const STRUCT_NAMESPACE_STRING = "_struct";

static const char* const ANNOTATION_GENERATE_PRINT = "@generatePrint";
static const char* const ANNOTATION_NO_VERIFIER = "@noVerifier";
static const char* const ANNOTATION_ABSTRACT = "@abstract";
static const char* const ANNOTATION_INSTANTIATED_ABSTRACT_CLASS =
    "@dirtyInstantiatedAbstractClass";
static const char* const ANNOTATION_HAS_SAME_INSTANCE_TYPE_AS_PARENT =
    "@hasSameInstanceTypeAsParent";
static const char* const ANNOTATION_GENERATE_CPP_CLASS = "@generateCppClass";
static const char* const ANNOTATION_HIGHEST_INSTANCE_TYPE_WITHIN_PARENT =
    "@highestInstanceTypeWithinParentClassRange";
static const char* const ANNOTATION_LOWEST_INSTANCE_TYPE_WITHIN_PARENT =
    "@lowestInstanceTypeWithinParentClassRange";
static const char* const ANNOTATION_RESERVE_BITS_IN_INSTANCE_TYPE =
    "@reserveBitsInInstanceType";
static const char* const ANNOTATION_INSTANCE_TYPE_VALUE =
    "@apiExposedInstanceTypeValue";
static const char* const ANNOTATION_IF = "@if";
static const char* const ANNOTATION_IFNOT = "@ifnot";

inline bool IsConstexprName(const std::string& name) {
  return name.substr(0, std::strlen(CONSTEXPR_TYPE_PREFIX)) ==
         CONSTEXPR_TYPE_PREFIX;
}

inline std::string GetNonConstexprName(const std::string& name) {
  if (!IsConstexprName(name)) return name;
  return name.substr(std::strlen(CONSTEXPR_TYPE_PREFIX));
}

inline std::string GetConstexprName(const std::string& name) {
  if (IsConstexprName(name)) return name;
  return CONSTEXPR_TYPE_PREFIX + name;
}

enum class ClassFlag {
  kNone = 0,
  kExtern = 1 << 0,
  kGeneratePrint = 1 << 1,
  kGenerateVerify = 1 << 2,
  kTransient = 1 << 3,
  kAbstract = 1 << 4,
  kInstantiatedAbstractClass = 1 << 5,
  kHasSameInstanceTypeAsParent = 1 << 6,
  kGenerateCppClassDefinitions = 1 << 7,
  kHasIndexedField = 1 << 8,
  kHighestInstanceTypeWithinParent = 1 << 9,
  kLowestInstanceTypeWithinParent = 1 << 10,
  kUndefinedLayout = 1 << 11,
};
using ClassFlags = base::Flags<ClassFlag>;

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_CONSTANTS_H_
