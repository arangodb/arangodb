// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_VARIABLES_H_
#define V8_AST_VARIABLES_H_

#include "src/ast/ast-value-factory.h"
#include "src/globals.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// The AST refers to variables via VariableProxies - placeholders for the actual
// variables. Variables themselves are never directly referred to from the AST,
// they are maintained by scopes, and referred to from VariableProxies and Slots
// after binding and variable allocation.
class Variable final : public ZoneObject {
 public:
  Variable(Scope* scope, const AstRawString* name, VariableMode mode,
           VariableKind kind, InitializationFlag initialization_flag,
           MaybeAssignedFlag maybe_assigned_flag = kNotAssigned)
      : scope_(scope),
        name_(name),
        local_if_not_shadowed_(nullptr),
        next_(nullptr),
        index_(-1),
        initializer_position_(kNoSourcePosition),
        bit_field_(MaybeAssignedFlagField::encode(maybe_assigned_flag) |
                   InitializationFlagField::encode(initialization_flag) |
                   VariableModeField::encode(mode) |
                   IsUsedField::encode(false) |
                   ForceContextAllocationField::encode(false) |
                   ForceHoleInitializationField::encode(false) |
                   LocationField::encode(VariableLocation::UNALLOCATED) |
                   VariableKindField::encode(kind)) {
    // Var declared variables never need initialization.
    DCHECK(!(mode == VariableMode::kVar &&
             initialization_flag == kNeedsInitialization));
  }

  explicit Variable(Variable* other);

  // The source code for an eval() call may refer to a variable that is
  // in an outer scope about which we don't know anything (it may not
  // be the script scope). scope() is nullptr in that case. Currently the
  // scope is only used to follow the context chain length.
  Scope* scope() const { return scope_; }

  // This is for adjusting the scope of temporaries used when desugaring
  // parameter initializers.
  void set_scope(Scope* scope) { scope_ = scope; }

  Handle<String> name() const { return name_->string(); }
  const AstRawString* raw_name() const { return name_; }
  VariableMode mode() const { return VariableModeField::decode(bit_field_); }
  bool has_forced_context_allocation() const {
    return ForceContextAllocationField::decode(bit_field_);
  }
  void ForceContextAllocation() {
    DCHECK(IsUnallocated() || IsContextSlot() ||
           location() == VariableLocation::MODULE);
    bit_field_ = ForceContextAllocationField::update(bit_field_, true);
  }
  bool is_used() { return IsUsedField::decode(bit_field_); }
  void set_is_used() { bit_field_ = IsUsedField::update(bit_field_, true); }
  MaybeAssignedFlag maybe_assigned() const {
    return MaybeAssignedFlagField::decode(bit_field_);
  }
  void set_maybe_assigned() {
    bit_field_ = MaybeAssignedFlagField::update(bit_field_, kMaybeAssigned);
  }

  int initializer_position() { return initializer_position_; }
  void set_initializer_position(int pos) { initializer_position_ = pos; }

  bool IsUnallocated() const {
    return location() == VariableLocation::UNALLOCATED;
  }
  bool IsParameter() const { return location() == VariableLocation::PARAMETER; }
  bool IsStackLocal() const { return location() == VariableLocation::LOCAL; }
  bool IsStackAllocated() const { return IsParameter() || IsStackLocal(); }
  bool IsContextSlot() const { return location() == VariableLocation::CONTEXT; }
  bool IsLookupSlot() const { return location() == VariableLocation::LOOKUP; }
  bool IsGlobalObjectProperty() const;

  bool is_dynamic() const { return IsDynamicVariableMode(mode()); }

  // Returns the InitializationFlag this Variable was created with.
  // Scope analysis may allow us to relax this initialization
  // requirement, which will be reflected in the return value of
  // binding_needs_init().
  InitializationFlag initialization_flag() const {
    return InitializationFlagField::decode(bit_field_);
  }

  // Whether this variable needs to be initialized with the hole at
  // declaration time. Only returns valid results after scope analysis.
  bool binding_needs_init() const {
    DCHECK_IMPLIES(initialization_flag() == kNeedsInitialization,
                   IsLexicalVariableMode(mode()));
    DCHECK_IMPLIES(ForceHoleInitializationField::decode(bit_field_),
                   initialization_flag() == kNeedsInitialization);

    // Always initialize if hole initialization was forced during
    // scope analysis.
    if (ForceHoleInitializationField::decode(bit_field_)) return true;

    // If initialization was not forced, no need for initialization
    // for stack allocated variables, since UpdateNeedsHoleCheck()
    // in scopes.cc has proven that no VariableProxy refers to
    // this variable in such a way that a runtime hole check
    // would be generated.
    if (IsStackAllocated()) return false;

    // Otherwise, defer to the flag set when this Variable was constructed.
    return initialization_flag() == kNeedsInitialization;
  }

  // Called during scope analysis when a VariableProxy is found to
  // reference this Variable in such a way that a hole check will
  // be required at runtime.
  void ForceHoleInitialization() {
    DCHECK_EQ(kNeedsInitialization, initialization_flag());
    DCHECK(IsLexicalVariableMode(mode()));
    bit_field_ = ForceHoleInitializationField::update(bit_field_, true);
  }

  bool throw_on_const_assignment(LanguageMode language_mode) const {
    return kind() != SLOPPY_FUNCTION_NAME_VARIABLE || is_strict(language_mode);
  }

  bool is_function() const { return kind() == FUNCTION_VARIABLE; }
  bool is_this() const { return kind() == THIS_VARIABLE; }
  bool is_sloppy_function_name() const {
    return kind() == SLOPPY_FUNCTION_NAME_VARIABLE;
  }

  Variable* local_if_not_shadowed() const {
    DCHECK(mode() == VariableMode::kDynamicLocal &&
           local_if_not_shadowed_ != nullptr);
    return local_if_not_shadowed_;
  }

  void set_local_if_not_shadowed(Variable* local) {
    local_if_not_shadowed_ = local;
  }

  VariableLocation location() const {
    return LocationField::decode(bit_field_);
  }
  VariableKind kind() const { return VariableKindField::decode(bit_field_); }

  int index() const { return index_; }

  bool IsReceiver() const {
    DCHECK(IsParameter());

    return index_ == -1;
  }

  bool IsExport() const {
    DCHECK_EQ(location(), VariableLocation::MODULE);
    DCHECK_NE(index(), 0);
    return index() > 0;
  }

  void AllocateTo(VariableLocation location, int index) {
    DCHECK(IsUnallocated() ||
           (this->location() == location && this->index() == index));
    DCHECK_IMPLIES(location == VariableLocation::MODULE, index != 0);
    bit_field_ = LocationField::update(bit_field_, location);
    DCHECK_EQ(location, this->location());
    index_ = index;
  }

  static InitializationFlag DefaultInitializationFlag(VariableMode mode) {
    DCHECK(IsDeclaredVariableMode(mode));
    return mode == VariableMode::kVar ? kCreatedInitialized
                                      : kNeedsInitialization;
  }

  typedef base::ThreadedList<Variable> List;

 private:
  Scope* scope_;
  const AstRawString* name_;

  // If this field is set, this variable references the stored locally bound
  // variable, but it might be shadowed by variable bindings introduced by
  // sloppy 'eval' calls between the reference scope (inclusive) and the
  // binding scope (exclusive).
  Variable* local_if_not_shadowed_;
  Variable* next_;
  int index_;
  int initializer_position_;
  uint16_t bit_field_;

  class VariableModeField : public BitField16<VariableMode, 0, 3> {};
  class VariableKindField
      : public BitField16<VariableKind, VariableModeField::kNext, 3> {};
  class LocationField
      : public BitField16<VariableLocation, VariableKindField::kNext, 3> {};
  class ForceContextAllocationField
      : public BitField16<bool, LocationField::kNext, 1> {};
  class IsUsedField
      : public BitField16<bool, ForceContextAllocationField::kNext, 1> {};
  class InitializationFlagField
      : public BitField16<InitializationFlag, IsUsedField::kNext, 1> {};
  class ForceHoleInitializationField
      : public BitField16<bool, InitializationFlagField::kNext, 1> {};
  class MaybeAssignedFlagField
      : public BitField16<MaybeAssignedFlag,
                          ForceHoleInitializationField::kNext, 1> {};
  Variable** next() { return &next_; }
  friend List;
  friend base::ThreadedListTraits<Variable>;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_AST_VARIABLES_H_
