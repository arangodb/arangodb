// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/bytecode-register-optimizer.h"

namespace v8 {
namespace internal {
namespace interpreter {

const uint32_t BytecodeRegisterOptimizer::kInvalidEquivalenceId = kMaxUInt32;

// A class for tracking the state of a register. This class tracks
// which equivalence set a register is a member of and also whether a
// register is materialized in the bytecode stream.
class BytecodeRegisterOptimizer::RegisterInfo final : public ZoneObject {
 public:
  RegisterInfo(Register reg, uint32_t equivalence_id, bool materialized,
               bool allocated)
      : register_(reg),
        equivalence_id_(equivalence_id),
        materialized_(materialized),
        allocated_(allocated),
        next_(this),
        prev_(this) {}

  void AddToEquivalenceSetOf(RegisterInfo* info);
  void MoveToNewEquivalenceSet(uint32_t equivalence_id, bool materialized);
  bool IsOnlyMemberOfEquivalenceSet() const;
  bool IsOnlyMaterializedMemberOfEquivalenceSet() const;
  bool IsInSameEquivalenceSet(RegisterInfo* info) const;

  // Get a member of this register's equivalence set that is
  // materialized. The materialized equivalent will be this register
  // if it is materialized. Returns nullptr if no materialized
  // equivalent exists.
  RegisterInfo* GetMaterializedEquivalent();

  // Get a member of this register's equivalence set that is
  // materialized and not register |reg|. The materialized equivalent
  // will be this register if it is materialized. Returns nullptr if
  // no materialized equivalent exists.
  RegisterInfo* GetMaterializedEquivalentOtherThan(Register reg);

  // Get a member of this register's equivalence set that is intended
  // to be materialized in place of this register (which is currently
  // materialized). The best candidate is deemed to be the register
  // with the lowest index as this permits temporary registers to be
  // removed from the bytecode stream. Returns nullptr if no candidate
  // exists.
  RegisterInfo* GetEquivalentToMaterialize();

  // Marks all temporary registers of the equivalence set as unmaterialized.
  void MarkTemporariesAsUnmaterialized(Register temporary_base);

  // Get an equivalent register. Returns this if none exists.
  RegisterInfo* GetEquivalent();

  Register register_value() const { return register_; }
  bool materialized() const { return materialized_; }
  void set_materialized(bool materialized) { materialized_ = materialized; }
  bool allocated() const { return allocated_; }
  void set_allocated(bool allocated) { allocated_ = allocated; }
  void set_equivalence_id(uint32_t equivalence_id) {
    equivalence_id_ = equivalence_id;
  }
  uint32_t equivalence_id() const { return equivalence_id_; }

 private:
  Register register_;
  uint32_t equivalence_id_;
  bool materialized_;
  bool allocated_;

  // Equivalence set pointers.
  RegisterInfo* next_;
  RegisterInfo* prev_;

  DISALLOW_COPY_AND_ASSIGN(RegisterInfo);
};

void BytecodeRegisterOptimizer::RegisterInfo::AddToEquivalenceSetOf(
    RegisterInfo* info) {
  DCHECK_NE(kInvalidEquivalenceId, info->equivalence_id());
  // Fix old list
  next_->prev_ = prev_;
  prev_->next_ = next_;
  // Add to new list.
  next_ = info->next_;
  prev_ = info;
  prev_->next_ = this;
  next_->prev_ = this;
  set_equivalence_id(info->equivalence_id());
  set_materialized(false);
}

void BytecodeRegisterOptimizer::RegisterInfo::MoveToNewEquivalenceSet(
    uint32_t equivalence_id, bool materialized) {
  next_->prev_ = prev_;
  prev_->next_ = next_;
  next_ = prev_ = this;
  equivalence_id_ = equivalence_id;
  materialized_ = materialized;
}

bool BytecodeRegisterOptimizer::RegisterInfo::IsOnlyMemberOfEquivalenceSet()
    const {
  return this->next_ == this;
}

bool BytecodeRegisterOptimizer::RegisterInfo::
    IsOnlyMaterializedMemberOfEquivalenceSet() const {
  DCHECK(materialized());

  const RegisterInfo* visitor = this->next_;
  while (visitor != this) {
    if (visitor->materialized()) {
      return false;
    }
    visitor = visitor->next_;
  }
  return true;
}

bool BytecodeRegisterOptimizer::RegisterInfo::IsInSameEquivalenceSet(
    RegisterInfo* info) const {
  return equivalence_id() == info->equivalence_id();
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetMaterializedEquivalent() {
  RegisterInfo* visitor = this;
  do {
    if (visitor->materialized()) {
      return visitor;
    }
    visitor = visitor->next_;
  } while (visitor != this);

  return nullptr;
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetMaterializedEquivalentOtherThan(
    Register reg) {
  RegisterInfo* visitor = this;
  do {
    if (visitor->materialized() && visitor->register_value() != reg) {
      return visitor;
    }
    visitor = visitor->next_;
  } while (visitor != this);

  return nullptr;
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetEquivalentToMaterialize() {
  DCHECK(this->materialized());
  RegisterInfo* visitor = this->next_;
  RegisterInfo* best_info = nullptr;
  while (visitor != this) {
    if (visitor->materialized()) {
      return nullptr;
    }
    if (visitor->allocated() &&
        (best_info == nullptr ||
         visitor->register_value() < best_info->register_value())) {
      best_info = visitor;
    }
    visitor = visitor->next_;
  }
  return best_info;
}

void BytecodeRegisterOptimizer::RegisterInfo::MarkTemporariesAsUnmaterialized(
    Register temporary_base) {
  DCHECK(this->register_value() < temporary_base);
  DCHECK(this->materialized());
  RegisterInfo* visitor = this->next_;
  while (visitor != this) {
    if (visitor->register_value() >= temporary_base) {
      visitor->set_materialized(false);
    }
    visitor = visitor->next_;
  }
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::RegisterInfo::GetEquivalent() {
  return next_;
}

BytecodeRegisterOptimizer::BytecodeRegisterOptimizer(
    Zone* zone, BytecodeRegisterAllocator* register_allocator,
    int fixed_registers_count, int parameter_count,
    BytecodePipelineStage* next_stage)
    : accumulator_(Register::virtual_accumulator()),
      temporary_base_(fixed_registers_count),
      max_register_index_(fixed_registers_count - 1),
      register_info_table_(zone),
      equivalence_id_(0),
      next_stage_(next_stage),
      flush_required_(false),
      zone_(zone) {
  register_allocator->set_observer(this);

  // Calculate offset so register index values can be mapped into
  // a vector of register metadata.
  if (parameter_count != 0) {
    register_info_table_offset_ =
        -Register::FromParameterIndex(0, parameter_count).index();
  } else {
    // TODO(oth): This path shouldn't be necessary in bytecode generated
    // from Javascript, but a set of tests do not include the JS receiver.
    register_info_table_offset_ = -accumulator_.index();
  }

  // Initialize register map for parameters, locals, and the
  // accumulator.
  register_info_table_.resize(register_info_table_offset_ +
                              static_cast<size_t>(temporary_base_.index()));
  for (size_t i = 0; i < register_info_table_.size(); ++i) {
    register_info_table_[i] = new (zone) RegisterInfo(
        RegisterFromRegisterInfoTableIndex(i), NextEquivalenceId(), true, true);
    DCHECK_EQ(register_info_table_[i]->register_value().index(),
              RegisterFromRegisterInfoTableIndex(i).index());
  }
  accumulator_info_ = GetRegisterInfo(accumulator_);
  DCHECK(accumulator_info_->register_value() == accumulator_);
}

void BytecodeRegisterOptimizer::Flush() {
  if (!flush_required_) {
    return;
  }

  // Materialize all live registers and break equivalences.
  size_t count = register_info_table_.size();
  for (size_t i = 0; i < count; ++i) {
    RegisterInfo* reg_info = register_info_table_[i];
    if (reg_info->materialized()) {
      // Walk equivalents of materialized registers, materializing
      // each equivalent register as necessary and placing in their
      // own equivalence set.
      RegisterInfo* equivalent;
      while ((equivalent = reg_info->GetEquivalent()) != reg_info) {
        if (equivalent->allocated() && !equivalent->materialized()) {
          OutputRegisterTransfer(reg_info, equivalent);
        }
        equivalent->MoveToNewEquivalenceSet(NextEquivalenceId(), true);
      }
    }
  }

  flush_required_ = false;
}

void BytecodeRegisterOptimizer::OutputRegisterTransfer(
    RegisterInfo* input_info, RegisterInfo* output_info,
    BytecodeSourceInfo source_info) {
  Register input = input_info->register_value();
  Register output = output_info->register_value();
  DCHECK_NE(input.index(), output.index());

  if (input == accumulator_) {
    uint32_t operand = static_cast<uint32_t>(output.ToOperand());
    BytecodeNode node = BytecodeNode::Star(source_info, operand);
    next_stage_->Write(&node);
  } else if (output == accumulator_) {
    uint32_t operand = static_cast<uint32_t>(input.ToOperand());
    BytecodeNode node = BytecodeNode::Ldar(source_info, operand);
    next_stage_->Write(&node);
  } else {
    uint32_t operand0 = static_cast<uint32_t>(input.ToOperand());
    uint32_t operand1 = static_cast<uint32_t>(output.ToOperand());
    BytecodeNode node = BytecodeNode::Mov(source_info, operand0, operand1);
    next_stage_->Write(&node);
  }
  if (output != accumulator_) {
    max_register_index_ = std::max(max_register_index_, output.index());
  }
  output_info->set_materialized(true);
}

void BytecodeRegisterOptimizer::CreateMaterializedEquivalent(
    RegisterInfo* info) {
  DCHECK(info->materialized());
  RegisterInfo* unmaterialized = info->GetEquivalentToMaterialize();
  if (unmaterialized) {
    OutputRegisterTransfer(info, unmaterialized);
  }
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::GetMaterializedEquivalent(RegisterInfo* info) {
  return info->materialized() ? info : info->GetMaterializedEquivalent();
}

BytecodeRegisterOptimizer::RegisterInfo*
BytecodeRegisterOptimizer::GetMaterializedEquivalentNotAccumulator(
    RegisterInfo* info) {
  if (info->materialized()) {
    return info;
  }

  RegisterInfo* result = info->GetMaterializedEquivalentOtherThan(accumulator_);
  if (result == nullptr) {
    Materialize(info);
    result = info;
  }
  DCHECK(result->register_value() != accumulator_);
  return result;
}

void BytecodeRegisterOptimizer::Materialize(RegisterInfo* info) {
  if (!info->materialized()) {
    RegisterInfo* materialized = info->GetMaterializedEquivalent();
    OutputRegisterTransfer(materialized, info);
  }
}

void BytecodeRegisterOptimizer::AddToEquivalenceSet(
    RegisterInfo* set_member, RegisterInfo* non_set_member) {
  non_set_member->AddToEquivalenceSetOf(set_member);
  // Flushing is only required when two or more registers are placed
  // in the same equivalence set.
  flush_required_ = true;
}

void BytecodeRegisterOptimizer::RegisterTransfer(
    RegisterInfo* input_info, RegisterInfo* output_info,
    BytecodeSourceInfo source_info) {
  // Materialize an alternate in the equivalence set that
  // |output_info| is leaving.
  if (output_info->materialized()) {
    CreateMaterializedEquivalent(output_info);
  }

  // Add |output_info| to new equivalence set.
  if (!output_info->IsInSameEquivalenceSet(input_info)) {
    AddToEquivalenceSet(input_info, output_info);
  }

  bool output_is_observable =
      RegisterIsObservable(output_info->register_value());
  if (output_is_observable) {
    // Force store to be emitted when register is observable.
    output_info->set_materialized(false);
    RegisterInfo* materialized_info = input_info->GetMaterializedEquivalent();
    OutputRegisterTransfer(materialized_info, output_info, source_info);
  } else if (source_info.is_valid()) {
    // Emit a placeholder nop to maintain source position info.
    EmitNopForSourceInfo(source_info);
  }

  bool input_is_observable = RegisterIsObservable(input_info->register_value());
  if (input_is_observable) {
    // If input is observable by the debugger, mark all other temporaries
    // registers as unmaterialized so that this register is used in preference.
    input_info->MarkTemporariesAsUnmaterialized(temporary_base_);
  }
}

void BytecodeRegisterOptimizer::EmitNopForSourceInfo(
    BytecodeSourceInfo source_info) const {
  DCHECK(source_info.is_valid());
  BytecodeNode nop = BytecodeNode::Nop(source_info);
  next_stage_->Write(&nop);
}

void BytecodeRegisterOptimizer::PrepareOutputRegister(Register reg) {
  RegisterInfo* reg_info = GetRegisterInfo(reg);
  if (reg_info->materialized()) {
    CreateMaterializedEquivalent(reg_info);
  }
  reg_info->MoveToNewEquivalenceSet(NextEquivalenceId(), true);
  max_register_index_ =
      std::max(max_register_index_, reg_info->register_value().index());
}

void BytecodeRegisterOptimizer::PrepareOutputRegisterList(
    RegisterList reg_list) {
  int start_index = reg_list.first_register().index();
  for (int i = 0; i < reg_list.register_count(); ++i) {
    Register current(start_index + i);
    PrepareOutputRegister(current);
  }
}

Register BytecodeRegisterOptimizer::GetInputRegister(Register reg) {
  RegisterInfo* reg_info = GetRegisterInfo(reg);
  if (reg_info->materialized()) {
    return reg;
  } else {
    RegisterInfo* equivalent_info =
        GetMaterializedEquivalentNotAccumulator(reg_info);
    return equivalent_info->register_value();
  }
}

RegisterList BytecodeRegisterOptimizer::GetInputRegisterList(
    RegisterList reg_list) {
  if (reg_list.register_count() == 1) {
    // If there is only a single register, treat it as a normal input register.
    Register reg(GetInputRegister(reg_list.first_register()));
    return RegisterList(reg.index(), 1);
  } else {
    int start_index = reg_list.first_register().index();
    for (int i = 0; i < reg_list.register_count(); ++i) {
      Register current(start_index + i);
      RegisterInfo* input_info = GetRegisterInfo(current);
      Materialize(input_info);
    }
    return reg_list;
  }
}

void BytecodeRegisterOptimizer::GrowRegisterMap(Register reg) {
  DCHECK(RegisterIsTemporary(reg));
  size_t index = GetRegisterInfoTableIndex(reg);
  if (index >= register_info_table_.size()) {
    size_t new_size = index + 1;
    size_t old_size = register_info_table_.size();
    register_info_table_.resize(new_size);
    for (size_t i = old_size; i < new_size; ++i) {
      register_info_table_[i] =
          new (zone()) RegisterInfo(RegisterFromRegisterInfoTableIndex(i),
                                    NextEquivalenceId(), false, false);
    }
  }
}

void BytecodeRegisterOptimizer::RegisterAllocateEvent(Register reg) {
  GetOrCreateRegisterInfo(reg)->set_allocated(true);
}

void BytecodeRegisterOptimizer::RegisterListAllocateEvent(
    RegisterList reg_list) {
  if (reg_list.register_count() != 0) {
    int first_index = reg_list.first_register().index();
    GrowRegisterMap(Register(first_index + reg_list.register_count() - 1));
    for (int i = 0; i < reg_list.register_count(); i++) {
      GetRegisterInfo(Register(first_index + i))->set_allocated(true);
    }
  }
}

void BytecodeRegisterOptimizer::RegisterListFreeEvent(RegisterList reg_list) {
  int first_index = reg_list.first_register().index();
  for (int i = 0; i < reg_list.register_count(); i++) {
    GetRegisterInfo(Register(first_index + i))->set_allocated(false);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
