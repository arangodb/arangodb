// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/register-allocator.h"

#include "src/assembler-inl.h"
#include "src/base/adapters.h"
#include "src/compiler/linkage.h"
#include "src/string-stream.h"

namespace v8 {
namespace internal {
namespace compiler {

#define TRACE(...)                             \
  do {                                         \
    if (FLAG_trace_alloc) PrintF(__VA_ARGS__); \
  } while (false)


namespace {

static constexpr int kFloat32Bit =
    RepresentationBit(MachineRepresentation::kFloat32);
static constexpr int kSimd128Bit =
    RepresentationBit(MachineRepresentation::kSimd128);

void RemoveElement(ZoneVector<LiveRange*>* v, LiveRange* range) {
  auto it = std::find(v->begin(), v->end(), range);
  DCHECK(it != v->end());
  v->erase(it);
}

int GetRegisterCount(const RegisterConfiguration* cfg, RegisterKind kind) {
  return kind == FP_REGISTERS ? cfg->num_double_registers()
                              : cfg->num_general_registers();
}


int GetAllocatableRegisterCount(const RegisterConfiguration* cfg,
                                RegisterKind kind) {
  return kind == FP_REGISTERS ? cfg->num_allocatable_double_registers()
                              : cfg->num_allocatable_general_registers();
}


const int* GetAllocatableRegisterCodes(const RegisterConfiguration* cfg,
                                       RegisterKind kind) {
  return kind == FP_REGISTERS ? cfg->allocatable_double_codes()
                              : cfg->allocatable_general_codes();
}


const InstructionBlock* GetContainingLoop(const InstructionSequence* sequence,
                                          const InstructionBlock* block) {
  RpoNumber index = block->loop_header();
  if (!index.IsValid()) return nullptr;
  return sequence->InstructionBlockAt(index);
}


const InstructionBlock* GetInstructionBlock(const InstructionSequence* code,
                                            LifetimePosition pos) {
  return code->GetInstructionBlock(pos.ToInstructionIndex());
}


Instruction* GetLastInstruction(InstructionSequence* code,
                                const InstructionBlock* block) {
  return code->InstructionAt(block->last_instruction_index());
}

// TODO(dcarney): fix frame to allow frame accesses to half size location.
int GetByteWidth(MachineRepresentation rep) {
  switch (rep) {
    case MachineRepresentation::kBit:
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kTaggedSigned:
    case MachineRepresentation::kTaggedPointer:
    case MachineRepresentation::kTagged:
    case MachineRepresentation::kFloat32:
      return kPointerSize;
    case MachineRepresentation::kWord64:
    case MachineRepresentation::kFloat64:
      return kDoubleSize;
    case MachineRepresentation::kSimd128:
      return kSimd128Size;
    case MachineRepresentation::kNone:
      break;
  }
  UNREACHABLE();
}

}  // namespace

class LiveRangeBound {
 public:
  explicit LiveRangeBound(LiveRange* range, bool skip)
      : range_(range), start_(range->Start()), end_(range->End()), skip_(skip) {
    DCHECK(!range->IsEmpty());
  }

  bool CanCover(LifetimePosition position) {
    return start_ <= position && position < end_;
  }

  LiveRange* const range_;
  const LifetimePosition start_;
  const LifetimePosition end_;
  const bool skip_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LiveRangeBound);
};


struct FindResult {
  LiveRange* cur_cover_;
  LiveRange* pred_cover_;
};


class LiveRangeBoundArray {
 public:
  LiveRangeBoundArray() : length_(0), start_(nullptr) {}

  bool ShouldInitialize() { return start_ == nullptr; }

  void Initialize(Zone* zone, TopLevelLiveRange* range) {
    length_ = range->GetChildCount();

    start_ = zone->NewArray<LiveRangeBound>(length_);
    LiveRangeBound* curr = start_;
    // Normally, spilled ranges do not need connecting moves, because the spill
    // location has been assigned at definition. For ranges spilled in deferred
    // blocks, that is not the case, so we need to connect the spilled children.
    for (LiveRange *i = range; i != nullptr; i = i->next(), ++curr) {
      new (curr) LiveRangeBound(i, i->spilled());
    }
  }

  LiveRangeBound* Find(const LifetimePosition position) const {
    size_t left_index = 0;
    size_t right_index = length_;
    while (true) {
      size_t current_index = left_index + (right_index - left_index) / 2;
      DCHECK(right_index > current_index);
      LiveRangeBound* bound = &start_[current_index];
      if (bound->start_ <= position) {
        if (position < bound->end_) return bound;
        DCHECK(left_index < current_index);
        left_index = current_index;
      } else {
        right_index = current_index;
      }
    }
  }

  LiveRangeBound* FindPred(const InstructionBlock* pred) {
    LifetimePosition pred_end =
        LifetimePosition::InstructionFromInstructionIndex(
            pred->last_instruction_index());
    return Find(pred_end);
  }

  LiveRangeBound* FindSucc(const InstructionBlock* succ) {
    LifetimePosition succ_start = LifetimePosition::GapFromInstructionIndex(
        succ->first_instruction_index());
    return Find(succ_start);
  }

  bool FindConnectableSubranges(const InstructionBlock* block,
                                const InstructionBlock* pred,
                                FindResult* result) const {
    LifetimePosition pred_end =
        LifetimePosition::InstructionFromInstructionIndex(
            pred->last_instruction_index());
    LiveRangeBound* bound = Find(pred_end);
    result->pred_cover_ = bound->range_;
    LifetimePosition cur_start = LifetimePosition::GapFromInstructionIndex(
        block->first_instruction_index());

    if (bound->CanCover(cur_start)) {
      // Both blocks are covered by the same range, so there is nothing to
      // connect.
      return false;
    }
    bound = Find(cur_start);
    if (bound->skip_) {
      return false;
    }
    result->cur_cover_ = bound->range_;
    DCHECK(result->pred_cover_ != nullptr && result->cur_cover_ != nullptr);
    return (result->cur_cover_ != result->pred_cover_);
  }

 private:
  size_t length_;
  LiveRangeBound* start_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeBoundArray);
};


class LiveRangeFinder {
 public:
  explicit LiveRangeFinder(const RegisterAllocationData* data, Zone* zone)
      : data_(data),
        bounds_length_(static_cast<int>(data_->live_ranges().size())),
        bounds_(zone->NewArray<LiveRangeBoundArray>(bounds_length_)),
        zone_(zone) {
    for (int i = 0; i < bounds_length_; ++i) {
      new (&bounds_[i]) LiveRangeBoundArray();
    }
  }

  LiveRangeBoundArray* ArrayFor(int operand_index) {
    DCHECK(operand_index < bounds_length_);
    TopLevelLiveRange* range = data_->live_ranges()[operand_index];
    DCHECK(range != nullptr && !range->IsEmpty());
    LiveRangeBoundArray* array = &bounds_[operand_index];
    if (array->ShouldInitialize()) {
      array->Initialize(zone_, range);
    }
    return array;
  }

 private:
  const RegisterAllocationData* const data_;
  const int bounds_length_;
  LiveRangeBoundArray* const bounds_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(LiveRangeFinder);
};


typedef std::pair<ParallelMove*, InstructionOperand> DelayedInsertionMapKey;


struct DelayedInsertionMapCompare {
  bool operator()(const DelayedInsertionMapKey& a,
                  const DelayedInsertionMapKey& b) const {
    if (a.first == b.first) {
      return a.second.Compare(b.second);
    }
    return a.first < b.first;
  }
};


typedef ZoneMap<DelayedInsertionMapKey, InstructionOperand,
                DelayedInsertionMapCompare> DelayedInsertionMap;


UsePosition::UsePosition(LifetimePosition pos, InstructionOperand* operand,
                         void* hint, UsePositionHintType hint_type)
    : operand_(operand), hint_(hint), next_(nullptr), pos_(pos), flags_(0) {
  DCHECK_IMPLIES(hint == nullptr, hint_type == UsePositionHintType::kNone);
  bool register_beneficial = true;
  UsePositionType type = UsePositionType::kRegisterOrSlot;
  if (operand_ != nullptr && operand_->IsUnallocated()) {
    const UnallocatedOperand* unalloc = UnallocatedOperand::cast(operand_);
    if (unalloc->HasRegisterPolicy()) {
      type = UsePositionType::kRequiresRegister;
    } else if (unalloc->HasSlotPolicy()) {
      type = UsePositionType::kRequiresSlot;
      register_beneficial = false;
    } else if (unalloc->HasRegisterOrSlotOrConstantPolicy()) {
      type = UsePositionType::kRegisterOrSlotOrConstant;
      register_beneficial = false;
    } else {
      register_beneficial = !unalloc->HasRegisterOrSlotPolicy();
    }
  }
  flags_ = TypeField::encode(type) | HintTypeField::encode(hint_type) |
           RegisterBeneficialField::encode(register_beneficial) |
           AssignedRegisterField::encode(kUnassignedRegister);
  DCHECK(pos_.IsValid());
}


bool UsePosition::HasHint() const {
  int hint_register;
  return HintRegister(&hint_register);
}


bool UsePosition::HintRegister(int* register_code) const {
  if (hint_ == nullptr) return false;
  switch (HintTypeField::decode(flags_)) {
    case UsePositionHintType::kNone:
    case UsePositionHintType::kUnresolved:
      return false;
    case UsePositionHintType::kUsePos: {
      UsePosition* use_pos = reinterpret_cast<UsePosition*>(hint_);
      int assigned_register = AssignedRegisterField::decode(use_pos->flags_);
      if (assigned_register == kUnassignedRegister) return false;
      *register_code = assigned_register;
      return true;
    }
    case UsePositionHintType::kOperand: {
      InstructionOperand* operand =
          reinterpret_cast<InstructionOperand*>(hint_);
      *register_code = LocationOperand::cast(operand)->register_code();
      return true;
    }
    case UsePositionHintType::kPhi: {
      RegisterAllocationData::PhiMapValue* phi =
          reinterpret_cast<RegisterAllocationData::PhiMapValue*>(hint_);
      int assigned_register = phi->assigned_register();
      if (assigned_register == kUnassignedRegister) return false;
      *register_code = assigned_register;
      return true;
    }
  }
  UNREACHABLE();
}


UsePositionHintType UsePosition::HintTypeForOperand(
    const InstructionOperand& op) {
  switch (op.kind()) {
    case InstructionOperand::CONSTANT:
    case InstructionOperand::IMMEDIATE:
    case InstructionOperand::EXPLICIT:
      return UsePositionHintType::kNone;
    case InstructionOperand::UNALLOCATED:
      return UsePositionHintType::kUnresolved;
    case InstructionOperand::ALLOCATED:
      if (op.IsRegister() || op.IsFPRegister()) {
        return UsePositionHintType::kOperand;
      } else {
        DCHECK(op.IsStackSlot() || op.IsFPStackSlot());
        return UsePositionHintType::kNone;
      }
    case InstructionOperand::INVALID:
      break;
  }
  UNREACHABLE();
}

void UsePosition::SetHint(UsePosition* use_pos) {
  DCHECK_NOT_NULL(use_pos);
  hint_ = use_pos;
  flags_ = HintTypeField::update(flags_, UsePositionHintType::kUsePos);
}

void UsePosition::ResolveHint(UsePosition* use_pos) {
  DCHECK_NOT_NULL(use_pos);
  if (HintTypeField::decode(flags_) != UsePositionHintType::kUnresolved) return;
  hint_ = use_pos;
  flags_ = HintTypeField::update(flags_, UsePositionHintType::kUsePos);
}


void UsePosition::set_type(UsePositionType type, bool register_beneficial) {
  DCHECK_IMPLIES(type == UsePositionType::kRequiresSlot, !register_beneficial);
  DCHECK_EQ(kUnassignedRegister, AssignedRegisterField::decode(flags_));
  flags_ = TypeField::encode(type) |
           RegisterBeneficialField::encode(register_beneficial) |
           HintTypeField::encode(HintTypeField::decode(flags_)) |
           AssignedRegisterField::encode(kUnassignedRegister);
}


UseInterval* UseInterval::SplitAt(LifetimePosition pos, Zone* zone) {
  DCHECK(Contains(pos) && pos != start());
  UseInterval* after = new (zone) UseInterval(pos, end_);
  after->next_ = next_;
  next_ = nullptr;
  end_ = pos;
  return after;
}

void LifetimePosition::Print() const { StdoutStream{} << *this << std::endl; }

std::ostream& operator<<(std::ostream& os, const LifetimePosition pos) {
  os << '@' << pos.ToInstructionIndex();
  if (pos.IsGapPosition()) {
    os << 'g';
  } else {
    os << 'i';
  }
  if (pos.IsStart()) {
    os << 's';
  } else {
    os << 'e';
  }
  return os;
}

LiveRange::LiveRange(int relative_id, MachineRepresentation rep,
                     TopLevelLiveRange* top_level)
    : relative_id_(relative_id),
      bits_(0),
      last_interval_(nullptr),
      first_interval_(nullptr),
      first_pos_(nullptr),
      top_level_(top_level),
      next_(nullptr),
      current_interval_(nullptr),
      last_processed_use_(nullptr),
      current_hint_position_(nullptr),
      splitting_pointer_(nullptr) {
  DCHECK(AllocatedOperand::IsSupportedRepresentation(rep));
  bits_ = AssignedRegisterField::encode(kUnassignedRegister) |
          RepresentationField::encode(rep);
}


void LiveRange::VerifyPositions() const {
  // Walk the positions, verifying that each is in an interval.
  UseInterval* interval = first_interval_;
  for (UsePosition* pos = first_pos_; pos != nullptr; pos = pos->next()) {
    CHECK(Start() <= pos->pos());
    CHECK(pos->pos() <= End());
    CHECK_NOT_NULL(interval);
    while (!interval->Contains(pos->pos()) && interval->end() != pos->pos()) {
      interval = interval->next();
      CHECK_NOT_NULL(interval);
    }
  }
}


void LiveRange::VerifyIntervals() const {
  DCHECK(first_interval()->start() == Start());
  LifetimePosition last_end = first_interval()->end();
  for (UseInterval* interval = first_interval()->next(); interval != nullptr;
       interval = interval->next()) {
    DCHECK(last_end <= interval->start());
    last_end = interval->end();
  }
  DCHECK(last_end == End());
}


void LiveRange::set_assigned_register(int reg) {
  DCHECK(!HasRegisterAssigned() && !spilled());
  bits_ = AssignedRegisterField::update(bits_, reg);
}


void LiveRange::UnsetAssignedRegister() {
  DCHECK(HasRegisterAssigned() && !spilled());
  bits_ = AssignedRegisterField::update(bits_, kUnassignedRegister);
}


void LiveRange::Spill() {
  DCHECK(!spilled());
  DCHECK(!TopLevel()->HasNoSpillType());
  set_spilled(true);
  bits_ = AssignedRegisterField::update(bits_, kUnassignedRegister);
}


RegisterKind LiveRange::kind() const {
  return IsFloatingPoint(representation()) ? FP_REGISTERS : GENERAL_REGISTERS;
}


UsePosition* LiveRange::FirstHintPosition(int* register_index) const {
  for (UsePosition* pos = first_pos_; pos != nullptr; pos = pos->next()) {
    if (pos->HintRegister(register_index)) return pos;
  }
  return nullptr;
}


UsePosition* LiveRange::NextUsePosition(LifetimePosition start) const {
  UsePosition* use_pos = last_processed_use_;
  if (use_pos == nullptr || use_pos->pos() > start) {
    use_pos = first_pos();
  }
  while (use_pos != nullptr && use_pos->pos() < start) {
    use_pos = use_pos->next();
  }
  last_processed_use_ = use_pos;
  return use_pos;
}


UsePosition* LiveRange::NextUsePositionRegisterIsBeneficial(
    LifetimePosition start) const {
  UsePosition* pos = NextUsePosition(start);
  while (pos != nullptr && !pos->RegisterIsBeneficial()) {
    pos = pos->next();
  }
  return pos;
}

LifetimePosition LiveRange::NextLifetimePositionRegisterIsBeneficial(
    const LifetimePosition& start) const {
  UsePosition* next_use = NextUsePositionRegisterIsBeneficial(start);
  if (next_use == nullptr) return End();
  return next_use->pos();
}

UsePosition* LiveRange::PreviousUsePositionRegisterIsBeneficial(
    LifetimePosition start) const {
  UsePosition* pos = first_pos();
  UsePosition* prev = nullptr;
  while (pos != nullptr && pos->pos() < start) {
    if (pos->RegisterIsBeneficial()) prev = pos;
    pos = pos->next();
  }
  return prev;
}


UsePosition* LiveRange::NextRegisterPosition(LifetimePosition start) const {
  UsePosition* pos = NextUsePosition(start);
  while (pos != nullptr && pos->type() != UsePositionType::kRequiresRegister) {
    pos = pos->next();
  }
  return pos;
}


UsePosition* LiveRange::NextSlotPosition(LifetimePosition start) const {
  for (UsePosition* pos = NextUsePosition(start); pos != nullptr;
       pos = pos->next()) {
    if (pos->type() != UsePositionType::kRequiresSlot) continue;
    return pos;
  }
  return nullptr;
}


bool LiveRange::CanBeSpilled(LifetimePosition pos) const {
  // We cannot spill a live range that has a use requiring a register
  // at the current or the immediate next position.
  UsePosition* use_pos = NextRegisterPosition(pos);
  if (use_pos == nullptr) return true;
  return use_pos->pos() > pos.NextStart().End();
}


bool LiveRange::IsTopLevel() const { return top_level_ == this; }


InstructionOperand LiveRange::GetAssignedOperand() const {
  if (HasRegisterAssigned()) {
    DCHECK(!spilled());
    return AllocatedOperand(LocationOperand::REGISTER, representation(),
                            assigned_register());
  }
  DCHECK(spilled());
  DCHECK(!HasRegisterAssigned());
  if (TopLevel()->HasSpillOperand()) {
    InstructionOperand* op = TopLevel()->GetSpillOperand();
    DCHECK(!op->IsUnallocated());
    return *op;
  }
  return TopLevel()->GetSpillRangeOperand();
}


UseInterval* LiveRange::FirstSearchIntervalForPosition(
    LifetimePosition position) const {
  if (current_interval_ == nullptr) return first_interval_;
  if (current_interval_->start() > position) {
    current_interval_ = nullptr;
    return first_interval_;
  }
  return current_interval_;
}


void LiveRange::AdvanceLastProcessedMarker(
    UseInterval* to_start_of, LifetimePosition but_not_past) const {
  if (to_start_of == nullptr) return;
  if (to_start_of->start() > but_not_past) return;
  LifetimePosition start = current_interval_ == nullptr
                               ? LifetimePosition::Invalid()
                               : current_interval_->start();
  if (to_start_of->start() > start) {
    current_interval_ = to_start_of;
  }
}


LiveRange* LiveRange::SplitAt(LifetimePosition position, Zone* zone) {
  int new_id = TopLevel()->GetNextChildId();
  LiveRange* child = new (zone) LiveRange(new_id, representation(), TopLevel());
  // If we split, we do so because we're about to switch registers or move
  // to/from a slot, so there's no value in connecting hints.
  DetachAt(position, child, zone, DoNotConnectHints);

  child->top_level_ = TopLevel();
  child->next_ = next_;
  next_ = child;
  return child;
}

UsePosition* LiveRange::DetachAt(LifetimePosition position, LiveRange* result,
                                 Zone* zone,
                                 HintConnectionOption connect_hints) {
  DCHECK(Start() < position);
  DCHECK(End() > position);
  DCHECK(result->IsEmpty());
  // Find the last interval that ends before the position. If the
  // position is contained in one of the intervals in the chain, we
  // split that interval and use the first part.
  UseInterval* current = FirstSearchIntervalForPosition(position);

  // If the split position coincides with the beginning of a use interval
  // we need to split use positons in a special way.
  bool split_at_start = false;

  if (current->start() == position) {
    // When splitting at start we need to locate the previous use interval.
    current = first_interval_;
  }

  UseInterval* after = nullptr;
  while (current != nullptr) {
    if (current->Contains(position)) {
      after = current->SplitAt(position, zone);
      break;
    }
    UseInterval* next = current->next();
    if (next->start() >= position) {
      split_at_start = (next->start() == position);
      after = next;
      current->set_next(nullptr);
      break;
    }
    current = next;
  }
  DCHECK_NOT_NULL(after);

  // Partition original use intervals to the two live ranges.
  UseInterval* before = current;
  result->last_interval_ =
      (last_interval_ == before)
          ? after            // Only interval in the range after split.
          : last_interval_;  // Last interval of the original range.
  result->first_interval_ = after;
  last_interval_ = before;

  // Find the last use position before the split and the first use
  // position after it.
  UsePosition* use_after =
      splitting_pointer_ == nullptr || splitting_pointer_->pos() > position
          ? first_pos()
          : splitting_pointer_;
  UsePosition* use_before = nullptr;
  if (split_at_start) {
    // The split position coincides with the beginning of a use interval (the
    // end of a lifetime hole). Use at this position should be attributed to
    // the split child because split child owns use interval covering it.
    while (use_after != nullptr && use_after->pos() < position) {
      use_before = use_after;
      use_after = use_after->next();
    }
  } else {
    while (use_after != nullptr && use_after->pos() <= position) {
      use_before = use_after;
      use_after = use_after->next();
    }
  }

  // Partition original use positions to the two live ranges.
  if (use_before != nullptr) {
    use_before->set_next(nullptr);
  } else {
    first_pos_ = nullptr;
  }
  result->first_pos_ = use_after;

  // Discard cached iteration state. It might be pointing
  // to the use that no longer belongs to this live range.
  last_processed_use_ = nullptr;
  current_interval_ = nullptr;

  if (connect_hints == ConnectHints && use_before != nullptr &&
      use_after != nullptr) {
    use_after->SetHint(use_before);
  }
#ifdef DEBUG
  VerifyChildStructure();
  result->VerifyChildStructure();
#endif
  return use_before;
}


void LiveRange::UpdateParentForAllChildren(TopLevelLiveRange* new_top_level) {
  LiveRange* child = this;
  for (; child != nullptr; child = child->next()) {
    child->top_level_ = new_top_level;
  }
}


void LiveRange::ConvertUsesToOperand(const InstructionOperand& op,
                                     const InstructionOperand& spill_op) {
  for (UsePosition* pos = first_pos(); pos != nullptr; pos = pos->next()) {
    DCHECK(Start() <= pos->pos() && pos->pos() <= End());
    if (!pos->HasOperand()) continue;
    switch (pos->type()) {
      case UsePositionType::kRequiresSlot:
        DCHECK(spill_op.IsStackSlot() || spill_op.IsFPStackSlot());
        InstructionOperand::ReplaceWith(pos->operand(), &spill_op);
        break;
      case UsePositionType::kRequiresRegister:
        DCHECK(op.IsRegister() || op.IsFPRegister());
        V8_FALLTHROUGH;
      case UsePositionType::kRegisterOrSlot:
      case UsePositionType::kRegisterOrSlotOrConstant:
        InstructionOperand::ReplaceWith(pos->operand(), &op);
        break;
    }
  }
}


// This implements an ordering on live ranges so that they are ordered by their
// start positions.  This is needed for the correctness of the register
// allocation algorithm.  If two live ranges start at the same offset then there
// is a tie breaker based on where the value is first used.  This part of the
// ordering is merely a heuristic.
bool LiveRange::ShouldBeAllocatedBefore(const LiveRange* other) const {
  LifetimePosition start = Start();
  LifetimePosition other_start = other->Start();
  if (start == other_start) {
    UsePosition* pos = first_pos();
    if (pos == nullptr) return false;
    UsePosition* other_pos = other->first_pos();
    if (other_pos == nullptr) return true;
    return pos->pos() < other_pos->pos();
  }
  return start < other_start;
}


void LiveRange::SetUseHints(int register_index) {
  for (UsePosition* pos = first_pos(); pos != nullptr; pos = pos->next()) {
    if (!pos->HasOperand()) continue;
    switch (pos->type()) {
      case UsePositionType::kRequiresSlot:
        break;
      case UsePositionType::kRequiresRegister:
      case UsePositionType::kRegisterOrSlot:
      case UsePositionType::kRegisterOrSlotOrConstant:
        pos->set_assigned_register(register_index);
        break;
    }
  }
}


bool LiveRange::CanCover(LifetimePosition position) const {
  if (IsEmpty()) return false;
  return Start() <= position && position < End();
}


bool LiveRange::Covers(LifetimePosition position) const {
  if (!CanCover(position)) return false;
  UseInterval* start_search = FirstSearchIntervalForPosition(position);
  for (UseInterval* interval = start_search; interval != nullptr;
       interval = interval->next()) {
    DCHECK(interval->next() == nullptr ||
           interval->next()->start() >= interval->start());
    AdvanceLastProcessedMarker(interval, position);
    if (interval->Contains(position)) return true;
    if (interval->start() > position) return false;
  }
  return false;
}


LifetimePosition LiveRange::FirstIntersection(LiveRange* other) const {
  UseInterval* b = other->first_interval();
  if (b == nullptr) return LifetimePosition::Invalid();
  LifetimePosition advance_last_processed_up_to = b->start();
  UseInterval* a = FirstSearchIntervalForPosition(b->start());
  while (a != nullptr && b != nullptr) {
    if (a->start() > other->End()) break;
    if (b->start() > End()) break;
    LifetimePosition cur_intersection = a->Intersect(b);
    if (cur_intersection.IsValid()) {
      return cur_intersection;
    }
    if (a->start() < b->start()) {
      a = a->next();
      if (a == nullptr || a->start() > other->End()) break;
      AdvanceLastProcessedMarker(a, advance_last_processed_up_to);
    } else {
      b = b->next();
    }
  }
  return LifetimePosition::Invalid();
}

void LiveRange::Print(const RegisterConfiguration* config,
                      bool with_children) const {
  StdoutStream os;
  PrintableLiveRange wrapper;
  wrapper.register_configuration_ = config;
  for (const LiveRange* i = this; i != nullptr; i = i->next()) {
    wrapper.range_ = i;
    os << wrapper << std::endl;
    if (!with_children) break;
  }
}


void LiveRange::Print(bool with_children) const {
  Print(RegisterConfiguration::Default(), with_children);
}


struct TopLevelLiveRange::SpillMoveInsertionList : ZoneObject {
  SpillMoveInsertionList(int gap_index, InstructionOperand* operand,
                         SpillMoveInsertionList* next)
      : gap_index(gap_index), operand(operand), next(next) {}
  const int gap_index;
  InstructionOperand* const operand;
  SpillMoveInsertionList* const next;
};


TopLevelLiveRange::TopLevelLiveRange(int vreg, MachineRepresentation rep)
    : LiveRange(0, rep, this),
      vreg_(vreg),
      last_child_id_(0),
      splintered_from_(nullptr),
      spill_operand_(nullptr),
      spill_move_insertion_locations_(nullptr),
      spilled_in_deferred_blocks_(false),
      spill_start_index_(kMaxInt),
      last_pos_(nullptr),
      splinter_(nullptr),
      has_preassigned_slot_(false) {
  bits_ |= SpillTypeField::encode(SpillType::kNoSpillType);
}


#if DEBUG
int TopLevelLiveRange::debug_virt_reg() const {
  return IsSplinter() ? splintered_from()->vreg() : vreg();
}
#endif


void TopLevelLiveRange::RecordSpillLocation(Zone* zone, int gap_index,
                                            InstructionOperand* operand) {
  DCHECK(HasNoSpillType());
  spill_move_insertion_locations_ = new (zone) SpillMoveInsertionList(
      gap_index, operand, spill_move_insertion_locations_);
}

void TopLevelLiveRange::CommitSpillMoves(InstructionSequence* sequence,
                                         const InstructionOperand& op,
                                         bool might_be_duplicated) {
  DCHECK_IMPLIES(op.IsConstant(), GetSpillMoveInsertionLocations() == nullptr);
  Zone* zone = sequence->zone();

  for (SpillMoveInsertionList* to_spill = GetSpillMoveInsertionLocations();
       to_spill != nullptr; to_spill = to_spill->next) {
    Instruction* instr = sequence->InstructionAt(to_spill->gap_index);
    ParallelMove* move =
        instr->GetOrCreateParallelMove(Instruction::START, zone);
    // Skip insertion if it's possible that the move exists already as a
    // constraint move from a fixed output register to a slot.
    if (might_be_duplicated || has_preassigned_slot()) {
      bool found = false;
      for (MoveOperands* move_op : *move) {
        if (move_op->IsEliminated()) continue;
        if (move_op->source().Equals(*to_spill->operand) &&
            move_op->destination().Equals(op)) {
          found = true;
          if (has_preassigned_slot()) move_op->Eliminate();
          break;
        }
      }
      if (found) continue;
    }
    if (!has_preassigned_slot()) {
      move->AddMove(*to_spill->operand, op);
    }
  }
}


void TopLevelLiveRange::SetSpillOperand(InstructionOperand* operand) {
  DCHECK(HasNoSpillType());
  DCHECK(!operand->IsUnallocated() && !operand->IsImmediate());
  set_spill_type(SpillType::kSpillOperand);
  spill_operand_ = operand;
}


void TopLevelLiveRange::SetSpillRange(SpillRange* spill_range) {
  DCHECK(!HasSpillOperand());
  DCHECK(spill_range);
  spill_range_ = spill_range;
}


AllocatedOperand TopLevelLiveRange::GetSpillRangeOperand() const {
  SpillRange* spill_range = GetSpillRange();
  int index = spill_range->assigned_slot();
  return AllocatedOperand(LocationOperand::STACK_SLOT, representation(), index);
}


void TopLevelLiveRange::Splinter(LifetimePosition start, LifetimePosition end,
                                 Zone* zone) {
  DCHECK(start != Start() || end != End());
  DCHECK(start < end);

  TopLevelLiveRange splinter_temp(-1, representation());
  UsePosition* last_in_splinter = nullptr;
  // Live ranges defined in deferred blocks stay in deferred blocks, so we
  // don't need to splinter them. That means that start should always be
  // after the beginning of the range.
  DCHECK(start > Start());

  if (end >= End()) {
    DCHECK(start > Start());
    DetachAt(start, &splinter_temp, zone, ConnectHints);
    next_ = nullptr;
  } else {
    DCHECK(start < End() && Start() < end);

    const int kInvalidId = std::numeric_limits<int>::max();

    UsePosition* last = DetachAt(start, &splinter_temp, zone, ConnectHints);

    LiveRange end_part(kInvalidId, this->representation(), nullptr);
    // The last chunk exits the deferred region, and we don't want to connect
    // hints here, because the non-deferred region shouldn't be affected
    // by allocation decisions on the deferred path.
    last_in_splinter =
        splinter_temp.DetachAt(end, &end_part, zone, DoNotConnectHints);

    next_ = end_part.next_;
    last_interval_->set_next(end_part.first_interval_);
    // The next splinter will happen either at or after the current interval.
    // We can optimize DetachAt by setting current_interval_ accordingly,
    // which will then be picked up by FirstSearchIntervalForPosition.
    current_interval_ = last_interval_;
    last_interval_ = end_part.last_interval_;

    if (first_pos_ == nullptr) {
      first_pos_ = end_part.first_pos_;
    } else {
      splitting_pointer_ = last;
      if (last != nullptr) last->set_next(end_part.first_pos_);
    }
  }

  if (splinter()->IsEmpty()) {
    splinter()->first_interval_ = splinter_temp.first_interval_;
    splinter()->last_interval_ = splinter_temp.last_interval_;
  } else {
    splinter()->last_interval_->set_next(splinter_temp.first_interval_);
    splinter()->last_interval_ = splinter_temp.last_interval_;
  }
  if (splinter()->first_pos() == nullptr) {
    splinter()->first_pos_ = splinter_temp.first_pos_;
  } else {
    splinter()->last_pos_->set_next(splinter_temp.first_pos_);
  }
  if (last_in_splinter != nullptr) {
    splinter()->last_pos_ = last_in_splinter;
  } else {
    if (splinter()->first_pos() != nullptr &&
        splinter()->last_pos_ == nullptr) {
      splinter()->last_pos_ = splinter()->first_pos();
      for (UsePosition* pos = splinter()->first_pos(); pos != nullptr;
           pos = pos->next()) {
        splinter()->last_pos_ = pos;
      }
    }
  }
#if DEBUG
  Verify();
  splinter()->Verify();
#endif
}


void TopLevelLiveRange::SetSplinteredFrom(TopLevelLiveRange* splinter_parent) {
  splintered_from_ = splinter_parent;
  if (!HasSpillOperand() && splinter_parent->spill_range_ != nullptr) {
    SetSpillRange(splinter_parent->spill_range_);
  }
}


void TopLevelLiveRange::UpdateSpillRangePostMerge(TopLevelLiveRange* merged) {
  DCHECK(merged->TopLevel() == this);

  if (HasNoSpillType() && merged->HasSpillRange()) {
    set_spill_type(merged->spill_type());
    DCHECK_LT(0, GetSpillRange()->live_ranges().size());
    merged->spill_range_ = nullptr;
    merged->bits_ =
        SpillTypeField::update(merged->bits_, SpillType::kNoSpillType);
  }
}


void TopLevelLiveRange::Merge(TopLevelLiveRange* other, Zone* zone) {
  DCHECK(Start() < other->Start());
  DCHECK(other->splintered_from() == this);

  LiveRange* first = this;
  LiveRange* second = other;
  DCHECK(first->Start() < second->Start());
  while (first != nullptr && second != nullptr) {
    DCHECK(first != second);
    // Make sure the ranges are in order each time we iterate.
    if (second->Start() < first->Start()) {
      LiveRange* tmp = second;
      second = first;
      first = tmp;
      continue;
    }

    if (first->End() <= second->Start()) {
      if (first->next() == nullptr ||
          first->next()->Start() > second->Start()) {
        // First is in order before second.
        LiveRange* temp = first->next();
        first->next_ = second;
        first = temp;
      } else {
        // First is in order before its successor (or second), so advance first.
        first = first->next();
      }
      continue;
    }

    DCHECK(first->Start() < second->Start());
    // If first and second intersect, split first.
    if (first->Start() < second->End() && second->Start() < first->End()) {
      LiveRange* temp = first->SplitAt(second->Start(), zone);
      CHECK(temp != first);
      temp->set_spilled(first->spilled());
      if (!temp->spilled())
        temp->set_assigned_register(first->assigned_register());

      first->next_ = second;
      first = temp;
      continue;
    }
    DCHECK(first->End() <= second->Start());
  }

  TopLevel()->UpdateParentForAllChildren(TopLevel());
  TopLevel()->UpdateSpillRangePostMerge(other);
  TopLevel()->set_has_slot_use(TopLevel()->has_slot_use() ||
                               other->has_slot_use());

#if DEBUG
  Verify();
#endif
}


void TopLevelLiveRange::VerifyChildrenInOrder() const {
  LifetimePosition last_end = End();
  for (const LiveRange* child = this->next(); child != nullptr;
       child = child->next()) {
    DCHECK(last_end <= child->Start());
    last_end = child->End();
  }
}


void TopLevelLiveRange::Verify() const {
  VerifyChildrenInOrder();
  for (const LiveRange* child = this; child != nullptr; child = child->next()) {
    VerifyChildStructure();
  }
}


void TopLevelLiveRange::ShortenTo(LifetimePosition start) {
  TRACE("Shorten live range %d to [%d\n", vreg(), start.value());
  DCHECK_NOT_NULL(first_interval_);
  DCHECK(first_interval_->start() <= start);
  DCHECK(start < first_interval_->end());
  first_interval_->set_start(start);
}


void TopLevelLiveRange::EnsureInterval(LifetimePosition start,
                                       LifetimePosition end, Zone* zone) {
  TRACE("Ensure live range %d in interval [%d %d[\n", vreg(), start.value(),
        end.value());
  LifetimePosition new_end = end;
  while (first_interval_ != nullptr && first_interval_->start() <= end) {
    if (first_interval_->end() > end) {
      new_end = first_interval_->end();
    }
    first_interval_ = first_interval_->next();
  }

  UseInterval* new_interval = new (zone) UseInterval(start, new_end);
  new_interval->set_next(first_interval_);
  first_interval_ = new_interval;
  if (new_interval->next() == nullptr) {
    last_interval_ = new_interval;
  }
}


void TopLevelLiveRange::AddUseInterval(LifetimePosition start,
                                       LifetimePosition end, Zone* zone) {
  TRACE("Add to live range %d interval [%d %d[\n", vreg(), start.value(),
        end.value());
  if (first_interval_ == nullptr) {
    UseInterval* interval = new (zone) UseInterval(start, end);
    first_interval_ = interval;
    last_interval_ = interval;
  } else {
    if (end == first_interval_->start()) {
      first_interval_->set_start(start);
    } else if (end < first_interval_->start()) {
      UseInterval* interval = new (zone) UseInterval(start, end);
      interval->set_next(first_interval_);
      first_interval_ = interval;
    } else {
      // Order of instruction's processing (see ProcessInstructions) guarantees
      // that each new use interval either precedes, intersects with or touches
      // the last added interval.
      DCHECK(start <= first_interval_->end());
      first_interval_->set_start(Min(start, first_interval_->start()));
      first_interval_->set_end(Max(end, first_interval_->end()));
    }
  }
}


void TopLevelLiveRange::AddUsePosition(UsePosition* use_pos) {
  LifetimePosition pos = use_pos->pos();
  TRACE("Add to live range %d use position %d\n", vreg(), pos.value());
  UsePosition* prev_hint = nullptr;
  UsePosition* prev = nullptr;
  UsePosition* current = first_pos_;
  while (current != nullptr && current->pos() < pos) {
    prev_hint = current->HasHint() ? current : prev_hint;
    prev = current;
    current = current->next();
  }

  if (prev == nullptr) {
    use_pos->set_next(first_pos_);
    first_pos_ = use_pos;
  } else {
    use_pos->set_next(prev->next());
    prev->set_next(use_pos);
  }

  if (prev_hint == nullptr && use_pos->HasHint()) {
    current_hint_position_ = use_pos;
  }
}


static bool AreUseIntervalsIntersecting(UseInterval* interval1,
                                        UseInterval* interval2) {
  while (interval1 != nullptr && interval2 != nullptr) {
    if (interval1->start() < interval2->start()) {
      if (interval1->end() > interval2->start()) {
        return true;
      }
      interval1 = interval1->next();
    } else {
      if (interval2->end() > interval1->start()) {
        return true;
      }
      interval2 = interval2->next();
    }
  }
  return false;
}


std::ostream& operator<<(std::ostream& os,
                         const PrintableLiveRange& printable_range) {
  const LiveRange* range = printable_range.range_;
  os << "Range: " << range->TopLevel()->vreg() << ":" << range->relative_id()
     << " ";
  if (range->TopLevel()->is_phi()) os << "phi ";
  if (range->TopLevel()->is_non_loop_phi()) os << "nlphi ";

  os << "{" << std::endl;
  UseInterval* interval = range->first_interval();
  UsePosition* use_pos = range->first_pos();
  PrintableInstructionOperand pio;
  pio.register_configuration_ = printable_range.register_configuration_;
  while (use_pos != nullptr) {
    if (use_pos->HasOperand()) {
      pio.op_ = *use_pos->operand();
      os << pio << use_pos->pos() << " ";
    }
    use_pos = use_pos->next();
  }
  os << std::endl;

  while (interval != nullptr) {
    os << '[' << interval->start() << ", " << interval->end() << ')'
       << std::endl;
    interval = interval->next();
  }
  os << "}";
  return os;
}

SpillRange::SpillRange(TopLevelLiveRange* parent, Zone* zone)
    : live_ranges_(zone),
      assigned_slot_(kUnassignedSlot),
      byte_width_(GetByteWidth(parent->representation())) {
  // Spill ranges are created for top level, non-splintered ranges. This is so
  // that, when merging decisions are made, we consider the full extent of the
  // virtual register, and avoid clobbering it.
  DCHECK(!parent->IsSplinter());
  UseInterval* result = nullptr;
  UseInterval* node = nullptr;
  // Copy the intervals for all ranges.
  for (LiveRange* range = parent; range != nullptr; range = range->next()) {
    UseInterval* src = range->first_interval();
    while (src != nullptr) {
      UseInterval* new_node = new (zone) UseInterval(src->start(), src->end());
      if (result == nullptr) {
        result = new_node;
      } else {
        node->set_next(new_node);
      }
      node = new_node;
      src = src->next();
    }
  }
  use_interval_ = result;
  live_ranges().push_back(parent);
  end_position_ = node->end();
  parent->SetSpillRange(this);
}

bool SpillRange::IsIntersectingWith(SpillRange* other) const {
  if (this->use_interval_ == nullptr || other->use_interval_ == nullptr ||
      this->End() <= other->use_interval_->start() ||
      other->End() <= this->use_interval_->start()) {
    return false;
  }
  return AreUseIntervalsIntersecting(use_interval_, other->use_interval_);
}


bool SpillRange::TryMerge(SpillRange* other) {
  if (HasSlot() || other->HasSlot()) return false;
  if (byte_width() != other->byte_width() || IsIntersectingWith(other))
    return false;

  LifetimePosition max = LifetimePosition::MaxPosition();
  if (End() < other->End() && other->End() != max) {
    end_position_ = other->End();
  }
  other->end_position_ = max;

  MergeDisjointIntervals(other->use_interval_);
  other->use_interval_ = nullptr;

  for (TopLevelLiveRange* range : other->live_ranges()) {
    DCHECK(range->GetSpillRange() == other);
    range->SetSpillRange(this);
  }

  live_ranges().insert(live_ranges().end(), other->live_ranges().begin(),
                       other->live_ranges().end());
  other->live_ranges().clear();

  return true;
}


void SpillRange::MergeDisjointIntervals(UseInterval* other) {
  UseInterval* tail = nullptr;
  UseInterval* current = use_interval_;
  while (other != nullptr) {
    // Make sure the 'current' list starts first
    if (current == nullptr || current->start() > other->start()) {
      std::swap(current, other);
    }
    // Check disjointness
    DCHECK(other == nullptr || current->end() <= other->start());
    // Append the 'current' node to the result accumulator and move forward
    if (tail == nullptr) {
      use_interval_ = current;
    } else {
      tail->set_next(current);
    }
    tail = current;
    current = current->next();
  }
  // Other list is empty => we are done
}


void SpillRange::Print() const {
  StdoutStream os;
  os << "{" << std::endl;
  for (TopLevelLiveRange* range : live_ranges()) {
    os << range->vreg() << " ";
  }
  os << std::endl;

  for (UseInterval* i = interval(); i != nullptr; i = i->next()) {
    os << '[' << i->start() << ", " << i->end() << ')' << std::endl;
  }
  os << "}" << std::endl;
}


RegisterAllocationData::PhiMapValue::PhiMapValue(PhiInstruction* phi,
                                                 const InstructionBlock* block,
                                                 Zone* zone)
    : phi_(phi),
      block_(block),
      incoming_operands_(zone),
      assigned_register_(kUnassignedRegister) {
  incoming_operands_.reserve(phi->operands().size());
}


void RegisterAllocationData::PhiMapValue::AddOperand(
    InstructionOperand* operand) {
  incoming_operands_.push_back(operand);
}


void RegisterAllocationData::PhiMapValue::CommitAssignment(
    const InstructionOperand& assigned) {
  for (InstructionOperand* operand : incoming_operands_) {
    InstructionOperand::ReplaceWith(operand, &assigned);
  }
}

RegisterAllocationData::RegisterAllocationData(
    const RegisterConfiguration* config, Zone* zone, Frame* frame,
    InstructionSequence* code, const char* debug_name)
    : allocation_zone_(zone),
      frame_(frame),
      code_(code),
      debug_name_(debug_name),
      config_(config),
      phi_map_(allocation_zone()),
      live_in_sets_(code->InstructionBlockCount(), nullptr, allocation_zone()),
      live_out_sets_(code->InstructionBlockCount(), nullptr, allocation_zone()),
      live_ranges_(code->VirtualRegisterCount() * 2, nullptr,
                   allocation_zone()),
      fixed_live_ranges_(this->config()->num_general_registers(), nullptr,
                         allocation_zone()),
      fixed_float_live_ranges_(allocation_zone()),
      fixed_double_live_ranges_(this->config()->num_double_registers(), nullptr,
                                allocation_zone()),
      fixed_simd128_live_ranges_(allocation_zone()),
      spill_ranges_(code->VirtualRegisterCount(), nullptr, allocation_zone()),
      delayed_references_(allocation_zone()),
      assigned_registers_(nullptr),
      assigned_double_registers_(nullptr),
      virtual_register_count_(code->VirtualRegisterCount()),
      preassigned_slot_ranges_(zone) {
  if (!kSimpleFPAliasing) {
    fixed_float_live_ranges_.resize(this->config()->num_float_registers(),
                                    nullptr);
    fixed_simd128_live_ranges_.resize(this->config()->num_simd128_registers(),
                                      nullptr);
  }

  assigned_registers_ = new (code_zone())
      BitVector(this->config()->num_general_registers(), code_zone());
  assigned_double_registers_ = new (code_zone())
      BitVector(this->config()->num_double_registers(), code_zone());
  this->frame()->SetAllocatedRegisters(assigned_registers_);
  this->frame()->SetAllocatedDoubleRegisters(assigned_double_registers_);
}


MoveOperands* RegisterAllocationData::AddGapMove(
    int index, Instruction::GapPosition position,
    const InstructionOperand& from, const InstructionOperand& to) {
  Instruction* instr = code()->InstructionAt(index);
  ParallelMove* moves = instr->GetOrCreateParallelMove(position, code_zone());
  return moves->AddMove(from, to);
}


MachineRepresentation RegisterAllocationData::RepresentationFor(
    int virtual_register) {
  DCHECK_LT(virtual_register, code()->VirtualRegisterCount());
  return code()->GetRepresentation(virtual_register);
}


TopLevelLiveRange* RegisterAllocationData::GetOrCreateLiveRangeFor(int index) {
  if (index >= static_cast<int>(live_ranges().size())) {
    live_ranges().resize(index + 1, nullptr);
  }
  TopLevelLiveRange* result = live_ranges()[index];
  if (result == nullptr) {
    result = NewLiveRange(index, RepresentationFor(index));
    live_ranges()[index] = result;
  }
  return result;
}


TopLevelLiveRange* RegisterAllocationData::NewLiveRange(
    int index, MachineRepresentation rep) {
  return new (allocation_zone()) TopLevelLiveRange(index, rep);
}


int RegisterAllocationData::GetNextLiveRangeId() {
  int vreg = virtual_register_count_++;
  if (vreg >= static_cast<int>(live_ranges().size())) {
    live_ranges().resize(vreg + 1, nullptr);
  }
  return vreg;
}


TopLevelLiveRange* RegisterAllocationData::NextLiveRange(
    MachineRepresentation rep) {
  int vreg = GetNextLiveRangeId();
  TopLevelLiveRange* ret = NewLiveRange(vreg, rep);
  return ret;
}


RegisterAllocationData::PhiMapValue* RegisterAllocationData::InitializePhiMap(
    const InstructionBlock* block, PhiInstruction* phi) {
  RegisterAllocationData::PhiMapValue* map_value = new (allocation_zone())
      RegisterAllocationData::PhiMapValue(phi, block, allocation_zone());
  auto res =
      phi_map_.insert(std::make_pair(phi->virtual_register(), map_value));
  DCHECK(res.second);
  USE(res);
  return map_value;
}


RegisterAllocationData::PhiMapValue* RegisterAllocationData::GetPhiMapValueFor(
    int virtual_register) {
  auto it = phi_map_.find(virtual_register);
  DCHECK(it != phi_map_.end());
  return it->second;
}


RegisterAllocationData::PhiMapValue* RegisterAllocationData::GetPhiMapValueFor(
    TopLevelLiveRange* top_range) {
  return GetPhiMapValueFor(top_range->vreg());
}


bool RegisterAllocationData::ExistsUseWithoutDefinition() {
  bool found = false;
  BitVector::Iterator iterator(live_in_sets()[0]);
  while (!iterator.Done()) {
    found = true;
    int operand_index = iterator.Current();
    PrintF("Register allocator error: live v%d reached first block.\n",
           operand_index);
    LiveRange* range = GetOrCreateLiveRangeFor(operand_index);
    PrintF("  (first use is at %d)\n", range->first_pos()->pos().value());
    if (debug_name() == nullptr) {
      PrintF("\n");
    } else {
      PrintF("  (function: %s)\n", debug_name());
    }
    iterator.Advance();
  }
  return found;
}


// If a range is defined in a deferred block, we can expect all the range
// to only cover positions in deferred blocks. Otherwise, a block on the
// hot path would be dominated by a deferred block, meaning it is unreachable
// without passing through the deferred block, which is contradictory.
// In particular, when such a range contributes a result back on the hot
// path, it will be as one of the inputs of a phi. In that case, the value
// will be transferred via a move in the Gap::END's of the last instruction
// of a deferred block.
bool RegisterAllocationData::RangesDefinedInDeferredStayInDeferred() {
  const size_t live_ranges_size = live_ranges().size();
  for (const TopLevelLiveRange* range : live_ranges()) {
    CHECK_EQ(live_ranges_size,
             live_ranges().size());  // TODO(neis): crbug.com/831822
    if (range == nullptr || range->IsEmpty() ||
        !code()
             ->GetInstructionBlock(range->Start().ToInstructionIndex())
             ->IsDeferred()) {
      continue;
    }
    for (const UseInterval* i = range->first_interval(); i != nullptr;
         i = i->next()) {
      int first = i->FirstGapIndex();
      int last = i->LastGapIndex();
      for (int instr = first; instr <= last;) {
        const InstructionBlock* block = code()->GetInstructionBlock(instr);
        if (!block->IsDeferred()) return false;
        instr = block->last_instruction_index() + 1;
      }
    }
  }
  return true;
}

SpillRange* RegisterAllocationData::AssignSpillRangeToLiveRange(
    TopLevelLiveRange* range) {
  DCHECK(!range->HasSpillOperand());

  SpillRange* spill_range = range->GetAllocatedSpillRange();
  if (spill_range == nullptr) {
    DCHECK(!range->IsSplinter());
    spill_range = new (allocation_zone()) SpillRange(range, allocation_zone());
  }
  range->set_spill_type(TopLevelLiveRange::SpillType::kSpillRange);

  int spill_range_index =
      range->IsSplinter() ? range->splintered_from()->vreg() : range->vreg();

  spill_ranges()[spill_range_index] = spill_range;

  return spill_range;
}


SpillRange* RegisterAllocationData::CreateSpillRangeForLiveRange(
    TopLevelLiveRange* range) {
  DCHECK(!range->HasSpillOperand());
  DCHECK(!range->IsSplinter());
  SpillRange* spill_range =
      new (allocation_zone()) SpillRange(range, allocation_zone());
  return spill_range;
}

void RegisterAllocationData::MarkAllocated(MachineRepresentation rep,
                                           int index) {
  switch (rep) {
    case MachineRepresentation::kFloat32:
    case MachineRepresentation::kSimd128:
      if (kSimpleFPAliasing) {
        assigned_double_registers_->Add(index);
      } else {
        int alias_base_index = -1;
        int aliases = config()->GetAliases(
            rep, index, MachineRepresentation::kFloat64, &alias_base_index);
        DCHECK(aliases > 0 || (aliases == 0 && alias_base_index == -1));
        while (aliases--) {
          int aliased_reg = alias_base_index + aliases;
          assigned_double_registers_->Add(aliased_reg);
        }
      }
      break;
    case MachineRepresentation::kFloat64:
      assigned_double_registers_->Add(index);
      break;
    default:
      DCHECK(!IsFloatingPoint(rep));
      assigned_registers_->Add(index);
      break;
  }
}

bool RegisterAllocationData::IsBlockBoundary(LifetimePosition pos) const {
  return pos.IsFullStart() &&
         code()->GetInstructionBlock(pos.ToInstructionIndex())->code_start() ==
             pos.ToInstructionIndex();
}


ConstraintBuilder::ConstraintBuilder(RegisterAllocationData* data)
    : data_(data) {}


InstructionOperand* ConstraintBuilder::AllocateFixed(
    UnallocatedOperand* operand, int pos, bool is_tagged) {
  TRACE("Allocating fixed reg for op %d\n", operand->virtual_register());
  DCHECK(operand->HasFixedPolicy());
  InstructionOperand allocated;
  MachineRepresentation rep = InstructionSequence::DefaultRepresentation();
  int virtual_register = operand->virtual_register();
  if (virtual_register != InstructionOperand::kInvalidVirtualRegister) {
    rep = data()->RepresentationFor(virtual_register);
  }
  if (operand->HasFixedSlotPolicy()) {
    allocated = AllocatedOperand(AllocatedOperand::STACK_SLOT, rep,
                                 operand->fixed_slot_index());
  } else if (operand->HasFixedRegisterPolicy()) {
    DCHECK(!IsFloatingPoint(rep));
    DCHECK(data()->config()->IsAllocatableGeneralCode(
        operand->fixed_register_index()));
    allocated = AllocatedOperand(AllocatedOperand::REGISTER, rep,
                                 operand->fixed_register_index());
  } else if (operand->HasFixedFPRegisterPolicy()) {
    DCHECK(IsFloatingPoint(rep));
    DCHECK_NE(InstructionOperand::kInvalidVirtualRegister, virtual_register);
    allocated = AllocatedOperand(AllocatedOperand::REGISTER, rep,
                                 operand->fixed_register_index());
  } else {
    UNREACHABLE();
  }
  InstructionOperand::ReplaceWith(operand, &allocated);
  if (is_tagged) {
    TRACE("Fixed reg is tagged at %d\n", pos);
    Instruction* instr = code()->InstructionAt(pos);
    if (instr->HasReferenceMap()) {
      instr->reference_map()->RecordReference(*AllocatedOperand::cast(operand));
    }
  }
  return operand;
}


void ConstraintBuilder::MeetRegisterConstraints() {
  for (InstructionBlock* block : code()->instruction_blocks()) {
    MeetRegisterConstraints(block);
  }
}


void ConstraintBuilder::MeetRegisterConstraints(const InstructionBlock* block) {
  int start = block->first_instruction_index();
  int end = block->last_instruction_index();
  DCHECK_NE(-1, start);
  for (int i = start; i <= end; ++i) {
    MeetConstraintsBefore(i);
    if (i != end) MeetConstraintsAfter(i);
  }
  // Meet register constraints for the instruction in the end.
  MeetRegisterConstraintsForLastInstructionInBlock(block);
}


void ConstraintBuilder::MeetRegisterConstraintsForLastInstructionInBlock(
    const InstructionBlock* block) {
  int end = block->last_instruction_index();
  Instruction* last_instruction = code()->InstructionAt(end);
  for (size_t i = 0; i < last_instruction->OutputCount(); i++) {
    InstructionOperand* output_operand = last_instruction->OutputAt(i);
    DCHECK(!output_operand->IsConstant());
    UnallocatedOperand* output = UnallocatedOperand::cast(output_operand);
    int output_vreg = output->virtual_register();
    TopLevelLiveRange* range = data()->GetOrCreateLiveRangeFor(output_vreg);
    bool assigned = false;
    if (output->HasFixedPolicy()) {
      AllocateFixed(output, -1, false);
      // This value is produced on the stack, we never need to spill it.
      if (output->IsStackSlot()) {
        DCHECK(LocationOperand::cast(output)->index() <
               data()->frame()->GetSpillSlotCount());
        range->SetSpillOperand(LocationOperand::cast(output));
        range->SetSpillStartIndex(end);
        assigned = true;
      }

      for (const RpoNumber& succ : block->successors()) {
        const InstructionBlock* successor = code()->InstructionBlockAt(succ);
        DCHECK_EQ(1, successor->PredecessorCount());
        int gap_index = successor->first_instruction_index();
        // Create an unconstrained operand for the same virtual register
        // and insert a gap move from the fixed output to the operand.
        UnallocatedOperand output_copy(UnallocatedOperand::REGISTER_OR_SLOT,
                                       output_vreg);
        data()->AddGapMove(gap_index, Instruction::START, *output, output_copy);
      }
    }

    if (!assigned) {
      for (const RpoNumber& succ : block->successors()) {
        const InstructionBlock* successor = code()->InstructionBlockAt(succ);
        DCHECK_EQ(1, successor->PredecessorCount());
        int gap_index = successor->first_instruction_index();
        range->RecordSpillLocation(allocation_zone(), gap_index, output);
        range->SetSpillStartIndex(gap_index);
      }
    }
  }
}


void ConstraintBuilder::MeetConstraintsAfter(int instr_index) {
  Instruction* first = code()->InstructionAt(instr_index);
  // Handle fixed temporaries.
  for (size_t i = 0; i < first->TempCount(); i++) {
    UnallocatedOperand* temp = UnallocatedOperand::cast(first->TempAt(i));
    if (temp->HasFixedPolicy()) AllocateFixed(temp, instr_index, false);
  }
  // Handle constant/fixed output operands.
  for (size_t i = 0; i < first->OutputCount(); i++) {
    InstructionOperand* output = first->OutputAt(i);
    if (output->IsConstant()) {
      int output_vreg = ConstantOperand::cast(output)->virtual_register();
      TopLevelLiveRange* range = data()->GetOrCreateLiveRangeFor(output_vreg);
      range->SetSpillStartIndex(instr_index + 1);
      range->SetSpillOperand(output);
      continue;
    }
    UnallocatedOperand* first_output = UnallocatedOperand::cast(output);
    TopLevelLiveRange* range =
        data()->GetOrCreateLiveRangeFor(first_output->virtual_register());
    bool assigned = false;
    if (first_output->HasFixedPolicy()) {
      int output_vreg = first_output->virtual_register();
      UnallocatedOperand output_copy(UnallocatedOperand::REGISTER_OR_SLOT,
                                     output_vreg);
      bool is_tagged = code()->IsReference(output_vreg);
      if (first_output->HasSecondaryStorage()) {
        range->MarkHasPreassignedSlot();
        data()->preassigned_slot_ranges().push_back(
            std::make_pair(range, first_output->GetSecondaryStorage()));
      }
      AllocateFixed(first_output, instr_index, is_tagged);

      // This value is produced on the stack, we never need to spill it.
      if (first_output->IsStackSlot()) {
        DCHECK(LocationOperand::cast(first_output)->index() <
               data()->frame()->GetTotalFrameSlotCount());
        range->SetSpillOperand(LocationOperand::cast(first_output));
        range->SetSpillStartIndex(instr_index + 1);
        assigned = true;
      }
      data()->AddGapMove(instr_index + 1, Instruction::START, *first_output,
                         output_copy);
    }
    // Make sure we add a gap move for spilling (if we have not done
    // so already).
    if (!assigned) {
      range->RecordSpillLocation(allocation_zone(), instr_index + 1,
                                 first_output);
      range->SetSpillStartIndex(instr_index + 1);
    }
  }
}


void ConstraintBuilder::MeetConstraintsBefore(int instr_index) {
  Instruction* second = code()->InstructionAt(instr_index);
  // Handle fixed input operands of second instruction.
  for (size_t i = 0; i < second->InputCount(); i++) {
    InstructionOperand* input = second->InputAt(i);
    if (input->IsImmediate() || input->IsExplicit()) {
      continue;  // Ignore immediates and explicitly reserved registers.
    }
    UnallocatedOperand* cur_input = UnallocatedOperand::cast(input);
    if (cur_input->HasFixedPolicy()) {
      int input_vreg = cur_input->virtual_register();
      UnallocatedOperand input_copy(UnallocatedOperand::REGISTER_OR_SLOT,
                                    input_vreg);
      bool is_tagged = code()->IsReference(input_vreg);
      AllocateFixed(cur_input, instr_index, is_tagged);
      data()->AddGapMove(instr_index, Instruction::END, input_copy, *cur_input);
    }
  }
  // Handle "output same as input" for second instruction.
  for (size_t i = 0; i < second->OutputCount(); i++) {
    InstructionOperand* output = second->OutputAt(i);
    if (!output->IsUnallocated()) continue;
    UnallocatedOperand* second_output = UnallocatedOperand::cast(output);
    if (!second_output->HasSameAsInputPolicy()) continue;
    DCHECK_EQ(0, i);  // Only valid for first output.
    UnallocatedOperand* cur_input =
        UnallocatedOperand::cast(second->InputAt(0));
    int output_vreg = second_output->virtual_register();
    int input_vreg = cur_input->virtual_register();
    UnallocatedOperand input_copy(UnallocatedOperand::REGISTER_OR_SLOT,
                                  input_vreg);
    *cur_input =
        UnallocatedOperand(*cur_input, second_output->virtual_register());
    MoveOperands* gap_move = data()->AddGapMove(instr_index, Instruction::END,
                                                input_copy, *cur_input);
    if (code()->IsReference(input_vreg) && !code()->IsReference(output_vreg)) {
      if (second->HasReferenceMap()) {
        RegisterAllocationData::DelayedReference delayed_reference = {
            second->reference_map(), &gap_move->source()};
        data()->delayed_references().push_back(delayed_reference);
      }
    } else if (!code()->IsReference(input_vreg) &&
               code()->IsReference(output_vreg)) {
      // The input is assumed to immediately have a tagged representation,
      // before the pointer map can be used. I.e. the pointer map at the
      // instruction will include the output operand (whose value at the
      // beginning of the instruction is equal to the input operand). If
      // this is not desired, then the pointer map at this instruction needs
      // to be adjusted manually.
    }
  }
}


void ConstraintBuilder::ResolvePhis() {
  // Process the blocks in reverse order.
  for (InstructionBlock* block : base::Reversed(code()->instruction_blocks())) {
    ResolvePhis(block);
  }
}


void ConstraintBuilder::ResolvePhis(const InstructionBlock* block) {
  for (PhiInstruction* phi : block->phis()) {
    int phi_vreg = phi->virtual_register();
    RegisterAllocationData::PhiMapValue* map_value =
        data()->InitializePhiMap(block, phi);
    InstructionOperand& output = phi->output();
    // Map the destination operands, so the commitment phase can find them.
    for (size_t i = 0; i < phi->operands().size(); ++i) {
      InstructionBlock* cur_block =
          code()->InstructionBlockAt(block->predecessors()[i]);
      UnallocatedOperand input(UnallocatedOperand::REGISTER_OR_SLOT,
                               phi->operands()[i]);
      MoveOperands* move = data()->AddGapMove(
          cur_block->last_instruction_index(), Instruction::END, input, output);
      map_value->AddOperand(&move->destination());
      DCHECK(!code()
                  ->InstructionAt(cur_block->last_instruction_index())
                  ->HasReferenceMap());
    }
    TopLevelLiveRange* live_range = data()->GetOrCreateLiveRangeFor(phi_vreg);
    int gap_index = block->first_instruction_index();
    live_range->RecordSpillLocation(allocation_zone(), gap_index, &output);
    live_range->SetSpillStartIndex(gap_index);
    // We use the phi-ness of some nodes in some later heuristics.
    live_range->set_is_phi(true);
    live_range->set_is_non_loop_phi(!block->IsLoopHeader());
  }
}


LiveRangeBuilder::LiveRangeBuilder(RegisterAllocationData* data,
                                   Zone* local_zone)
    : data_(data), phi_hints_(local_zone) {}


BitVector* LiveRangeBuilder::ComputeLiveOut(const InstructionBlock* block,
                                            RegisterAllocationData* data) {
  size_t block_index = block->rpo_number().ToSize();
  BitVector* live_out = data->live_out_sets()[block_index];
  if (live_out == nullptr) {
    // Compute live out for the given block, except not including backward
    // successor edges.
    Zone* zone = data->allocation_zone();
    const InstructionSequence* code = data->code();

    live_out = new (zone) BitVector(code->VirtualRegisterCount(), zone);

    // Process all successor blocks.
    for (const RpoNumber& succ : block->successors()) {
      // Add values live on entry to the successor.
      if (succ <= block->rpo_number()) continue;
      BitVector* live_in = data->live_in_sets()[succ.ToSize()];
      if (live_in != nullptr) live_out->Union(*live_in);

      // All phi input operands corresponding to this successor edge are live
      // out from this block.
      const InstructionBlock* successor = code->InstructionBlockAt(succ);
      size_t index = successor->PredecessorIndexOf(block->rpo_number());
      DCHECK(index < successor->PredecessorCount());
      for (PhiInstruction* phi : successor->phis()) {
        live_out->Add(phi->operands()[index]);
      }
    }
    data->live_out_sets()[block_index] = live_out;
  }
  return live_out;
}


void LiveRangeBuilder::AddInitialIntervals(const InstructionBlock* block,
                                           BitVector* live_out) {
  // Add an interval that includes the entire block to the live range for
  // each live_out value.
  LifetimePosition start = LifetimePosition::GapFromInstructionIndex(
      block->first_instruction_index());
  LifetimePosition end = LifetimePosition::InstructionFromInstructionIndex(
                             block->last_instruction_index())
                             .NextStart();
  BitVector::Iterator iterator(live_out);
  while (!iterator.Done()) {
    int operand_index = iterator.Current();
    TopLevelLiveRange* range = data()->GetOrCreateLiveRangeFor(operand_index);
    range->AddUseInterval(start, end, allocation_zone());
    iterator.Advance();
  }
}

int LiveRangeBuilder::FixedFPLiveRangeID(int index, MachineRepresentation rep) {
  int result = -index - 1;
  switch (rep) {
    case MachineRepresentation::kSimd128:
      result -= config()->num_float_registers();
      V8_FALLTHROUGH;
    case MachineRepresentation::kFloat32:
      result -= config()->num_double_registers();
      V8_FALLTHROUGH;
    case MachineRepresentation::kFloat64:
      result -= config()->num_general_registers();
      break;
    default:
      UNREACHABLE();
      break;
  }
  return result;
}

TopLevelLiveRange* LiveRangeBuilder::FixedLiveRangeFor(int index) {
  DCHECK(index < config()->num_general_registers());
  TopLevelLiveRange* result = data()->fixed_live_ranges()[index];
  if (result == nullptr) {
    MachineRepresentation rep = InstructionSequence::DefaultRepresentation();
    result = data()->NewLiveRange(FixedLiveRangeID(index), rep);
    DCHECK(result->IsFixed());
    result->set_assigned_register(index);
    data()->MarkAllocated(rep, index);
    data()->fixed_live_ranges()[index] = result;
  }
  return result;
}

TopLevelLiveRange* LiveRangeBuilder::FixedFPLiveRangeFor(
    int index, MachineRepresentation rep) {
  int num_regs = config()->num_double_registers();
  ZoneVector<TopLevelLiveRange*>* live_ranges =
      &data()->fixed_double_live_ranges();
  if (!kSimpleFPAliasing) {
    switch (rep) {
      case MachineRepresentation::kFloat32:
        num_regs = config()->num_float_registers();
        live_ranges = &data()->fixed_float_live_ranges();
        break;
      case MachineRepresentation::kSimd128:
        num_regs = config()->num_simd128_registers();
        live_ranges = &data()->fixed_simd128_live_ranges();
        break;
      default:
        break;
    }
  }

  DCHECK(index < num_regs);
  USE(num_regs);
  TopLevelLiveRange* result = (*live_ranges)[index];
  if (result == nullptr) {
    result = data()->NewLiveRange(FixedFPLiveRangeID(index, rep), rep);
    DCHECK(result->IsFixed());
    result->set_assigned_register(index);
    data()->MarkAllocated(rep, index);
    (*live_ranges)[index] = result;
  }
  return result;
}

TopLevelLiveRange* LiveRangeBuilder::LiveRangeFor(InstructionOperand* operand) {
  if (operand->IsUnallocated()) {
    return data()->GetOrCreateLiveRangeFor(
        UnallocatedOperand::cast(operand)->virtual_register());
  } else if (operand->IsConstant()) {
    return data()->GetOrCreateLiveRangeFor(
        ConstantOperand::cast(operand)->virtual_register());
  } else if (operand->IsRegister()) {
    return FixedLiveRangeFor(
        LocationOperand::cast(operand)->GetRegister().code());
  } else if (operand->IsFPRegister()) {
    LocationOperand* op = LocationOperand::cast(operand);
    return FixedFPLiveRangeFor(op->register_code(), op->representation());
  } else {
    return nullptr;
  }
}


UsePosition* LiveRangeBuilder::NewUsePosition(LifetimePosition pos,
                                              InstructionOperand* operand,
                                              void* hint,
                                              UsePositionHintType hint_type) {
  return new (allocation_zone()) UsePosition(pos, operand, hint, hint_type);
}


UsePosition* LiveRangeBuilder::Define(LifetimePosition position,
                                      InstructionOperand* operand, void* hint,
                                      UsePositionHintType hint_type) {
  TopLevelLiveRange* range = LiveRangeFor(operand);
  if (range == nullptr) return nullptr;

  if (range->IsEmpty() || range->Start() > position) {
    // Can happen if there is a definition without use.
    range->AddUseInterval(position, position.NextStart(), allocation_zone());
    range->AddUsePosition(NewUsePosition(position.NextStart()));
  } else {
    range->ShortenTo(position);
  }
  if (!operand->IsUnallocated()) return nullptr;
  UnallocatedOperand* unalloc_operand = UnallocatedOperand::cast(operand);
  UsePosition* use_pos =
      NewUsePosition(position, unalloc_operand, hint, hint_type);
  range->AddUsePosition(use_pos);
  return use_pos;
}


UsePosition* LiveRangeBuilder::Use(LifetimePosition block_start,
                                   LifetimePosition position,
                                   InstructionOperand* operand, void* hint,
                                   UsePositionHintType hint_type) {
  TopLevelLiveRange* range = LiveRangeFor(operand);
  if (range == nullptr) return nullptr;
  UsePosition* use_pos = nullptr;
  if (operand->IsUnallocated()) {
    UnallocatedOperand* unalloc_operand = UnallocatedOperand::cast(operand);
    use_pos = NewUsePosition(position, unalloc_operand, hint, hint_type);
    range->AddUsePosition(use_pos);
  }
  range->AddUseInterval(block_start, position, allocation_zone());
  return use_pos;
}


void LiveRangeBuilder::ProcessInstructions(const InstructionBlock* block,
                                           BitVector* live) {
  int block_start = block->first_instruction_index();
  LifetimePosition block_start_position =
      LifetimePosition::GapFromInstructionIndex(block_start);
  bool fixed_float_live_ranges = false;
  bool fixed_simd128_live_ranges = false;
  if (!kSimpleFPAliasing) {
    int mask = data()->code()->representation_mask();
    fixed_float_live_ranges = (mask & kFloat32Bit) != 0;
    fixed_simd128_live_ranges = (mask & kSimd128Bit) != 0;
  }

  for (int index = block->last_instruction_index(); index >= block_start;
       index--) {
    LifetimePosition curr_position =
        LifetimePosition::InstructionFromInstructionIndex(index);
    Instruction* instr = code()->InstructionAt(index);
    DCHECK_NOT_NULL(instr);
    DCHECK(curr_position.IsInstructionPosition());
    // Process output, inputs, and temps of this instruction.
    for (size_t i = 0; i < instr->OutputCount(); i++) {
      InstructionOperand* output = instr->OutputAt(i);
      if (output->IsUnallocated()) {
        // Unsupported.
        DCHECK(!UnallocatedOperand::cast(output)->HasSlotPolicy());
        int out_vreg = UnallocatedOperand::cast(output)->virtual_register();
        live->Remove(out_vreg);
      } else if (output->IsConstant()) {
        int out_vreg = ConstantOperand::cast(output)->virtual_register();
        live->Remove(out_vreg);
      }
      if (block->IsHandler() && index == block_start && output->IsAllocated() &&
          output->IsRegister() &&
          AllocatedOperand::cast(output)->GetRegister() ==
              v8::internal::kReturnRegister0) {
        // The register defined here is blocked from gap start - it is the
        // exception value.
        // TODO(mtrofin): should we explore an explicit opcode for
        // the first instruction in the handler?
        Define(LifetimePosition::GapFromInstructionIndex(index), output);
      } else {
        Define(curr_position, output);
      }
    }

    if (instr->ClobbersRegisters()) {
      for (int i = 0; i < config()->num_allocatable_general_registers(); ++i) {
        // Create a UseInterval at this instruction for all fixed registers,
        // (including the instruction outputs). Adding another UseInterval here
        // is OK because AddUseInterval will just merge it with the existing
        // one at the end of the range.
        int code = config()->GetAllocatableGeneralCode(i);
        TopLevelLiveRange* range = FixedLiveRangeFor(code);
        range->AddUseInterval(curr_position, curr_position.End(),
                              allocation_zone());
      }
    }

    if (instr->ClobbersDoubleRegisters()) {
      for (int i = 0; i < config()->num_allocatable_double_registers(); ++i) {
        // Add a UseInterval for all DoubleRegisters. See comment above for
        // general registers.
        int code = config()->GetAllocatableDoubleCode(i);
        TopLevelLiveRange* range =
            FixedFPLiveRangeFor(code, MachineRepresentation::kFloat64);
        range->AddUseInterval(curr_position, curr_position.End(),
                              allocation_zone());
      }
      // Clobber fixed float registers on archs with non-simple aliasing.
      if (!kSimpleFPAliasing) {
        if (fixed_float_live_ranges) {
          for (int i = 0; i < config()->num_allocatable_float_registers();
               ++i) {
            // Add a UseInterval for all FloatRegisters. See comment above for
            // general registers.
            int code = config()->GetAllocatableFloatCode(i);
            TopLevelLiveRange* range =
                FixedFPLiveRangeFor(code, MachineRepresentation::kFloat32);
            range->AddUseInterval(curr_position, curr_position.End(),
                                  allocation_zone());
          }
        }
        if (fixed_simd128_live_ranges) {
          for (int i = 0; i < config()->num_allocatable_simd128_registers();
               ++i) {
            int code = config()->GetAllocatableSimd128Code(i);
            TopLevelLiveRange* range =
                FixedFPLiveRangeFor(code, MachineRepresentation::kSimd128);
            range->AddUseInterval(curr_position, curr_position.End(),
                                  allocation_zone());
          }
        }
      }
    }

    for (size_t i = 0; i < instr->InputCount(); i++) {
      InstructionOperand* input = instr->InputAt(i);
      if (input->IsImmediate() || input->IsExplicit()) {
        continue;  // Ignore immediates and explicitly reserved registers.
      }
      LifetimePosition use_pos;
      if (input->IsUnallocated() &&
          UnallocatedOperand::cast(input)->IsUsedAtStart()) {
        use_pos = curr_position;
      } else {
        use_pos = curr_position.End();
      }

      if (input->IsUnallocated()) {
        UnallocatedOperand* unalloc = UnallocatedOperand::cast(input);
        int vreg = unalloc->virtual_register();
        live->Add(vreg);
        if (unalloc->HasSlotPolicy()) {
          data()->GetOrCreateLiveRangeFor(vreg)->set_has_slot_use(true);
        }
      }
      Use(block_start_position, use_pos, input);
    }

    for (size_t i = 0; i < instr->TempCount(); i++) {
      InstructionOperand* temp = instr->TempAt(i);
      // Unsupported.
      DCHECK_IMPLIES(temp->IsUnallocated(),
                     !UnallocatedOperand::cast(temp)->HasSlotPolicy());
      if (instr->ClobbersTemps()) {
        if (temp->IsRegister()) continue;
        if (temp->IsUnallocated()) {
          UnallocatedOperand* temp_unalloc = UnallocatedOperand::cast(temp);
          if (temp_unalloc->HasFixedPolicy()) {
            continue;
          }
        }
      }
      Use(block_start_position, curr_position.End(), temp);
      Define(curr_position, temp);
    }

    // Process the moves of the instruction's gaps, making their sources live.
    const Instruction::GapPosition kPositions[] = {Instruction::END,
                                                   Instruction::START};
    curr_position = curr_position.PrevStart();
    DCHECK(curr_position.IsGapPosition());
    for (const Instruction::GapPosition& position : kPositions) {
      ParallelMove* move = instr->GetParallelMove(position);
      if (move == nullptr) continue;
      if (position == Instruction::END) {
        curr_position = curr_position.End();
      } else {
        curr_position = curr_position.Start();
      }
      for (MoveOperands* cur : *move) {
        InstructionOperand& from = cur->source();
        InstructionOperand& to = cur->destination();
        void* hint = &to;
        UsePositionHintType hint_type = UsePosition::HintTypeForOperand(to);
        UsePosition* to_use = nullptr;
        int phi_vreg = -1;
        if (to.IsUnallocated()) {
          int to_vreg = UnallocatedOperand::cast(to).virtual_register();
          TopLevelLiveRange* to_range =
              data()->GetOrCreateLiveRangeFor(to_vreg);
          if (to_range->is_phi()) {
            phi_vreg = to_vreg;
            if (to_range->is_non_loop_phi()) {
              hint = to_range->current_hint_position();
              hint_type = hint == nullptr ? UsePositionHintType::kNone
                                          : UsePositionHintType::kUsePos;
            } else {
              hint_type = UsePositionHintType::kPhi;
              hint = data()->GetPhiMapValueFor(to_vreg);
            }
          } else {
            if (live->Contains(to_vreg)) {
              to_use = Define(curr_position, &to, &from,
                              UsePosition::HintTypeForOperand(from));
              live->Remove(to_vreg);
            } else {
              cur->Eliminate();
              continue;
            }
          }
        } else {
          Define(curr_position, &to);
        }
        UsePosition* from_use =
            Use(block_start_position, curr_position, &from, hint, hint_type);
        // Mark range live.
        if (from.IsUnallocated()) {
          live->Add(UnallocatedOperand::cast(from).virtual_register());
        }
        // Resolve use position hints just created.
        if (to_use != nullptr && from_use != nullptr) {
          to_use->ResolveHint(from_use);
          from_use->ResolveHint(to_use);
        }
        DCHECK_IMPLIES(to_use != nullptr, to_use->IsResolved());
        DCHECK_IMPLIES(from_use != nullptr, from_use->IsResolved());
        // Potentially resolve phi hint.
        if (phi_vreg != -1) ResolvePhiHint(&from, from_use);
      }
    }
  }
}

void LiveRangeBuilder::ProcessPhis(const InstructionBlock* block,
                                   BitVector* live) {
  for (PhiInstruction* phi : block->phis()) {
    // The live range interval already ends at the first instruction of the
    // block.
    int phi_vreg = phi->virtual_register();
    live->Remove(phi_vreg);
    // Select a hint from a predecessor block that precedes this block in the
    // rpo order. In order of priority:
    // - Avoid hints from deferred blocks.
    // - Prefer hints from allocated (or explicit) operands.
    // - Prefer hints from empty blocks (containing just parallel moves and a
    //   jump). In these cases, if we can elide the moves, the jump threader
    //   is likely to be able to elide the jump.
    // The enforcement of hinting in rpo order is required because hint
    // resolution that happens later in the compiler pipeline visits
    // instructions in reverse rpo order, relying on the fact that phis are
    // encountered before their hints.
    InstructionOperand* hint = nullptr;
    int hint_preference = 0;

    // The cost of hinting increases with the number of predecessors. At the
    // same time, the typical benefit decreases, since this hinting only
    // optimises the execution path through one predecessor. A limit of 2 is
    // sufficient to hit the common if/else pattern.
    int predecessor_limit = 2;

    for (RpoNumber predecessor : block->predecessors()) {
      const InstructionBlock* predecessor_block =
          code()->InstructionBlockAt(predecessor);
      DCHECK_EQ(predecessor_block->rpo_number(), predecessor);

      // Only take hints from earlier rpo numbers.
      if (predecessor >= block->rpo_number()) continue;

      // Look up the predecessor instruction.
      const Instruction* predecessor_instr =
          GetLastInstruction(code(), predecessor_block);
      InstructionOperand* predecessor_hint = nullptr;
      // Phis are assigned in the END position of the last instruction in each
      // predecessor block.
      for (MoveOperands* move :
           *predecessor_instr->GetParallelMove(Instruction::END)) {
        InstructionOperand& to = move->destination();
        if (to.IsUnallocated() &&
            UnallocatedOperand::cast(to).virtual_register() == phi_vreg) {
          predecessor_hint = &move->source();
          break;
        }
      }
      DCHECK_NOT_NULL(predecessor_hint);

      // For each predecessor, generate a score according to the priorities
      // described above, and pick the best one. Flags in higher-order bits have
      // a higher priority than those in lower-order bits.
      int predecessor_hint_preference = 0;
      const int kNotDeferredBlockPreference = (1 << 2);
      const int kMoveIsAllocatedPreference = (1 << 1);
      const int kBlockIsEmptyPreference = (1 << 0);

      // - Avoid hints from deferred blocks.
      if (!predecessor_block->IsDeferred()) {
        predecessor_hint_preference |= kNotDeferredBlockPreference;
      }

      // - Prefer hints from allocated (or explicit) operands.
      //
      // Already-allocated or explicit operands are typically assigned using
      // the parallel moves on the last instruction. For example:
      //
      //      gap (v101 = [x0|R|w32]) (v100 = v101)
      //      ArchJmp
      //    ...
      //    phi: v100 = v101 v102
      //
      // We have already found the END move, so look for a matching START move
      // from an allocated (or explicit) operand.
      //
      // Note that we cannot simply look up data()->live_ranges()[vreg] here
      // because the live ranges are still being built when this function is
      // called.
      // TODO(v8): Find a way to separate hinting from live range analysis in
      // BuildLiveRanges so that we can use the O(1) live-range look-up.
      auto moves = predecessor_instr->GetParallelMove(Instruction::START);
      if (moves != nullptr) {
        for (MoveOperands* move : *moves) {
          InstructionOperand& to = move->destination();
          if (predecessor_hint->Equals(to)) {
            if (move->source().IsAllocated() || move->source().IsExplicit()) {
              predecessor_hint_preference |= kMoveIsAllocatedPreference;
            }
            break;
          }
        }
      }

      // - Prefer hints from empty blocks.
      if (predecessor_block->last_instruction_index() ==
          predecessor_block->first_instruction_index()) {
        predecessor_hint_preference |= kBlockIsEmptyPreference;
      }

      if ((hint == nullptr) ||
          (predecessor_hint_preference > hint_preference)) {
        // Take the hint from this predecessor.
        hint = predecessor_hint;
        hint_preference = predecessor_hint_preference;
      }

      if (--predecessor_limit <= 0) break;
    }
    DCHECK_NOT_NULL(hint);

    LifetimePosition block_start = LifetimePosition::GapFromInstructionIndex(
        block->first_instruction_index());
    UsePosition* use_pos = Define(block_start, &phi->output(), hint,
                                  UsePosition::HintTypeForOperand(*hint));
    MapPhiHint(hint, use_pos);
  }
}


void LiveRangeBuilder::ProcessLoopHeader(const InstructionBlock* block,
                                         BitVector* live) {
  DCHECK(block->IsLoopHeader());
  // Add a live range stretching from the first loop instruction to the last
  // for each value live on entry to the header.
  BitVector::Iterator iterator(live);
  LifetimePosition start = LifetimePosition::GapFromInstructionIndex(
      block->first_instruction_index());
  LifetimePosition end = LifetimePosition::GapFromInstructionIndex(
                             code()->LastLoopInstructionIndex(block))
                             .NextFullStart();
  while (!iterator.Done()) {
    int operand_index = iterator.Current();
    TopLevelLiveRange* range = data()->GetOrCreateLiveRangeFor(operand_index);
    range->EnsureInterval(start, end, allocation_zone());
    iterator.Advance();
  }
  // Insert all values into the live in sets of all blocks in the loop.
  for (int i = block->rpo_number().ToInt() + 1; i < block->loop_end().ToInt();
       ++i) {
    live_in_sets()[i]->Union(*live);
  }
}


void LiveRangeBuilder::BuildLiveRanges() {
  // Process the blocks in reverse order.
  for (int block_id = code()->InstructionBlockCount() - 1; block_id >= 0;
       --block_id) {
    InstructionBlock* block =
        code()->InstructionBlockAt(RpoNumber::FromInt(block_id));
    BitVector* live = ComputeLiveOut(block, data());
    // Initially consider all live_out values live for the entire block. We
    // will shorten these intervals if necessary.
    AddInitialIntervals(block, live);
    // Process the instructions in reverse order, generating and killing
    // live values.
    ProcessInstructions(block, live);
    // All phi output operands are killed by this block.
    ProcessPhis(block, live);
    // Now live is live_in for this block except not including values live
    // out on backward successor edges.
    if (block->IsLoopHeader()) ProcessLoopHeader(block, live);
    live_in_sets()[block_id] = live;
  }
  // Postprocess the ranges.
  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (range == nullptr) continue;
    // Give slots to all ranges with a non fixed slot use.
    if (range->has_slot_use() && range->HasNoSpillType()) {
      data()->AssignSpillRangeToLiveRange(range);
    }
    // TODO(bmeurer): This is a horrible hack to make sure that for constant
    // live ranges, every use requires the constant to be in a register.
    // Without this hack, all uses with "any" policy would get the constant
    // operand assigned.
    if (range->HasSpillOperand() && range->GetSpillOperand()->IsConstant()) {
      for (UsePosition* pos = range->first_pos(); pos != nullptr;
           pos = pos->next()) {
        if (pos->type() == UsePositionType::kRequiresSlot ||
            pos->type() == UsePositionType::kRegisterOrSlotOrConstant) {
          continue;
        }
        UsePositionType new_type = UsePositionType::kRegisterOrSlot;
        // Can't mark phis as needing a register.
        if (!pos->pos().IsGapPosition()) {
          new_type = UsePositionType::kRequiresRegister;
        }
        pos->set_type(new_type, true);
      }
    }
  }
  for (auto preassigned : data()->preassigned_slot_ranges()) {
    TopLevelLiveRange* range = preassigned.first;
    int slot_id = preassigned.second;
    SpillRange* spill = range->HasSpillRange()
                            ? range->GetSpillRange()
                            : data()->AssignSpillRangeToLiveRange(range);
    spill->set_assigned_slot(slot_id);
  }
#ifdef DEBUG
  Verify();
#endif
}


void LiveRangeBuilder::MapPhiHint(InstructionOperand* operand,
                                  UsePosition* use_pos) {
  DCHECK(!use_pos->IsResolved());
  auto res = phi_hints_.insert(std::make_pair(operand, use_pos));
  DCHECK(res.second);
  USE(res);
}


void LiveRangeBuilder::ResolvePhiHint(InstructionOperand* operand,
                                      UsePosition* use_pos) {
  auto it = phi_hints_.find(operand);
  if (it == phi_hints_.end()) return;
  DCHECK(!it->second->IsResolved());
  it->second->ResolveHint(use_pos);
}


void LiveRangeBuilder::Verify() const {
  for (auto& hint : phi_hints_) {
    CHECK(hint.second->IsResolved());
  }
  for (const TopLevelLiveRange* current : data()->live_ranges()) {
    if (current != nullptr && !current->IsEmpty()) {
      // New LiveRanges should not be split.
      CHECK_NULL(current->next());
      // General integrity check.
      current->Verify();
      const UseInterval* first = current->first_interval();
      if (first->next() == nullptr) continue;

      // Consecutive intervals should not end and start in the same block,
      // otherwise the intervals should have been joined, because the
      // variable is live throughout that block.
      CHECK(NextIntervalStartsInDifferentBlocks(first));

      for (const UseInterval* i = first->next(); i != nullptr; i = i->next()) {
        // Except for the first interval, the other intevals must start at
        // a block boundary, otherwise data wouldn't flow to them.
        CHECK(IntervalStartsAtBlockBoundary(i));
        // The last instruction of the predecessors of the block the interval
        // starts must be covered by the range.
        CHECK(IntervalPredecessorsCoveredByRange(i, current));
        if (i->next() != nullptr) {
          // Check the consecutive intervals property, except for the last
          // interval, where it doesn't apply.
          CHECK(NextIntervalStartsInDifferentBlocks(i));
        }
      }
    }
  }
}

bool LiveRangeBuilder::IntervalStartsAtBlockBoundary(
    const UseInterval* interval) const {
  LifetimePosition start = interval->start();
  if (!start.IsFullStart()) return false;
  int instruction_index = start.ToInstructionIndex();
  const InstructionBlock* block =
      data()->code()->GetInstructionBlock(instruction_index);
  return block->first_instruction_index() == instruction_index;
}

bool LiveRangeBuilder::IntervalPredecessorsCoveredByRange(
    const UseInterval* interval, const TopLevelLiveRange* range) const {
  LifetimePosition start = interval->start();
  int instruction_index = start.ToInstructionIndex();
  const InstructionBlock* block =
      data()->code()->GetInstructionBlock(instruction_index);
  for (RpoNumber pred_index : block->predecessors()) {
    const InstructionBlock* predecessor =
        data()->code()->InstructionBlockAt(pred_index);
    LifetimePosition last_pos = LifetimePosition::GapFromInstructionIndex(
        predecessor->last_instruction_index());
    last_pos = last_pos.NextStart().End();
    if (!range->Covers(last_pos)) return false;
  }
  return true;
}

bool LiveRangeBuilder::NextIntervalStartsInDifferentBlocks(
    const UseInterval* interval) const {
  DCHECK_NOT_NULL(interval->next());
  LifetimePosition end = interval->end();
  LifetimePosition next_start = interval->next()->start();
  // Since end is not covered, but the previous position is, move back a
  // position
  end = end.IsStart() ? end.PrevStart().End() : end.Start();
  int last_covered_index = end.ToInstructionIndex();
  const InstructionBlock* block =
      data()->code()->GetInstructionBlock(last_covered_index);
  const InstructionBlock* next_block =
      data()->code()->GetInstructionBlock(next_start.ToInstructionIndex());
  return block->rpo_number() < next_block->rpo_number();
}

RegisterAllocator::RegisterAllocator(RegisterAllocationData* data,
                                     RegisterKind kind)
    : data_(data),
      mode_(kind),
      num_registers_(GetRegisterCount(data->config(), kind)),
      num_allocatable_registers_(
          GetAllocatableRegisterCount(data->config(), kind)),
      allocatable_register_codes_(
          GetAllocatableRegisterCodes(data->config(), kind)),
      check_fp_aliasing_(false) {
  if (!kSimpleFPAliasing && kind == FP_REGISTERS) {
    check_fp_aliasing_ = (data->code()->representation_mask() &
                          (kFloat32Bit | kSimd128Bit)) != 0;
  }
}

LifetimePosition RegisterAllocator::GetSplitPositionForInstruction(
    const LiveRange* range, int instruction_index) {
  LifetimePosition ret = LifetimePosition::Invalid();

  ret = LifetimePosition::GapFromInstructionIndex(instruction_index);
  if (range->Start() >= ret || ret >= range->End()) {
    return LifetimePosition::Invalid();
  }
  return ret;
}

void RegisterAllocator::SplitAndSpillRangesDefinedByMemoryOperand() {
  size_t initial_range_count = data()->live_ranges().size();
  for (size_t i = 0; i < initial_range_count; ++i) {
    CHECK_EQ(initial_range_count,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    TopLevelLiveRange* range = data()->live_ranges()[i];
    if (!CanProcessRange(range)) continue;
    if (range->HasNoSpillType() ||
        (range->HasSpillRange() && !range->has_slot_use())) {
      continue;
    }
    LifetimePosition start = range->Start();
    TRACE("Live range %d:%d is defined by a spill operand.\n",
          range->TopLevel()->vreg(), range->relative_id());
    LifetimePosition next_pos = start;
    if (next_pos.IsGapPosition()) {
      next_pos = next_pos.NextStart();
    }

    // With splinters, we can be more strict and skip over positions
    // not strictly needing registers.
    UsePosition* pos =
        range->IsSplinter()
            ? range->NextRegisterPosition(next_pos)
            : range->NextUsePositionRegisterIsBeneficial(next_pos);
    // If the range already has a spill operand and it doesn't need a
    // register immediately, split it and spill the first part of the range.
    if (pos == nullptr) {
      Spill(range);
    } else if (pos->pos() > range->Start().NextStart()) {
      // Do not spill live range eagerly if use position that can benefit from
      // the register is too close to the start of live range.
      LifetimePosition split_pos = GetSplitPositionForInstruction(
          range, pos->pos().ToInstructionIndex());
      // There is no place to split, so we can't split and spill.
      if (!split_pos.IsValid()) continue;

      split_pos =
          FindOptimalSplitPos(range->Start().NextFullStart(), split_pos);

      SplitRangeAt(range, split_pos);
      Spill(range);
    }
  }
}


LiveRange* RegisterAllocator::SplitRangeAt(LiveRange* range,
                                           LifetimePosition pos) {
  DCHECK(!range->TopLevel()->IsFixed());
  TRACE("Splitting live range %d:%d at %d\n", range->TopLevel()->vreg(),
        range->relative_id(), pos.value());

  if (pos <= range->Start()) return range;

  // We can't properly connect liveranges if splitting occurred at the end
  // a block.
  DCHECK(pos.IsStart() || pos.IsGapPosition() ||
         (GetInstructionBlock(code(), pos)->last_instruction_index() !=
          pos.ToInstructionIndex()));

  LiveRange* result = range->SplitAt(pos, allocation_zone());
  return result;
}


LiveRange* RegisterAllocator::SplitBetween(LiveRange* range,
                                           LifetimePosition start,
                                           LifetimePosition end) {
  DCHECK(!range->TopLevel()->IsFixed());
  TRACE("Splitting live range %d:%d in position between [%d, %d]\n",
        range->TopLevel()->vreg(), range->relative_id(), start.value(),
        end.value());

  LifetimePosition split_pos = FindOptimalSplitPos(start, end);
  DCHECK(split_pos >= start);
  return SplitRangeAt(range, split_pos);
}


LifetimePosition RegisterAllocator::FindOptimalSplitPos(LifetimePosition start,
                                                        LifetimePosition end) {
  int start_instr = start.ToInstructionIndex();
  int end_instr = end.ToInstructionIndex();
  DCHECK_LE(start_instr, end_instr);

  // We have no choice
  if (start_instr == end_instr) return end;

  const InstructionBlock* start_block = GetInstructionBlock(code(), start);
  const InstructionBlock* end_block = GetInstructionBlock(code(), end);

  if (end_block == start_block) {
    // The interval is split in the same basic block. Split at the latest
    // possible position.
    return end;
  }

  const InstructionBlock* block = end_block;
  // Find header of outermost loop.
  do {
    const InstructionBlock* loop = GetContainingLoop(code(), block);
    if (loop == nullptr ||
        loop->rpo_number().ToInt() <= start_block->rpo_number().ToInt()) {
      // No more loops or loop starts before the lifetime start.
      break;
    }
    block = loop;
  } while (true);

  // We did not find any suitable outer loop. Split at the latest possible
  // position unless end_block is a loop header itself.
  if (block == end_block && !end_block->IsLoopHeader()) return end;

  return LifetimePosition::GapFromInstructionIndex(
      block->first_instruction_index());
}


LifetimePosition RegisterAllocator::FindOptimalSpillingPos(
    LiveRange* range, LifetimePosition pos) {
  const InstructionBlock* block = GetInstructionBlock(code(), pos.Start());
  const InstructionBlock* loop_header =
      block->IsLoopHeader() ? block : GetContainingLoop(code(), block);

  if (loop_header == nullptr) return pos;

  const UsePosition* prev_use =
      range->PreviousUsePositionRegisterIsBeneficial(pos);

  while (loop_header != nullptr) {
    // We are going to spill live range inside the loop.
    // If possible try to move spilling position backwards to loop header.
    // This will reduce number of memory moves on the back edge.
    LifetimePosition loop_start = LifetimePosition::GapFromInstructionIndex(
        loop_header->first_instruction_index());

    if (range->Covers(loop_start)) {
      if (prev_use == nullptr || prev_use->pos() < loop_start) {
        // No register beneficial use inside the loop before the pos.
        pos = loop_start;
      }
    }

    // Try hoisting out to an outer loop.
    loop_header = GetContainingLoop(code(), loop_header);
  }

  return pos;
}


void RegisterAllocator::Spill(LiveRange* range) {
  DCHECK(!range->spilled());
  TopLevelLiveRange* first = range->TopLevel();
  TRACE("Spilling live range %d:%d\n", first->vreg(), range->relative_id());

  if (first->HasNoSpillType()) {
    data()->AssignSpillRangeToLiveRange(first);
  }
  range->Spill();
}

const char* RegisterAllocator::RegisterName(int register_code) const {
  if (mode() == GENERAL_REGISTERS) {
    return data()->config()->GetGeneralRegisterName(register_code);
  } else {
    return data()->config()->GetDoubleRegisterName(register_code);
  }
}

LinearScanAllocator::LinearScanAllocator(RegisterAllocationData* data,
                                         RegisterKind kind, Zone* local_zone)
    : RegisterAllocator(data, kind),
      unhandled_live_ranges_(local_zone),
      active_live_ranges_(local_zone),
      inactive_live_ranges_(local_zone) {
  active_live_ranges().reserve(8);
  inactive_live_ranges().reserve(8);
  // TryAllocateFreeReg and AllocateBlockedReg assume this
  // when allocating local arrays.
  DCHECK_GE(RegisterConfiguration::kMaxFPRegisters,
            this->data()->config()->num_general_registers());
}


void LinearScanAllocator::AllocateRegisters() {
  DCHECK(unhandled_live_ranges().empty());
  DCHECK(active_live_ranges().empty());
  DCHECK(inactive_live_ranges().empty());

  SplitAndSpillRangesDefinedByMemoryOperand();

  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (!CanProcessRange(range)) continue;
    for (LiveRange* to_add = range; to_add != nullptr;
         to_add = to_add->next()) {
      if (!to_add->spilled()) {
        AddToUnhandled(to_add);
      }
    }
  }

  if (mode() == GENERAL_REGISTERS) {
    for (TopLevelLiveRange* current : data()->fixed_live_ranges()) {
      if (current != nullptr) AddToInactive(current);
    }
  } else {
    for (TopLevelLiveRange* current : data()->fixed_double_live_ranges()) {
      if (current != nullptr) AddToInactive(current);
    }
    if (!kSimpleFPAliasing && check_fp_aliasing()) {
      for (TopLevelLiveRange* current : data()->fixed_float_live_ranges()) {
        if (current != nullptr) AddToInactive(current);
      }
      for (TopLevelLiveRange* current : data()->fixed_simd128_live_ranges()) {
        if (current != nullptr) AddToInactive(current);
      }
    }
  }

  while (!unhandled_live_ranges().empty()) {
    LiveRange* current = *unhandled_live_ranges().begin();
    unhandled_live_ranges().erase(unhandled_live_ranges().begin());
    LifetimePosition position = current->Start();
#ifdef DEBUG
    allocation_finger_ = position;
#endif
    TRACE("Processing interval %d:%d start=%d\n", current->TopLevel()->vreg(),
          current->relative_id(), position.value());

    if (current->IsTopLevel() && TryReuseSpillForPhi(current->TopLevel()))
      continue;

    for (size_t i = 0; i < active_live_ranges().size(); ++i) {
      LiveRange* cur_active = active_live_ranges()[i];
      if (cur_active->End() <= position) {
        ActiveToHandled(cur_active);
        --i;  // The live range was removed from the list of active live ranges.
      } else if (!cur_active->Covers(position)) {
        ActiveToInactive(cur_active);
        --i;  // The live range was removed from the list of active live ranges.
      }
    }

    for (size_t i = 0; i < inactive_live_ranges().size(); ++i) {
      LiveRange* cur_inactive = inactive_live_ranges()[i];
      if (cur_inactive->End() <= position) {
        InactiveToHandled(cur_inactive);
        --i;  // Live range was removed from the list of inactive live ranges.
      } else if (cur_inactive->Covers(position)) {
        InactiveToActive(cur_inactive);
        --i;  // Live range was removed from the list of inactive live ranges.
      }
    }

    DCHECK(!current->HasRegisterAssigned() && !current->spilled());

    ProcessCurrentRange(current);
  }
}

bool LinearScanAllocator::TrySplitAndSpillSplinter(LiveRange* range) {
  DCHECK(range->TopLevel()->IsSplinter());
  // If we can spill the whole range, great. Otherwise, split above the
  // first use needing a register and spill the top part.
  const UsePosition* next_reg = range->NextRegisterPosition(range->Start());
  if (next_reg == nullptr) {
    Spill(range);
    return true;
  } else if (range->FirstHintPosition() == nullptr) {
    // If there was no hint, but we have a use position requiring a
    // register, apply the hot path heuristics.
    return false;
  } else if (next_reg->pos().PrevStart() > range->Start()) {
    LiveRange* tail = SplitRangeAt(range, next_reg->pos().PrevStart());
    AddToUnhandled(tail);
    Spill(range);
    return true;
  }
  return false;
}

void LinearScanAllocator::SetLiveRangeAssignedRegister(LiveRange* range,
                                                       int reg) {
  data()->MarkAllocated(range->representation(), reg);
  range->set_assigned_register(reg);
  range->SetUseHints(reg);
  if (range->IsTopLevel() && range->TopLevel()->is_phi()) {
    data()->GetPhiMapValueFor(range->TopLevel())->set_assigned_register(reg);
  }
}


void LinearScanAllocator::AddToActive(LiveRange* range) {
  TRACE("Add live range %d:%d to active\n", range->TopLevel()->vreg(),
        range->relative_id());
  active_live_ranges().push_back(range);
}


void LinearScanAllocator::AddToInactive(LiveRange* range) {
  TRACE("Add live range %d:%d to inactive\n", range->TopLevel()->vreg(),
        range->relative_id());
  inactive_live_ranges().push_back(range);
}

void LinearScanAllocator::AddToUnhandled(LiveRange* range) {
  if (range == nullptr || range->IsEmpty()) return;
  DCHECK(!range->HasRegisterAssigned() && !range->spilled());
  DCHECK(allocation_finger_ <= range->Start());

  TRACE("Add live range %d:%d to unhandled\n", range->TopLevel()->vreg(),
        range->relative_id());
  unhandled_live_ranges().insert(range);
}


void LinearScanAllocator::ActiveToHandled(LiveRange* range) {
  RemoveElement(&active_live_ranges(), range);
  TRACE("Moving live range %d:%d from active to handled\n",
        range->TopLevel()->vreg(), range->relative_id());
}


void LinearScanAllocator::ActiveToInactive(LiveRange* range) {
  RemoveElement(&active_live_ranges(), range);
  inactive_live_ranges().push_back(range);
  TRACE("Moving live range %d:%d from active to inactive\n",
        range->TopLevel()->vreg(), range->relative_id());
}


void LinearScanAllocator::InactiveToHandled(LiveRange* range) {
  RemoveElement(&inactive_live_ranges(), range);
  TRACE("Moving live range %d:%d from inactive to handled\n",
        range->TopLevel()->vreg(), range->relative_id());
}


void LinearScanAllocator::InactiveToActive(LiveRange* range) {
  RemoveElement(&inactive_live_ranges(), range);
  active_live_ranges().push_back(range);
  TRACE("Moving live range %d:%d from inactive to active\n",
        range->TopLevel()->vreg(), range->relative_id());
}

void LinearScanAllocator::GetFPRegisterSet(MachineRepresentation rep,
                                           int* num_regs, int* num_codes,
                                           const int** codes) const {
  DCHECK(!kSimpleFPAliasing);
  if (rep == MachineRepresentation::kFloat32) {
    *num_regs = data()->config()->num_float_registers();
    *num_codes = data()->config()->num_allocatable_float_registers();
    *codes = data()->config()->allocatable_float_codes();
  } else if (rep == MachineRepresentation::kSimd128) {
    *num_regs = data()->config()->num_simd128_registers();
    *num_codes = data()->config()->num_allocatable_simd128_registers();
    *codes = data()->config()->allocatable_simd128_codes();
  } else {
    UNREACHABLE();
  }
}

void LinearScanAllocator::FindFreeRegistersForRange(
    LiveRange* range, Vector<LifetimePosition> positions) {
  int num_regs = num_registers();
  int num_codes = num_allocatable_registers();
  const int* codes = allocatable_register_codes();
  MachineRepresentation rep = range->representation();
  if (!kSimpleFPAliasing && (rep == MachineRepresentation::kFloat32 ||
                             rep == MachineRepresentation::kSimd128))
    GetFPRegisterSet(rep, &num_regs, &num_codes, &codes);
  DCHECK_GE(positions.length(), num_regs);

  for (int i = 0; i < num_regs; ++i) {
    positions[i] = LifetimePosition::MaxPosition();
  }

  for (LiveRange* cur_active : active_live_ranges()) {
    int cur_reg = cur_active->assigned_register();
    if (kSimpleFPAliasing || !check_fp_aliasing()) {
      positions[cur_reg] = LifetimePosition::GapFromInstructionIndex(0);
      TRACE("Register %s is free until pos %d (1)\n", RegisterName(cur_reg),
            LifetimePosition::GapFromInstructionIndex(0).value());
    } else {
      int alias_base_index = -1;
      int aliases = data()->config()->GetAliases(
          cur_active->representation(), cur_reg, rep, &alias_base_index);
      DCHECK(aliases > 0 || (aliases == 0 && alias_base_index == -1));
      while (aliases--) {
        int aliased_reg = alias_base_index + aliases;
        positions[aliased_reg] = LifetimePosition::GapFromInstructionIndex(0);
      }
    }
  }

  for (LiveRange* cur_inactive : inactive_live_ranges()) {
    DCHECK(cur_inactive->End() > range->Start());
    int cur_reg = cur_inactive->assigned_register();
    // No need to carry out intersections, when this register won't be
    // interesting to this range anyway.
    // TODO(mtrofin): extend to aliased ranges, too.
    if ((kSimpleFPAliasing || !check_fp_aliasing()) &&
        positions[cur_reg] < range->Start()) {
      continue;
    }

    LifetimePosition next_intersection = cur_inactive->FirstIntersection(range);
    if (!next_intersection.IsValid()) continue;
    if (kSimpleFPAliasing || !check_fp_aliasing()) {
      positions[cur_reg] = Min(positions[cur_reg], next_intersection);
      TRACE("Register %s is free until pos %d (2)\n", RegisterName(cur_reg),
            Min(positions[cur_reg], next_intersection).value());
    } else {
      int alias_base_index = -1;
      int aliases = data()->config()->GetAliases(
          cur_inactive->representation(), cur_reg, rep, &alias_base_index);
      DCHECK(aliases > 0 || (aliases == 0 && alias_base_index == -1));
      while (aliases--) {
        int aliased_reg = alias_base_index + aliases;
        positions[aliased_reg] = Min(positions[aliased_reg], next_intersection);
      }
    }
  }
}

// High-level register allocation summary:
//
// For regular, or hot (i.e. not splinter) ranges, we attempt to first
// allocate first the preferred (hint) register. If that is not possible,
// we find a register that's free, and allocate that. If that's not possible,
// we search for a register to steal from a range that was allocated. The
// goal is to optimize for throughput by avoiding register-to-memory
// moves, which are expensive.
//
// For splinters, the goal is to minimize the number of moves. First we try
// to allocate the preferred register (more discussion follows). Failing that,
// we bail out and spill as far as we can, unless the first use is at start,
// case in which we apply the same behavior as we do for regular ranges.
// If there is no hint, we apply the hot-path behavior.
//
// For the splinter, the hint register may come from:
//
// - the hot path (we set it at splintering time with SetHint). In this case, if
// we cannot offer the hint register, spilling is better because it's at most
// 1 move, while trying to find and offer another register is at least 1 move.
//
// - a constraint. If we cannot offer that register, it's because  there is some
// interference. So offering the hint register up to the interference would
// result
// in a move at the interference, plus a move to satisfy the constraint. This is
// also the number of moves if we spill, with the potential of the range being
// already spilled and thus saving a move (the spill).
// Note that this can only be an input constraint, if it were an output one,
// the range wouldn't be a splinter because it means it'd be defined in a
// deferred
// block, and we don't mark those as splinters (they live in deferred blocks
// only).
//
// - a phi. The same analysis as in the case of the input constraint applies.
//
void LinearScanAllocator::ProcessCurrentRange(LiveRange* current) {
  LifetimePosition free_until_pos_buff[RegisterConfiguration::kMaxFPRegisters];
  Vector<LifetimePosition> free_until_pos(
      free_until_pos_buff, RegisterConfiguration::kMaxFPRegisters);
  FindFreeRegistersForRange(current, free_until_pos);
  if (!TryAllocatePreferredReg(current, free_until_pos)) {
    if (current->TopLevel()->IsSplinter()) {
      if (TrySplitAndSpillSplinter(current)) return;
    }
    if (!TryAllocateFreeReg(current, free_until_pos)) {
      AllocateBlockedReg(current);
    }
  }
  if (current->HasRegisterAssigned()) {
    AddToActive(current);
  }
}

bool LinearScanAllocator::TryAllocatePreferredReg(
    LiveRange* current, const Vector<LifetimePosition>& free_until_pos) {
  int hint_register;
  if (current->FirstHintPosition(&hint_register) != nullptr) {
    TRACE(
        "Found reg hint %s (free until [%d) for live range %d:%d (end %d[).\n",
        RegisterName(hint_register), free_until_pos[hint_register].value(),
        current->TopLevel()->vreg(), current->relative_id(),
        current->End().value());

    // The desired register is free until the end of the current live range.
    if (free_until_pos[hint_register] >= current->End()) {
      TRACE("Assigning preferred reg %s to live range %d:%d\n",
            RegisterName(hint_register), current->TopLevel()->vreg(),
            current->relative_id());
      SetLiveRangeAssignedRegister(current, hint_register);
      return true;
    }
  }
  return false;
}

bool LinearScanAllocator::TryAllocateFreeReg(
    LiveRange* current, const Vector<LifetimePosition>& free_until_pos) {
  int num_regs = 0;  // used only for the call to GetFPRegisterSet.
  int num_codes = num_allocatable_registers();
  const int* codes = allocatable_register_codes();
  MachineRepresentation rep = current->representation();
  if (!kSimpleFPAliasing && (rep == MachineRepresentation::kFloat32 ||
                             rep == MachineRepresentation::kSimd128)) {
    GetFPRegisterSet(rep, &num_regs, &num_codes, &codes);
  }

  DCHECK_GE(free_until_pos.length(), num_codes);

  // Find the register which stays free for the longest time. Check for
  // the hinted register first, as we might want to use that one. Only
  // count full instructions for free ranges, as an instruction's internal
  // positions do not help but might shadow a hinted register. This is
  // typically the case for function calls, where all registered are
  // cloberred after the call except for the argument registers, which are
  // set before the call. Hence, the argument registers always get ignored,
  // as their available time is shorter.
  int reg;
  if (current->FirstHintPosition(&reg) == nullptr) {
    reg = codes[0];
  }
  for (int i = 0; i < num_codes; ++i) {
    int code = codes[i];
    if (free_until_pos[code].ToInstructionIndex() >
        free_until_pos[reg].ToInstructionIndex()) {
      reg = code;
    }
  }

  LifetimePosition pos = free_until_pos[reg];

  if (pos <= current->Start()) {
    // All registers are blocked.
    return false;
  }

  if (pos < current->End()) {
    // Register reg is available at the range start but becomes blocked before
    // the range end. Split current at position where it becomes blocked.
    LiveRange* tail = SplitRangeAt(current, pos);
    AddToUnhandled(tail);

    // Try to allocate preferred register once more.
    if (TryAllocatePreferredReg(current, free_until_pos)) return true;
  }

  // Register reg is available at the range start and is free until the range
  // end.
  DCHECK(pos >= current->End());
  TRACE("Assigning free reg %s to live range %d:%d\n", RegisterName(reg),
        current->TopLevel()->vreg(), current->relative_id());
  SetLiveRangeAssignedRegister(current, reg);

  return true;
}

void LinearScanAllocator::AllocateBlockedReg(LiveRange* current) {
  UsePosition* register_use = current->NextRegisterPosition(current->Start());
  if (register_use == nullptr) {
    // There is no use in the current live range that requires a register.
    // We can just spill it.
    Spill(current);
    return;
  }

  int num_regs = num_registers();
  int num_codes = num_allocatable_registers();
  const int* codes = allocatable_register_codes();
  MachineRepresentation rep = current->representation();
  if (!kSimpleFPAliasing && (rep == MachineRepresentation::kFloat32 ||
                             rep == MachineRepresentation::kSimd128))
    GetFPRegisterSet(rep, &num_regs, &num_codes, &codes);

  // use_pos keeps track of positions a register/alias is used at.
  // block_pos keeps track of positions where a register/alias is blocked
  // from.
  LifetimePosition use_pos[RegisterConfiguration::kMaxFPRegisters];
  LifetimePosition block_pos[RegisterConfiguration::kMaxFPRegisters];
  for (int i = 0; i < num_regs; i++) {
    use_pos[i] = block_pos[i] = LifetimePosition::MaxPosition();
  }

  for (LiveRange* range : active_live_ranges()) {
    int cur_reg = range->assigned_register();
    bool is_fixed_or_cant_spill =
        range->TopLevel()->IsFixed() || !range->CanBeSpilled(current->Start());
    if (kSimpleFPAliasing || !check_fp_aliasing()) {
      if (is_fixed_or_cant_spill) {
        block_pos[cur_reg] = use_pos[cur_reg] =
            LifetimePosition::GapFromInstructionIndex(0);
      } else {
        DCHECK_NE(LifetimePosition::GapFromInstructionIndex(0),
                  block_pos[cur_reg]);
        use_pos[cur_reg] =
            range->NextLifetimePositionRegisterIsBeneficial(current->Start());
      }
    } else {
      int alias_base_index = -1;
      int aliases = data()->config()->GetAliases(
          range->representation(), cur_reg, rep, &alias_base_index);
      DCHECK(aliases > 0 || (aliases == 0 && alias_base_index == -1));
      while (aliases--) {
        int aliased_reg = alias_base_index + aliases;
        if (is_fixed_or_cant_spill) {
          block_pos[aliased_reg] = use_pos[aliased_reg] =
              LifetimePosition::GapFromInstructionIndex(0);
        } else {
          use_pos[aliased_reg] =
              Min(block_pos[aliased_reg],
                  range->NextLifetimePositionRegisterIsBeneficial(
                      current->Start()));
        }
      }
    }
  }

  for (LiveRange* range : inactive_live_ranges()) {
    DCHECK(range->End() > current->Start());
    int cur_reg = range->assigned_register();
    bool is_fixed = range->TopLevel()->IsFixed();

    // Don't perform costly intersections if they are guaranteed to not update
    // block_pos or use_pos.
    // TODO(mtrofin): extend to aliased ranges, too.
    if ((kSimpleFPAliasing || !check_fp_aliasing())) {
      if (is_fixed) {
        if (block_pos[cur_reg] < range->Start()) continue;
      } else {
        if (use_pos[cur_reg] < range->Start()) continue;
      }
    }

    LifetimePosition next_intersection = range->FirstIntersection(current);
    if (!next_intersection.IsValid()) continue;

    if (kSimpleFPAliasing || !check_fp_aliasing()) {
      if (is_fixed) {
        block_pos[cur_reg] = Min(block_pos[cur_reg], next_intersection);
        use_pos[cur_reg] = Min(block_pos[cur_reg], use_pos[cur_reg]);
      } else {
        use_pos[cur_reg] = Min(use_pos[cur_reg], next_intersection);
      }
    } else {
      int alias_base_index = -1;
      int aliases = data()->config()->GetAliases(
          range->representation(), cur_reg, rep, &alias_base_index);
      DCHECK(aliases > 0 || (aliases == 0 && alias_base_index == -1));
      while (aliases--) {
        int aliased_reg = alias_base_index + aliases;
        if (is_fixed) {
          block_pos[aliased_reg] =
              Min(block_pos[aliased_reg], next_intersection);
          use_pos[aliased_reg] =
              Min(block_pos[aliased_reg], use_pos[aliased_reg]);
        } else {
          use_pos[aliased_reg] = Min(use_pos[aliased_reg], next_intersection);
        }
      }
    }
  }

  int reg = codes[0];
  for (int i = 1; i < num_codes; ++i) {
    int code = codes[i];
    if (use_pos[code] > use_pos[reg]) {
      reg = code;
    }
  }

  if (use_pos[reg] < register_use->pos()) {
    // If there is a gap position before the next register use, we can
    // spill until there. The gap position will then fit the fill move.
    if (LifetimePosition::ExistsGapPositionBetween(current->Start(),
                                                   register_use->pos())) {
      SpillBetween(current, current->Start(), register_use->pos());
      return;
    }
  }

  // We couldn't spill until the next register use. Split before the register
  // is blocked, if applicable.
  if (block_pos[reg] < current->End()) {
    // Register becomes blocked before the current range end. Split before that
    // position.
    LiveRange* tail =
        SplitBetween(current, current->Start(), block_pos[reg].Start());
    AddToUnhandled(tail);
  }

  // Register reg is not blocked for the whole range.
  DCHECK(block_pos[reg] >= current->End());
  TRACE("Assigning blocked reg %s to live range %d:%d\n", RegisterName(reg),
        current->TopLevel()->vreg(), current->relative_id());
  SetLiveRangeAssignedRegister(current, reg);

  // This register was not free. Thus we need to find and spill
  // parts of active and inactive live regions that use the same register
  // at the same lifetime positions as current.
  SplitAndSpillIntersecting(current);
}


void LinearScanAllocator::SplitAndSpillIntersecting(LiveRange* current) {
  DCHECK(current->HasRegisterAssigned());
  int reg = current->assigned_register();
  LifetimePosition split_pos = current->Start();
  for (size_t i = 0; i < active_live_ranges().size(); ++i) {
    LiveRange* range = active_live_ranges()[i];
    if (kSimpleFPAliasing || !check_fp_aliasing()) {
      if (range->assigned_register() != reg) continue;
    } else {
      if (!data()->config()->AreAliases(current->representation(), reg,
                                        range->representation(),
                                        range->assigned_register())) {
        continue;
      }
    }

    UsePosition* next_pos = range->NextRegisterPosition(current->Start());
    LifetimePosition spill_pos = FindOptimalSpillingPos(range, split_pos);
    if (next_pos == nullptr) {
      SpillAfter(range, spill_pos);
    } else {
      // When spilling between spill_pos and next_pos ensure that the range
      // remains spilled at least until the start of the current live range.
      // This guarantees that we will not introduce new unhandled ranges that
      // start before the current range as this violates allocation invariants
      // and will lead to an inconsistent state of active and inactive
      // live-ranges: ranges are allocated in order of their start positions,
      // ranges are retired from active/inactive when the start of the
      // current live-range is larger than their end.
      DCHECK(LifetimePosition::ExistsGapPositionBetween(current->Start(),
                                                        next_pos->pos()));
      SpillBetweenUntil(range, spill_pos, current->Start(), next_pos->pos());
    }
    ActiveToHandled(range);
    --i;
  }

  for (size_t i = 0; i < inactive_live_ranges().size(); ++i) {
    LiveRange* range = inactive_live_ranges()[i];
    DCHECK(range->End() > current->Start());
    if (range->TopLevel()->IsFixed()) continue;
    if (kSimpleFPAliasing || !check_fp_aliasing()) {
      if (range->assigned_register() != reg) continue;
    } else {
      if (!data()->config()->AreAliases(current->representation(), reg,
                                        range->representation(),
                                        range->assigned_register()))
        continue;
    }

    LifetimePosition next_intersection = range->FirstIntersection(current);
    if (next_intersection.IsValid()) {
      UsePosition* next_pos = range->NextRegisterPosition(current->Start());
      if (next_pos == nullptr) {
        SpillAfter(range, split_pos);
      } else {
        next_intersection = Min(next_intersection, next_pos->pos());
        SpillBetween(range, split_pos, next_intersection);
      }
      InactiveToHandled(range);
      --i;
    }
  }
}


bool LinearScanAllocator::TryReuseSpillForPhi(TopLevelLiveRange* range) {
  if (!range->is_phi()) return false;

  DCHECK(!range->HasSpillOperand());
  RegisterAllocationData::PhiMapValue* phi_map_value =
      data()->GetPhiMapValueFor(range);
  const PhiInstruction* phi = phi_map_value->phi();
  const InstructionBlock* block = phi_map_value->block();
  // Count the number of spilled operands.
  size_t spilled_count = 0;
  LiveRange* first_op = nullptr;
  for (size_t i = 0; i < phi->operands().size(); i++) {
    int op = phi->operands()[i];
    LiveRange* op_range = data()->GetOrCreateLiveRangeFor(op);
    if (!op_range->TopLevel()->HasSpillRange()) continue;
    const InstructionBlock* pred =
        code()->InstructionBlockAt(block->predecessors()[i]);
    LifetimePosition pred_end =
        LifetimePosition::InstructionFromInstructionIndex(
            pred->last_instruction_index());
    while (op_range != nullptr && !op_range->CanCover(pred_end)) {
      op_range = op_range->next();
    }
    if (op_range != nullptr && op_range->spilled()) {
      spilled_count++;
      if (first_op == nullptr) {
        first_op = op_range->TopLevel();
      }
    }
  }

  // Only continue if more than half of the operands are spilled.
  if (spilled_count * 2 <= phi->operands().size()) {
    return false;
  }

  // Try to merge the spilled operands and count the number of merged spilled
  // operands.
  DCHECK_NOT_NULL(first_op);
  SpillRange* first_op_spill = first_op->TopLevel()->GetSpillRange();
  size_t num_merged = 1;
  for (size_t i = 1; i < phi->operands().size(); i++) {
    int op = phi->operands()[i];
    TopLevelLiveRange* op_range = data()->live_ranges()[op];
    if (!op_range->HasSpillRange()) continue;
    SpillRange* op_spill = op_range->GetSpillRange();
    if (op_spill == first_op_spill || first_op_spill->TryMerge(op_spill)) {
      num_merged++;
    }
  }

  // Only continue if enough operands could be merged to the
  // same spill slot.
  if (num_merged * 2 <= phi->operands().size() ||
      AreUseIntervalsIntersecting(first_op_spill->interval(),
                                  range->first_interval())) {
    return false;
  }

  // If the range does not need register soon, spill it to the merged
  // spill range.
  LifetimePosition next_pos = range->Start();
  if (next_pos.IsGapPosition()) next_pos = next_pos.NextStart();
  UsePosition* pos = range->NextUsePositionRegisterIsBeneficial(next_pos);
  if (pos == nullptr) {
    SpillRange* spill_range =
        range->TopLevel()->HasSpillRange()
            ? range->TopLevel()->GetSpillRange()
            : data()->AssignSpillRangeToLiveRange(range->TopLevel());
    bool merged = first_op_spill->TryMerge(spill_range);
    if (!merged) return false;
    Spill(range);
    return true;
  } else if (pos->pos() > range->Start().NextStart()) {
    SpillRange* spill_range =
        range->TopLevel()->HasSpillRange()
            ? range->TopLevel()->GetSpillRange()
            : data()->AssignSpillRangeToLiveRange(range->TopLevel());
    bool merged = first_op_spill->TryMerge(spill_range);
    if (!merged) return false;
    SpillBetween(range, range->Start(), pos->pos());
    return true;
  }
  return false;
}


void LinearScanAllocator::SpillAfter(LiveRange* range, LifetimePosition pos) {
  LiveRange* second_part = SplitRangeAt(range, pos);
  Spill(second_part);
}


void LinearScanAllocator::SpillBetween(LiveRange* range, LifetimePosition start,
                                       LifetimePosition end) {
  SpillBetweenUntil(range, start, start, end);
}


void LinearScanAllocator::SpillBetweenUntil(LiveRange* range,
                                            LifetimePosition start,
                                            LifetimePosition until,
                                            LifetimePosition end) {
  CHECK(start < end);
  LiveRange* second_part = SplitRangeAt(range, start);

  if (second_part->Start() < end) {
    // The split result intersects with [start, end[.
    // Split it at position between ]start+1, end[, spill the middle part
    // and put the rest to unhandled.
    LifetimePosition third_part_end = end.PrevStart().End();
    if (data()->IsBlockBoundary(end.Start())) {
      third_part_end = end.Start();
    }
    LiveRange* third_part = SplitBetween(
        second_part, Max(second_part->Start().End(), until), third_part_end);

    DCHECK(third_part != second_part);

    Spill(second_part);
    AddToUnhandled(third_part);
  } else {
    // The split result does not intersect with [start, end[.
    // Nothing to spill. Just put it to unhandled as whole.
    AddToUnhandled(second_part);
  }
}


SpillSlotLocator::SpillSlotLocator(RegisterAllocationData* data)
    : data_(data) {}


void SpillSlotLocator::LocateSpillSlots() {
  const InstructionSequence* code = data()->code();
  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (range == nullptr || range->IsEmpty()) continue;
    // We care only about ranges which spill in the frame.
    if (!range->HasSpillRange() || range->IsSpilledOnlyInDeferredBlocks()) {
      continue;
    }
    TopLevelLiveRange::SpillMoveInsertionList* spills =
        range->GetSpillMoveInsertionLocations();
    DCHECK_NOT_NULL(spills);
    for (; spills != nullptr; spills = spills->next) {
      code->GetInstructionBlock(spills->gap_index)->mark_needs_frame();
    }
  }
}


OperandAssigner::OperandAssigner(RegisterAllocationData* data) : data_(data) {}


void OperandAssigner::AssignSpillSlots() {
  ZoneVector<SpillRange*>& spill_ranges = data()->spill_ranges();
  // Merge disjoint spill ranges
  for (size_t i = 0; i < spill_ranges.size(); ++i) {
    SpillRange* range = spill_ranges[i];
    if (range == nullptr) continue;
    if (range->IsEmpty()) continue;
    for (size_t j = i + 1; j < spill_ranges.size(); ++j) {
      SpillRange* other = spill_ranges[j];
      if (other != nullptr && !other->IsEmpty()) {
        range->TryMerge(other);
      }
    }
  }
  // Allocate slots for the merged spill ranges.
  for (SpillRange* range : spill_ranges) {
    if (range == nullptr || range->IsEmpty()) continue;
    // Allocate a new operand referring to the spill slot.
    if (!range->HasSlot()) {
      int index = data()->frame()->AllocateSpillSlot(range->byte_width());
      range->set_assigned_slot(index);
    }
  }
}


void OperandAssigner::CommitAssignment() {
  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* top_range : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (top_range == nullptr || top_range->IsEmpty()) continue;
    InstructionOperand spill_operand;
    if (top_range->HasSpillOperand()) {
      spill_operand = *top_range->TopLevel()->GetSpillOperand();
    } else if (top_range->TopLevel()->HasSpillRange()) {
      spill_operand = top_range->TopLevel()->GetSpillRangeOperand();
    }
    if (top_range->is_phi()) {
      data()->GetPhiMapValueFor(top_range)->CommitAssignment(
          top_range->GetAssignedOperand());
    }
    for (LiveRange* range = top_range; range != nullptr;
         range = range->next()) {
      InstructionOperand assigned = range->GetAssignedOperand();
      DCHECK(!assigned.IsUnallocated());
      range->ConvertUsesToOperand(assigned, spill_operand);
    }

    if (!spill_operand.IsInvalid()) {
      // If this top level range has a child spilled in a deferred block, we use
      // the range and control flow connection mechanism instead of spilling at
      // definition. Refer to the ConnectLiveRanges and ResolveControlFlow
      // phases. Normally, when we spill at definition, we do not insert a
      // connecting move when a successor child range is spilled - because the
      // spilled range picks up its value from the slot which was assigned at
      // definition. For ranges that are determined to spill only in deferred
      // blocks, we let ConnectLiveRanges and ResolveControlFlow find the blocks
      // where a spill operand is expected, and then finalize by inserting the
      // spills in the deferred blocks dominators.
      if (!top_range->IsSpilledOnlyInDeferredBlocks()) {
        // Spill at definition if the range isn't spilled only in deferred
        // blocks.
        top_range->CommitSpillMoves(
            data()->code(), spill_operand,
            top_range->has_slot_use() || top_range->spilled());
      }
    }
  }
}


ReferenceMapPopulator::ReferenceMapPopulator(RegisterAllocationData* data)
    : data_(data) {}


bool ReferenceMapPopulator::SafePointsAreInOrder() const {
  int safe_point = 0;
  for (ReferenceMap* map : *data()->code()->reference_maps()) {
    if (safe_point > map->instruction_position()) return false;
    safe_point = map->instruction_position();
  }
  return true;
}


void ReferenceMapPopulator::PopulateReferenceMaps() {
  DCHECK(SafePointsAreInOrder());
  // Map all delayed references.
  for (RegisterAllocationData::DelayedReference& delayed_reference :
       data()->delayed_references()) {
    delayed_reference.map->RecordReference(
        AllocatedOperand::cast(*delayed_reference.operand));
  }
  // Iterate over all safe point positions and record a pointer
  // for all spilled live ranges at this point.
  int last_range_start = 0;
  const ReferenceMapDeque* reference_maps = data()->code()->reference_maps();
  ReferenceMapDeque::const_iterator first_it = reference_maps->begin();
  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* range : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (range == nullptr) continue;
    // Skip non-reference values.
    if (!data()->IsReference(range)) continue;
    // Skip empty live ranges.
    if (range->IsEmpty()) continue;
    if (range->has_preassigned_slot()) continue;

    // Find the extent of the range and its children.
    int start = range->Start().ToInstructionIndex();
    int end = 0;
    for (LiveRange* cur = range; cur != nullptr; cur = cur->next()) {
      LifetimePosition this_end = cur->End();
      if (this_end.ToInstructionIndex() > end)
        end = this_end.ToInstructionIndex();
      DCHECK(cur->Start().ToInstructionIndex() >= start);
    }

    // Most of the ranges are in order, but not all.  Keep an eye on when they
    // step backwards and reset the first_it so we don't miss any safe points.
    if (start < last_range_start) first_it = reference_maps->begin();
    last_range_start = start;

    // Step across all the safe points that are before the start of this range,
    // recording how far we step in order to save doing this for the next range.
    for (; first_it != reference_maps->end(); ++first_it) {
      ReferenceMap* map = *first_it;
      if (map->instruction_position() >= start) break;
    }

    InstructionOperand spill_operand;
    if (((range->HasSpillOperand() &&
          !range->GetSpillOperand()->IsConstant()) ||
         range->HasSpillRange())) {
      if (range->HasSpillOperand()) {
        spill_operand = *range->GetSpillOperand();
      } else {
        spill_operand = range->GetSpillRangeOperand();
      }
      DCHECK(spill_operand.IsStackSlot());
      DCHECK(CanBeTaggedPointer(
          AllocatedOperand::cast(spill_operand).representation()));
    }

    LiveRange* cur = range;
    // Step through the safe points to see whether they are in the range.
    for (auto it = first_it; it != reference_maps->end(); ++it) {
      ReferenceMap* map = *it;
      int safe_point = map->instruction_position();

      // The safe points are sorted so we can stop searching here.
      if (safe_point - 1 > end) break;

      // Advance to the next active range that covers the current
      // safe point position.
      LifetimePosition safe_point_pos =
          LifetimePosition::InstructionFromInstructionIndex(safe_point);

      // Search for the child range (cur) that covers safe_point_pos. If we
      // don't find it before the children pass safe_point_pos, keep cur at
      // the last child, because the next safe_point_pos may be covered by cur.
      // This may happen if cur has more than one interval, and the current
      // safe_point_pos is in between intervals.
      // For that reason, cur may be at most the last child.
      DCHECK_NOT_NULL(cur);
      DCHECK(safe_point_pos >= cur->Start() || range == cur);
      bool found = false;
      while (!found) {
        if (cur->Covers(safe_point_pos)) {
          found = true;
        } else {
          LiveRange* next = cur->next();
          if (next == nullptr || next->Start() > safe_point_pos) {
            break;
          }
          cur = next;
        }
      }

      if (!found) {
        continue;
      }

      // Check if the live range is spilled and the safe point is after
      // the spill position.
      int spill_index = range->IsSpilledOnlyInDeferredBlocks()
                            ? cur->Start().ToInstructionIndex()
                            : range->spill_start_index();

      if (!spill_operand.IsInvalid() && safe_point >= spill_index) {
        TRACE("Pointer for range %d (spilled at %d) at safe point %d\n",
              range->vreg(), spill_index, safe_point);
        map->RecordReference(AllocatedOperand::cast(spill_operand));
      }

      if (!cur->spilled()) {
        TRACE(
            "Pointer in register for range %d:%d (start at %d) "
            "at safe point %d\n",
            range->vreg(), cur->relative_id(), cur->Start().value(),
            safe_point);
        InstructionOperand operand = cur->GetAssignedOperand();
        DCHECK(!operand.IsStackSlot());
        DCHECK(CanBeTaggedPointer(
            AllocatedOperand::cast(operand).representation()));
        map->RecordReference(AllocatedOperand::cast(operand));
      }
    }
  }
}


LiveRangeConnector::LiveRangeConnector(RegisterAllocationData* data)
    : data_(data) {}


bool LiveRangeConnector::CanEagerlyResolveControlFlow(
    const InstructionBlock* block) const {
  if (block->PredecessorCount() != 1) return false;
  return block->predecessors()[0].IsNext(block->rpo_number());
}


void LiveRangeConnector::ResolveControlFlow(Zone* local_zone) {
  // Lazily linearize live ranges in memory for fast lookup.
  LiveRangeFinder finder(data(), local_zone);
  ZoneVector<BitVector*>& live_in_sets = data()->live_in_sets();
  for (const InstructionBlock* block : code()->instruction_blocks()) {
    if (CanEagerlyResolveControlFlow(block)) continue;
    BitVector* live = live_in_sets[block->rpo_number().ToInt()];
    BitVector::Iterator iterator(live);
    while (!iterator.Done()) {
      int vreg = iterator.Current();
      LiveRangeBoundArray* array = finder.ArrayFor(vreg);
      for (const RpoNumber& pred : block->predecessors()) {
        FindResult result;
        const InstructionBlock* pred_block = code()->InstructionBlockAt(pred);
        if (!array->FindConnectableSubranges(block, pred_block, &result)) {
          continue;
        }
        InstructionOperand pred_op = result.pred_cover_->GetAssignedOperand();
        InstructionOperand cur_op = result.cur_cover_->GetAssignedOperand();
        if (pred_op.Equals(cur_op)) continue;
        if (!pred_op.IsAnyRegister() && cur_op.IsAnyRegister()) {
          // We're doing a reload.
          // We don't need to, if:
          // 1) there's no register use in this block, and
          // 2) the range ends before the block does, and
          // 3) we don't have a successor, or the successor is spilled.
          LifetimePosition block_start =
              LifetimePosition::GapFromInstructionIndex(block->code_start());
          LifetimePosition block_end =
              LifetimePosition::GapFromInstructionIndex(block->code_end());
          const LiveRange* current = result.cur_cover_;
          const LiveRange* successor = current->next();
          if (current->End() < block_end &&
              (successor == nullptr || successor->spilled())) {
            // verify point 1: no register use. We can go to the end of the
            // range, since it's all within the block.

            bool uses_reg = false;
            for (const UsePosition* use = current->NextUsePosition(block_start);
                 use != nullptr; use = use->next()) {
              if (use->operand()->IsAnyRegister()) {
                uses_reg = true;
                break;
              }
            }
            if (!uses_reg) continue;
          }
          if (current->TopLevel()->IsSpilledOnlyInDeferredBlocks() &&
              pred_block->IsDeferred()) {
            // The spill location should be defined in pred_block, so add
            // pred_block to the list of blocks requiring a spill operand.
            current->TopLevel()->GetListOfBlocksRequiringSpillOperands()->Add(
                pred_block->rpo_number().ToInt());
          }
        }
        int move_loc = ResolveControlFlow(block, cur_op, pred_block, pred_op);
        USE(move_loc);
        DCHECK_IMPLIES(
            result.cur_cover_->TopLevel()->IsSpilledOnlyInDeferredBlocks() &&
                !(pred_op.IsAnyRegister() && cur_op.IsAnyRegister()),
            code()->GetInstructionBlock(move_loc)->IsDeferred());
      }
      iterator.Advance();
    }
  }

  // At this stage, we collected blocks needing a spill operand from
  // ConnectRanges and from ResolveControlFlow. Time to commit the spills for
  // deferred blocks.
  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* top : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (top == nullptr || top->IsEmpty() ||
        !top->IsSpilledOnlyInDeferredBlocks())
      continue;
    CommitSpillsInDeferredBlocks(top, finder.ArrayFor(top->vreg()), local_zone);
  }
}


int LiveRangeConnector::ResolveControlFlow(const InstructionBlock* block,
                                           const InstructionOperand& cur_op,
                                           const InstructionBlock* pred,
                                           const InstructionOperand& pred_op) {
  DCHECK(!pred_op.Equals(cur_op));
  int gap_index;
  Instruction::GapPosition position;
  if (block->PredecessorCount() == 1) {
    gap_index = block->first_instruction_index();
    position = Instruction::START;
  } else {
    DCHECK_EQ(1, pred->SuccessorCount());
    DCHECK(!code()
                ->InstructionAt(pred->last_instruction_index())
                ->HasReferenceMap());
    gap_index = pred->last_instruction_index();
    position = Instruction::END;
  }
  data()->AddGapMove(gap_index, position, pred_op, cur_op);
  return gap_index;
}

void LiveRangeConnector::ConnectRanges(Zone* local_zone) {
  DelayedInsertionMap delayed_insertion_map(local_zone);
  const size_t live_ranges_size = data()->live_ranges().size();
  for (TopLevelLiveRange* top_range : data()->live_ranges()) {
    CHECK_EQ(live_ranges_size,
             data()->live_ranges().size());  // TODO(neis): crbug.com/831822
    if (top_range == nullptr) continue;
    bool connect_spilled = top_range->IsSpilledOnlyInDeferredBlocks();
    LiveRange* first_range = top_range;
    for (LiveRange *second_range = first_range->next(); second_range != nullptr;
         first_range = second_range, second_range = second_range->next()) {
      LifetimePosition pos = second_range->Start();
      // Add gap move if the two live ranges touch and there is no block
      // boundary.
      if (second_range->spilled()) continue;
      if (first_range->End() != pos) continue;
      if (data()->IsBlockBoundary(pos) &&
          !CanEagerlyResolveControlFlow(GetInstructionBlock(code(), pos))) {
        continue;
      }
      InstructionOperand prev_operand = first_range->GetAssignedOperand();
      InstructionOperand cur_operand = second_range->GetAssignedOperand();
      if (prev_operand.Equals(cur_operand)) continue;
      bool delay_insertion = false;
      Instruction::GapPosition gap_pos;
      int gap_index = pos.ToInstructionIndex();
      if (connect_spilled && !prev_operand.IsAnyRegister() &&
          cur_operand.IsAnyRegister()) {
        const InstructionBlock* block = code()->GetInstructionBlock(gap_index);
        DCHECK(block->IsDeferred());
        // Performing a reload in this block, meaning the spill operand must
        // be defined here.
        top_range->GetListOfBlocksRequiringSpillOperands()->Add(
            block->rpo_number().ToInt());
      }

      if (pos.IsGapPosition()) {
        gap_pos = pos.IsStart() ? Instruction::START : Instruction::END;
      } else {
        if (pos.IsStart()) {
          delay_insertion = true;
        } else {
          gap_index++;
        }
        gap_pos = delay_insertion ? Instruction::END : Instruction::START;
      }
      // Reloads or spills for spilled in deferred blocks ranges must happen
      // only in deferred blocks.
      DCHECK_IMPLIES(
          connect_spilled &&
              !(prev_operand.IsAnyRegister() && cur_operand.IsAnyRegister()),
          code()->GetInstructionBlock(gap_index)->IsDeferred());

      ParallelMove* move =
          code()->InstructionAt(gap_index)->GetOrCreateParallelMove(
              gap_pos, code_zone());
      if (!delay_insertion) {
        move->AddMove(prev_operand, cur_operand);
      } else {
        delayed_insertion_map.insert(
            std::make_pair(std::make_pair(move, prev_operand), cur_operand));
      }
    }
  }
  if (delayed_insertion_map.empty()) return;
  // Insert all the moves which should occur after the stored move.
  ZoneVector<MoveOperands*> to_insert(local_zone);
  ZoneVector<MoveOperands*> to_eliminate(local_zone);
  to_insert.reserve(4);
  to_eliminate.reserve(4);
  ParallelMove* moves = delayed_insertion_map.begin()->first.first;
  for (auto it = delayed_insertion_map.begin();; ++it) {
    bool done = it == delayed_insertion_map.end();
    if (done || it->first.first != moves) {
      // Commit the MoveOperands for current ParallelMove.
      for (MoveOperands* move : to_eliminate) {
        move->Eliminate();
      }
      for (MoveOperands* move : to_insert) {
        moves->push_back(move);
      }
      if (done) break;
      // Reset state.
      to_eliminate.clear();
      to_insert.clear();
      moves = it->first.first;
    }
    // Gather all MoveOperands for a single ParallelMove.
    MoveOperands* move =
        new (code_zone()) MoveOperands(it->first.second, it->second);
    moves->PrepareInsertAfter(move, &to_eliminate);
    to_insert.push_back(move);
  }
}


void LiveRangeConnector::CommitSpillsInDeferredBlocks(
    TopLevelLiveRange* range, LiveRangeBoundArray* array, Zone* temp_zone) {
  DCHECK(range->IsSpilledOnlyInDeferredBlocks());
  DCHECK(!range->spilled());

  InstructionSequence* code = data()->code();
  InstructionOperand spill_operand = range->GetSpillRangeOperand();

  TRACE("Live Range %d will be spilled only in deferred blocks.\n",
        range->vreg());
  // If we have ranges that aren't spilled but require the operand on the stack,
  // make sure we insert the spill.
  for (const LiveRange* child = range; child != nullptr;
       child = child->next()) {
    for (const UsePosition* pos = child->first_pos(); pos != nullptr;
         pos = pos->next()) {
      if (pos->type() != UsePositionType::kRequiresSlot && !child->spilled())
        continue;
      range->AddBlockRequiringSpillOperand(
          code->GetInstructionBlock(pos->pos().ToInstructionIndex())
              ->rpo_number());
    }
  }

  ZoneQueue<int> worklist(temp_zone);

  for (BitVector::Iterator iterator(
           range->GetListOfBlocksRequiringSpillOperands());
       !iterator.Done(); iterator.Advance()) {
    worklist.push(iterator.Current());
  }

  ZoneSet<std::pair<RpoNumber, int>> done_moves(temp_zone);
  // Seek the deferred blocks that dominate locations requiring spill operands,
  // and spill there. We only need to spill at the start of such blocks.
  BitVector done_blocks(
      range->GetListOfBlocksRequiringSpillOperands()->length(), temp_zone);
  while (!worklist.empty()) {
    int block_id = worklist.front();
    worklist.pop();
    if (done_blocks.Contains(block_id)) continue;
    done_blocks.Add(block_id);
    InstructionBlock* spill_block =
        code->InstructionBlockAt(RpoNumber::FromInt(block_id));

    for (const RpoNumber& pred : spill_block->predecessors()) {
      const InstructionBlock* pred_block = code->InstructionBlockAt(pred);

      if (pred_block->IsDeferred()) {
        worklist.push(pred_block->rpo_number().ToInt());
      } else {
        LifetimePosition pred_end =
            LifetimePosition::InstructionFromInstructionIndex(
                pred_block->last_instruction_index());

        LiveRangeBound* bound = array->Find(pred_end);

        InstructionOperand pred_op = bound->range_->GetAssignedOperand();

        RpoNumber spill_block_number = spill_block->rpo_number();
        if (done_moves.find(std::make_pair(
                spill_block_number, range->vreg())) == done_moves.end()) {
          data()->AddGapMove(spill_block->first_instruction_index(),
                             Instruction::GapPosition::START, pred_op,
                             spill_operand);
          done_moves.insert(std::make_pair(spill_block_number, range->vreg()));
          spill_block->mark_needs_frame();
        }
      }
    }
  }
}

#undef TRACE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
