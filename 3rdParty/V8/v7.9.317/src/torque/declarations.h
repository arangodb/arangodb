// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_DECLARATIONS_H_
#define V8_TORQUE_DECLARATIONS_H_

#include <memory>
#include <string>

#include "src/torque/declarable.h"
#include "src/torque/utils.h"

namespace v8 {
namespace internal {
namespace torque {

static constexpr const char* const kFromConstexprMacroName = "FromConstexpr";
static constexpr const char* kMacroEndLabelName = "__macro_end";
static constexpr const char* kBreakLabelName = "__break";
static constexpr const char* kContinueLabelName = "__continue";
static constexpr const char* kCatchLabelName = "__catch";
static constexpr const char* kNextCaseLabelName = "__NextCase";

template <class T>
std::vector<T*> FilterDeclarables(const std::vector<Declarable*> list) {
  std::vector<T*> result;
  for (Declarable* declarable : list) {
    if (T* t = T::DynamicCast(declarable)) {
      result.push_back(t);
    }
  }
  return result;
}

class Declarations {
 public:
  static std::vector<Declarable*> TryLookup(const QualifiedName& name) {
    return CurrentScope::Get()->Lookup(name);
  }

  static std::vector<Declarable*> TryLookupShallow(const QualifiedName& name) {
    return CurrentScope::Get()->LookupShallow(name);
  }

  template <class T>
  static std::vector<T*> TryLookup(const QualifiedName& name) {
    return FilterDeclarables<T>(TryLookup(name));
  }

  static std::vector<Declarable*> Lookup(const QualifiedName& name) {
    std::vector<Declarable*> d = TryLookup(name);
    if (d.empty()) {
      ReportError("cannot find \"", name, "\"");
    }
    return d;
  }

  static std::vector<Declarable*> LookupGlobalScope(const std::string& name);

  static const TypeAlias* LookupTypeAlias(const QualifiedName& name);
  static const Type* LookupType(const QualifiedName& name);
  static const Type* LookupType(const Identifier* identifier);
  static const Type* LookupGlobalType(const std::string& name);

  static Builtin* FindSomeInternalBuiltinWithType(
      const BuiltinPointerType* type);

  static Value* LookupValue(const QualifiedName& name);

  static Macro* TryLookupMacro(const std::string& name,
                               const TypeVector& types);
  static base::Optional<Builtin*> TryLookupBuiltin(const QualifiedName& name);

  static std::vector<Generic*> LookupGeneric(const std::string& name);
  static Generic* LookupUniqueGeneric(const QualifiedName& name);

  static GenericStructType* LookupUniqueGenericStructType(
      const QualifiedName& name);
  static base::Optional<GenericStructType*> TryLookupGenericStructType(
      const QualifiedName& name);

  static Namespace* DeclareNamespace(const std::string& name);
  static TypeAlias* DeclareType(const Identifier* name, const Type* type);

  static const TypeAlias* PredeclareTypeAlias(const Identifier* name,
                                              TypeDeclaration* type,
                                              bool redeclaration);
  static TorqueMacro* CreateTorqueMacro(std::string external_name,
                                        std::string readable_name,
                                        bool exported_to_csa,
                                        Signature signature,
                                        base::Optional<Statement*> body,
                                        bool is_user_defined);
  static ExternMacro* CreateExternMacro(std::string name,
                                        std::string external_assembler_name,
                                        Signature signature);
  static Macro* DeclareMacro(
      const std::string& name, bool accessible_from_csa,
      base::Optional<std::string> external_assembler_name,
      const Signature& signature, base::Optional<Statement*> body,
      base::Optional<std::string> op = {}, bool is_user_defined = true);

  static Method* CreateMethod(AggregateType* class_type,
                              const std::string& name, Signature signature,
                              Statement* body);

  static Intrinsic* CreateIntrinsic(const std::string& name,
                                    const Signature& signature);

  static Intrinsic* DeclareIntrinsic(const std::string& name,
                                     const Signature& signature);

  static Builtin* CreateBuiltin(std::string external_name,
                                std::string readable_name, Builtin::Kind kind,
                                Signature signature,
                                base::Optional<Statement*> body);
  static Builtin* DeclareBuiltin(const std::string& name, Builtin::Kind kind,
                                 const Signature& signature,
                                 base::Optional<Statement*> body);

  static RuntimeFunction* DeclareRuntimeFunction(const std::string& name,
                                                 const Signature& signature);

  static void DeclareExternConstant(Identifier* name, const Type* type,
                                    std::string value);
  static NamespaceConstant* DeclareNamespaceConstant(Identifier* name,
                                                     const Type* type,
                                                     Expression* body);

  static Generic* DeclareGeneric(const std::string& name,
                                 GenericDeclaration* generic);
  static GenericStructType* DeclareGenericStructType(const std::string& name,
                                                     StructDeclaration* decl);

  template <class T>
  static T* Declare(const std::string& name, T* d) {
    CurrentScope::Get()->AddDeclarable(name, d);
    return d;
  }
  template <class T>
  static T* Declare(const std::string& name, std::unique_ptr<T> d) {
    return CurrentScope::Get()->AddDeclarable(name,
                                              RegisterDeclarable(std::move(d)));
  }
  static Macro* DeclareOperator(const std::string& name, Macro* m);

  static std::string GetGeneratedCallableName(
      const std::string& name, const TypeVector& specialized_types);
};

}  // namespace torque
}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_DECLARATIONS_H_
