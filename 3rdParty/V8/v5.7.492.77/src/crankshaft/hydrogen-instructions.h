// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_INSTRUCTIONS_H_
#define V8_CRANKSHAFT_HYDROGEN_INSTRUCTIONS_H_

#include <cstring>
#include <iosfwd>

#include "src/allocation.h"
#include "src/ast/ast.h"
#include "src/base/bits.h"
#include "src/bit-vector.h"
#include "src/conversions.h"
#include "src/crankshaft/hydrogen-types.h"
#include "src/crankshaft/unique.h"
#include "src/deoptimizer.h"
#include "src/globals.h"
#include "src/interface-descriptors.h"
#include "src/small-pointer-list.h"
#include "src/utils.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// Forward declarations.
struct ChangesOf;
class HBasicBlock;
class HDiv;
class HEnvironment;
class HInferRepresentationPhase;
class HInstruction;
class HLoopInformation;
class HStoreNamedField;
class HValue;
class LInstruction;
class LChunkBuilder;
class SmallMapList;

#define HYDROGEN_ABSTRACT_INSTRUCTION_LIST(V) \
  V(ArithmeticBinaryOperation)                \
  V(BinaryOperation)                          \
  V(BitwiseBinaryOperation)                   \
  V(ControlInstruction)                       \
  V(Instruction)


#define HYDROGEN_CONCRETE_INSTRUCTION_LIST(V) \
  V(AbnormalExit)                             \
  V(AccessArgumentsAt)                        \
  V(Add)                                      \
  V(Allocate)                                 \
  V(ApplyArguments)                           \
  V(ArgumentsElements)                        \
  V(ArgumentsLength)                          \
  V(ArgumentsObject)                          \
  V(Bitwise)                                  \
  V(BlockEntry)                               \
  V(BoundsCheck)                              \
  V(Branch)                                   \
  V(CallWithDescriptor)                       \
  V(CallNewArray)                             \
  V(CallRuntime)                              \
  V(CapturedObject)                           \
  V(Change)                                   \
  V(CheckArrayBufferNotNeutered)              \
  V(CheckHeapObject)                          \
  V(CheckInstanceType)                        \
  V(CheckMaps)                                \
  V(CheckMapValue)                            \
  V(CheckSmi)                                 \
  V(CheckValue)                               \
  V(ClampToUint8)                             \
  V(ClassOfTestAndBranch)                     \
  V(CompareNumericAndBranch)                  \
  V(CompareHoleAndBranch)                     \
  V(CompareGeneric)                           \
  V(CompareObjectEqAndBranch)                 \
  V(CompareMap)                               \
  V(Constant)                                 \
  V(Context)                                  \
  V(DebugBreak)                               \
  V(DeclareGlobals)                           \
  V(Deoptimize)                               \
  V(Div)                                      \
  V(DummyUse)                                 \
  V(EnterInlined)                             \
  V(EnvironmentMarker)                        \
  V(ForceRepresentation)                      \
  V(ForInCacheArray)                          \
  V(ForInPrepareMap)                          \
  V(Goto)                                     \
  V(HasInstanceTypeAndBranch)                 \
  V(InnerAllocatedObject)                     \
  V(InvokeFunction)                           \
  V(HasInPrototypeChainAndBranch)             \
  V(IsStringAndBranch)                        \
  V(IsSmiAndBranch)                           \
  V(IsUndetectableAndBranch)                  \
  V(LeaveInlined)                             \
  V(LoadContextSlot)                          \
  V(LoadFieldByIndex)                         \
  V(LoadFunctionPrototype)                    \
  V(LoadKeyed)                                \
  V(LoadNamedField)                           \
  V(LoadRoot)                                 \
  V(MathFloorOfDiv)                           \
  V(MathMinMax)                               \
  V(MaybeGrowElements)                        \
  V(Mod)                                      \
  V(Mul)                                      \
  V(OsrEntry)                                 \
  V(Parameter)                                \
  V(Power)                                    \
  V(Prologue)                                 \
  V(PushArguments)                            \
  V(Return)                                   \
  V(Ror)                                      \
  V(Sar)                                      \
  V(SeqStringGetChar)                         \
  V(SeqStringSetChar)                         \
  V(Shl)                                      \
  V(Shr)                                      \
  V(Simulate)                                 \
  V(StackCheck)                               \
  V(StoreCodeEntry)                           \
  V(StoreContextSlot)                         \
  V(StoreKeyed)                               \
  V(StoreNamedField)                          \
  V(StringAdd)                                \
  V(StringCharCodeAt)                         \
  V(StringCharFromCode)                       \
  V(StringCompareAndBranch)                   \
  V(Sub)                                      \
  V(ThisFunction)                             \
  V(TransitionElementsKind)                   \
  V(TrapAllocationMemento)                    \
  V(Typeof)                                   \
  V(TypeofIsAndBranch)                        \
  V(UnaryMathOperation)                       \
  V(UnknownOSRValue)                          \
  V(UseConst)                                 \
  V(WrapReceiver)

#define GVN_TRACKED_FLAG_LIST(V)               \
  V(NewSpacePromotion)

#define GVN_UNTRACKED_FLAG_LIST(V)             \
  V(ArrayElements)                             \
  V(ArrayLengths)                              \
  V(StringLengths)                             \
  V(BackingStoreFields)                        \
  V(Calls)                                     \
  V(ContextSlots)                              \
  V(DoubleArrayElements)                       \
  V(DoubleFields)                              \
  V(ElementsKind)                              \
  V(ElementsPointer)                           \
  V(GlobalVars)                                \
  V(InobjectFields)                            \
  V(Maps)                                      \
  V(OsrEntries)                                \
  V(ExternalMemory)                            \
  V(StringChars)                               \
  V(TypedArrayElements)


#define DECLARE_ABSTRACT_INSTRUCTION(type)     \
  bool Is##type() const final { return true; } \
  static H##type* cast(HValue* value) {        \
    DCHECK(value->Is##type());                 \
    return reinterpret_cast<H##type*>(value);  \
  }


#define DECLARE_CONCRETE_INSTRUCTION(type)                      \
  LInstruction* CompileToLithium(LChunkBuilder* builder) final; \
  static H##type* cast(HValue* value) {                         \
    DCHECK(value->Is##type());                                  \
    return reinterpret_cast<H##type*>(value);                   \
  }                                                             \
  Opcode opcode() const final { return HValue::k##type; }


enum PropertyAccessType { LOAD, STORE };

Representation RepresentationFromMachineType(MachineType type);

class Range final : public ZoneObject {
 public:
  Range()
      : lower_(kMinInt),
        upper_(kMaxInt),
        next_(NULL),
        can_be_minus_zero_(false) { }

  Range(int32_t lower, int32_t upper)
      : lower_(lower),
        upper_(upper),
        next_(NULL),
        can_be_minus_zero_(false) { }

  int32_t upper() const { return upper_; }
  int32_t lower() const { return lower_; }
  Range* next() const { return next_; }
  Range* CopyClearLower(Zone* zone) const {
    return new(zone) Range(kMinInt, upper_);
  }
  Range* CopyClearUpper(Zone* zone) const {
    return new(zone) Range(lower_, kMaxInt);
  }
  Range* Copy(Zone* zone) const {
    Range* result = new(zone) Range(lower_, upper_);
    result->set_can_be_minus_zero(CanBeMinusZero());
    return result;
  }
  int32_t Mask() const;
  void set_can_be_minus_zero(bool b) { can_be_minus_zero_ = b; }
  bool CanBeMinusZero() const { return CanBeZero() && can_be_minus_zero_; }
  bool CanBeZero() const { return upper_ >= 0 && lower_ <= 0; }
  bool CanBeNegative() const { return lower_ < 0; }
  bool CanBePositive() const { return upper_ > 0; }
  bool Includes(int value) const { return lower_ <= value && upper_ >= value; }
  bool IsMostGeneric() const {
    return lower_ == kMinInt && upper_ == kMaxInt && CanBeMinusZero();
  }
  bool IsInSmiRange() const {
    return lower_ >= Smi::kMinValue && upper_ <= Smi::kMaxValue;
  }
  void ClampToSmi() {
    lower_ = Max(lower_, Smi::kMinValue);
    upper_ = Min(upper_, Smi::kMaxValue);
  }
  void Clear();
  void KeepOrder();
#ifdef DEBUG
  void Verify() const;
#endif

  void StackUpon(Range* other) {
    Intersect(other);
    next_ = other;
  }

  void Intersect(Range* other);
  void Union(Range* other);
  void CombinedMax(Range* other);
  void CombinedMin(Range* other);

  void AddConstant(int32_t value);
  void Sar(int32_t value);
  void Shl(int32_t value);
  bool AddAndCheckOverflow(const Representation& r, Range* other);
  bool SubAndCheckOverflow(const Representation& r, Range* other);
  bool MulAndCheckOverflow(const Representation& r, Range* other);

 private:
  int32_t lower_;
  int32_t upper_;
  Range* next_;
  bool can_be_minus_zero_;
};


class HUseListNode: public ZoneObject {
 public:
  HUseListNode(HValue* value, int index, HUseListNode* tail)
      : tail_(tail), value_(value), index_(index) {
  }

  HUseListNode* tail();
  HValue* value() const { return value_; }
  int index() const { return index_; }

  void set_tail(HUseListNode* list) { tail_ = list; }

#ifdef DEBUG
  void Zap() {
    tail_ = reinterpret_cast<HUseListNode*>(1);
    value_ = NULL;
    index_ = -1;
  }
#endif

 private:
  HUseListNode* tail_;
  HValue* value_;
  int index_;
};


// We reuse use list nodes behind the scenes as uses are added and deleted.
// This class is the safe way to iterate uses while deleting them.
class HUseIterator final BASE_EMBEDDED {
 public:
  bool Done() { return current_ == NULL; }
  void Advance();

  HValue* value() {
    DCHECK(!Done());
    return value_;
  }

  int index() {
    DCHECK(!Done());
    return index_;
  }

 private:
  explicit HUseIterator(HUseListNode* head);

  HUseListNode* current_;
  HUseListNode* next_;
  HValue* value_;
  int index_;

  friend class HValue;
};


// All tracked flags should appear before untracked ones.
enum GVNFlag {
  // Declare global value numbering flags.
#define DECLARE_FLAG(Type) k##Type,
  GVN_TRACKED_FLAG_LIST(DECLARE_FLAG)
  GVN_UNTRACKED_FLAG_LIST(DECLARE_FLAG)
#undef DECLARE_FLAG
#define COUNT_FLAG(Type) + 1
  kNumberOfTrackedSideEffects = 0 GVN_TRACKED_FLAG_LIST(COUNT_FLAG),
  kNumberOfUntrackedSideEffects = 0 GVN_UNTRACKED_FLAG_LIST(COUNT_FLAG),
#undef COUNT_FLAG
  kNumberOfFlags = kNumberOfTrackedSideEffects + kNumberOfUntrackedSideEffects
};


static inline GVNFlag GVNFlagFromInt(int i) {
  DCHECK(i >= 0);
  DCHECK(i < kNumberOfFlags);
  return static_cast<GVNFlag>(i);
}


class DecompositionResult final BASE_EMBEDDED {
 public:
  DecompositionResult() : base_(NULL), offset_(0), scale_(0) {}

  HValue* base() { return base_; }
  int offset() { return offset_; }
  int scale() { return scale_; }

  bool Apply(HValue* other_base, int other_offset, int other_scale = 0) {
    if (base_ == NULL) {
      base_ = other_base;
      offset_ = other_offset;
      scale_ = other_scale;
      return true;
    } else {
      if (scale_ == 0) {
        base_ = other_base;
        offset_ += other_offset;
        scale_ = other_scale;
        return true;
      } else {
        return false;
      }
    }
  }

  void SwapValues(HValue** other_base, int* other_offset, int* other_scale) {
    swap(&base_, other_base);
    swap(&offset_, other_offset);
    swap(&scale_, other_scale);
  }

 private:
  template <class T> void swap(T* a, T* b) {
    T c(*a);
    *a = *b;
    *b = c;
  }

  HValue* base_;
  int offset_;
  int scale_;
};


typedef EnumSet<GVNFlag, int32_t> GVNFlagSet;


class HValue : public ZoneObject {
 public:
  static const int kNoNumber = -1;

  enum Flag {
    kFlexibleRepresentation,
    kCannotBeTagged,
    // Participate in Global Value Numbering, i.e. elimination of
    // unnecessary recomputations. If an instruction sets this flag, it must
    // implement DataEquals(), which will be used to determine if other
    // occurrences of the instruction are indeed the same.
    kUseGVN,
    // Track instructions that are dominating side effects. If an instruction
    // sets this flag, it must implement HandleSideEffectDominator() and should
    // indicate which side effects to track by setting GVN flags.
    kTrackSideEffectDominators,
    kCanOverflow,
    kBailoutOnMinusZero,
    kCanBeDivByZero,
    kLeftCanBeMinInt,
    kLeftCanBeNegative,
    kLeftCanBePositive,
    kTruncatingToNumber,
    kIsArguments,
    kTruncatingToInt32,
    kAllUsesTruncatingToInt32,
    kTruncatingToSmi,
    kAllUsesTruncatingToSmi,
    // Set after an instruction is killed.
    kIsDead,
    // Instructions that are allowed to produce full range unsigned integer
    // values are marked with kUint32 flag. If arithmetic shift or a load from
    // EXTERNAL_UINT32_ELEMENTS array is not marked with this flag
    // it will deoptimize if result does not fit into signed integer range.
    // HGraph::ComputeSafeUint32Operations is responsible for setting this
    // flag.
    kUint32,
    kHasNoObservableSideEffects,
    // Indicates an instruction shouldn't be replaced by optimization, this flag
    // is useful to set in cases where recomputing a value is cheaper than
    // extending the value's live range and spilling it.
    kCantBeReplaced,
    // Indicates the instruction is live during dead code elimination.
    kIsLive,

    // HEnvironmentMarkers are deleted before dead code
    // elimination takes place, so they can repurpose the kIsLive flag:
    kEndsLiveRange = kIsLive,

    // TODO(everyone): Don't forget to update this!
    kLastFlag = kIsLive
  };

  STATIC_ASSERT(kLastFlag < kBitsPerInt);

  static HValue* cast(HValue* value) { return value; }

  enum Opcode {
    // Declare a unique enum value for each hydrogen instruction.
  #define DECLARE_OPCODE(type) k##type,
    HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_OPCODE)
    kPhi
  #undef DECLARE_OPCODE
  };
  virtual Opcode opcode() const = 0;

  // Declare a non-virtual predicates for each concrete HInstruction or HValue.
  #define DECLARE_PREDICATE(type) \
    bool Is##type() const { return opcode() == k##type; }
    HYDROGEN_CONCRETE_INSTRUCTION_LIST(DECLARE_PREDICATE)
  #undef DECLARE_PREDICATE
    bool IsPhi() const { return opcode() == kPhi; }

  // Declare virtual predicates for abstract HInstruction or HValue
  #define DECLARE_PREDICATE(type) \
    virtual bool Is##type() const { return false; }
    HYDROGEN_ABSTRACT_INSTRUCTION_LIST(DECLARE_PREDICATE)
  #undef DECLARE_PREDICATE

  bool IsBitwiseBinaryShift() {
    return IsShl() || IsShr() || IsSar();
  }

  explicit HValue(HType type = HType::Tagged())
      : block_(NULL),
        id_(kNoNumber),
        type_(type),
        use_list_(NULL),
        range_(NULL),
#ifdef DEBUG
        range_poisoned_(false),
#endif
        flags_(0) {}
  virtual ~HValue() {}

  virtual SourcePosition position() const { return SourcePosition::Unknown(); }

  HBasicBlock* block() const { return block_; }
  void SetBlock(HBasicBlock* block);

  // Note: Never call this method for an unlinked value.
  Isolate* isolate() const;

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  HUseIterator uses() const { return HUseIterator(use_list_); }

  virtual bool EmitAtUses() { return false; }

  Representation representation() const { return representation_; }
  void ChangeRepresentation(Representation r) {
    DCHECK(CheckFlag(kFlexibleRepresentation));
    DCHECK(!CheckFlag(kCannotBeTagged) || !r.IsTagged());
    RepresentationChanged(r);
    representation_ = r;
    if (r.IsTagged()) {
      // Tagged is the bottom of the lattice, don't go any further.
      ClearFlag(kFlexibleRepresentation);
    }
  }
  virtual void AssumeRepresentation(Representation r);

  virtual Representation KnownOptimalRepresentation() {
    Representation r = representation();
    if (r.IsTagged()) {
      HType t = type();
      if (t.IsSmi()) return Representation::Smi();
      if (t.IsHeapNumber()) return Representation::Double();
      if (t.IsHeapObject()) return r;
      return Representation::None();
    }
    return r;
  }

  HType type() const { return type_; }
  void set_type(HType new_type) {
    DCHECK(new_type.IsSubtypeOf(type_));
    type_ = new_type;
  }

  // There are HInstructions that do not really change a value, they
  // only add pieces of information to it (like bounds checks, map checks,
  // smi checks...).
  // We call these instructions "informative definitions", or "iDef".
  // One of the iDef operands is special because it is the value that is
  // "transferred" to the output, we call it the "redefined operand".
  // If an HValue is an iDef it must override RedefinedOperandIndex() so that
  // it does not return kNoRedefinedOperand;
  static const int kNoRedefinedOperand = -1;
  virtual int RedefinedOperandIndex() { return kNoRedefinedOperand; }
  bool IsInformativeDefinition() {
    return RedefinedOperandIndex() != kNoRedefinedOperand;
  }
  HValue* RedefinedOperand() {
    int index = RedefinedOperandIndex();
    return index == kNoRedefinedOperand ? NULL : OperandAt(index);
  }

  bool CanReplaceWithDummyUses();

  virtual int argument_delta() const { return 0; }

  // A purely informative definition is an idef that will not emit code and
  // should therefore be removed from the graph in the RestoreActualValues
  // phase (so that live ranges will be shorter).
  virtual bool IsPurelyInformativeDefinition() { return false; }

  // This method must always return the original HValue SSA definition,
  // regardless of any chain of iDefs of this value.
  HValue* ActualValue() {
    HValue* value = this;
    int index;
    while ((index = value->RedefinedOperandIndex()) != kNoRedefinedOperand) {
      value = value->OperandAt(index);
    }
    return value;
  }

  bool IsInteger32Constant();
  int32_t GetInteger32Constant();
  bool EqualsInteger32Constant(int32_t value);

  bool IsDefinedAfter(HBasicBlock* other) const;

  // Operands.
  virtual int OperandCount() const = 0;
  virtual HValue* OperandAt(int index) const = 0;
  void SetOperandAt(int index, HValue* value);

  void DeleteAndReplaceWith(HValue* other);
  void ReplaceAllUsesWith(HValue* other);
  bool HasNoUses() const { return use_list_ == NULL; }
  bool HasOneUse() const {
    return use_list_ != NULL && use_list_->tail() == NULL;
  }
  bool HasMultipleUses() const {
    return use_list_ != NULL && use_list_->tail() != NULL;
  }
  int UseCount() const;

  // Mark this HValue as dead and to be removed from other HValues' use lists.
  void Kill();

  int flags() const { return flags_; }
  void SetFlag(Flag f) { flags_ |= (1 << f); }
  void ClearFlag(Flag f) { flags_ &= ~(1 << f); }
  bool CheckFlag(Flag f) const { return (flags_ & (1 << f)) != 0; }
  void CopyFlag(Flag f, HValue* other) {
    if (other->CheckFlag(f)) SetFlag(f);
  }

  // Returns true if the flag specified is set for all uses, false otherwise.
  bool CheckUsesForFlag(Flag f) const;
  // Same as before and the first one without the flag is returned in value.
  bool CheckUsesForFlag(Flag f, HValue** value) const;
  // Returns true if the flag specified is set for all uses, and this set
  // of uses is non-empty.
  bool HasAtLeastOneUseWithFlagAndNoneWithout(Flag f) const;

  GVNFlagSet ChangesFlags() const { return changes_flags_; }
  GVNFlagSet DependsOnFlags() const { return depends_on_flags_; }
  void SetChangesFlag(GVNFlag f) { changes_flags_.Add(f); }
  void SetDependsOnFlag(GVNFlag f) { depends_on_flags_.Add(f); }
  void ClearChangesFlag(GVNFlag f) { changes_flags_.Remove(f); }
  void ClearDependsOnFlag(GVNFlag f) { depends_on_flags_.Remove(f); }
  bool CheckChangesFlag(GVNFlag f) const {
    return changes_flags_.Contains(f);
  }
  bool CheckDependsOnFlag(GVNFlag f) const {
    return depends_on_flags_.Contains(f);
  }
  void SetAllSideEffects() { changes_flags_.Add(AllSideEffectsFlagSet()); }
  void ClearAllSideEffects() {
    changes_flags_.Remove(AllSideEffectsFlagSet());
  }
  bool HasSideEffects() const {
    return changes_flags_.ContainsAnyOf(AllSideEffectsFlagSet());
  }
  bool HasObservableSideEffects() const {
    return !CheckFlag(kHasNoObservableSideEffects) &&
        changes_flags_.ContainsAnyOf(AllObservableSideEffectsFlagSet());
  }

  GVNFlagSet SideEffectFlags() const {
    GVNFlagSet result = ChangesFlags();
    result.Intersect(AllSideEffectsFlagSet());
    return result;
  }

  GVNFlagSet ObservableChangesFlags() const {
    GVNFlagSet result = ChangesFlags();
    result.Intersect(AllObservableSideEffectsFlagSet());
    return result;
  }

  Range* range() const {
    DCHECK(!range_poisoned_);
    return range_;
  }
  bool HasRange() const {
    DCHECK(!range_poisoned_);
    return range_ != NULL;
  }
#ifdef DEBUG
  void PoisonRange() { range_poisoned_ = true; }
#endif
  void AddNewRange(Range* r, Zone* zone);
  void RemoveLastAddedRange();
  void ComputeInitialRange(Zone* zone);

  // Escape analysis helpers.
  virtual bool HasEscapingOperandAt(int index) { return true; }
  virtual bool HasOutOfBoundsAccess(int size) { return false; }

  // Representation helpers.
  virtual Representation observed_input_representation(int index) {
    return Representation::None();
  }
  virtual Representation RequiredInputRepresentation(int index) = 0;
  virtual void InferRepresentation(HInferRepresentationPhase* h_infer);

  // This gives the instruction an opportunity to replace itself with an
  // instruction that does the same in some better way.  To replace an
  // instruction with a new one, first add the new instruction to the graph,
  // then return it.  Return NULL to have the instruction deleted.
  virtual HValue* Canonicalize() { return this; }

  bool Equals(HValue* other);
  virtual intptr_t Hashcode();

  // Compute unique ids upfront that is safe wrt GC and concurrent compilation.
  virtual void FinalizeUniqueness() { }

  // Printing support.
  virtual std::ostream& PrintTo(std::ostream& os) const = 0;  // NOLINT

  const char* Mnemonic() const;

  // Type information helpers.
  bool HasMonomorphicJSObjectType();

  // TODO(mstarzinger): For now instructions can override this function to
  // specify statically known types, once HType can convey more information
  // it should be based on the HType.
  virtual Handle<Map> GetMonomorphicJSObjectMap() { return Handle<Map>(); }

  // Updated the inferred type of this instruction and returns true if
  // it has changed.
  bool UpdateInferredType();

  virtual HType CalculateInferredType();

  // This function must be overridden for instructions which have the
  // kTrackSideEffectDominators flag set, to track instructions that are
  // dominating side effects.
  // It returns true if it removed an instruction which had side effects.
  virtual bool HandleSideEffectDominator(GVNFlag side_effect,
                                         HValue* dominator) {
    UNREACHABLE();
    return false;
  }

  // Check if this instruction has some reason that prevents elimination.
  bool CannotBeEliminated() const {
    return HasObservableSideEffects() || !IsDeletable();
  }

#ifdef DEBUG
  virtual void Verify() = 0;
#endif

  // Returns true conservatively if the program might be able to observe a
  // ToString() operation on this value.
  bool ToStringCanBeObserved() const {
    return ToStringOrToNumberCanBeObserved();
  }

  // Returns true conservatively if the program might be able to observe a
  // ToNumber() operation on this value.
  bool ToNumberCanBeObserved() const {
    return ToStringOrToNumberCanBeObserved();
  }

  MinusZeroMode GetMinusZeroMode() {
    return CheckFlag(kBailoutOnMinusZero)
        ? FAIL_ON_MINUS_ZERO : TREAT_MINUS_ZERO_AS_ZERO;
  }

 protected:
  // This function must be overridden for instructions with flag kUseGVN, to
  // compare the non-Operand parts of the instruction.
  virtual bool DataEquals(HValue* other) {
    UNREACHABLE();
    return false;
  }

  bool ToStringOrToNumberCanBeObserved() const {
    if (type().IsTaggedPrimitive()) return false;
    if (type().IsJSReceiver()) return true;
    return !representation().IsSmiOrInteger32() && !representation().IsDouble();
  }

  virtual Representation RepresentationFromInputs() {
    return representation();
  }
  virtual Representation RepresentationFromUses();
  Representation RepresentationFromUseRequirements();
  bool HasNonSmiUse();
  virtual void UpdateRepresentation(Representation new_rep,
                                    HInferRepresentationPhase* h_infer,
                                    const char* reason);
  void AddDependantsToWorklist(HInferRepresentationPhase* h_infer);

  virtual void RepresentationChanged(Representation to) { }

  virtual Range* InferRange(Zone* zone);
  virtual void DeleteFromGraph() = 0;
  virtual void InternalSetOperandAt(int index, HValue* value) = 0;
  void clear_block() {
    DCHECK(block_ != NULL);
    block_ = NULL;
  }

  void set_representation(Representation r) {
    DCHECK(representation_.IsNone() && !r.IsNone());
    representation_ = r;
  }

  static GVNFlagSet AllFlagSet() {
    GVNFlagSet result;
#define ADD_FLAG(Type) result.Add(k##Type);
  GVN_TRACKED_FLAG_LIST(ADD_FLAG)
  GVN_UNTRACKED_FLAG_LIST(ADD_FLAG)
#undef ADD_FLAG
    return result;
  }

  // A flag mask to mark an instruction as having arbitrary side effects.
  static GVNFlagSet AllSideEffectsFlagSet() {
    GVNFlagSet result = AllFlagSet();
    result.Remove(kOsrEntries);
    return result;
  }
  friend std::ostream& operator<<(std::ostream& os, const ChangesOf& v);

  // A flag mask of all side effects that can make observable changes in
  // an executing program (i.e. are not safe to repeat, move or remove);
  static GVNFlagSet AllObservableSideEffectsFlagSet() {
    GVNFlagSet result = AllFlagSet();
    result.Remove(kNewSpacePromotion);
    result.Remove(kElementsKind);
    result.Remove(kElementsPointer);
    result.Remove(kMaps);
    return result;
  }

  // Remove the matching use from the use list if present.  Returns the
  // removed list node or NULL.
  HUseListNode* RemoveUse(HValue* value, int index);

  void RegisterUse(int index, HValue* new_value);

  HBasicBlock* block_;

  // The id of this instruction in the hydrogen graph, assigned when first
  // added to the graph. Reflects creation order.
  int id_;

  Representation representation_;
  HType type_;
  HUseListNode* use_list_;
  Range* range_;
#ifdef DEBUG
  bool range_poisoned_;
#endif
  int flags_;
  GVNFlagSet changes_flags_;
  GVNFlagSet depends_on_flags_;

 private:
  virtual bool IsDeletable() const { return false; }

  DISALLOW_COPY_AND_ASSIGN(HValue);
};

// Support for printing various aspects of an HValue.
struct NameOf {
  explicit NameOf(const HValue* const v) : value(v) {}
  const HValue* value;
};


struct TypeOf {
  explicit TypeOf(const HValue* const v) : value(v) {}
  const HValue* value;
};


struct ChangesOf {
  explicit ChangesOf(const HValue* const v) : value(v) {}
  const HValue* value;
};


std::ostream& operator<<(std::ostream& os, const HValue& v);
std::ostream& operator<<(std::ostream& os, const NameOf& v);
std::ostream& operator<<(std::ostream& os, const TypeOf& v);
std::ostream& operator<<(std::ostream& os, const ChangesOf& v);


#define DECLARE_INSTRUCTION_FACTORY_P0(I)                        \
  static I* New(Isolate* isolate, Zone* zone, HValue* context) { \
    return new (zone) I();                                       \
  }

#define DECLARE_INSTRUCTION_FACTORY_P1(I, P1)                           \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1) { \
    return new (zone) I(p1);                                            \
  }

#define DECLARE_INSTRUCTION_FACTORY_P2(I, P1, P2)                              \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2) { \
    return new (zone) I(p1, p2);                                               \
  }

#define DECLARE_INSTRUCTION_FACTORY_P3(I, P1, P2, P3)                        \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3) {                                                     \
    return new (zone) I(p1, p2, p3);                                         \
  }

#define DECLARE_INSTRUCTION_FACTORY_P4(I, P1, P2, P3, P4)                    \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3, P4 p4) {                                              \
    return new (zone) I(p1, p2, p3, p4);                                     \
  }

#define DECLARE_INSTRUCTION_FACTORY_P5(I, P1, P2, P3, P4, P5)                \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3, P4 p4, P5 p5) {                                       \
    return new (zone) I(p1, p2, p3, p4, p5);                                 \
  }

#define DECLARE_INSTRUCTION_FACTORY_P6(I, P1, P2, P3, P4, P5, P6)            \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3, P4 p4, P5 p5, P6 p6) {                                \
    return new (zone) I(p1, p2, p3, p4, p5, p6);                             \
  }

#define DECLARE_INSTRUCTION_FACTORY_P7(I, P1, P2, P3, P4, P5, P6, P7)        \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3, P4 p4, P5 p5, P6 p6, P7 p7) {                         \
    return new (zone) I(p1, p2, p3, p4, p5, p6, p7);                         \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P0(I)           \
  static I* New(Isolate* isolate, Zone* zone, HValue* context) { \
    return new (zone) I(context);                                \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(I, P1)              \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1) { \
    return new (zone) I(context, p1);                                   \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(I, P1, P2)                 \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2) { \
    return new (zone) I(context, p1, p2);                                      \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(I, P1, P2, P3)           \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3) {                                                     \
    return new (zone) I(context, p1, p2, p3);                                \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(I, P1, P2, P3, P4)       \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3, P4 p4) {                                              \
    return new (zone) I(context, p1, p2, p3, p4);                            \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P5(I, P1, P2, P3, P4, P5)   \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2, \
                P3 p3, P4 p4, P5 p5) {                                       \
    return new (zone) I(context, p1, p2, p3, p4, p5);                        \
  }

#define DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P6(I, P1, P2, P3, P4, P5, P6) \
  static I* New(Isolate* isolate, Zone* zone, HValue* context, P1 p1, P2 p2,   \
                P3 p3, P4 p4, P5 p5, P6 p6) {                                  \
    return new (zone) I(context, p1, p2, p3, p4, p5, p6);                      \
  }

class HInstruction : public HValue {
 public:
  HInstruction* next() const { return next_; }
  HInstruction* previous() const { return previous_; }

  std::ostream& PrintTo(std::ostream& os) const override;          // NOLINT
  virtual std::ostream& PrintDataTo(std::ostream& os) const;       // NOLINT

  bool IsLinked() const { return block() != NULL; }
  void Unlink();

  void InsertBefore(HInstruction* next);

  template<class T> T* Prepend(T* instr) {
    instr->InsertBefore(this);
    return instr;
  }

  void InsertAfter(HInstruction* previous);

  template<class T> T* Append(T* instr) {
    instr->InsertAfter(this);
    return instr;
  }

  // The position is a write-once variable.
  SourcePosition position() const override { return position_; }
  bool has_position() const { return position_.IsKnown(); }
  void set_position(SourcePosition position) {
    DCHECK(position.IsKnown());
    position_ = position;
  }

  bool Dominates(HInstruction* other);
  bool CanTruncateToSmi() const { return CheckFlag(kTruncatingToSmi); }
  bool CanTruncateToInt32() const { return CheckFlag(kTruncatingToInt32); }
  bool CanTruncateToNumber() const { return CheckFlag(kTruncatingToNumber); }

  virtual LInstruction* CompileToLithium(LChunkBuilder* builder) = 0;

#ifdef DEBUG
  void Verify() override;
#endif

  bool CanDeoptimize();

  virtual bool HasStackCheck() { return false; }

  DECLARE_ABSTRACT_INSTRUCTION(Instruction)

 protected:
  explicit HInstruction(HType type = HType::Tagged())
      : HValue(type),
        next_(NULL),
        previous_(NULL),
        position_(SourcePosition::Unknown()) {
    SetDependsOnFlag(kOsrEntries);
  }

  void DeleteFromGraph() override { Unlink(); }

 private:
  void InitializeAsFirst(HBasicBlock* block) {
    DCHECK(!IsLinked());
    SetBlock(block);
  }

  HInstruction* next_;
  HInstruction* previous_;
  SourcePosition position_;

  friend class HBasicBlock;
};


template<int V>
class HTemplateInstruction : public HInstruction {
 public:
  int OperandCount() const final { return V; }
  HValue* OperandAt(int i) const final { return inputs_[i]; }

 protected:
  explicit HTemplateInstruction(HType type = HType::Tagged())
      : HInstruction(type) {}

  void InternalSetOperandAt(int i, HValue* value) final { inputs_[i] = value; }

 private:
  EmbeddedContainer<HValue*, V> inputs_;
};


class HControlInstruction : public HInstruction {
 public:
  virtual HBasicBlock* SuccessorAt(int i) const = 0;
  virtual int SuccessorCount() const = 0;
  virtual void SetSuccessorAt(int i, HBasicBlock* block) = 0;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  virtual bool KnownSuccessorBlock(HBasicBlock** block) {
    *block = NULL;
    return false;
  }

  HBasicBlock* FirstSuccessor() {
    return SuccessorCount() > 0 ? SuccessorAt(0) : NULL;
  }
  HBasicBlock* SecondSuccessor() {
    return SuccessorCount() > 1 ? SuccessorAt(1) : NULL;
  }

  void Not() {
    HBasicBlock* swap = SuccessorAt(0);
    SetSuccessorAt(0, SuccessorAt(1));
    SetSuccessorAt(1, swap);
  }

  DECLARE_ABSTRACT_INSTRUCTION(ControlInstruction)
};


class HSuccessorIterator final BASE_EMBEDDED {
 public:
  explicit HSuccessorIterator(const HControlInstruction* instr)
      : instr_(instr), current_(0) {}

  bool Done() { return current_ >= instr_->SuccessorCount(); }
  HBasicBlock* Current() { return instr_->SuccessorAt(current_); }
  void Advance() { current_++; }

 private:
  const HControlInstruction* instr_;
  int current_;
};


template<int S, int V>
class HTemplateControlInstruction : public HControlInstruction {
 public:
  int SuccessorCount() const override { return S; }
  HBasicBlock* SuccessorAt(int i) const override { return successors_[i]; }
  void SetSuccessorAt(int i, HBasicBlock* block) override {
    successors_[i] = block;
  }

  int OperandCount() const override { return V; }
  HValue* OperandAt(int i) const override { return inputs_[i]; }


 protected:
  void InternalSetOperandAt(int i, HValue* value) override {
    inputs_[i] = value;
  }

 private:
  EmbeddedContainer<HBasicBlock*, S> successors_;
  EmbeddedContainer<HValue*, V> inputs_;
};


class HBlockEntry final : public HTemplateInstruction<0> {
 public:
  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(BlockEntry)
};


class HDummyUse final : public HTemplateInstruction<1> {
 public:
  explicit HDummyUse(HValue* value)
      : HTemplateInstruction<1>(HType::Smi()) {
    SetOperandAt(0, value);
    // Pretend to be a Smi so that the HChange instructions inserted
    // before any use generate as little code as possible.
    set_representation(Representation::Tagged());
  }

  HValue* value() const { return OperandAt(0); }

  bool HasEscapingOperandAt(int index) override { return false; }
  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(DummyUse);
};


// Inserts an int3/stop break instruction for debugging purposes.
class HDebugBreak final : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HDebugBreak);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(DebugBreak)
};


class HPrologue final : public HTemplateInstruction<0> {
 public:
  static HPrologue* New(Zone* zone) { return new (zone) HPrologue(); }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Prologue)
};


class HGoto final : public HTemplateControlInstruction<1, 0> {
 public:
  explicit HGoto(HBasicBlock* target) {
    SetSuccessorAt(0, target);
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override {
    *block = FirstSuccessor();
    return true;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Goto)
};


class HDeoptimize final : public HTemplateControlInstruction<1, 0> {
 public:
  static HDeoptimize* New(Isolate* isolate, Zone* zone, HValue* context,
                          DeoptimizeReason reason,
                          Deoptimizer::BailoutType type,
                          HBasicBlock* unreachable_continuation) {
    return new(zone) HDeoptimize(reason, type, unreachable_continuation);
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override {
    *block = NULL;
    return true;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DeoptimizeReason reason() const { return reason_; }
  Deoptimizer::BailoutType type() { return type_; }

  DECLARE_CONCRETE_INSTRUCTION(Deoptimize)

 private:
  explicit HDeoptimize(DeoptimizeReason reason, Deoptimizer::BailoutType type,
                       HBasicBlock* unreachable_continuation)
      : reason_(reason), type_(type) {
    SetSuccessorAt(0, unreachable_continuation);
  }

  DeoptimizeReason reason_;
  Deoptimizer::BailoutType type_;
};


class HUnaryControlInstruction : public HTemplateControlInstruction<2, 1> {
 public:
  HUnaryControlInstruction(HValue* value,
                           HBasicBlock* true_target,
                           HBasicBlock* false_target) {
    SetOperandAt(0, value);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* value() const { return OperandAt(0); }
};


class HBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P2(HBranch, HValue*, ToBooleanHints);
  DECLARE_INSTRUCTION_FACTORY_P4(HBranch, HValue*, ToBooleanHints, HBasicBlock*,
                                 HBasicBlock*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }
  Representation observed_input_representation(int index) override;

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  ToBooleanHints expected_input_types() const { return expected_input_types_; }

  DECLARE_CONCRETE_INSTRUCTION(Branch)

 private:
  HBranch(HValue* value,
          ToBooleanHints expected_input_types = ToBooleanHint::kNone,
          HBasicBlock* true_target = NULL, HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        expected_input_types_(expected_input_types) {}

  ToBooleanHints expected_input_types_;
};


class HCompareMap final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCompareMap, HValue*, Handle<Map>);
  DECLARE_INSTRUCTION_FACTORY_P4(HCompareMap, HValue*, Handle<Map>,
                                 HBasicBlock*, HBasicBlock*);

  bool KnownSuccessorBlock(HBasicBlock** block) override {
    if (known_successor_index() != kNoKnownSuccessorIndex) {
      *block = SuccessorAt(known_successor_index());
      return true;
    }
    *block = NULL;
    return false;
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const {
    return KnownSuccessorIndexField::decode(bit_field_) -
           kInternalKnownSuccessorOffset;
  }
  void set_known_successor_index(int index) {
    DCHECK(index >= 0 - kInternalKnownSuccessorOffset);
    bit_field_ = KnownSuccessorIndexField::update(
        bit_field_, index + kInternalKnownSuccessorOffset);
  }

  Unique<Map> map() const { return map_; }
  bool map_is_stable() const { return MapIsStableField::decode(bit_field_); }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareMap)

 protected:
  int RedefinedOperandIndex() override { return 0; }

 private:
  HCompareMap(HValue* value, Handle<Map> map, HBasicBlock* true_target = NULL,
              HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        bit_field_(KnownSuccessorIndexField::encode(
                       kNoKnownSuccessorIndex + kInternalKnownSuccessorOffset) |
                   MapIsStableField::encode(map->is_stable())),
        map_(Unique<Map>::CreateImmovable(map)) {
    set_representation(Representation::Tagged());
  }

  // BitFields can only store unsigned values, so use an offset.
  // Adding kInternalKnownSuccessorOffset must yield an unsigned value.
  static const int kInternalKnownSuccessorOffset = 1;
  STATIC_ASSERT(kNoKnownSuccessorIndex + kInternalKnownSuccessorOffset >= 0);

  class KnownSuccessorIndexField : public BitField<int, 0, 31> {};
  class MapIsStableField : public BitField<bool, 31, 1> {};

  uint32_t bit_field_;
  Unique<Map> map_;
};


class HContext final : public HTemplateInstruction<0> {
 public:
  static HContext* New(Zone* zone) {
    return new(zone) HContext();
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(Context)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HContext() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return true; }
};


class HReturn final : public HTemplateControlInstruction<0, 3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HReturn, HValue*, HValue*);
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HReturn, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    // TODO(titzer): require an Int32 input for faster returns.
    if (index == 2) return Representation::Smi();
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* value() const { return OperandAt(0); }
  HValue* context() const { return OperandAt(1); }
  HValue* parameter_count() const { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(Return)

 private:
  HReturn(HValue* context, HValue* value, HValue* parameter_count = 0) {
    SetOperandAt(0, value);
    SetOperandAt(1, context);
    SetOperandAt(2, parameter_count);
  }
};


class HAbnormalExit final : public HTemplateControlInstruction<0, 0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HAbnormalExit);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(AbnormalExit)
 private:
  HAbnormalExit() {}
};


class HUnaryOperation : public HTemplateInstruction<1> {
 public:
  explicit HUnaryOperation(HValue* value, HType type = HType::Tagged())
      : HTemplateInstruction<1>(type) {
    SetOperandAt(0, value);
  }

  static HUnaryOperation* cast(HValue* value) {
    return reinterpret_cast<HUnaryOperation*>(value);
  }

  HValue* value() const { return OperandAt(0); }
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT
};


class HUseConst final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HUseConst, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(UseConst)

 private:
    explicit HUseConst(HValue* old_value) : HUnaryOperation(old_value) { }
};


class HForceRepresentation final : public HTemplateInstruction<1> {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* value,
                           Representation required_representation);

  HValue* value() const { return OperandAt(0); }

  Representation observed_input_representation(int index) override {
    // We haven't actually *observed* this, but it's closer to the truth
    // than 'None'.
    return representation();  // Same as the output representation.
  }
  Representation RequiredInputRepresentation(int index) override {
    return representation();  // Same as the output representation.
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(ForceRepresentation)

 private:
  HForceRepresentation(HValue* value, Representation required_representation) {
    SetOperandAt(0, value);
    set_representation(required_representation);
  }
};

class HChange final : public HUnaryOperation {
 public:
  HChange(HValue* value, Representation to, bool is_truncating_to_smi,
          bool is_truncating_to_int32, bool is_truncating_to_number)
      : HUnaryOperation(value) {
    DCHECK(!value->representation().IsNone());
    DCHECK(!to.IsNone());
    DCHECK(!value->representation().Equals(to));
    set_representation(to);
    SetFlag(kUseGVN);
    SetFlag(kCanOverflow);
    if (is_truncating_to_smi && to.IsSmi()) {
      SetFlag(kTruncatingToSmi);
      SetFlag(kTruncatingToInt32);
      SetFlag(kTruncatingToNumber);
    } else if (is_truncating_to_int32) {
      SetFlag(kTruncatingToInt32);
      SetFlag(kTruncatingToNumber);
    } else if (is_truncating_to_number) {
      SetFlag(kTruncatingToNumber);
    }
    if (value->representation().IsSmi() || value->type().IsSmi()) {
      set_type(HType::Smi());
    } else {
      set_type(HType::TaggedNumber());
      if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
    }
  }

  HType CalculateInferredType() override;
  HValue* Canonicalize() override;

  Representation from() const { return value()->representation(); }
  Representation to() const { return representation(); }
  bool deoptimize_on_minus_zero() const {
    return CheckFlag(kBailoutOnMinusZero);
  }
  Representation RequiredInputRepresentation(int index) override {
    return from();
  }

  Range* InferRange(Zone* zone) override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Change)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  bool IsDeletable() const override {
    return !from().IsTagged() || value()->type().IsSmi();
  }
};


class HClampToUint8 final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HClampToUint8, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ClampToUint8)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HClampToUint8(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kTruncatingToNumber);
    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return true; }
};


enum RemovableSimulate {
  REMOVABLE_SIMULATE,
  FIXED_SIMULATE
};


class HSimulate final : public HInstruction {
 public:
  HSimulate(BailoutId ast_id, int pop_count, Zone* zone,
            RemovableSimulate removable)
      : ast_id_(ast_id),
        pop_count_(pop_count),
        values_(2, zone),
        assigned_indexes_(2, zone),
        zone_(zone),
        bit_field_(RemovableField::encode(removable) |
                   DoneWithReplayField::encode(false)) {}
  ~HSimulate() {}

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  bool HasAstId() const { return !ast_id_.IsNone(); }
  BailoutId ast_id() const { return ast_id_; }
  void set_ast_id(BailoutId id) {
    DCHECK(!HasAstId());
    ast_id_ = id;
  }

  int pop_count() const { return pop_count_; }
  const ZoneList<HValue*>* values() const { return &values_; }
  int GetAssignedIndexAt(int index) const {
    DCHECK(HasAssignedIndexAt(index));
    return assigned_indexes_[index];
  }
  bool HasAssignedIndexAt(int index) const {
    return assigned_indexes_[index] != kNoIndex;
  }
  void AddAssignedValue(int index, HValue* value) {
    AddValue(index, value);
  }
  void AddPushedValue(HValue* value) {
    AddValue(kNoIndex, value);
  }
  int ToOperandIndex(int environment_index) {
    for (int i = 0; i < assigned_indexes_.length(); ++i) {
      if (assigned_indexes_[i] == environment_index) return i;
    }
    return -1;
  }
  int OperandCount() const override { return values_.length(); }
  HValue* OperandAt(int index) const override { return values_[index]; }

  bool HasEscapingOperandAt(int index) override { return false; }
  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  void MergeWith(ZoneList<HSimulate*>* list);
  bool is_candidate_for_removal() {
    return RemovableField::decode(bit_field_) == REMOVABLE_SIMULATE;
  }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  DECLARE_CONCRETE_INSTRUCTION(Simulate)

#ifdef DEBUG
  void Verify() override;
  void set_closure(Handle<JSFunction> closure) { closure_ = closure; }
  Handle<JSFunction> closure() const { return closure_; }
#endif

 protected:
  void InternalSetOperandAt(int index, HValue* value) override {
    values_[index] = value;
  }

 private:
  static const int kNoIndex = -1;
  void AddValue(int index, HValue* value) {
    assigned_indexes_.Add(index, zone_);
    // Resize the list of pushed values.
    values_.Add(NULL, zone_);
    // Set the operand through the base method in HValue to make sure that the
    // use lists are correctly updated.
    SetOperandAt(values_.length() - 1, value);
  }
  bool HasValueForIndex(int index) {
    for (int i = 0; i < assigned_indexes_.length(); ++i) {
      if (assigned_indexes_[i] == index) return true;
    }
    return false;
  }
  bool is_done_with_replay() const {
    return DoneWithReplayField::decode(bit_field_);
  }
  void set_done_with_replay() {
    bit_field_ = DoneWithReplayField::update(bit_field_, true);
  }

  class RemovableField : public BitField<RemovableSimulate, 0, 1> {};
  class DoneWithReplayField : public BitField<bool, 1, 1> {};

  BailoutId ast_id_;
  int pop_count_;
  ZoneList<HValue*> values_;
  ZoneList<int> assigned_indexes_;
  Zone* zone_;
  uint32_t bit_field_;

#ifdef DEBUG
  Handle<JSFunction> closure_;
#endif
};


class HEnvironmentMarker final : public HTemplateInstruction<1> {
 public:
  enum Kind { BIND, LOOKUP };

  DECLARE_INSTRUCTION_FACTORY_P2(HEnvironmentMarker, Kind, int);

  Kind kind() const { return kind_; }
  int index() const { return index_; }
  HSimulate* next_simulate() { return next_simulate_; }
  void set_next_simulate(HSimulate* simulate) {
    next_simulate_ = simulate;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

#ifdef DEBUG
  void set_closure(Handle<JSFunction> closure) {
    DCHECK(closure_.is_null());
    DCHECK(!closure.is_null());
    closure_ = closure;
  }
  Handle<JSFunction> closure() const { return closure_; }
#endif

  DECLARE_CONCRETE_INSTRUCTION(EnvironmentMarker);

 private:
  HEnvironmentMarker(Kind kind, int index)
      : kind_(kind), index_(index), next_simulate_(NULL) { }

  Kind kind_;
  int index_;
  HSimulate* next_simulate_;

#ifdef DEBUG
  Handle<JSFunction> closure_;
#endif
};


class HStackCheck final : public HTemplateInstruction<1> {
 public:
  enum Type {
    kFunctionEntry,
    kBackwardsBranch
  };

  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HStackCheck, Type);

  HValue* context() { return OperandAt(0); }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  void Eliminate() {
    // The stack check eliminator might try to eliminate the same stack
    // check instruction multiple times.
    if (IsLinked()) {
      DeleteAndReplaceWith(NULL);
    }
  }

  bool is_function_entry() { return type_ == kFunctionEntry; }
  bool is_backwards_branch() { return type_ == kBackwardsBranch; }

  DECLARE_CONCRETE_INSTRUCTION(StackCheck)

 private:
  HStackCheck(HValue* context, Type type) : type_(type) {
    SetOperandAt(0, context);
    SetChangesFlag(kNewSpacePromotion);
  }

  Type type_;
};


enum InliningKind {
  NORMAL_RETURN,          // Drop the function from the environment on return.
  CONSTRUCT_CALL_RETURN,  // Either use allocated receiver or return value.
  GETTER_CALL_RETURN,     // Returning from a getter, need to restore context.
  SETTER_CALL_RETURN      // Use the RHS of the assignment as the return value.
};


class HArgumentsObject;
class HConstant;


class HEnterInlined final : public HTemplateInstruction<0> {
 public:
  static HEnterInlined* New(Isolate* isolate, Zone* zone, HValue* context,
                            BailoutId return_id, Handle<JSFunction> closure,
                            HConstant* closure_context, int arguments_count,
                            FunctionLiteral* function,
                            InliningKind inlining_kind, Variable* arguments_var,
                            HArgumentsObject* arguments_object,
                            TailCallMode syntactic_tail_call_mode) {
    return new (zone)
        HEnterInlined(return_id, closure, closure_context, arguments_count,
                      function, inlining_kind, arguments_var, arguments_object,
                      syntactic_tail_call_mode, zone);
  }

  void RegisterReturnTarget(HBasicBlock* return_target, Zone* zone);
  ZoneList<HBasicBlock*>* return_targets() { return &return_targets_; }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Handle<SharedFunctionInfo> shared() const { return shared_; }
  Handle<JSFunction> closure() const { return closure_; }
  HConstant* closure_context() const { return closure_context_; }
  int arguments_count() const { return arguments_count_; }
  bool arguments_pushed() const { return arguments_pushed_; }
  void set_arguments_pushed() { arguments_pushed_ = true; }
  FunctionLiteral* function() const { return function_; }
  InliningKind inlining_kind() const { return inlining_kind_; }
  TailCallMode syntactic_tail_call_mode() const {
    return syntactic_tail_call_mode_;
  }
  BailoutId ReturnId() const { return return_id_; }
  int inlining_id() const { return inlining_id_; }
  void set_inlining_id(int inlining_id) { inlining_id_ = inlining_id; }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  Variable* arguments_var() { return arguments_var_; }
  HArgumentsObject* arguments_object() { return arguments_object_; }

  DECLARE_CONCRETE_INSTRUCTION(EnterInlined)

 private:
  HEnterInlined(BailoutId return_id, Handle<JSFunction> closure,
                HConstant* closure_context, int arguments_count,
                FunctionLiteral* function, InliningKind inlining_kind,
                Variable* arguments_var, HArgumentsObject* arguments_object,
                TailCallMode syntactic_tail_call_mode, Zone* zone)
      : return_id_(return_id),
        shared_(handle(closure->shared())),
        closure_(closure),
        closure_context_(closure_context),
        arguments_count_(arguments_count),
        arguments_pushed_(false),
        function_(function),
        inlining_kind_(inlining_kind),
        syntactic_tail_call_mode_(syntactic_tail_call_mode),
        inlining_id_(-1),
        arguments_var_(arguments_var),
        arguments_object_(arguments_object),
        return_targets_(2, zone) {}

  BailoutId return_id_;
  Handle<SharedFunctionInfo> shared_;
  Handle<JSFunction> closure_;
  HConstant* closure_context_;
  int arguments_count_;
  bool arguments_pushed_;
  FunctionLiteral* function_;
  InliningKind inlining_kind_;
  TailCallMode syntactic_tail_call_mode_;
  int inlining_id_;
  Variable* arguments_var_;
  HArgumentsObject* arguments_object_;
  ZoneList<HBasicBlock*> return_targets_;
};


class HLeaveInlined final : public HTemplateInstruction<0> {
 public:
  HLeaveInlined(HEnterInlined* entry,
                int drop_count)
      : entry_(entry),
        drop_count_(drop_count) { }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  int argument_delta() const override {
    return entry_->arguments_pushed() ? -drop_count_ : 0;
  }

  DECLARE_CONCRETE_INSTRUCTION(LeaveInlined)

 private:
  HEnterInlined* entry_;
  int drop_count_;
};


class HPushArguments final : public HInstruction {
 public:
  static HPushArguments* New(Isolate* isolate, Zone* zone, HValue* context) {
    return new(zone) HPushArguments(zone);
  }
  static HPushArguments* New(Isolate* isolate, Zone* zone, HValue* context,
                             HValue* arg1) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    return instr;
  }
  static HPushArguments* New(Isolate* isolate, Zone* zone, HValue* context,
                             HValue* arg1, HValue* arg2) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    instr->AddInput(arg2);
    return instr;
  }
  static HPushArguments* New(Isolate* isolate, Zone* zone, HValue* context,
                             HValue* arg1, HValue* arg2, HValue* arg3) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    instr->AddInput(arg2);
    instr->AddInput(arg3);
    return instr;
  }
  static HPushArguments* New(Isolate* isolate, Zone* zone, HValue* context,
                             HValue* arg1, HValue* arg2, HValue* arg3,
                             HValue* arg4) {
    HPushArguments* instr = new(zone) HPushArguments(zone);
    instr->AddInput(arg1);
    instr->AddInput(arg2);
    instr->AddInput(arg3);
    instr->AddInput(arg4);
    return instr;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  int argument_delta() const override { return inputs_.length(); }
  HValue* argument(int i) { return OperandAt(i); }

  int OperandCount() const final { return inputs_.length(); }
  HValue* OperandAt(int i) const final { return inputs_[i]; }

  void AddInput(HValue* value);

  DECLARE_CONCRETE_INSTRUCTION(PushArguments)

 protected:
  void InternalSetOperandAt(int i, HValue* value) final { inputs_[i] = value; }

 private:
  explicit HPushArguments(Zone* zone)
      : HInstruction(HType::Tagged()), inputs_(4, zone) {
    set_representation(Representation::Tagged());
  }

  ZoneList<HValue*> inputs_;
};


class HThisFunction final : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P0(HThisFunction);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(ThisFunction)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HThisFunction() {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return true; }
};


class HDeclareGlobals final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HDeclareGlobals,
                                              Handle<FixedArray>, int,
                                              Handle<TypeFeedbackVector>);

  HValue* context() { return OperandAt(0); }
  Handle<FixedArray> declarations() const { return declarations_; }
  int flags() const { return flags_; }
  Handle<TypeFeedbackVector> feedback_vector() const {
    return feedback_vector_;
  }

  DECLARE_CONCRETE_INSTRUCTION(DeclareGlobals)

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

 private:
  HDeclareGlobals(HValue* context, Handle<FixedArray> declarations, int flags,
                  Handle<TypeFeedbackVector> feedback_vector)
      : HUnaryOperation(context),
        declarations_(declarations),
        feedback_vector_(feedback_vector),
        flags_(flags) {
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Handle<FixedArray> declarations_;
  Handle<TypeFeedbackVector> feedback_vector_;
  int flags_;
};


template <int V>
class HCall : public HTemplateInstruction<V> {
 public:
  // The argument count includes the receiver.
  explicit HCall<V>(int argument_count) : argument_count_(argument_count) {
    this->set_representation(Representation::Tagged());
    this->SetAllSideEffects();
  }

  virtual int argument_count() const {
    return argument_count_;
  }

  int argument_delta() const override { return -argument_count(); }

 private:
  int argument_count_;
};


class HUnaryCall : public HCall<1> {
 public:
  HUnaryCall(HValue* value, int argument_count)
      : HCall<1>(argument_count) {
    SetOperandAt(0, value);
  }

  Representation RequiredInputRepresentation(int index) final {
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* value() const { return OperandAt(0); }
};


class HBinaryCall : public HCall<2> {
 public:
  HBinaryCall(HValue* first, HValue* second, int argument_count)
      : HCall<2>(argument_count) {
    SetOperandAt(0, first);
    SetOperandAt(1, second);
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) final {
    return Representation::Tagged();
  }

  HValue* first() const { return OperandAt(0); }
  HValue* second() const { return OperandAt(1); }
};


class HCallWithDescriptor final : public HInstruction {
 public:
  static HCallWithDescriptor* New(
      Isolate* isolate, Zone* zone, HValue* context, HValue* target,
      int argument_count, CallInterfaceDescriptor descriptor,
      const Vector<HValue*>& operands,
      TailCallMode syntactic_tail_call_mode = TailCallMode::kDisallow,
      TailCallMode tail_call_mode = TailCallMode::kDisallow) {
    HCallWithDescriptor* res = new (zone) HCallWithDescriptor(
        Code::STUB, context, target, argument_count, descriptor, operands,
        syntactic_tail_call_mode, tail_call_mode, zone);
    return res;
  }

  static HCallWithDescriptor* New(
      Isolate* isolate, Zone* zone, HValue* context, Code::Kind kind,
      HValue* target, int argument_count, CallInterfaceDescriptor descriptor,
      const Vector<HValue*>& operands,
      TailCallMode syntactic_tail_call_mode = TailCallMode::kDisallow,
      TailCallMode tail_call_mode = TailCallMode::kDisallow) {
    HCallWithDescriptor* res = new (zone) HCallWithDescriptor(
        kind, context, target, argument_count, descriptor, operands,
        syntactic_tail_call_mode, tail_call_mode, zone);
    return res;
  }

  int OperandCount() const final { return values_.length(); }
  HValue* OperandAt(int index) const final { return values_[index]; }

  Representation RequiredInputRepresentation(int index) final {
    if (index == 0 || index == 1) {
      // Target + context
      return Representation::Tagged();
    } else {
      int par_index = index - 2;
      DCHECK(par_index < GetParameterCount());
      return RepresentationFromMachineType(
          descriptor_.GetParameterType(par_index));
    }
  }

  DECLARE_CONCRETE_INSTRUCTION(CallWithDescriptor)

  // Defines whether this instruction corresponds to a JS call at tail position.
  TailCallMode syntactic_tail_call_mode() const {
    return SyntacticTailCallModeField::decode(bit_field_);
  }

  // Defines whether this call should be generated as a tail call.
  TailCallMode tail_call_mode() const {
    return TailCallModeField::decode(bit_field_);
  }
  bool IsTailCall() const { return tail_call_mode() == TailCallMode::kAllow; }

  Code::Kind kind() const { return KindField::decode(bit_field_); }

  virtual int argument_count() const {
    return argument_count_;
  }

  int argument_delta() const override { return -argument_count_; }

  CallInterfaceDescriptor descriptor() const { return descriptor_; }

  HValue* target() { return OperandAt(0); }
  HValue* context() { return OperandAt(1); }
  HValue* parameter(int index) {
    DCHECK_LT(index, GetParameterCount());
    return OperandAt(index + 2);
  }

  HValue* Canonicalize() override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

 private:
  // The argument count includes the receiver.
  HCallWithDescriptor(Code::Kind kind, HValue* context, HValue* target,
                      int argument_count, CallInterfaceDescriptor descriptor,
                      const Vector<HValue*>& operands,
                      TailCallMode syntactic_tail_call_mode,
                      TailCallMode tail_call_mode, Zone* zone)
      : descriptor_(descriptor),
        values_(GetParameterCount() + 2, zone),  // +2 for context and target.
        argument_count_(argument_count),
        bit_field_(
            TailCallModeField::encode(tail_call_mode) |
            SyntacticTailCallModeField::encode(syntactic_tail_call_mode) |
            KindField::encode(kind)) {
    DCHECK_EQ(operands.length(), GetParameterCount());
    // We can only tail call without any stack arguments.
    DCHECK(tail_call_mode != TailCallMode::kAllow || argument_count == 0);
    AddOperand(target, zone);
    AddOperand(context, zone);
    for (int i = 0; i < operands.length(); i++) {
      AddOperand(operands[i], zone);
    }
    this->set_representation(Representation::Tagged());
    this->SetAllSideEffects();
  }

  void AddOperand(HValue* v, Zone* zone) {
    values_.Add(NULL, zone);
    SetOperandAt(values_.length() - 1, v);
  }

  int GetParameterCount() const { return descriptor_.GetParameterCount(); }

  void InternalSetOperandAt(int index, HValue* value) final {
    values_[index] = value;
  }

  CallInterfaceDescriptor descriptor_;
  ZoneList<HValue*> values_;
  int argument_count_;
  class TailCallModeField : public BitField<TailCallMode, 0, 1> {};
  class SyntacticTailCallModeField
      : public BitField<TailCallMode, TailCallModeField::kNext, 1> {};
  class KindField
      : public BitField<Code::Kind, SyntacticTailCallModeField::kNext, 5> {};
  uint32_t bit_field_;
};


class HInvokeFunction final : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P5(HInvokeFunction, HValue*,
                                              Handle<JSFunction>, int,
                                              TailCallMode, TailCallMode);

  HValue* context() { return first(); }
  HValue* function() { return second(); }
  Handle<JSFunction> known_function() { return known_function_; }
  int formal_parameter_count() const { return formal_parameter_count_; }

  bool HasStackCheck() final { return HasStackCheckField::decode(bit_field_); }

  // Defines whether this instruction corresponds to a JS call at tail position.
  TailCallMode syntactic_tail_call_mode() const {
    return SyntacticTailCallModeField::decode(bit_field_);
  }

  // Defines whether this call should be generated as a tail call.
  TailCallMode tail_call_mode() const {
    return TailCallModeField::decode(bit_field_);
  }

  DECLARE_CONCRETE_INSTRUCTION(InvokeFunction)

  std::ostream& PrintTo(std::ostream& os) const override;      // NOLINT
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

 private:
  void set_has_stack_check(bool has_stack_check) {
    bit_field_ = HasStackCheckField::update(bit_field_, has_stack_check);
  }

  HInvokeFunction(HValue* context, HValue* function,
                  Handle<JSFunction> known_function, int argument_count,
                  TailCallMode syntactic_tail_call_mode,
                  TailCallMode tail_call_mode)
      : HBinaryCall(context, function, argument_count),
        known_function_(known_function),
        bit_field_(
            TailCallModeField::encode(tail_call_mode) |
            SyntacticTailCallModeField::encode(syntactic_tail_call_mode)) {
    DCHECK(tail_call_mode != TailCallMode::kAllow ||
           syntactic_tail_call_mode == TailCallMode::kAllow);
    formal_parameter_count_ =
        known_function.is_null()
            ? 0
            : known_function->shared()->internal_formal_parameter_count();
    set_has_stack_check(
        !known_function.is_null() &&
        (known_function->code()->kind() == Code::FUNCTION ||
         known_function->code()->kind() == Code::OPTIMIZED_FUNCTION));
  }

  Handle<JSFunction> known_function_;
  int formal_parameter_count_;

  class HasStackCheckField : public BitField<bool, 0, 1> {};
  class TailCallModeField
      : public BitField<TailCallMode, HasStackCheckField::kNext, 1> {};
  class SyntacticTailCallModeField
      : public BitField<TailCallMode, TailCallModeField::kNext, 1> {};
  uint32_t bit_field_;
};


class HCallNewArray final : public HBinaryCall {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(HCallNewArray, HValue*, int,
                                              ElementsKind,
                                              Handle<AllocationSite>);

  HValue* context() { return first(); }
  HValue* constructor() { return second(); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  ElementsKind elements_kind() const { return elements_kind_; }
  Handle<AllocationSite> site() const { return site_; }

  DECLARE_CONCRETE_INSTRUCTION(CallNewArray)

 private:
  HCallNewArray(HValue* context, HValue* constructor, int argument_count,
                ElementsKind elements_kind, Handle<AllocationSite> site)
      : HBinaryCall(context, constructor, argument_count),
        elements_kind_(elements_kind),
        site_(site) {}

  ElementsKind elements_kind_;
  Handle<AllocationSite> site_;
};


class HCallRuntime final : public HCall<1> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HCallRuntime,
                                              const Runtime::Function*, int);

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* context() { return OperandAt(0); }
  const Runtime::Function* function() const { return c_function_; }
  SaveFPRegsMode save_doubles() const { return save_doubles_; }
  void set_save_doubles(SaveFPRegsMode save_doubles) {
    save_doubles_ = save_doubles;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CallRuntime)

 private:
  HCallRuntime(HValue* context, const Runtime::Function* c_function,
               int argument_count)
      : HCall<1>(argument_count),
        c_function_(c_function),
        save_doubles_(kDontSaveFPRegs) {
    SetOperandAt(0, context);
  }

  const Runtime::Function* c_function_;
  SaveFPRegsMode save_doubles_;
};


class HUnaryMathOperation final : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* value, BuiltinFunctionId op);

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      switch (op_) {
        case kMathCos:
        case kMathFloor:
        case kMathRound:
        case kMathFround:
        case kMathSin:
        case kMathSqrt:
        case kMathPowHalf:
        case kMathLog:
        case kMathExp:
          return Representation::Double();
        case kMathAbs:
          return representation();
        case kMathClz32:
          return Representation::Integer32();
        default:
          UNREACHABLE();
          return Representation::None();
      }
    }
  }

  Range* InferRange(Zone* zone) override;

  HValue* Canonicalize() override;
  Representation RepresentationFromUses() override;
  Representation RepresentationFromInputs() override;

  BuiltinFunctionId op() const { return op_; }
  const char* OpName() const;

  DECLARE_CONCRETE_INSTRUCTION(UnaryMathOperation)

 protected:
  bool DataEquals(HValue* other) override {
    HUnaryMathOperation* b = HUnaryMathOperation::cast(other);
    return op_ == b->op();
  }

 private:
  // Indicates if we support a double (and int32) output for Math.floor and
  // Math.round.
  bool SupportsFlexibleFloorAndRound() const {
#if V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_PPC
    return true;
#elif V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
    return CpuFeatures::IsSupported(SSE4_1);
#else
    return false;
#endif
  }
  HUnaryMathOperation(HValue* context, HValue* value, BuiltinFunctionId op)
      : HTemplateInstruction<2>(HType::TaggedNumber()), op_(op) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    switch (op) {
      case kMathFloor:
      case kMathRound:
        if (SupportsFlexibleFloorAndRound()) {
          SetFlag(kFlexibleRepresentation);
        } else {
          set_representation(Representation::Integer32());
        }
        break;
      case kMathClz32:
        set_representation(Representation::Integer32());
        break;
      case kMathAbs:
        // Not setting representation here: it is None intentionally.
        SetFlag(kFlexibleRepresentation);
        // TODO(svenpanne) This flag is actually only needed if representation()
        // is tagged, and not when it is an unboxed double or unboxed integer.
        SetChangesFlag(kNewSpacePromotion);
        break;
      case kMathCos:
      case kMathFround:
      case kMathLog:
      case kMathExp:
      case kMathSin:
      case kMathSqrt:
      case kMathPowHalf:
        set_representation(Representation::Double());
        break;
      default:
        UNREACHABLE();
    }
    SetFlag(kUseGVN);
    SetFlag(kTruncatingToNumber);
  }

  bool IsDeletable() const override {
    // TODO(crankshaft): This should be true, however the semantics of this
    // instruction also include the ToNumber conversion that is mentioned in the
    // spec, which is of course observable.
    return false;
  }

  HValue* SimplifiedDividendForMathFloorOfDiv(HDiv* hdiv);
  HValue* SimplifiedDivisorForMathFloorOfDiv(HDiv* hdiv);

  BuiltinFunctionId op_;
};


class HLoadRoot final : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadRoot, Heap::RootListIndex);
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadRoot, Heap::RootListIndex, HType);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  Heap::RootListIndex index() const { return index_; }

  DECLARE_CONCRETE_INSTRUCTION(LoadRoot)

 protected:
  bool DataEquals(HValue* other) override {
    HLoadRoot* b = HLoadRoot::cast(other);
    return index_ == b->index_;
  }

 private:
  explicit HLoadRoot(Heap::RootListIndex index, HType type = HType::Tagged())
      : HTemplateInstruction<0>(type), index_(index) {
    SetFlag(kUseGVN);
    // TODO(bmeurer): We'll need kDependsOnRoots once we add the
    // corresponding HStoreRoot instruction.
    SetDependsOnFlag(kCalls);
    set_representation(Representation::Tagged());
  }

  bool IsDeletable() const override { return true; }

  const Heap::RootListIndex index_;
};


class HCheckMaps final : public HTemplateInstruction<2> {
 public:
  static HCheckMaps* New(Isolate* isolate, Zone* zone, HValue* context,
                         HValue* value, Handle<Map> map,
                         HValue* typecheck = NULL) {
    return new(zone) HCheckMaps(value, new(zone) UniqueSet<Map>(
            Unique<Map>::CreateImmovable(map), zone), typecheck);
  }
  static HCheckMaps* New(Isolate* isolate, Zone* zone, HValue* context,
                         HValue* value, SmallMapList* map_list,
                         HValue* typecheck = NULL) {
    UniqueSet<Map>* maps = new(zone) UniqueSet<Map>(map_list->length(), zone);
    for (int i = 0; i < map_list->length(); ++i) {
      maps->Add(Unique<Map>::CreateImmovable(map_list->at(i)), zone);
    }
    return new(zone) HCheckMaps(value, maps, typecheck);
  }

  bool IsStabilityCheck() const {
    return IsStabilityCheckField::decode(bit_field_);
  }
  void MarkAsStabilityCheck() {
    bit_field_ = MapsAreStableField::encode(true) |
                 HasMigrationTargetField::encode(false) |
                 IsStabilityCheckField::encode(true);
    ClearChangesFlag(kNewSpacePromotion);
    ClearDependsOnFlag(kElementsKind);
    ClearDependsOnFlag(kMaps);
  }

  bool HasEscapingOperandAt(int index) override { return false; }
  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HType CalculateInferredType() override {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* value() const { return OperandAt(0); }
  HValue* typecheck() const { return OperandAt(1); }

  const UniqueSet<Map>* maps() const { return maps_; }
  void set_maps(const UniqueSet<Map>* maps) { maps_ = maps; }

  bool maps_are_stable() const {
    return MapsAreStableField::decode(bit_field_);
  }

  bool HasMigrationTarget() const {
    return HasMigrationTargetField::decode(bit_field_);
  }

  HValue* Canonicalize() override;

  static HCheckMaps* CreateAndInsertAfter(Zone* zone,
                                          HValue* value,
                                          Unique<Map> map,
                                          bool map_is_stable,
                                          HInstruction* instr) {
    return instr->Append(new(zone) HCheckMaps(
            value, new(zone) UniqueSet<Map>(map, zone), map_is_stable));
  }

  static HCheckMaps* CreateAndInsertBefore(Zone* zone,
                                           HValue* value,
                                           const UniqueSet<Map>* maps,
                                           bool maps_are_stable,
                                           HInstruction* instr) {
    return instr->Prepend(new(zone) HCheckMaps(value, maps, maps_are_stable));
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckMaps)

 protected:
  bool DataEquals(HValue* other) override {
    return this->maps()->Equals(HCheckMaps::cast(other)->maps());
  }

  int RedefinedOperandIndex() override { return 0; }

 private:
  HCheckMaps(HValue* value, const UniqueSet<Map>* maps, bool maps_are_stable)
      : HTemplateInstruction<2>(HType::HeapObject()),
        maps_(maps),
        bit_field_(HasMigrationTargetField::encode(false) |
                   IsStabilityCheckField::encode(false) |
                   MapsAreStableField::encode(maps_are_stable)) {
    DCHECK_NE(0, maps->size());
    SetOperandAt(0, value);
    // Use the object value for the dependency.
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
  }

  HCheckMaps(HValue* value, const UniqueSet<Map>* maps, HValue* typecheck)
      : HTemplateInstruction<2>(HType::HeapObject()),
        maps_(maps),
        bit_field_(HasMigrationTargetField::encode(false) |
                   IsStabilityCheckField::encode(false) |
                   MapsAreStableField::encode(true)) {
    DCHECK_NE(0, maps->size());
    SetOperandAt(0, value);
    // Use the object value for the dependency if NULL is passed.
    SetOperandAt(1, typecheck ? typecheck : value);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
    for (int i = 0; i < maps->size(); ++i) {
      Handle<Map> map = maps->at(i).handle();
      if (map->is_migration_target()) {
        bit_field_ = HasMigrationTargetField::update(bit_field_, true);
      }
      if (!map->is_stable()) {
        bit_field_ = MapsAreStableField::update(bit_field_, false);
      }
    }
    if (HasMigrationTarget()) SetChangesFlag(kNewSpacePromotion);
  }

  class HasMigrationTargetField : public BitField<bool, 0, 1> {};
  class IsStabilityCheckField : public BitField<bool, 1, 1> {};
  class MapsAreStableField : public BitField<bool, 2, 1> {};

  const UniqueSet<Map>* maps_;
  uint32_t bit_field_;
};


class HCheckValue final : public HUnaryOperation {
 public:
  static HCheckValue* New(Isolate* isolate, Zone* zone, HValue* context,
                          HValue* value, Handle<JSFunction> func) {
    bool in_new_space = isolate->heap()->InNewSpace(*func);
    // NOTE: We create an uninitialized Unique and initialize it later.
    // This is because a JSFunction can move due to GC during graph creation.
    Unique<JSFunction> target = Unique<JSFunction>::CreateUninitialized(func);
    HCheckValue* check = new(zone) HCheckValue(value, target, in_new_space);
    return check;
  }
  static HCheckValue* New(Isolate* isolate, Zone* zone, HValue* context,
                          HValue* value, Unique<HeapObject> target,
                          bool object_in_new_space) {
    return new(zone) HCheckValue(value, target, object_in_new_space);
  }

  void FinalizeUniqueness() override {
    object_ = Unique<HeapObject>(object_.handle());
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* Canonicalize() override;

#ifdef DEBUG
  void Verify() override;
#endif

  Unique<HeapObject> object() const { return object_; }
  bool object_in_new_space() const { return object_in_new_space_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckValue)

 protected:
  bool DataEquals(HValue* other) override {
    HCheckValue* b = HCheckValue::cast(other);
    return object_ == b->object_;
  }

 private:
  HCheckValue(HValue* value, Unique<HeapObject> object,
               bool object_in_new_space)
      : HUnaryOperation(value, value->type()),
        object_(object),
        object_in_new_space_(object_in_new_space) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  Unique<HeapObject> object_;
  bool object_in_new_space_;
};


class HCheckInstanceType final : public HUnaryOperation {
 public:
  enum Check {
    IS_JS_RECEIVER,
    IS_JS_ARRAY,
    IS_JS_FUNCTION,
    IS_JS_DATE,
    IS_STRING,
    IS_INTERNALIZED_STRING,
    LAST_INTERVAL_CHECK = IS_JS_DATE
  };

  DECLARE_INSTRUCTION_FACTORY_P2(HCheckInstanceType, HValue*, Check);

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HType CalculateInferredType() override {
    switch (check_) {
      case IS_JS_RECEIVER: return HType::JSReceiver();
      case IS_JS_ARRAY: return HType::JSArray();
      case IS_JS_FUNCTION:
        return HType::JSObject();
      case IS_JS_DATE: return HType::JSObject();
      case IS_STRING: return HType::String();
      case IS_INTERNALIZED_STRING: return HType::String();
    }
    UNREACHABLE();
    return HType::Tagged();
  }

  HValue* Canonicalize() override;

  bool is_interval_check() const { return check_ <= LAST_INTERVAL_CHECK; }
  void GetCheckInterval(InstanceType* first, InstanceType* last);
  void GetCheckMaskAndTag(uint8_t* mask, uint8_t* tag);

  Check check() const { return check_; }

  DECLARE_CONCRETE_INSTRUCTION(CheckInstanceType)

 protected:
  // TODO(ager): It could be nice to allow the ommision of instance
  // type checks if we have already performed an instance type check
  // with a larger range.
  bool DataEquals(HValue* other) override {
    HCheckInstanceType* b = HCheckInstanceType::cast(other);
    return check_ == b->check_;
  }

  int RedefinedOperandIndex() override { return 0; }

 private:
  const char* GetCheckName() const;

  HCheckInstanceType(HValue* value, Check check)
      : HUnaryOperation(value, HType::HeapObject()), check_(check) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  const Check check_;
};


class HCheckSmi final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckSmi, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* Canonicalize() override {
    HType value_type = value()->type();
    if (value_type.IsSmi()) {
      return NULL;
    }
    return this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckSmi)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HCheckSmi(HValue* value) : HUnaryOperation(value, HType::Smi()) {
    set_representation(Representation::Smi());
    SetFlag(kUseGVN);
  }
};


class HCheckArrayBufferNotNeutered final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckArrayBufferNotNeutered, HValue*);

  bool HasEscapingOperandAt(int index) override { return false; }
  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HType CalculateInferredType() override {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckArrayBufferNotNeutered)

 protected:
  bool DataEquals(HValue* other) override { return true; }
  int RedefinedOperandIndex() override { return 0; }

 private:
  explicit HCheckArrayBufferNotNeutered(HValue* value)
      : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kCalls);
  }
};


class HCheckHeapObject final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCheckHeapObject, HValue*);

  bool HasEscapingOperandAt(int index) override { return false; }
  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HType CalculateInferredType() override {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

#ifdef DEBUG
  void Verify() override;
#endif

  HValue* Canonicalize() override {
    return value()->type().IsHeapObject() ? NULL : this;
  }

  DECLARE_CONCRETE_INSTRUCTION(CheckHeapObject)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HCheckHeapObject(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }
};


class HPhi final : public HValue {
 public:
  HPhi(int merged_index, Zone* zone)
      : inputs_(2, zone), merged_index_(merged_index) {
    DCHECK(merged_index >= 0 || merged_index == kInvalidMergedIndex);
    SetFlag(kFlexibleRepresentation);
  }

  Representation RepresentationFromInputs() override;

  Range* InferRange(Zone* zone) override;
  void InferRepresentation(HInferRepresentationPhase* h_infer) override;
  Representation RequiredInputRepresentation(int index) override {
    return representation();
  }
  Representation KnownOptimalRepresentation() override {
    return representation();
  }
  HType CalculateInferredType() override;
  int OperandCount() const override { return inputs_.length(); }
  HValue* OperandAt(int index) const override { return inputs_[index]; }
  HValue* GetRedundantReplacement();
  void AddInput(HValue* value);
  bool HasRealUses();

  bool IsReceiver() const { return merged_index_ == 0; }
  bool HasMergedIndex() const { return merged_index_ != kInvalidMergedIndex; }

  SourcePosition position() const override;

  int merged_index() const { return merged_index_; }

  std::ostream& PrintTo(std::ostream& os) const override;  // NOLINT

#ifdef DEBUG
  void Verify() override;
#endif

  void InitRealUses(int id);
  void AddNonPhiUsesFrom(HPhi* other);

  Representation representation_from_indirect_uses() const {
    return representation_from_indirect_uses_;
  }

  bool has_type_feedback_from_uses() const {
    return has_type_feedback_from_uses_;
  }

  int phi_id() { return phi_id_; }

  static HPhi* cast(HValue* value) {
    DCHECK(value->IsPhi());
    return reinterpret_cast<HPhi*>(value);
  }
  Opcode opcode() const override { return HValue::kPhi; }

  void SimplifyConstantInputs();

  // Marker value representing an invalid merge index.
  static const int kInvalidMergedIndex = -1;

 protected:
  void DeleteFromGraph() override;
  void InternalSetOperandAt(int index, HValue* value) override {
    inputs_[index] = value;
  }

 private:
  Representation representation_from_non_phi_uses() const {
    return representation_from_non_phi_uses_;
  }

  ZoneList<HValue*> inputs_;
  int merged_index_ = 0;

  int phi_id_ = -1;

  Representation representation_from_indirect_uses_ = Representation::None();
  Representation representation_from_non_phi_uses_ = Representation::None();
  bool has_type_feedback_from_uses_ = false;

  bool IsDeletable() const override { return !IsReceiver(); }
};


// Common base class for HArgumentsObject and HCapturedObject.
class HDematerializedObject : public HInstruction {
 public:
  HDematerializedObject(int count, Zone* zone) : values_(count, zone) {}

  int OperandCount() const final { return values_.length(); }
  HValue* OperandAt(int index) const final { return values_[index]; }

  bool HasEscapingOperandAt(int index) final { return false; }
  Representation RequiredInputRepresentation(int index) final {
    return Representation::None();
  }

 protected:
  void InternalSetOperandAt(int index, HValue* value) final {
    values_[index] = value;
  }

  // List of values tracked by this marker.
  ZoneList<HValue*> values_;
};


class HArgumentsObject final : public HDematerializedObject {
 public:
  static HArgumentsObject* New(Isolate* isolate, Zone* zone, HValue* context,
                               int count) {
    return new(zone) HArgumentsObject(count, zone);
  }

  // The values contain a list of all elements in the arguments object
  // including the receiver object, which is skipped when materializing.
  const ZoneList<HValue*>* arguments_values() const { return &values_; }
  int arguments_count() const { return values_.length(); }

  void AddArgument(HValue* argument, Zone* zone) {
    values_.Add(NULL, zone);  // Resize list.
    SetOperandAt(values_.length() - 1, argument);
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsObject)

 private:
  HArgumentsObject(int count, Zone* zone)
      : HDematerializedObject(count, zone) {
    set_representation(Representation::Tagged());
    SetFlag(kIsArguments);
  }
};


class HCapturedObject final : public HDematerializedObject {
 public:
  HCapturedObject(int length, int id, Zone* zone)
      : HDematerializedObject(length, zone), capture_id_(id) {
    set_representation(Representation::Tagged());
    values_.AddBlock(NULL, length, zone);  // Resize list.
  }

  // The values contain a list of all in-object properties inside the
  // captured object and is index by field index. Properties in the
  // properties or elements backing store are not tracked here.
  const ZoneList<HValue*>* values() const { return &values_; }
  int length() const { return values_.length(); }
  int capture_id() const { return capture_id_; }

  // Shortcut for the map value of this captured object.
  HValue* map_value() const { return values()->first(); }

  void ReuseSideEffectsFromStore(HInstruction* store) {
    DCHECK(store->HasObservableSideEffects());
    DCHECK(store->IsStoreNamedField());
    changes_flags_.Add(store->ChangesFlags());
  }

  // Replay effects of this instruction on the given environment.
  void ReplayEnvironment(HEnvironment* env);

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(CapturedObject)

 private:
  int capture_id_;

  // Note that we cannot DCE captured objects as they are used to replay
  // the environment. This method is here as an explicit reminder.
  // TODO(mstarzinger): Turn HSimulates into full snapshots maybe?
  bool IsDeletable() const final { return false; }
};


class HConstant final : public HTemplateInstruction<0> {
 public:
  enum Special { kHoleNaN };

  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, Special);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, int32_t);
  DECLARE_INSTRUCTION_FACTORY_P2(HConstant, int32_t, Representation);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, double);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, Handle<Object>);
  DECLARE_INSTRUCTION_FACTORY_P1(HConstant, ExternalReference);

  static HConstant* CreateAndInsertAfter(Isolate* isolate, Zone* zone,
                                         HValue* context, int32_t value,
                                         Representation representation,
                                         HInstruction* instruction) {
    return instruction->Append(
        HConstant::New(isolate, zone, context, value, representation));
  }

  Handle<Map> GetMonomorphicJSObjectMap() override {
    Handle<Object> object = object_.handle();
    if (!object.is_null() && object->IsHeapObject()) {
      return v8::internal::handle(HeapObject::cast(*object)->map());
    }
    return Handle<Map>();
  }

  static HConstant* CreateAndInsertBefore(Isolate* isolate, Zone* zone,
                                          HValue* context, int32_t value,
                                          Representation representation,
                                          HInstruction* instruction) {
    return instruction->Prepend(
        HConstant::New(isolate, zone, context, value, representation));
  }

  static HConstant* CreateAndInsertBefore(Zone* zone,
                                          Unique<Map> map,
                                          bool map_is_stable,
                                          HInstruction* instruction) {
    return instruction->Prepend(new(zone) HConstant(
        map, Unique<Map>(Handle<Map>::null()), map_is_stable,
        Representation::Tagged(), HType::HeapObject(), true,
        false, false, MAP_TYPE));
  }

  static HConstant* CreateAndInsertAfter(Zone* zone,
                                         Unique<Map> map,
                                         bool map_is_stable,
                                         HInstruction* instruction) {
    return instruction->Append(new(zone) HConstant(
            map, Unique<Map>(Handle<Map>::null()), map_is_stable,
            Representation::Tagged(), HType::HeapObject(), true,
            false, false, MAP_TYPE));
  }

  Handle<Object> handle(Isolate* isolate) {
    if (object_.handle().is_null()) {
      // Default arguments to is_not_in_new_space depend on this heap number
      // to be tenured so that it's guaranteed not to be located in new space.
      object_ = Unique<Object>::CreateUninitialized(
          isolate->factory()->NewNumber(double_value_, TENURED));
    }
    AllowDeferredHandleDereference smi_check;
    DCHECK(HasInteger32Value() || !object_.handle()->IsSmi());
    return object_.handle();
  }

  bool IsSpecialDouble() const {
    return HasDoubleValue() &&
           (bit_cast<int64_t>(double_value_) == bit_cast<int64_t>(-0.0) ||
            std::isnan(double_value_));
  }

  bool NotInNewSpace() const {
    return IsNotInNewSpaceField::decode(bit_field_);
  }

  bool ImmortalImmovable() const;

  bool IsCell() const {
    InstanceType instance_type = GetInstanceType();
    return instance_type == CELL_TYPE;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  Representation KnownOptimalRepresentation() override {
    if (HasSmiValue() && SmiValuesAre31Bits()) return Representation::Smi();
    if (HasInteger32Value()) return Representation::Integer32();
    if (HasNumberValue()) return Representation::Double();
    if (HasExternalReferenceValue()) return Representation::External();
    return Representation::Tagged();
  }

  bool EmitAtUses() override;
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT
  HConstant* CopyToRepresentation(Representation r, Zone* zone) const;
  Maybe<HConstant*> CopyToTruncatedInt32(Zone* zone);
  Maybe<HConstant*> CopyToTruncatedNumber(Isolate* isolate, Zone* zone);
  bool HasInteger32Value() const {
    return HasInt32ValueField::decode(bit_field_);
  }
  int32_t Integer32Value() const {
    DCHECK(HasInteger32Value());
    return int32_value_;
  }
  bool HasSmiValue() const { return HasSmiValueField::decode(bit_field_); }
  bool HasDoubleValue() const {
    return HasDoubleValueField::decode(bit_field_);
  }
  double DoubleValue() const {
    DCHECK(HasDoubleValue());
    return double_value_;
  }
  uint64_t DoubleValueAsBits() const {
    uint64_t bits;
    DCHECK(HasDoubleValue());
    STATIC_ASSERT(sizeof(bits) == sizeof(double_value_));
    std::memcpy(&bits, &double_value_, sizeof(bits));
    return bits;
  }
  bool IsTheHole() const {
    if (HasDoubleValue() && DoubleValueAsBits() == kHoleNanInt64) {
      return true;
    }
    return object_.IsInitialized() &&
           object_.IsKnownGlobal(isolate()->heap()->the_hole_value());
  }
  bool HasNumberValue() const { return HasDoubleValue(); }
  int32_t NumberValueAsInteger32() const {
    DCHECK(HasNumberValue());
    // Irrespective of whether a numeric HConstant can be safely
    // represented as an int32, we store the (in some cases lossy)
    // representation of the number in int32_value_.
    return int32_value_;
  }
  bool HasStringValue() const {
    if (HasNumberValue()) return false;
    DCHECK(!object_.handle().is_null());
    return GetInstanceType() < FIRST_NONSTRING_TYPE;
  }
  Handle<String> StringValue() const {
    DCHECK(HasStringValue());
    return Handle<String>::cast(object_.handle());
  }
  bool HasInternalizedStringValue() const {
    return HasStringValue() && StringShape(GetInstanceType()).IsInternalized();
  }

  bool HasExternalReferenceValue() const {
    return HasExternalReferenceValueField::decode(bit_field_);
  }
  ExternalReference ExternalReferenceValue() const {
    return external_reference_value_;
  }

  bool HasBooleanValue() const { return type_.IsBoolean(); }
  bool BooleanValue() const { return BooleanValueField::decode(bit_field_); }
  bool IsCallable() const { return IsCallableField::decode(bit_field_); }
  bool IsUndetectable() const {
    return IsUndetectableField::decode(bit_field_);
  }
  InstanceType GetInstanceType() const {
    return InstanceTypeField::decode(bit_field_);
  }

  bool HasMapValue() const { return GetInstanceType() == MAP_TYPE; }
  Unique<Map> MapValue() const {
    DCHECK(HasMapValue());
    return Unique<Map>::cast(GetUnique());
  }
  bool HasStableMapValue() const {
    DCHECK(HasMapValue() || !HasStableMapValueField::decode(bit_field_));
    return HasStableMapValueField::decode(bit_field_);
  }

  bool HasObjectMap() const { return !object_map_.IsNull(); }
  Unique<Map> ObjectMap() const {
    DCHECK(HasObjectMap());
    return object_map_;
  }

  intptr_t Hashcode() override {
    if (HasInteger32Value()) {
      return static_cast<intptr_t>(int32_value_);
    } else if (HasDoubleValue()) {
      uint64_t bits = DoubleValueAsBits();
      if (sizeof(bits) > sizeof(intptr_t)) {
        bits ^= (bits >> 32);
      }
      return static_cast<intptr_t>(bits);
    } else if (HasExternalReferenceValue()) {
      return reinterpret_cast<intptr_t>(external_reference_value_.address());
    } else {
      DCHECK(!object_.handle().is_null());
      return object_.Hashcode();
    }
  }

  void FinalizeUniqueness() override {
    if (!HasDoubleValue() && !HasExternalReferenceValue()) {
      DCHECK(!object_.handle().is_null());
      object_ = Unique<Object>(object_.handle());
    }
  }

  Unique<Object> GetUnique() const {
    return object_;
  }

  bool EqualsUnique(Unique<Object> other) const {
    return object_.IsInitialized() && object_ == other;
  }

  bool DataEquals(HValue* other) override {
    HConstant* other_constant = HConstant::cast(other);
    if (HasInteger32Value()) {
      return other_constant->HasInteger32Value() &&
             int32_value_ == other_constant->int32_value_;
    } else if (HasDoubleValue()) {
      return other_constant->HasDoubleValue() &&
             std::memcmp(&double_value_, &other_constant->double_value_,
                         sizeof(double_value_)) == 0;
    } else if (HasExternalReferenceValue()) {
      return other_constant->HasExternalReferenceValue() &&
             external_reference_value_ ==
                 other_constant->external_reference_value_;
    } else {
      if (other_constant->HasInteger32Value() ||
          other_constant->HasDoubleValue() ||
          other_constant->HasExternalReferenceValue()) {
        return false;
      }
      DCHECK(!object_.handle().is_null());
      return other_constant->object_ == object_;
    }
  }

#ifdef DEBUG
  void Verify() override {}
#endif

  DECLARE_CONCRETE_INSTRUCTION(Constant)

 protected:
  Range* InferRange(Zone* zone) override;

 private:
  friend class HGraph;
  explicit HConstant(Special special);
  explicit HConstant(Handle<Object> handle,
                     Representation r = Representation::None());
  HConstant(int32_t value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Unique<Object> optional = Unique<Object>(Handle<Object>::null()));
  HConstant(double value,
            Representation r = Representation::None(),
            bool is_not_in_new_space = true,
            Unique<Object> optional = Unique<Object>(Handle<Object>::null()));
  HConstant(Unique<Object> object,
            Unique<Map> object_map,
            bool has_stable_map_value,
            Representation r,
            HType type,
            bool is_not_in_new_space,
            bool boolean_value,
            bool is_undetectable,
            InstanceType instance_type);

  explicit HConstant(ExternalReference reference);

  void Initialize(Representation r);

  bool IsDeletable() const override { return true; }

  // If object_ is a map, this indicates whether the map is stable.
  class HasStableMapValueField : public BitField<bool, 0, 1> {};

  // We store the HConstant in the most specific form safely possible.
  // These flags tell us if the respective member fields hold valid, safe
  // representations of the constant. More specific flags imply more general
  // flags, but not the converse (i.e. smi => int32 => double).
  class HasSmiValueField : public BitField<bool, 1, 1> {};
  class HasInt32ValueField : public BitField<bool, 2, 1> {};
  class HasDoubleValueField : public BitField<bool, 3, 1> {};

  class HasExternalReferenceValueField : public BitField<bool, 4, 1> {};
  class IsNotInNewSpaceField : public BitField<bool, 5, 1> {};
  class BooleanValueField : public BitField<bool, 6, 1> {};
  class IsUndetectableField : public BitField<bool, 7, 1> {};
  class IsCallableField : public BitField<bool, 8, 1> {};

  static const InstanceType kUnknownInstanceType = FILLER_TYPE;
  class InstanceTypeField : public BitField<InstanceType, 16, 8> {};

  // If this is a numerical constant, object_ either points to the
  // HeapObject the constant originated from or is null.  If the
  // constant is non-numeric, object_ always points to a valid
  // constant HeapObject.
  Unique<Object> object_;

  // If object_ is a heap object, this points to the stable map of the object.
  Unique<Map> object_map_;

  uint32_t bit_field_;

  int32_t int32_value_;
  double double_value_;
  ExternalReference external_reference_value_;
};


class HBinaryOperation : public HTemplateInstruction<3> {
 public:
  HBinaryOperation(HValue* context, HValue* left, HValue* right,
                   HType type = HType::Tagged())
      : HTemplateInstruction<3>(type),
        observed_output_representation_(Representation::None()) {
    DCHECK(left != NULL && right != NULL);
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    SetOperandAt(2, right);
    observed_input_representation_[0] = Representation::None();
    observed_input_representation_[1] = Representation::None();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* left() const { return OperandAt(1); }
  HValue* right() const { return OperandAt(2); }

  // True if switching left and right operands likely generates better code.
  bool AreOperandsBetterSwitched() {
    if (!IsCommutative()) return false;

    // Constant operands are better off on the right, they can be inlined in
    // many situations on most platforms.
    if (left()->IsConstant()) return true;
    if (right()->IsConstant()) return false;

    // Otherwise, if there is only one use of the right operand, it would be
    // better off on the left for platforms that only have 2-arg arithmetic
    // ops (e.g ia32, x64) that clobber the left operand.
    return right()->HasOneUse();
  }

  HValue* BetterLeftOperand() {
    return AreOperandsBetterSwitched() ? right() : left();
  }

  HValue* BetterRightOperand() {
    return AreOperandsBetterSwitched() ? left() : right();
  }

  void set_observed_input_representation(int index, Representation rep) {
    DCHECK(index >= 1 && index <= 2);
    observed_input_representation_[index - 1] = rep;
  }

  virtual void initialize_output_representation(Representation observed) {
    observed_output_representation_ = observed;
  }

  Representation observed_input_representation(int index) override {
    if (index == 0) return Representation::Tagged();
    return observed_input_representation_[index - 1];
  }

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    Representation rep = !FLAG_smi_binop && new_rep.IsSmi()
        ? Representation::Integer32() : new_rep;
    HValue::UpdateRepresentation(rep, h_infer, reason);
  }

  void InferRepresentation(HInferRepresentationPhase* h_infer) override;
  Representation RepresentationFromInputs() override;
  Representation RepresentationFromOutput();
  void AssumeRepresentation(Representation r) override;

  virtual bool IsCommutative() const { return false; }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    if (index == 0) return Representation::Tagged();
    return representation();
  }

  bool RightIsPowerOf2() {
    if (!right()->IsInteger32Constant()) return false;
    int32_t value = right()->GetInteger32Constant();
    if (value < 0) {
      return base::bits::IsPowerOfTwo32(static_cast<uint32_t>(-value));
    }
    return base::bits::IsPowerOfTwo32(static_cast<uint32_t>(value));
  }

  DECLARE_ABSTRACT_INSTRUCTION(BinaryOperation)

 private:
  bool IgnoreObservedOutputRepresentation(Representation current_rep);

  Representation observed_input_representation_[2];
  Representation observed_output_representation_;
};


class HWrapReceiver final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HWrapReceiver, HValue*, HValue*);

  bool DataEquals(HValue* other) override { return true; }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* receiver() const { return OperandAt(0); }
  HValue* function() const { return OperandAt(1); }

  HValue* Canonicalize() override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT
  bool known_function() const { return known_function_; }

  DECLARE_CONCRETE_INSTRUCTION(WrapReceiver)

 private:
  HWrapReceiver(HValue* receiver, HValue* function) {
    known_function_ = function->IsConstant() &&
        HConstant::cast(function)->handle(function->isolate())->IsJSFunction();
    set_representation(Representation::Tagged());
    SetOperandAt(0, receiver);
    SetOperandAt(1, function);
    SetFlag(kUseGVN);
  }

  bool known_function_;
};


class HApplyArguments final : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P5(HApplyArguments, HValue*, HValue*, HValue*,
                                 HValue*, TailCallMode);

  Representation RequiredInputRepresentation(int index) override {
    // The length is untagged, all other inputs are tagged.
    return (index == 2)
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* function() { return OperandAt(0); }
  HValue* receiver() { return OperandAt(1); }
  HValue* length() { return OperandAt(2); }
  HValue* elements() { return OperandAt(3); }

  TailCallMode tail_call_mode() const {
    return TailCallModeField::decode(bit_field_);
  }

  DECLARE_CONCRETE_INSTRUCTION(ApplyArguments)

 private:
  HApplyArguments(HValue* function, HValue* receiver, HValue* length,
                  HValue* elements, TailCallMode tail_call_mode)
      : bit_field_(TailCallModeField::encode(tail_call_mode)) {
    set_representation(Representation::Tagged());
    SetOperandAt(0, function);
    SetOperandAt(1, receiver);
    SetOperandAt(2, length);
    SetOperandAt(3, elements);
    SetAllSideEffects();
  }

  class TailCallModeField : public BitField<TailCallMode, 0, 1> {};
  uint32_t bit_field_;
};


class HArgumentsElements final : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HArgumentsElements, bool);
  DECLARE_INSTRUCTION_FACTORY_P2(HArgumentsElements, bool, bool);

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsElements)

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  bool from_inlined() const { return from_inlined_; }
  bool arguments_adaptor() const { return arguments_adaptor_; }

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HArgumentsElements(bool from_inlined, bool arguments_adaptor = true)
      : from_inlined_(from_inlined), arguments_adaptor_(arguments_adaptor) {
    // The value produced by this instruction is a pointer into the stack
    // that looks as if it was a smi because of alignment.
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return true; }

  bool from_inlined_;
  bool arguments_adaptor_;
};


class HArgumentsLength final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HArgumentsLength, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(ArgumentsLength)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HArgumentsLength(HValue* value) : HUnaryOperation(value) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return true; }
};


class HAccessArgumentsAt final : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HAccessArgumentsAt, HValue*, HValue*, HValue*);

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    // The arguments elements is considered tagged.
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* arguments() const { return OperandAt(0); }
  HValue* length() const { return OperandAt(1); }
  HValue* index() const { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(AccessArgumentsAt)

 private:
  HAccessArgumentsAt(HValue* arguments, HValue* length, HValue* index) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetOperandAt(0, arguments);
    SetOperandAt(1, length);
    SetOperandAt(2, index);
  }

  bool DataEquals(HValue* other) override { return true; }
};


class HBoundsCheck final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HBoundsCheck, HValue*, HValue*);

  bool skip_check() const { return skip_check_; }
  void set_skip_check() { skip_check_ = true; }

  HValue* base() const { return base_; }
  int offset() const { return offset_; }
  int scale() const { return scale_; }

  Representation RequiredInputRepresentation(int index) override {
    return representation();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT
  void InferRepresentation(HInferRepresentationPhase* h_infer) override;

  HValue* index() const { return OperandAt(0); }
  HValue* length() const { return OperandAt(1); }
  bool allow_equality() const { return allow_equality_; }
  void set_allow_equality(bool v) { allow_equality_ = v; }

  int RedefinedOperandIndex() override { return 0; }
  bool IsPurelyInformativeDefinition() override { return skip_check(); }

  DECLARE_CONCRETE_INSTRUCTION(BoundsCheck)

 protected:
  Range* InferRange(Zone* zone) override;

  bool DataEquals(HValue* other) override { return true; }
  bool skip_check_;
  HValue* base_;
  int offset_;
  int scale_;
  bool allow_equality_;

 private:
  // Normally HBoundsCheck should be created using the
  // HGraphBuilder::AddBoundsCheck() helper.
  // However when building stubs, where we know that the arguments are Int32,
  // it makes sense to invoke this constructor directly.
  HBoundsCheck(HValue* index, HValue* length)
    : skip_check_(false),
      base_(NULL), offset_(0), scale_(0),
      allow_equality_(false) {
    SetOperandAt(0, index);
    SetOperandAt(1, length);
    SetFlag(kFlexibleRepresentation);
    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return skip_check() && !FLAG_debug_code; }
};


class HBitwiseBinaryOperation : public HBinaryOperation {
 public:
  HBitwiseBinaryOperation(HValue* context, HValue* left, HValue* right,
                          HType type = HType::TaggedNumber())
      : HBinaryOperation(context, left, right, type) {
    SetFlag(kFlexibleRepresentation);
    SetFlag(kTruncatingToInt32);
    SetFlag(kTruncatingToNumber);
    SetAllSideEffects();
  }

  void RepresentationChanged(Representation to) override {
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
    if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
  }

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    // We only generate either int32 or generic tagged bitwise operations.
    if (new_rep.IsDouble()) new_rep = Representation::Integer32();
    HBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  Representation observed_input_representation(int index) override {
    Representation r = HBinaryOperation::observed_input_representation(index);
    if (r.IsDouble()) return Representation::Integer32();
    return r;
  }

  void initialize_output_representation(Representation observed) override {
    if (observed.IsDouble()) observed = Representation::Integer32();
    HBinaryOperation::initialize_output_representation(observed);
  }

  DECLARE_ABSTRACT_INSTRUCTION(BitwiseBinaryOperation)

 private:
  bool IsDeletable() const override { return true; }
};


class HMathFloorOfDiv final : public HBinaryOperation {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HMathFloorOfDiv,
                                              HValue*,
                                              HValue*);

  DECLARE_CONCRETE_INSTRUCTION(MathFloorOfDiv)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HMathFloorOfDiv(HValue* context, HValue* left, HValue* right)
      : HBinaryOperation(context, left, right) {
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetFlag(kCanOverflow);
    SetFlag(kCanBeDivByZero);
    SetFlag(kLeftCanBeMinInt);
    SetFlag(kLeftCanBeNegative);
    SetFlag(kLeftCanBePositive);
    SetFlag(kTruncatingToNumber);
  }

  Range* InferRange(Zone* zone) override;

  bool IsDeletable() const override { return true; }
};


class HArithmeticBinaryOperation : public HBinaryOperation {
 public:
  HArithmeticBinaryOperation(HValue* context, HValue* left, HValue* right,
                             HType type = HType::TaggedNumber())
      : HBinaryOperation(context, left, right, type) {
    SetAllSideEffects();
    SetFlag(kFlexibleRepresentation);
    SetFlag(kTruncatingToNumber);
  }

  void RepresentationChanged(Representation to) override {
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
    if (to.IsTagged()) SetChangesFlag(kNewSpacePromotion);
  }

  DECLARE_ABSTRACT_INSTRUCTION(ArithmeticBinaryOperation)

 private:
  bool IsDeletable() const override { return true; }
};


class HCompareGeneric final : public HBinaryOperation {
 public:
  static HCompareGeneric* New(Isolate* isolate, Zone* zone, HValue* context,
                              HValue* left, HValue* right, Token::Value token) {
    return new (zone) HCompareGeneric(context, left, right, token);
  }

  Representation RequiredInputRepresentation(int index) override {
    return index == 0
        ? Representation::Tagged()
        : representation();
  }

  Token::Value token() const { return token_; }
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(CompareGeneric)

 private:
  HCompareGeneric(HValue* context, HValue* left, HValue* right,
                  Token::Value token)
      : HBinaryOperation(context, left, right, HType::Boolean()),
        token_(token) {
    DCHECK(Token::IsCompareOp(token));
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }

  Token::Value token_;
};


class HCompareNumericAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  static HCompareNumericAndBranch* New(Isolate* isolate, Zone* zone,
                                       HValue* context, HValue* left,
                                       HValue* right, Token::Value token,
                                       HBasicBlock* true_target = NULL,
                                       HBasicBlock* false_target = NULL) {
    return new (zone)
        HCompareNumericAndBranch(left, right, token, true_target, false_target);
  }

  HValue* left() const { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }
  Token::Value token() const { return token_; }

  void set_observed_input_representation(Representation left,
                                         Representation right) {
      observed_input_representation_[0] = left;
      observed_input_representation_[1] = right;
  }

  void InferRepresentation(HInferRepresentationPhase* h_infer) override;

  Representation RequiredInputRepresentation(int index) override {
    return representation();
  }
  Representation observed_input_representation(int index) override {
    return observed_input_representation_[index];
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(CompareNumericAndBranch)

 private:
  HCompareNumericAndBranch(HValue* left, HValue* right, Token::Value token,
                           HBasicBlock* true_target, HBasicBlock* false_target)
      : token_(token) {
    SetFlag(kFlexibleRepresentation);
    DCHECK(Token::IsCompareOp(token));
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  Representation observed_input_representation_[2];
  Token::Value token_;
};


class HCompareHoleAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HCompareHoleAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HCompareHoleAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  void InferRepresentation(HInferRepresentationPhase* h_infer) override;

  Representation RequiredInputRepresentation(int index) override {
    return representation();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareHoleAndBranch)

 private:
  HCompareHoleAndBranch(HValue* value,
                        HBasicBlock* true_target = NULL,
                        HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {
    SetFlag(kFlexibleRepresentation);
  }
};


class HCompareObjectEqAndBranch : public HTemplateControlInstruction<2, 2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCompareObjectEqAndBranch, HValue*, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P4(HCompareObjectEqAndBranch, HValue*, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const { return known_successor_index_; }
  void set_known_successor_index(int known_successor_index) {
    known_successor_index_ = known_successor_index;
  }

  HValue* left() const { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  Representation observed_input_representation(int index) override {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(CompareObjectEqAndBranch)

 private:
  HCompareObjectEqAndBranch(HValue* left,
                            HValue* right,
                            HBasicBlock* true_target = NULL,
                            HBasicBlock* false_target = NULL)
      : known_successor_index_(kNoKnownSuccessorIndex) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    SetSuccessorAt(0, true_target);
    SetSuccessorAt(1, false_target);
  }

  int known_successor_index_;
};


class HIsStringAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsStringAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsStringAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  static const int kNoKnownSuccessorIndex = -1;
  int known_successor_index() const { return known_successor_index_; }
  void set_known_successor_index(int known_successor_index) {
    known_successor_index_ = known_successor_index;
  }

  DECLARE_CONCRETE_INSTRUCTION(IsStringAndBranch)

 protected:
  int RedefinedOperandIndex() override { return 0; }

 private:
  HIsStringAndBranch(HValue* value, HBasicBlock* true_target = NULL,
                     HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target),
        known_successor_index_(kNoKnownSuccessorIndex) {
    set_representation(Representation::Tagged());
  }

  int known_successor_index_;
};


class HIsSmiAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsSmiAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsSmiAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  DECLARE_CONCRETE_INSTRUCTION(IsSmiAndBranch)

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

 protected:
  bool DataEquals(HValue* other) override { return true; }
  int RedefinedOperandIndex() override { return 0; }

 private:
  HIsSmiAndBranch(HValue* value,
                  HBasicBlock* true_target = NULL,
                  HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {
    set_representation(Representation::Tagged());
  }
};


class HIsUndetectableAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HIsUndetectableAndBranch, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P3(HIsUndetectableAndBranch, HValue*,
                                 HBasicBlock*, HBasicBlock*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  DECLARE_CONCRETE_INSTRUCTION(IsUndetectableAndBranch)

 private:
  HIsUndetectableAndBranch(HValue* value,
                           HBasicBlock* true_target = NULL,
                           HBasicBlock* false_target = NULL)
      : HUnaryControlInstruction(value, true_target, false_target) {}
};


class HStringCompareAndBranch final : public HTemplateControlInstruction<2, 3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P3(HStringCompareAndBranch,
                                              HValue*,
                                              HValue*,
                                              Token::Value);

  HValue* context() const { return OperandAt(0); }
  HValue* left() const { return OperandAt(1); }
  HValue* right() const { return OperandAt(2); }
  Token::Value token() const { return token_; }

  std::ostream& PrintDataTo(std::ostream& os) const final;  // NOLINT

  Representation RequiredInputRepresentation(int index) final {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(StringCompareAndBranch)

 private:
  HStringCompareAndBranch(HValue* context, HValue* left, HValue* right,
                          Token::Value token)
      : token_(token) {
    DCHECK(Token::IsCompareOp(token));
    SetOperandAt(0, context);
    SetOperandAt(1, left);
    SetOperandAt(2, right);
    set_representation(Representation::Tagged());
    SetChangesFlag(kNewSpacePromotion);
    SetDependsOnFlag(kStringChars);
    SetDependsOnFlag(kStringLengths);
  }

  Token::Value const token_;
};


class HHasInstanceTypeAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(
      HHasInstanceTypeAndBranch, HValue*, InstanceType);
  DECLARE_INSTRUCTION_FACTORY_P3(
      HHasInstanceTypeAndBranch, HValue*, InstanceType, InstanceType);

  InstanceType from() { return from_; }
  InstanceType to() { return to_; }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  DECLARE_CONCRETE_INSTRUCTION(HasInstanceTypeAndBranch)

 private:
  HHasInstanceTypeAndBranch(HValue* value, InstanceType type)
      : HUnaryControlInstruction(value, NULL, NULL), from_(type), to_(type) { }
  HHasInstanceTypeAndBranch(HValue* value, InstanceType from, InstanceType to)
      : HUnaryControlInstruction(value, NULL, NULL), from_(from), to_(to) {
    DCHECK(to == LAST_TYPE);  // Others not implemented yet in backend.
  }

  InstanceType from_;
  InstanceType to_;  // Inclusive range, not all combinations work.
};

class HClassOfTestAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HClassOfTestAndBranch, HValue*,
                                 Handle<String>);

  DECLARE_CONCRETE_INSTRUCTION(ClassOfTestAndBranch)

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Handle<String> class_name() const { return class_name_; }

 private:
  HClassOfTestAndBranch(HValue* value, Handle<String> class_name)
      : HUnaryControlInstruction(value, NULL, NULL),
        class_name_(class_name) { }

  Handle<String> class_name_;
};


class HTypeofIsAndBranch final : public HUnaryControlInstruction {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HTypeofIsAndBranch, HValue*, Handle<String>);

  Handle<String> type_literal() const { return type_literal_.handle(); }
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(TypeofIsAndBranch)

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  bool KnownSuccessorBlock(HBasicBlock** block) override;

  void FinalizeUniqueness() override {
    type_literal_ = Unique<String>(type_literal_.handle());
  }

 private:
  HTypeofIsAndBranch(HValue* value, Handle<String> type_literal)
      : HUnaryControlInstruction(value, NULL, NULL),
        type_literal_(Unique<String>::CreateUninitialized(type_literal)) { }

  Unique<String> type_literal_;
};


class HHasInPrototypeChainAndBranch final
    : public HTemplateControlInstruction<2, 2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HHasInPrototypeChainAndBranch, HValue*,
                                 HValue*);

  HValue* object() const { return OperandAt(0); }
  HValue* prototype() const { return OperandAt(1); }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  bool ObjectNeedsSmiCheck() const {
    return !object()->type().IsHeapObject() &&
           !object()->representation().IsHeapObject();
  }

  DECLARE_CONCRETE_INSTRUCTION(HasInPrototypeChainAndBranch)

 private:
  HHasInPrototypeChainAndBranch(HValue* object, HValue* prototype) {
    SetOperandAt(0, object);
    SetOperandAt(1, prototype);
    SetDependsOnFlag(kCalls);
  }
};


class HPower final : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  HValue* left() { return OperandAt(0); }
  HValue* right() const { return OperandAt(1); }

  Representation RequiredInputRepresentation(int index) override {
    return index == 0
      ? Representation::Double()
      : Representation::None();
  }
  Representation observed_input_representation(int index) override {
    return RequiredInputRepresentation(index);
  }

  DECLARE_CONCRETE_INSTRUCTION(Power)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HPower(HValue* left, HValue* right) {
    SetOperandAt(0, left);
    SetOperandAt(1, right);
    set_representation(Representation::Double());
    SetFlag(kUseGVN);
    SetChangesFlag(kNewSpacePromotion);
  }

  bool IsDeletable() const override {
    return !right()->representation().IsTagged();
  }
};


enum ExternalAddType {
  AddOfExternalAndTagged,
  AddOfExternalAndInt32,
  NoExternalAdd
};


class HAdd final : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right,
                           ExternalAddType external_add_type);

  // Add is only commutative if two integer values are added and not if two
  // tagged values are added (because it might be a String concatenation).
  // We also do not commute (pointer + offset).
  bool IsCommutative() const override {
    return !representation().IsTagged() && !representation().IsExternal();
  }

  HValue* Canonicalize() override;

  void RepresentationChanged(Representation to) override {
    if (to.IsTagged() &&
        (left()->ToNumberCanBeObserved() || right()->ToNumberCanBeObserved() ||
         left()->ToStringCanBeObserved() || right()->ToStringCanBeObserved())) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      ClearAllSideEffects();
      SetFlag(kUseGVN);
    }
    if (to.IsTagged()) {
      SetChangesFlag(kNewSpacePromotion);
      ClearFlag(kTruncatingToNumber);
    }
    if (!right()->type().IsTaggedNumber() &&
        !right()->representation().IsDouble() &&
        !right()->representation().IsSmiOrInteger32()) {
      ClearFlag(kTruncatingToNumber);
    }
  }

  Representation RepresentationFromInputs() override;

  Representation RequiredInputRepresentation(int index) override;

  bool IsConsistentExternalRepresentation() {
    return left()->representation().IsExternal() &&
           ((external_add_type_ == AddOfExternalAndInt32 &&
             right()->representation().IsInteger32()) ||
            (external_add_type_ == AddOfExternalAndTagged &&
             right()->representation().IsTagged()));
  }

  ExternalAddType external_add_type() const { return external_add_type_; }

  DECLARE_CONCRETE_INSTRUCTION(Add)

 protected:
  bool DataEquals(HValue* other) override { return true; }

  Range* InferRange(Zone* zone) override;

 private:
  HAdd(HValue* context, HValue* left, HValue* right,
       ExternalAddType external_add_type = NoExternalAdd)
      : HArithmeticBinaryOperation(context, left, right, HType::Tagged()),
        external_add_type_(external_add_type) {
    SetFlag(kCanOverflow);
    switch (external_add_type_) {
      case AddOfExternalAndTagged:
        DCHECK(left->representation().IsExternal());
        DCHECK(right->representation().IsTagged());
        SetDependsOnFlag(kNewSpacePromotion);
        ClearFlag(HValue::kCanOverflow);
        SetFlag(kHasNoObservableSideEffects);
        break;

      case NoExternalAdd:
        // This is a bit of a hack: The call to this constructor is generated
        // by a macro that also supports sub and mul, so it doesn't pass in
        // a value for external_add_type but uses the default.
        if (left->representation().IsExternal()) {
          external_add_type_ = AddOfExternalAndInt32;
        }
        break;

      case AddOfExternalAndInt32:
        // See comment above.
        UNREACHABLE();
        break;
    }
  }

  ExternalAddType external_add_type_;
};


class HSub final : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  HValue* Canonicalize() override;

  DECLARE_CONCRETE_INSTRUCTION(Sub)

 protected:
  bool DataEquals(HValue* other) override { return true; }

  Range* InferRange(Zone* zone) override;

 private:
  HSub(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMul final : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  static HInstruction* NewImul(Isolate* isolate, Zone* zone, HValue* context,
                               HValue* left, HValue* right) {
    HInstruction* instr = HMul::New(isolate, zone, context, left, right);
    if (!instr->IsMul()) return instr;
    HMul* mul = HMul::cast(instr);
    // TODO(mstarzinger): Prevent bailout on minus zero for imul.
    mul->AssumeRepresentation(Representation::Integer32());
    mul->ClearFlag(HValue::kCanOverflow);
    return mul;
  }

  HValue* Canonicalize() override;

  // Only commutative if it is certain that not two objects are multiplicated.
  bool IsCommutative() const override { return !representation().IsTagged(); }

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  bool MulMinusOne();

  DECLARE_CONCRETE_INSTRUCTION(Mul)

 protected:
  bool DataEquals(HValue* other) override { return true; }

  Range* InferRange(Zone* zone) override;

 private:
  HMul(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanOverflow);
  }
};


class HMod final : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  HValue* Canonicalize() override;

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Mod)

 protected:
  bool DataEquals(HValue* other) override { return true; }

  Range* InferRange(Zone* zone) override;

 private:
  HMod(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
    SetFlag(kLeftCanBeNegative);
  }
};


class HDiv final : public HArithmeticBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  HValue* Canonicalize() override;

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HArithmeticBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Div)

 protected:
  bool DataEquals(HValue* other) override { return true; }

  Range* InferRange(Zone* zone) override;

 private:
  HDiv(HValue* context, HValue* left, HValue* right)
      : HArithmeticBinaryOperation(context, left, right) {
    SetFlag(kCanBeDivByZero);
    SetFlag(kCanOverflow);
  }
};


class HMathMinMax final : public HArithmeticBinaryOperation {
 public:
  enum Operation { kMathMin, kMathMax };

  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right, Operation op);

  Representation observed_input_representation(int index) override {
    return RequiredInputRepresentation(index);
  }

  void InferRepresentation(HInferRepresentationPhase* h_infer) override;

  Representation RepresentationFromInputs() override {
    Representation left_rep = left()->representation();
    Representation right_rep = right()->representation();
    Representation result = Representation::Smi();
    result = result.generalize(left_rep);
    result = result.generalize(right_rep);
    if (result.IsTagged()) return Representation::Double();
    return result;
  }

  bool IsCommutative() const override { return true; }

  Operation operation() { return operation_; }

  DECLARE_CONCRETE_INSTRUCTION(MathMinMax)

 protected:
  bool DataEquals(HValue* other) override {
    return other->IsMathMinMax() &&
        HMathMinMax::cast(other)->operation_ == operation_;
  }

  Range* InferRange(Zone* zone) override;

 private:
  HMathMinMax(HValue* context, HValue* left, HValue* right, Operation op)
      : HArithmeticBinaryOperation(context, left, right), operation_(op) {}

  Operation operation_;
};


class HBitwise final : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           Token::Value op, HValue* left, HValue* right);

  Token::Value op() const { return op_; }

  bool IsCommutative() const override { return true; }

  HValue* Canonicalize() override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Bitwise)

 protected:
  bool DataEquals(HValue* other) override {
    return op() == HBitwise::cast(other)->op();
  }

  Range* InferRange(Zone* zone) override;

 private:
  HBitwise(HValue* context, Token::Value op, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right), op_(op) {
    DCHECK(op == Token::BIT_AND || op == Token::BIT_OR || op == Token::BIT_XOR);
    // BIT_AND with a smi-range positive value will always unset the
    // entire sign-extension of the smi-sign.
    if (op == Token::BIT_AND &&
        ((left->IsConstant() &&
          left->representation().IsSmi() &&
          HConstant::cast(left)->Integer32Value() >= 0) ||
         (right->IsConstant() &&
          right->representation().IsSmi() &&
          HConstant::cast(right)->Integer32Value() >= 0))) {
      SetFlag(kTruncatingToSmi);
      SetFlag(kTruncatingToInt32);
    // BIT_OR with a smi-range negative value will always set the entire
    // sign-extension of the smi-sign.
    } else if (op == Token::BIT_OR &&
        ((left->IsConstant() &&
          left->representation().IsSmi() &&
          HConstant::cast(left)->Integer32Value() < 0) ||
         (right->IsConstant() &&
          right->representation().IsSmi() &&
          HConstant::cast(right)->Integer32Value() < 0))) {
      SetFlag(kTruncatingToSmi);
      SetFlag(kTruncatingToInt32);
    }
  }

  Token::Value op_;
};


class HShl final : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  Range* InferRange(Zone* zone) override;

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    if (new_rep.IsSmi() &&
        !(right()->IsInteger32Constant() &&
          right()->GetInteger32Constant() >= 0)) {
      new_rep = Representation::Integer32();
    }
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Shl)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HShl(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) {}
};


class HShr final : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  Range* InferRange(Zone* zone) override;

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Shr)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HShr(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) {}
};


class HSar final : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right);

  Range* InferRange(Zone* zone) override;

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Sar)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HSar(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) {}
};


class HRor final : public HBitwiseBinaryOperation {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* left, HValue* right) {
    return new (zone) HRor(context, left, right);
  }

  void UpdateRepresentation(Representation new_rep,
                            HInferRepresentationPhase* h_infer,
                            const char* reason) override {
    if (new_rep.IsSmi()) new_rep = Representation::Integer32();
    HBitwiseBinaryOperation::UpdateRepresentation(new_rep, h_infer, reason);
  }

  DECLARE_CONCRETE_INSTRUCTION(Ror)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  HRor(HValue* context, HValue* left, HValue* right)
      : HBitwiseBinaryOperation(context, left, right) {
    ChangeRepresentation(Representation::Integer32());
  }
};


class HOsrEntry final : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HOsrEntry, BailoutId);

  BailoutId ast_id() const { return ast_id_; }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  DECLARE_CONCRETE_INSTRUCTION(OsrEntry)

 private:
  explicit HOsrEntry(BailoutId ast_id) : ast_id_(ast_id) {
    SetChangesFlag(kOsrEntries);
    SetChangesFlag(kNewSpacePromotion);
  }

  BailoutId ast_id_;
};


class HParameter final : public HTemplateInstruction<0> {
 public:
  enum ParameterKind {
    STACK_PARAMETER,
    REGISTER_PARAMETER
  };

  DECLARE_INSTRUCTION_FACTORY_P1(HParameter, unsigned);
  DECLARE_INSTRUCTION_FACTORY_P2(HParameter, unsigned, ParameterKind);
  DECLARE_INSTRUCTION_FACTORY_P3(HParameter, unsigned, ParameterKind,
                                 Representation);

  unsigned index() const { return index_; }
  ParameterKind kind() const { return kind_; }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  Representation KnownOptimalRepresentation() override {
    // If a parameter is an input to a phi, that phi should not
    // choose any more optimistic representation than Tagged.
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Parameter)

 private:
  explicit HParameter(unsigned index,
                      ParameterKind kind = STACK_PARAMETER)
      : index_(index),
        kind_(kind) {
    set_representation(Representation::Tagged());
  }

  explicit HParameter(unsigned index,
                      ParameterKind kind,
                      Representation r)
      : index_(index),
        kind_(kind) {
    set_representation(r);
  }

  unsigned index_;
  ParameterKind kind_;
};


class HUnknownOSRValue final : public HTemplateInstruction<0> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HUnknownOSRValue, HEnvironment*, int);

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    return Representation::None();
  }

  void set_incoming_value(HPhi* value) { incoming_value_ = value; }
  HPhi* incoming_value() { return incoming_value_; }
  HEnvironment *environment() { return environment_; }
  int index() { return index_; }

  Representation KnownOptimalRepresentation() override {
    if (incoming_value_ == NULL) return Representation::None();
    return incoming_value_->KnownOptimalRepresentation();
  }

  DECLARE_CONCRETE_INSTRUCTION(UnknownOSRValue)

 private:
  HUnknownOSRValue(HEnvironment* environment, int index)
      : environment_(environment),
        index_(index),
        incoming_value_(NULL) {
    set_representation(Representation::Tagged());
  }

  HEnvironment* environment_;
  int index_;
  HPhi* incoming_value_;
};

class HAllocate final : public HTemplateInstruction<3> {
 public:
  static bool CompatibleInstanceTypes(InstanceType type1,
                                      InstanceType type2) {
    return ComputeFlags(TENURED, type1) == ComputeFlags(TENURED, type2) &&
        ComputeFlags(NOT_TENURED, type1) == ComputeFlags(NOT_TENURED, type2);
  }

  static HAllocate* New(
      Isolate* isolate, Zone* zone, HValue* context, HValue* size, HType type,
      PretenureFlag pretenure_flag, InstanceType instance_type,
      HValue* dominator,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null()) {
    return new (zone) HAllocate(context, size, type, pretenure_flag,
                                instance_type, dominator, allocation_site);
  }

  // Maximum instance size for which allocations will be inlined.
  static const int kMaxInlineSize = 64 * kPointerSize;

  HValue* context() const { return OperandAt(0); }
  HValue* size() const { return OperandAt(1); }
  HValue* allocation_folding_dominator() const { return OperandAt(2); }

  Representation RequiredInputRepresentation(int index) override {
    if (index == 0) {
      return Representation::Tagged();
    } else {
      return Representation::Integer32();
    }
  }

  Handle<Map> GetMonomorphicJSObjectMap() override {
    return known_initial_map_;
  }

  void set_known_initial_map(Handle<Map> known_initial_map) {
    known_initial_map_ = known_initial_map;
  }

  bool IsNewSpaceAllocation() const {
    return (flags_ & ALLOCATE_IN_NEW_SPACE) != 0;
  }

  bool IsOldSpaceAllocation() const {
    return (flags_ & ALLOCATE_IN_OLD_SPACE) != 0;
  }

  bool MustAllocateDoubleAligned() const {
    return (flags_ & ALLOCATE_DOUBLE_ALIGNED) != 0;
  }

  bool MustPrefillWithFiller() const {
    return (flags_ & PREFILL_WITH_FILLER) != 0;
  }

  void MakePrefillWithFiller() {
    flags_ = static_cast<HAllocate::Flags>(flags_ | PREFILL_WITH_FILLER);
  }

  void MakeDoubleAligned() {
    flags_ = static_cast<HAllocate::Flags>(flags_ | ALLOCATE_DOUBLE_ALIGNED);
  }

  void MakeAllocationFoldingDominator() {
    flags_ =
        static_cast<HAllocate::Flags>(flags_ | ALLOCATION_FOLDING_DOMINATOR);
  }

  bool IsAllocationFoldingDominator() const {
    return (flags_ & ALLOCATION_FOLDING_DOMINATOR) != 0;
  }

  void MakeFoldedAllocation(HAllocate* dominator) {
    flags_ = static_cast<HAllocate::Flags>(flags_ | ALLOCATION_FOLDED);
    ClearFlag(kTrackSideEffectDominators);
    ClearChangesFlag(kNewSpacePromotion);
    SetOperandAt(2, dominator);
  }

  bool IsAllocationFolded() const { return (flags_ & ALLOCATION_FOLDED) != 0; }

  bool HandleSideEffectDominator(GVNFlag side_effect,
                                 HValue* dominator) override;

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(Allocate)

 private:
  enum Flags {
    ALLOCATE_IN_NEW_SPACE = 1 << 0,
    ALLOCATE_IN_OLD_SPACE = 1 << 2,
    ALLOCATE_DOUBLE_ALIGNED = 1 << 3,
    PREFILL_WITH_FILLER = 1 << 4,
    ALLOCATION_FOLDING_DOMINATOR = 1 << 5,
    ALLOCATION_FOLDED = 1 << 6
  };

  HAllocate(
      HValue* context, HValue* size, HType type, PretenureFlag pretenure_flag,
      InstanceType instance_type, HValue* dominator,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null())
      : HTemplateInstruction<3>(type),
        flags_(ComputeFlags(pretenure_flag, instance_type)) {
    SetOperandAt(0, context);
    UpdateSize(size);
    SetOperandAt(2, dominator);
    set_representation(Representation::Tagged());
    SetFlag(kTrackSideEffectDominators);
    SetChangesFlag(kNewSpacePromotion);
    SetDependsOnFlag(kNewSpacePromotion);

    if (FLAG_trace_pretenuring) {
      PrintF("HAllocate with AllocationSite %p %s\n",
             allocation_site.is_null()
                 ? static_cast<void*>(NULL)
                 : static_cast<void*>(*allocation_site),
             pretenure_flag == TENURED ? "tenured" : "not tenured");
    }
  }

  static Flags ComputeFlags(PretenureFlag pretenure_flag,
                            InstanceType instance_type) {
    Flags flags = pretenure_flag == TENURED ? ALLOCATE_IN_OLD_SPACE
                                            : ALLOCATE_IN_NEW_SPACE;
    if (instance_type == FIXED_DOUBLE_ARRAY_TYPE) {
      flags = static_cast<Flags>(flags | ALLOCATE_DOUBLE_ALIGNED);
    }
    // We have to fill the allocated object with one word fillers if we do
    // not use allocation folding since some allocations may depend on each
    // other, i.e., have a pointer to each other. A GC in between these
    // allocations may leave such objects behind in a not completely initialized
    // state.
    if (!FLAG_use_gvn || !FLAG_use_allocation_folding) {
      flags = static_cast<Flags>(flags | PREFILL_WITH_FILLER);
    }
    return flags;
  }

  void UpdateSize(HValue* size) {
    SetOperandAt(1, size);
  }

  bool IsFoldable(HAllocate* allocate) {
    return (IsNewSpaceAllocation() && allocate->IsNewSpaceAllocation()) ||
           (IsOldSpaceAllocation() && allocate->IsOldSpaceAllocation());
  }

  Flags flags_;
  Handle<Map> known_initial_map_;
};


class HStoreCodeEntry final : public HTemplateInstruction<2> {
 public:
  static HStoreCodeEntry* New(Isolate* isolate, Zone* zone, HValue* context,
                              HValue* function, HValue* code) {
    return new(zone) HStoreCodeEntry(function, code);
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* function() { return OperandAt(0); }
  HValue* code_object() { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(StoreCodeEntry)

 private:
  HStoreCodeEntry(HValue* function, HValue* code) {
    SetOperandAt(0, function);
    SetOperandAt(1, code);
  }
};


class HInnerAllocatedObject final : public HTemplateInstruction<2> {
 public:
  static HInnerAllocatedObject* New(Isolate* isolate, Zone* zone,
                                    HValue* context, HValue* value,
                                    HValue* offset, HType type) {
    return new(zone) HInnerAllocatedObject(value, offset, type);
  }

  HValue* base_object() const { return OperandAt(0); }
  HValue* offset() const { return OperandAt(1); }

  Representation RequiredInputRepresentation(int index) override {
    return index == 0 ? Representation::Tagged() : Representation::Integer32();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(InnerAllocatedObject)

 private:
  HInnerAllocatedObject(HValue* value,
                        HValue* offset,
                        HType type) : HTemplateInstruction<2>(type) {
    DCHECK(value->IsAllocate());
    DCHECK(type.IsHeapObject());
    SetOperandAt(0, value);
    SetOperandAt(1, offset);
    set_representation(Representation::Tagged());
  }
};


inline bool StoringValueNeedsWriteBarrier(HValue* value) {
  return !value->type().IsSmi()
      && !value->type().IsNull()
      && !value->type().IsBoolean()
      && !value->type().IsUndefined()
      && !(value->IsConstant() && HConstant::cast(value)->ImmortalImmovable());
}


inline bool ReceiverObjectNeedsWriteBarrier(HValue* object,
                                            HValue* value,
                                            HValue* dominator) {
  // There may be multiple inner allocates dominated by one allocate.
  while (object->IsInnerAllocatedObject()) {
    object = HInnerAllocatedObject::cast(object)->base_object();
  }

  if (object->IsAllocate()) {
    HAllocate* allocate = HAllocate::cast(object);
    if (allocate->IsAllocationFolded()) {
      HValue* dominator = allocate->allocation_folding_dominator();
      // There is no guarantee that all allocations are folded together because
      // GVN performs a fixpoint.
      if (HAllocate::cast(dominator)->IsAllocationFoldingDominator()) {
        object = dominator;
      }
    }
  }

  if (object->IsConstant() &&
      HConstant::cast(object)->HasExternalReferenceValue()) {
    // Stores to external references require no write barriers
    return false;
  }
  // We definitely need a write barrier unless the object is the allocation
  // dominator.
  if (object == dominator && object->IsAllocate()) {
    // Stores to new space allocations require no write barriers.
    if (HAllocate::cast(object)->IsNewSpaceAllocation()) {
      return false;
    }
  }
  return true;
}


inline PointersToHereCheck PointersToHereCheckForObject(HValue* object,
                                                        HValue* dominator) {
  while (object->IsInnerAllocatedObject()) {
    object = HInnerAllocatedObject::cast(object)->base_object();
  }
  if (object == dominator &&
      object->IsAllocate() &&
      HAllocate::cast(object)->IsNewSpaceAllocation()) {
    return kPointersToHereAreAlwaysInteresting;
  }
  return kPointersToHereMaybeInteresting;
}


class HLoadContextSlot final : public HUnaryOperation {
 public:
  enum Mode {
    // Perform a normal load of the context slot without checking its value.
    kNoCheck,
    // Load and check the value of the context slot. Deoptimize if it's the
    // hole value. This is used for checking for loading of uninitialized
    // harmony bindings where we deoptimize into full-codegen generated code
    // which will subsequently throw a reference error.
    kCheckDeoptimize
  };

  HLoadContextSlot(HValue* context, int slot_index, Mode mode)
      : HUnaryOperation(context), slot_index_(slot_index), mode_(mode) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kContextSlots);
  }

  int slot_index() const { return slot_index_; }
  Mode mode() const { return mode_; }

  bool DeoptimizesOnHole() {
    return mode_ == kCheckDeoptimize;
  }

  bool RequiresHoleCheck() const {
    return mode_ != kNoCheck;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(LoadContextSlot)

 protected:
  bool DataEquals(HValue* other) override {
    HLoadContextSlot* b = HLoadContextSlot::cast(other);
    return (slot_index() == b->slot_index());
  }

 private:
  bool IsDeletable() const override { return !RequiresHoleCheck(); }

  int slot_index_;
  Mode mode_;
};


class HStoreContextSlot final : public HTemplateInstruction<2> {
 public:
  enum Mode {
    // Perform a normal store to the context slot without checking its previous
    // value.
    kNoCheck,
    // Check the previous value of the context slot and deoptimize if it's the
    // hole value. This is used for checking for assignments to uninitialized
    // harmony bindings where we deoptimize into full-codegen generated code
    // which will subsequently throw a reference error.
    kCheckDeoptimize
  };

  DECLARE_INSTRUCTION_FACTORY_P4(HStoreContextSlot, HValue*, int,
                                 Mode, HValue*);

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }
  int slot_index() const { return slot_index_; }
  Mode mode() const { return mode_; }

  bool NeedsWriteBarrier() {
    return StoringValueNeedsWriteBarrier(value());
  }

  bool DeoptimizesOnHole() {
    return mode_ == kCheckDeoptimize;
  }

  bool RequiresHoleCheck() {
    return mode_ != kNoCheck;
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(StoreContextSlot)

 private:
  HStoreContextSlot(HValue* context, int slot_index, Mode mode, HValue* value)
      : slot_index_(slot_index), mode_(mode) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    SetChangesFlag(kContextSlots);
  }

  int slot_index_;
  Mode mode_;
};


// Represents an access to a portion of an object, such as the map pointer,
// array elements pointer, etc, but not accesses to array elements themselves.
class HObjectAccess final {
 public:
  inline bool IsInobject() const {
    return portion() != kBackingStore && portion() != kExternalMemory;
  }

  inline bool IsExternalMemory() const {
    return portion() == kExternalMemory;
  }

  inline bool IsStringLength() const {
    return portion() == kStringLengths;
  }

  inline bool IsMap() const {
    return portion() == kMaps;
  }

  inline int offset() const {
    return OffsetField::decode(value_);
  }

  inline Representation representation() const {
    return Representation::FromKind(RepresentationField::decode(value_));
  }

  inline Handle<Name> name() const { return name_; }

  inline bool immutable() const {
    return ImmutableField::decode(value_);
  }

  // Returns true if access is being made to an in-object property that
  // was already added to the object.
  inline bool existing_inobject_property() const {
    return ExistingInobjectPropertyField::decode(value_);
  }

  inline HObjectAccess WithRepresentation(Representation representation) {
    return HObjectAccess(portion(), offset(), representation, name(),
                         immutable(), existing_inobject_property());
  }

  static HObjectAccess ForHeapNumberValue() {
    return HObjectAccess(
        kDouble, HeapNumber::kValueOffset, Representation::Double());
  }

  static HObjectAccess ForHeapNumberValueLowestBits() {
    return HObjectAccess(kDouble,
                         HeapNumber::kValueOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForHeapNumberValueHighestBits() {
    return HObjectAccess(kDouble,
                         HeapNumber::kValueOffset + kIntSize,
                         Representation::Integer32());
  }

  static HObjectAccess ForOddballToNumber(
      Representation representation = Representation::Tagged()) {
    return HObjectAccess(kInobject, Oddball::kToNumberOffset, representation);
  }

  static HObjectAccess ForOddballTypeOf() {
    return HObjectAccess(kInobject, Oddball::kTypeOfOffset,
                         Representation::HeapObject());
  }

  static HObjectAccess ForElementsPointer() {
    return HObjectAccess(kElementsPointer, JSObject::kElementsOffset);
  }

  static HObjectAccess ForLiteralsPointer() {
    return HObjectAccess(kInobject, JSFunction::kLiteralsOffset);
  }

  static HObjectAccess ForNextFunctionLinkPointer() {
    return HObjectAccess(kInobject, JSFunction::kNextFunctionLinkOffset);
  }

  static HObjectAccess ForArrayLength(ElementsKind elements_kind) {
    return HObjectAccess(
        kArrayLengths,
        JSArray::kLengthOffset,
        IsFastElementsKind(elements_kind)
            ? Representation::Smi() : Representation::Tagged());
  }

  static HObjectAccess ForAllocationSiteOffset(int offset);

  static HObjectAccess ForAllocationSiteList() {
    return HObjectAccess(kExternalMemory, 0, Representation::Tagged(),
                         Handle<Name>::null(), false, false);
  }

  static HObjectAccess ForFixedArrayLength() {
    return HObjectAccess(
        kArrayLengths,
        FixedArray::kLengthOffset,
        Representation::Smi());
  }

  static HObjectAccess ForFixedTypedArrayBaseBasePointer() {
    return HObjectAccess(kInobject, FixedTypedArrayBase::kBasePointerOffset,
                         Representation::Tagged());
  }

  static HObjectAccess ForFixedTypedArrayBaseExternalPointer() {
    return HObjectAccess::ForObservableJSObjectOffset(
        FixedTypedArrayBase::kExternalPointerOffset,
        Representation::External());
  }

  static HObjectAccess ForStringHashField() {
    return HObjectAccess(kInobject,
                         String::kHashFieldOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForStringLength() {
    STATIC_ASSERT(String::kMaxLength <= Smi::kMaxValue);
    return HObjectAccess(
        kStringLengths,
        String::kLengthOffset,
        Representation::Smi());
  }

  static HObjectAccess ForConsStringFirst() {
    return HObjectAccess(kInobject, ConsString::kFirstOffset);
  }

  static HObjectAccess ForConsStringSecond() {
    return HObjectAccess(kInobject, ConsString::kSecondOffset);
  }

  static HObjectAccess ForPropertiesPointer() {
    return HObjectAccess(kInobject, JSObject::kPropertiesOffset);
  }

  static HObjectAccess ForPrototypeOrInitialMap() {
    return HObjectAccess(kInobject, JSFunction::kPrototypeOrInitialMapOffset);
  }

  static HObjectAccess ForSharedFunctionInfoPointer() {
    return HObjectAccess(kInobject, JSFunction::kSharedFunctionInfoOffset);
  }

  static HObjectAccess ForCodeEntryPointer() {
    return HObjectAccess(kInobject, JSFunction::kCodeEntryOffset);
  }

  static HObjectAccess ForCodeOffset() {
    return HObjectAccess(kInobject, SharedFunctionInfo::kCodeOffset);
  }

  static HObjectAccess ForOptimizedCodeMap() {
    return HObjectAccess(kInobject,
                         SharedFunctionInfo::kOptimizedCodeMapOffset);
  }

  static HObjectAccess ForFunctionContextPointer() {
    return HObjectAccess(kInobject, JSFunction::kContextOffset);
  }

  static HObjectAccess ForMap() {
    return HObjectAccess(kMaps, JSObject::kMapOffset);
  }

  static HObjectAccess ForPrototype() {
    return HObjectAccess(kMaps, Map::kPrototypeOffset);
  }

  static HObjectAccess ForMapAsInteger32() {
    return HObjectAccess(kMaps, JSObject::kMapOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForMapInObjectPropertiesOrConstructorFunctionIndex() {
    return HObjectAccess(
        kInobject, Map::kInObjectPropertiesOrConstructorFunctionIndexOffset,
        Representation::UInteger8());
  }

  static HObjectAccess ForMapInstanceType() {
    return HObjectAccess(kInobject,
                         Map::kInstanceTypeOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapInstanceSize() {
    return HObjectAccess(kInobject,
                         Map::kInstanceSizeOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapBitField() {
    return HObjectAccess(kInobject,
                         Map::kBitFieldOffset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapBitField2() {
    return HObjectAccess(kInobject,
                         Map::kBitField2Offset,
                         Representation::UInteger8());
  }

  static HObjectAccess ForMapBitField3() {
    return HObjectAccess(kInobject, Map::kBitField3Offset,
                         Representation::Integer32());
  }

  static HObjectAccess ForMapDescriptors() {
    return HObjectAccess(kInobject, Map::kDescriptorsOffset);
  }

  static HObjectAccess ForNameHashField() {
    return HObjectAccess(kInobject,
                         Name::kHashFieldOffset,
                         Representation::Integer32());
  }

  static HObjectAccess ForMapInstanceTypeAndBitField() {
    STATIC_ASSERT((Map::kInstanceTypeAndBitFieldOffset & 1) == 0);
    // Ensure the two fields share one 16-bit word, endian-independent.
    STATIC_ASSERT((Map::kBitFieldOffset & ~1) ==
                  (Map::kInstanceTypeOffset & ~1));
    return HObjectAccess(kInobject,
                         Map::kInstanceTypeAndBitFieldOffset,
                         Representation::UInteger16());
  }

  static HObjectAccess ForPropertyCellValue() {
    return HObjectAccess(kInobject, PropertyCell::kValueOffset);
  }

  static HObjectAccess ForPropertyCellDetails() {
    return HObjectAccess(kInobject, PropertyCell::kDetailsOffset,
                         Representation::Smi());
  }

  static HObjectAccess ForCellValue() {
    return HObjectAccess(kInobject, Cell::kValueOffset);
  }

  static HObjectAccess ForWeakCellValue() {
    return HObjectAccess(kInobject, WeakCell::kValueOffset);
  }

  static HObjectAccess ForWeakCellNext() {
    return HObjectAccess(kInobject, WeakCell::kNextOffset);
  }

  static HObjectAccess ForAllocationMementoSite() {
    return HObjectAccess(kInobject, AllocationMemento::kAllocationSiteOffset);
  }

  static HObjectAccess ForCounter() {
    return HObjectAccess(kExternalMemory, 0, Representation::Integer32(),
                         Handle<Name>::null(), false, false);
  }

  static HObjectAccess ForExternalUInteger8() {
    return HObjectAccess(kExternalMemory, 0, Representation::UInteger8(),
                         Handle<Name>::null(), false, false);
  }

  static HObjectAccess ForBoundTargetFunction() {
    return HObjectAccess(kInobject,
                         JSBoundFunction::kBoundTargetFunctionOffset);
  }

  static HObjectAccess ForBoundThis() {
    return HObjectAccess(kInobject, JSBoundFunction::kBoundThisOffset);
  }

  static HObjectAccess ForBoundArguments() {
    return HObjectAccess(kInobject, JSBoundFunction::kBoundArgumentsOffset);
  }

  // Create an access to an offset in a fixed array header.
  static HObjectAccess ForFixedArrayHeader(int offset);

  // Create an access to an in-object property in a JSObject.
  // This kind of access must be used when the object |map| is known and
  // in-object properties are being accessed. Accesses of the in-object
  // properties can have different semantics depending on whether corresponding
  // property was added to the map or not.
  static HObjectAccess ForMapAndOffset(Handle<Map> map, int offset,
      Representation representation = Representation::Tagged());

  // Create an access to an in-object property in a JSObject.
  // This kind of access can be used for accessing object header fields or
  // in-object properties if the map of the object is not known.
  static HObjectAccess ForObservableJSObjectOffset(int offset,
      Representation representation = Representation::Tagged()) {
    return ForMapAndOffset(Handle<Map>::null(), offset, representation);
  }

  // Create an access to an in-object property in a JSArray.
  static HObjectAccess ForJSArrayOffset(int offset);

  static HObjectAccess ForContextSlot(int index);

  static HObjectAccess ForScriptContext(int index);

  // Create an access to the backing store of an object.
  static HObjectAccess ForBackingStoreOffset(int offset,
      Representation representation = Representation::Tagged());

  // Create an access to a resolved field (in-object or backing store).
  static HObjectAccess ForField(Handle<Map> map, int index,
                                Representation representation,
                                Handle<Name> name);

  static HObjectAccess ForJSTypedArrayLength() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSTypedArray::kLengthOffset);
  }

  static HObjectAccess ForJSArrayBufferBackingStore() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kBackingStoreOffset, Representation::External());
  }

  static HObjectAccess ForJSArrayBufferByteLength() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kByteLengthOffset, Representation::Tagged());
  }

  static HObjectAccess ForJSArrayBufferBitField() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kBitFieldOffset, Representation::Integer32());
  }

  static HObjectAccess ForJSArrayBufferBitFieldSlot() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBuffer::kBitFieldSlot, Representation::Smi());
  }

  static HObjectAccess ForJSArrayBufferViewBuffer() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kBufferOffset);
  }

  static HObjectAccess ForJSArrayBufferViewByteOffset() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kByteOffsetOffset);
  }

  static HObjectAccess ForJSArrayBufferViewByteLength() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSArrayBufferView::kByteLengthOffset);
  }

  static HObjectAccess ForJSGlobalObjectNativeContext() {
    return HObjectAccess(kInobject, JSGlobalObject::kNativeContextOffset);
  }

  static HObjectAccess ForJSRegExpFlags() {
    return HObjectAccess(kInobject, JSRegExp::kFlagsOffset);
  }

  static HObjectAccess ForJSRegExpSource() {
    return HObjectAccess(kInobject, JSRegExp::kSourceOffset);
  }

  static HObjectAccess ForJSCollectionTable() {
    return HObjectAccess::ForObservableJSObjectOffset(
        JSCollection::kTableOffset);
  }

  template <typename CollectionType>
  static HObjectAccess ForOrderedHashTableNumberOfBuckets() {
    return HObjectAccess(kInobject, CollectionType::kNumberOfBucketsOffset,
                         Representation::Smi());
  }

  template <typename CollectionType>
  static HObjectAccess ForOrderedHashTableNumberOfElements() {
    return HObjectAccess(kInobject, CollectionType::kNumberOfElementsOffset,
                         Representation::Smi());
  }

  template <typename CollectionType>
  static HObjectAccess ForOrderedHashTableNumberOfDeletedElements() {
    return HObjectAccess(kInobject,
                         CollectionType::kNumberOfDeletedElementsOffset,
                         Representation::Smi());
  }

  template <typename CollectionType>
  static HObjectAccess ForOrderedHashTableNextTable() {
    return HObjectAccess(kInobject, CollectionType::kNextTableOffset);
  }

  template <typename CollectionType>
  static HObjectAccess ForOrderedHashTableBucket(int bucket) {
    return HObjectAccess(kInobject, CollectionType::kHashTableStartOffset +
                                        (bucket * kPointerSize),
                         Representation::Smi());
  }

  // Access into the data table of an OrderedHashTable with a
  // known-at-compile-time bucket count.
  template <typename CollectionType, int kBucketCount>
  static HObjectAccess ForOrderedHashTableDataTableIndex(int index) {
    return HObjectAccess(kInobject, CollectionType::kHashTableStartOffset +
                                        (kBucketCount * kPointerSize) +
                                        (index * kPointerSize));
  }

  inline bool Equals(HObjectAccess that) const {
    return value_ == that.value_;  // portion and offset must match
  }

 protected:
  void SetGVNFlags(HValue *instr, PropertyAccessType access_type);

 private:
  // internal use only; different parts of an object or array
  enum Portion {
    kMaps,             // map of an object
    kArrayLengths,     // the length of an array
    kStringLengths,    // the length of a string
    kElementsPointer,  // elements pointer
    kBackingStore,     // some field in the backing store
    kDouble,           // some double field
    kInobject,         // some other in-object field
    kExternalMemory    // some field in external memory
  };

  HObjectAccess() : value_(0) {}

  HObjectAccess(Portion portion, int offset,
                Representation representation = Representation::Tagged(),
                Handle<Name> name = Handle<Name>::null(),
                bool immutable = false, bool existing_inobject_property = true)
      : value_(PortionField::encode(portion) |
               RepresentationField::encode(representation.kind()) |
               ImmutableField::encode(immutable ? 1 : 0) |
               ExistingInobjectPropertyField::encode(
                   existing_inobject_property ? 1 : 0) |
               OffsetField::encode(offset)),
        name_(name) {
    // assert that the fields decode correctly
    DCHECK(this->offset() == offset);
    DCHECK(this->portion() == portion);
    DCHECK(this->immutable() == immutable);
    DCHECK(this->existing_inobject_property() == existing_inobject_property);
    DCHECK(RepresentationField::decode(value_) == representation.kind());
    DCHECK(!this->existing_inobject_property() || IsInobject());
  }

  class PortionField : public BitField<Portion, 0, 3> {};
  class RepresentationField : public BitField<Representation::Kind, 3, 4> {};
  class ImmutableField : public BitField<bool, 7, 1> {};
  class ExistingInobjectPropertyField : public BitField<bool, 8, 1> {};
  class OffsetField : public BitField<int, 9, 23> {};

  uint32_t value_;  // encodes portion, representation, immutable, and offset
  Handle<Name> name_;

  friend class HLoadNamedField;
  friend class HStoreNamedField;
  friend class SideEffectsTracker;
  friend std::ostream& operator<<(std::ostream& os,
                                  const HObjectAccess& access);

  inline Portion portion() const {
    return PortionField::decode(value_);
  }
};


std::ostream& operator<<(std::ostream& os, const HObjectAccess& access);


class HLoadNamedField final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HLoadNamedField, HValue*,
                                 HValue*, HObjectAccess);
  DECLARE_INSTRUCTION_FACTORY_P5(HLoadNamedField, HValue*, HValue*,
                                 HObjectAccess, const UniqueSet<Map>*, HType);

  HValue* object() const { return OperandAt(0); }
  HValue* dependency() const {
    DCHECK(HasDependency());
    return OperandAt(1);
  }
  bool HasDependency() const { return OperandAt(0) != OperandAt(1); }
  HObjectAccess access() const { return access_; }
  Representation field_representation() const {
      return access_.representation();
  }

  const UniqueSet<Map>* maps() const { return maps_; }

  bool HasEscapingOperandAt(int index) override { return false; }
  bool HasOutOfBoundsAccess(int size) override {
    return !access().IsInobject() || access().offset() >= size;
  }
  Representation RequiredInputRepresentation(int index) override {
    if (index == 0) {
      // object must be external in case of external memory access
      return access().IsExternalMemory() ? Representation::External()
                                         : Representation::Tagged();
    }
    DCHECK(index == 1);
    return Representation::None();
  }
  Range* InferRange(Zone* zone) override;
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  bool CanBeReplacedWith(HValue* other) const {
    if (!CheckFlag(HValue::kCantBeReplaced)) return false;
    if (!type().Equals(other->type())) return false;
    if (!representation().Equals(other->representation())) return false;
    if (!other->IsLoadNamedField()) return true;
    HLoadNamedField* that = HLoadNamedField::cast(other);
    if (this->maps_ == that->maps_) return true;
    if (this->maps_ == NULL || that->maps_ == NULL) return false;
    return this->maps_->IsSubset(that->maps_);
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadNamedField)

 protected:
  bool DataEquals(HValue* other) override {
    HLoadNamedField* that = HLoadNamedField::cast(other);
    if (!this->access_.Equals(that->access_)) return false;
    if (this->maps_ == that->maps_) return true;
    return (this->maps_ != NULL &&
            that->maps_ != NULL &&
            this->maps_->Equals(that->maps_));
  }

 private:
  HLoadNamedField(HValue* object,
                  HValue* dependency,
                  HObjectAccess access)
      : access_(access), maps_(NULL) {
    DCHECK_NOT_NULL(object);
    SetOperandAt(0, object);
    SetOperandAt(1, dependency ? dependency : object);

    Representation representation = access.representation();
    if (representation.IsInteger8() ||
        representation.IsUInteger8() ||
        representation.IsInteger16() ||
        representation.IsUInteger16()) {
      set_representation(Representation::Integer32());
    } else if (representation.IsSmi()) {
      set_type(HType::Smi());
      if (SmiValuesAre32Bits()) {
        set_representation(Representation::Integer32());
      } else {
        set_representation(representation);
      }
    } else if (representation.IsDouble() ||
               representation.IsExternal() ||
               representation.IsInteger32()) {
      set_representation(representation);
    } else if (representation.IsHeapObject()) {
      set_type(HType::HeapObject());
      set_representation(Representation::Tagged());
    } else {
      set_representation(Representation::Tagged());
    }
    access.SetGVNFlags(this, LOAD);
  }

  HLoadNamedField(HValue* object,
                  HValue* dependency,
                  HObjectAccess access,
                  const UniqueSet<Map>* maps,
                  HType type)
      : HTemplateInstruction<2>(type), access_(access), maps_(maps) {
    DCHECK_NOT_NULL(maps);
    DCHECK_NE(0, maps->size());

    DCHECK_NOT_NULL(object);
    SetOperandAt(0, object);
    SetOperandAt(1, dependency ? dependency : object);

    DCHECK(access.representation().IsHeapObject());
    DCHECK(type.IsHeapObject());
    set_representation(Representation::Tagged());

    access.SetGVNFlags(this, LOAD);
  }

  bool IsDeletable() const override { return true; }

  HObjectAccess access_;
  const UniqueSet<Map>* maps_;
};


class HLoadFunctionPrototype final : public HUnaryOperation {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HLoadFunctionPrototype, HValue*);

  HValue* function() { return OperandAt(0); }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(LoadFunctionPrototype)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HLoadFunctionPrototype(HValue* function)
      : HUnaryOperation(function) {
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kCalls);
  }
};

class ArrayInstructionInterface {
 public:
  virtual HValue* GetKey() = 0;
  virtual void SetKey(HValue* key) = 0;
  virtual ElementsKind elements_kind() const = 0;
  // TryIncreaseBaseOffset returns false if overflow would result.
  virtual bool TryIncreaseBaseOffset(uint32_t increase_by_value) = 0;
  virtual bool IsDehoisted() const = 0;
  virtual void SetDehoisted(bool is_dehoisted) = 0;
  virtual ~ArrayInstructionInterface() { }

  static Representation KeyedAccessIndexRequirement(Representation r) {
    return r.IsInteger32() || SmiValuesAre32Bits()
        ? Representation::Integer32() : Representation::Smi();
  }
};


static const int kDefaultKeyedHeaderOffsetSentinel = -1;

enum LoadKeyedHoleMode {
  NEVER_RETURN_HOLE,
  ALLOW_RETURN_HOLE,
  CONVERT_HOLE_TO_UNDEFINED
};


class HLoadKeyed final : public HTemplateInstruction<4>,
                         public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P5(HLoadKeyed, HValue*, HValue*, HValue*, HValue*,
                                 ElementsKind);
  DECLARE_INSTRUCTION_FACTORY_P6(HLoadKeyed, HValue*, HValue*, HValue*, HValue*,
                                 ElementsKind, LoadKeyedHoleMode);
  DECLARE_INSTRUCTION_FACTORY_P7(HLoadKeyed, HValue*, HValue*, HValue*, HValue*,
                                 ElementsKind, LoadKeyedHoleMode, int);

  bool is_fixed_typed_array() const {
    return IsFixedTypedArrayElementsKind(elements_kind());
  }
  HValue* elements() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* dependency() const {
    DCHECK(HasDependency());
    return OperandAt(2);
  }
  bool HasDependency() const { return OperandAt(0) != OperandAt(2); }
  HValue* backing_store_owner() const {
    DCHECK(HasBackingStoreOwner());
    return OperandAt(3);
  }
  bool HasBackingStoreOwner() const { return OperandAt(0) != OperandAt(3); }
  uint32_t base_offset() const { return BaseOffsetField::decode(bit_field_); }
  bool TryIncreaseBaseOffset(uint32_t increase_by_value) override;
  HValue* GetKey() override { return key(); }
  void SetKey(HValue* key) override { SetOperandAt(1, key); }
  bool IsDehoisted() const override {
    return IsDehoistedField::decode(bit_field_);
  }
  void SetDehoisted(bool is_dehoisted) override {
    bit_field_ = IsDehoistedField::update(bit_field_, is_dehoisted);
  }
  ElementsKind elements_kind() const override {
    return ElementsKindField::decode(bit_field_);
  }
  LoadKeyedHoleMode hole_mode() const {
    return HoleModeField::decode(bit_field_);
  }

  Representation RequiredInputRepresentation(int index) override {
    // kind_fast:                 tagged[int32] (none)
    // kind_double:               tagged[int32] (none)
    // kind_fixed_typed_array:    external[int32] (none)
    // kind_external:             external[int32] (none)
    if (index == 0) {
      return is_fixed_typed_array() ? Representation::External()
                                    : Representation::Tagged();
    }
    if (index == 1) {
      return ArrayInstructionInterface::KeyedAccessIndexRequirement(
          OperandAt(1)->representation());
    }
    if (index == 2) {
      return Representation::None();
    }
    DCHECK_EQ(3, index);
    return HasBackingStoreOwner() ? Representation::Tagged()
                                  : Representation::None();
  }

  Representation observed_input_representation(int index) override {
    return RequiredInputRepresentation(index);
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  bool UsesMustHandleHole() const;
  bool AllUsesCanTreatHoleAsNaN() const;
  bool RequiresHoleCheck() const;

  Range* InferRange(Zone* zone) override;

  DECLARE_CONCRETE_INSTRUCTION(LoadKeyed)

 protected:
  bool DataEquals(HValue* other) override {
    if (!other->IsLoadKeyed()) return false;
    HLoadKeyed* other_load = HLoadKeyed::cast(other);

    if (base_offset() != other_load->base_offset()) return false;
    return elements_kind() == other_load->elements_kind();
  }

 private:
  HLoadKeyed(HValue* obj, HValue* key, HValue* dependency,
             HValue* backing_store_owner, ElementsKind elements_kind,
             LoadKeyedHoleMode mode = NEVER_RETURN_HOLE,
             int offset = kDefaultKeyedHeaderOffsetSentinel)
      : bit_field_(0) {
    offset = offset == kDefaultKeyedHeaderOffsetSentinel
        ? GetDefaultHeaderSizeForElementsKind(elements_kind)
        : offset;
    bit_field_ = ElementsKindField::encode(elements_kind) |
        HoleModeField::encode(mode) |
        BaseOffsetField::encode(offset);

    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, dependency != nullptr ? dependency : obj);
    SetOperandAt(3, backing_store_owner != nullptr ? backing_store_owner : obj);
    DCHECK_EQ(HasBackingStoreOwner(), is_fixed_typed_array());

    if (!is_fixed_typed_array()) {
      // I can detect the case between storing double (holey and fast) and
      // smi/object by looking at elements_kind_.
      DCHECK(IsFastSmiOrObjectElementsKind(elements_kind) ||
             IsFastDoubleElementsKind(elements_kind));

      if (IsFastSmiOrObjectElementsKind(elements_kind)) {
        if (IsFastSmiElementsKind(elements_kind) &&
            (!IsHoleyElementsKind(elements_kind) ||
             mode == NEVER_RETURN_HOLE)) {
          set_type(HType::Smi());
          if (SmiValuesAre32Bits() && !RequiresHoleCheck()) {
            set_representation(Representation::Integer32());
          } else {
            set_representation(Representation::Smi());
          }
        } else {
          set_representation(Representation::Tagged());
        }

        SetDependsOnFlag(kArrayElements);
      } else {
        set_representation(Representation::Double());
        SetDependsOnFlag(kDoubleArrayElements);
      }
    } else {
      if (elements_kind == FLOAT32_ELEMENTS ||
          elements_kind == FLOAT64_ELEMENTS) {
        set_representation(Representation::Double());
      } else {
        set_representation(Representation::Integer32());
      }

      if (is_fixed_typed_array()) {
        SetDependsOnFlag(kExternalMemory);
        SetDependsOnFlag(kTypedArrayElements);
      } else {
        UNREACHABLE();
      }
      // Native code could change the specialized array.
      SetDependsOnFlag(kCalls);
    }

    SetFlag(kUseGVN);
  }

  bool IsDeletable() const override { return !RequiresHoleCheck(); }

  // Establish some checks around our packed fields
  enum LoadKeyedBits {
    kBitsForElementsKind = 5,
    kBitsForHoleMode = 2,
    kBitsForBaseOffset = 24,
    kBitsForIsDehoisted = 1,

    kStartElementsKind = 0,
    kStartHoleMode = kStartElementsKind + kBitsForElementsKind,
    kStartBaseOffset = kStartHoleMode + kBitsForHoleMode,
    kStartIsDehoisted = kStartBaseOffset + kBitsForBaseOffset
  };

  STATIC_ASSERT((kBitsForElementsKind + kBitsForHoleMode + kBitsForBaseOffset +
                 kBitsForIsDehoisted) <= sizeof(uint32_t) * 8);
  STATIC_ASSERT(kElementsKindCount <= (1 << kBitsForElementsKind));
  class ElementsKindField:
    public BitField<ElementsKind, kStartElementsKind, kBitsForElementsKind>
    {};  // NOLINT
  class HoleModeField:
    public BitField<LoadKeyedHoleMode, kStartHoleMode, kBitsForHoleMode>
    {};  // NOLINT
  class BaseOffsetField:
    public BitField<uint32_t, kStartBaseOffset, kBitsForBaseOffset>
    {};  // NOLINT
  class IsDehoistedField:
    public BitField<bool, kStartIsDehoisted, kBitsForIsDehoisted>
    {};  // NOLINT
  uint32_t bit_field_;
};


// Indicates whether the store is a store to an entry that was previously
// initialized or not.
enum StoreFieldOrKeyedMode {
  // The entry could be either previously initialized or not.
  INITIALIZING_STORE,
  // At the time of this store it is guaranteed that the entry is already
  // initialized.
  STORE_TO_INITIALIZED_ENTRY
};


class HStoreNamedField final : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*);
  DECLARE_INSTRUCTION_FACTORY_P4(HStoreNamedField, HValue*,
                                 HObjectAccess, HValue*, StoreFieldOrKeyedMode);

  DECLARE_CONCRETE_INSTRUCTION(StoreNamedField)

  bool HasEscapingOperandAt(int index) override { return index == 1; }
  bool HasOutOfBoundsAccess(int size) override {
    return !access().IsInobject() || access().offset() >= size;
  }
  Representation RequiredInputRepresentation(int index) override {
    if (index == 0 && access().IsExternalMemory()) {
      // object must be external in case of external memory access
      return Representation::External();
    } else if (index == 1) {
      if (field_representation().IsInteger8() ||
          field_representation().IsUInteger8() ||
          field_representation().IsInteger16() ||
          field_representation().IsUInteger16() ||
          field_representation().IsInteger32()) {
        return Representation::Integer32();
      } else if (field_representation().IsDouble()) {
        return field_representation();
      } else if (field_representation().IsSmi()) {
        if (SmiValuesAre32Bits() &&
            store_mode() == STORE_TO_INITIALIZED_ENTRY) {
          return Representation::Integer32();
        }
        return field_representation();
      } else if (field_representation().IsExternal()) {
        return Representation::External();
      }
    }
    return Representation::Tagged();
  }
  bool HandleSideEffectDominator(GVNFlag side_effect,
                                 HValue* dominator) override {
    DCHECK(side_effect == kNewSpacePromotion);
    if (!FLAG_use_write_barrier_elimination) return false;
    dominator_ = dominator;
    return false;
  }
  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HValue* object() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }
  HValue* transition() const { return OperandAt(2); }

  HObjectAccess access() const { return access_; }
  HValue* dominator() const { return dominator_; }
  bool has_transition() const { return HasTransitionField::decode(bit_field_); }
  StoreFieldOrKeyedMode store_mode() const {
    return StoreModeField::decode(bit_field_);
  }

  Handle<Map> transition_map() const {
    if (has_transition()) {
      return Handle<Map>::cast(
          HConstant::cast(transition())->handle(isolate()));
    } else {
      return Handle<Map>();
    }
  }

  void SetTransition(HConstant* transition) {
    DCHECK(!has_transition());  // Only set once.
    SetOperandAt(2, transition);
    bit_field_ = HasTransitionField::update(bit_field_, true);
    SetChangesFlag(kMaps);
  }

  bool NeedsWriteBarrier() const {
    DCHECK(!field_representation().IsDouble() ||
           (FLAG_unbox_double_fields && access_.IsInobject()) ||
           !has_transition());
    if (field_representation().IsDouble()) return false;
    if (field_representation().IsSmi()) return false;
    if (field_representation().IsInteger32()) return false;
    if (field_representation().IsExternal()) return false;
    return StoringValueNeedsWriteBarrier(value()) &&
        ReceiverObjectNeedsWriteBarrier(object(), value(), dominator());
  }

  bool NeedsWriteBarrierForMap() {
    return ReceiverObjectNeedsWriteBarrier(object(), transition(),
                                           dominator());
  }

  SmiCheck SmiCheckForWriteBarrier() const {
    if (field_representation().IsHeapObject()) return OMIT_SMI_CHECK;
    if (value()->type().IsHeapObject()) return OMIT_SMI_CHECK;
    return INLINE_SMI_CHECK;
  }

  PointersToHereCheck PointersToHereCheckForValue() const {
    return PointersToHereCheckForObject(value(), dominator());
  }

  Representation field_representation() const {
    return access_.representation();
  }

  void UpdateValue(HValue* value) {
    SetOperandAt(1, value);
  }

  bool CanBeReplacedWith(HStoreNamedField* that) const {
    if (!this->access().Equals(that->access())) return false;
    if (SmiValuesAre32Bits() &&
        this->field_representation().IsSmi() &&
        this->store_mode() == INITIALIZING_STORE &&
        that->store_mode() == STORE_TO_INITIALIZED_ENTRY) {
      // We cannot replace an initializing store to a smi field with a store to
      // an initialized entry on 64-bit architectures (with 32-bit smis).
      return false;
    }
    return true;
  }

 private:
  HStoreNamedField(HValue* obj, HObjectAccess access, HValue* val,
                   StoreFieldOrKeyedMode store_mode = INITIALIZING_STORE)
      : access_(access),
        dominator_(NULL),
        bit_field_(HasTransitionField::encode(false) |
                   StoreModeField::encode(store_mode)) {
    // Stores to a non existing in-object property are allowed only to the
    // newly allocated objects (via HAllocate or HInnerAllocatedObject).
    DCHECK(!access.IsInobject() || access.existing_inobject_property() ||
           obj->IsAllocate() || obj->IsInnerAllocatedObject());
    SetOperandAt(0, obj);
    SetOperandAt(1, val);
    SetOperandAt(2, obj);
    access.SetGVNFlags(this, STORE);
  }

  class HasTransitionField : public BitField<bool, 0, 1> {};
  class StoreModeField : public BitField<StoreFieldOrKeyedMode, 1, 1> {};

  HObjectAccess access_;
  HValue* dominator_;
  uint32_t bit_field_;
};

class HStoreKeyed final : public HTemplateInstruction<4>,
                          public ArrayInstructionInterface {
 public:
  DECLARE_INSTRUCTION_FACTORY_P5(HStoreKeyed, HValue*, HValue*, HValue*,
                                 HValue*, ElementsKind);
  DECLARE_INSTRUCTION_FACTORY_P6(HStoreKeyed, HValue*, HValue*, HValue*,
                                 HValue*, ElementsKind, StoreFieldOrKeyedMode);
  DECLARE_INSTRUCTION_FACTORY_P7(HStoreKeyed, HValue*, HValue*, HValue*,
                                 HValue*, ElementsKind, StoreFieldOrKeyedMode,
                                 int);

  Representation RequiredInputRepresentation(int index) override {
    // kind_fast:               tagged[int32] = tagged
    // kind_double:             tagged[int32] = double
    // kind_smi   :             tagged[int32] = smi
    // kind_fixed_typed_array:  tagged[int32] = (double | int32)
    // kind_external:           external[int32] = (double | int32)
    if (index == 0) {
      return is_fixed_typed_array() ? Representation::External()
                                    : Representation::Tagged();
    } else if (index == 1) {
      return ArrayInstructionInterface::KeyedAccessIndexRequirement(
          OperandAt(1)->representation());
    } else if (index == 2) {
      return RequiredValueRepresentation(elements_kind(), store_mode());
    }

    DCHECK_EQ(3, index);
    return HasBackingStoreOwner() ? Representation::Tagged()
                                  : Representation::None();
  }

  static Representation RequiredValueRepresentation(
      ElementsKind kind, StoreFieldOrKeyedMode mode) {
    if (IsDoubleOrFloatElementsKind(kind)) {
      return Representation::Double();
    }

    if (kind == FAST_SMI_ELEMENTS && SmiValuesAre32Bits() &&
        mode == STORE_TO_INITIALIZED_ENTRY) {
      return Representation::Integer32();
    }

    if (IsFastSmiElementsKind(kind)) {
      return Representation::Smi();
    }

    if (IsFixedTypedArrayElementsKind(kind)) {
      return Representation::Integer32();
    }
    return Representation::Tagged();
  }

  bool is_fixed_typed_array() const {
    return IsFixedTypedArrayElementsKind(elements_kind());
  }

  Representation observed_input_representation(int index) override {
    if (index != 2) return RequiredInputRepresentation(index);
    if (IsUninitialized()) {
      return Representation::None();
    }
    Representation r =
        RequiredValueRepresentation(elements_kind(), store_mode());
    // For fast object elements kinds, don't assume anything.
    if (r.IsTagged()) return Representation::None();
    return r;
  }

  HValue* elements() const { return OperandAt(0); }
  HValue* key() const { return OperandAt(1); }
  HValue* value() const { return OperandAt(2); }
  HValue* backing_store_owner() const {
    DCHECK(HasBackingStoreOwner());
    return OperandAt(3);
  }
  bool HasBackingStoreOwner() const { return OperandAt(0) != OperandAt(3); }
  bool value_is_smi() const { return IsFastSmiElementsKind(elements_kind()); }
  StoreFieldOrKeyedMode store_mode() const {
    return StoreModeField::decode(bit_field_);
  }
  ElementsKind elements_kind() const override {
    return ElementsKindField::decode(bit_field_);
  }
  uint32_t base_offset() const { return base_offset_; }
  bool TryIncreaseBaseOffset(uint32_t increase_by_value) override;
  HValue* GetKey() override { return key(); }
  void SetKey(HValue* key) override { SetOperandAt(1, key); }
  bool IsDehoisted() const override {
    return IsDehoistedField::decode(bit_field_);
  }
  void SetDehoisted(bool is_dehoisted) override {
    bit_field_ = IsDehoistedField::update(bit_field_, is_dehoisted);
  }
  bool IsUninitialized() { return IsUninitializedField::decode(bit_field_); }
  void SetUninitialized(bool is_uninitialized) {
    bit_field_ = IsUninitializedField::update(bit_field_, is_uninitialized);
  }

  bool IsConstantHoleStore() {
    return value()->IsConstant() && HConstant::cast(value())->IsTheHole();
  }

  bool HandleSideEffectDominator(GVNFlag side_effect,
                                 HValue* dominator) override {
    DCHECK(side_effect == kNewSpacePromotion);
    dominator_ = dominator;
    return false;
  }

  HValue* dominator() const { return dominator_; }

  bool NeedsWriteBarrier() {
    if (value_is_smi()) {
      return false;
    } else {
      return StoringValueNeedsWriteBarrier(value()) &&
          ReceiverObjectNeedsWriteBarrier(elements(), value(), dominator());
    }
  }

  PointersToHereCheck PointersToHereCheckForValue() const {
    return PointersToHereCheckForObject(value(), dominator());
  }

  bool NeedsCanonicalization();

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(StoreKeyed)

 private:
  HStoreKeyed(HValue* obj, HValue* key, HValue* val,
              HValue* backing_store_owner, ElementsKind elements_kind,
              StoreFieldOrKeyedMode store_mode = INITIALIZING_STORE,
              int offset = kDefaultKeyedHeaderOffsetSentinel)
      : base_offset_(offset == kDefaultKeyedHeaderOffsetSentinel
                         ? GetDefaultHeaderSizeForElementsKind(elements_kind)
                         : offset),
        bit_field_(IsDehoistedField::encode(false) |
                   IsUninitializedField::encode(false) |
                   StoreModeField::encode(store_mode) |
                   ElementsKindField::encode(elements_kind)),
        dominator_(NULL) {
    SetOperandAt(0, obj);
    SetOperandAt(1, key);
    SetOperandAt(2, val);
    SetOperandAt(3, backing_store_owner != nullptr ? backing_store_owner : obj);
    DCHECK_EQ(HasBackingStoreOwner(), is_fixed_typed_array());

    if (IsFastObjectElementsKind(elements_kind)) {
      SetFlag(kTrackSideEffectDominators);
      SetDependsOnFlag(kNewSpacePromotion);
    }
    if (IsFastDoubleElementsKind(elements_kind)) {
      SetChangesFlag(kDoubleArrayElements);
    } else if (IsFastSmiElementsKind(elements_kind)) {
      SetChangesFlag(kArrayElements);
    } else if (is_fixed_typed_array()) {
      SetChangesFlag(kTypedArrayElements);
      SetChangesFlag(kExternalMemory);
      SetFlag(kTruncatingToNumber);
    } else {
      SetChangesFlag(kArrayElements);
    }

    // {UNSIGNED_,}{BYTE,SHORT,INT}_ELEMENTS are truncating.
    if (elements_kind >= UINT8_ELEMENTS && elements_kind <= INT32_ELEMENTS) {
      SetFlag(kTruncatingToInt32);
    }
  }

  class IsDehoistedField : public BitField<bool, 0, 1> {};
  class IsUninitializedField : public BitField<bool, 1, 1> {};
  class StoreModeField : public BitField<StoreFieldOrKeyedMode, 2, 1> {};
  class ElementsKindField : public BitField<ElementsKind, 3, 5> {};

  uint32_t base_offset_;
  uint32_t bit_field_;
  HValue* dominator_;
};

class HTransitionElementsKind final : public HTemplateInstruction<2> {
 public:
  inline static HTransitionElementsKind* New(Isolate* isolate, Zone* zone,
                                             HValue* context, HValue* object,
                                             Handle<Map> original_map,
                                             Handle<Map> transitioned_map) {
    return new(zone) HTransitionElementsKind(context, object,
                                             original_map, transitioned_map);
  }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* object() const { return OperandAt(0); }
  HValue* context() const { return OperandAt(1); }
  Unique<Map> original_map() const { return original_map_; }
  Unique<Map> transitioned_map() const { return transitioned_map_; }
  ElementsKind from_kind() const {
    return FromElementsKindField::decode(bit_field_);
  }
  ElementsKind to_kind() const {
    return ToElementsKindField::decode(bit_field_);
  }
  bool map_is_stable() const { return MapIsStableField::decode(bit_field_); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(TransitionElementsKind)

 protected:
  bool DataEquals(HValue* other) override {
    HTransitionElementsKind* instr = HTransitionElementsKind::cast(other);
    return original_map_ == instr->original_map_ &&
           transitioned_map_ == instr->transitioned_map_;
  }

  int RedefinedOperandIndex() override { return 0; }

 private:
  HTransitionElementsKind(HValue* context, HValue* object,
                          Handle<Map> original_map,
                          Handle<Map> transitioned_map)
      : original_map_(Unique<Map>(original_map)),
        transitioned_map_(Unique<Map>(transitioned_map)),
        bit_field_(
            FromElementsKindField::encode(original_map->elements_kind()) |
            ToElementsKindField::encode(transitioned_map->elements_kind()) |
            MapIsStableField::encode(transitioned_map->is_stable())) {
    SetOperandAt(0, object);
    SetOperandAt(1, context);
    SetFlag(kUseGVN);
    SetChangesFlag(kElementsKind);
    if (!IsSimpleMapChangeTransition(from_kind(), to_kind())) {
      SetChangesFlag(kElementsPointer);
      SetChangesFlag(kNewSpacePromotion);
    }
    set_representation(Representation::Tagged());
  }

  class FromElementsKindField : public BitField<ElementsKind, 0, 5> {};
  class ToElementsKindField : public BitField<ElementsKind, 5, 5> {};
  class MapIsStableField : public BitField<bool, 10, 1> {};

  Unique<Map> original_map_;
  Unique<Map> transitioned_map_;
  uint32_t bit_field_;
};


class HStringAdd final : public HBinaryOperation {
 public:
  static HInstruction* New(
      Isolate* isolate, Zone* zone, HValue* context, HValue* left,
      HValue* right, PretenureFlag pretenure_flag = NOT_TENURED,
      StringAddFlags flags = STRING_ADD_CHECK_BOTH,
      Handle<AllocationSite> allocation_site = Handle<AllocationSite>::null());

  StringAddFlags flags() const { return flags_; }
  PretenureFlag pretenure_flag() const { return pretenure_flag_; }

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  DECLARE_CONCRETE_INSTRUCTION(StringAdd)

 protected:
  bool DataEquals(HValue* other) override {
    return flags_ == HStringAdd::cast(other)->flags_ &&
        pretenure_flag_ == HStringAdd::cast(other)->pretenure_flag_;
  }

 private:
  HStringAdd(HValue* context, HValue* left, HValue* right,
             PretenureFlag pretenure_flag, StringAddFlags flags,
             Handle<AllocationSite> allocation_site)
      : HBinaryOperation(context, left, right, HType::String()),
        flags_(flags),
        pretenure_flag_(pretenure_flag) {
    set_representation(Representation::Tagged());
    if ((flags & STRING_ADD_CONVERT) == STRING_ADD_CONVERT) {
      SetAllSideEffects();
      ClearFlag(kUseGVN);
    } else {
      SetChangesFlag(kNewSpacePromotion);
      SetFlag(kUseGVN);
    }
    SetDependsOnFlag(kMaps);
    if (FLAG_trace_pretenuring) {
      PrintF("HStringAdd with AllocationSite %p %s\n",
             allocation_site.is_null()
                 ? static_cast<void*>(NULL)
                 : static_cast<void*>(*allocation_site),
             pretenure_flag == TENURED ? "tenured" : "not tenured");
    }
  }

  bool IsDeletable() const final {
    return (flags_ & STRING_ADD_CONVERT) != STRING_ADD_CONVERT;
  }

  const StringAddFlags flags_;
  const PretenureFlag pretenure_flag_;
};


class HStringCharCodeAt final : public HTemplateInstruction<3> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P2(HStringCharCodeAt,
                                              HValue*,
                                              HValue*);

  Representation RequiredInputRepresentation(int index) override {
    // The index is supposed to be Integer32.
    return index == 2
        ? Representation::Integer32()
        : Representation::Tagged();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* string() const { return OperandAt(1); }
  HValue* index() const { return OperandAt(2); }

  DECLARE_CONCRETE_INSTRUCTION(StringCharCodeAt)

 protected:
  bool DataEquals(HValue* other) override { return true; }

  Range* InferRange(Zone* zone) override {
    return new(zone) Range(0, String::kMaxUtf16CodeUnit);
  }

 private:
  HStringCharCodeAt(HValue* context, HValue* string, HValue* index) {
    SetOperandAt(0, context);
    SetOperandAt(1, string);
    SetOperandAt(2, index);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kStringChars);
    SetChangesFlag(kNewSpacePromotion);
  }

  // No side effects: runtime function assumes string + number inputs.
  bool IsDeletable() const override { return true; }
};


class HStringCharFromCode final : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           HValue* char_code);

  Representation RequiredInputRepresentation(int index) override {
    return index == 0
        ? Representation::Tagged()
        : Representation::Integer32();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  bool DataEquals(HValue* other) override { return true; }

  DECLARE_CONCRETE_INSTRUCTION(StringCharFromCode)

 private:
  HStringCharFromCode(HValue* context, HValue* char_code)
      : HTemplateInstruction<2>(HType::String()) {
    SetOperandAt(0, context);
    SetOperandAt(1, char_code);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetChangesFlag(kNewSpacePromotion);
  }

  bool IsDeletable() const override {
    return !value()->ToNumberCanBeObserved();
  }
};


class HTypeof final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HTypeof, HValue*);

  HValue* context() const { return OperandAt(0); }
  HValue* value() const { return OperandAt(1); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  DECLARE_CONCRETE_INSTRUCTION(Typeof)

 private:
  explicit HTypeof(HValue* context, HValue* value) {
    SetOperandAt(0, context);
    SetOperandAt(1, value);
    set_representation(Representation::Tagged());
  }

  bool IsDeletable() const override { return true; }
};


class HTrapAllocationMemento final : public HTemplateInstruction<1> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P1(HTrapAllocationMemento, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* object() { return OperandAt(0); }

  DECLARE_CONCRETE_INSTRUCTION(TrapAllocationMemento)

 private:
  explicit HTrapAllocationMemento(HValue* obj) {
    SetOperandAt(0, obj);
  }
};


class HMaybeGrowElements final : public HTemplateInstruction<5> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P6(HMaybeGrowElements, HValue*,
                                              HValue*, HValue*, HValue*, bool,
                                              ElementsKind);

  Representation RequiredInputRepresentation(int index) override {
    if (index < 3) {
      return Representation::Tagged();
    }
    DCHECK(index == 3 || index == 4);
    return Representation::Integer32();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* object() const { return OperandAt(1); }
  HValue* elements() const { return OperandAt(2); }
  HValue* key() const { return OperandAt(3); }
  HValue* current_capacity() const { return OperandAt(4); }

  bool is_js_array() const { return is_js_array_; }
  ElementsKind kind() const { return kind_; }

  DECLARE_CONCRETE_INSTRUCTION(MaybeGrowElements)

 protected:
  bool DataEquals(HValue* other) override { return true; }

 private:
  explicit HMaybeGrowElements(HValue* context, HValue* object, HValue* elements,
                              HValue* key, HValue* current_capacity,
                              bool is_js_array, ElementsKind kind) {
    is_js_array_ = is_js_array;
    kind_ = kind;

    SetOperandAt(0, context);
    SetOperandAt(1, object);
    SetOperandAt(2, elements);
    SetOperandAt(3, key);
    SetOperandAt(4, current_capacity);

    SetFlag(kUseGVN);
    SetChangesFlag(kElementsPointer);
    SetChangesFlag(kNewSpacePromotion);
    set_representation(Representation::Tagged());
  }

  bool is_js_array_;
  ElementsKind kind_;
};


class HSeqStringGetChar final : public HTemplateInstruction<2> {
 public:
  static HInstruction* New(Isolate* isolate, Zone* zone, HValue* context,
                           String::Encoding encoding, HValue* string,
                           HValue* index);

  Representation RequiredInputRepresentation(int index) override {
    return (index == 0) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  String::Encoding encoding() const { return encoding_; }
  HValue* string() const { return OperandAt(0); }
  HValue* index() const { return OperandAt(1); }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringGetChar)

 protected:
  bool DataEquals(HValue* other) override {
    return encoding() == HSeqStringGetChar::cast(other)->encoding();
  }

  Range* InferRange(Zone* zone) override {
    if (encoding() == String::ONE_BYTE_ENCODING) {
      return new(zone) Range(0, String::kMaxOneByteCharCode);
    } else {
      DCHECK_EQ(String::TWO_BYTE_ENCODING, encoding());
      return  new(zone) Range(0, String::kMaxUtf16CodeUnit);
    }
  }

 private:
  HSeqStringGetChar(String::Encoding encoding,
                    HValue* string,
                    HValue* index) : encoding_(encoding) {
    SetOperandAt(0, string);
    SetOperandAt(1, index);
    set_representation(Representation::Integer32());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kStringChars);
  }

  bool IsDeletable() const override { return true; }

  String::Encoding encoding_;
};


class HSeqStringSetChar final : public HTemplateInstruction<4> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P4(
      HSeqStringSetChar, String::Encoding,
      HValue*, HValue*, HValue*);

  String::Encoding encoding() { return encoding_; }
  HValue* context() { return OperandAt(0); }
  HValue* string() { return OperandAt(1); }
  HValue* index() { return OperandAt(2); }
  HValue* value() { return OperandAt(3); }

  Representation RequiredInputRepresentation(int index) override {
    return (index <= 1) ? Representation::Tagged()
                        : Representation::Integer32();
  }

  DECLARE_CONCRETE_INSTRUCTION(SeqStringSetChar)

 private:
  HSeqStringSetChar(HValue* context,
                    String::Encoding encoding,
                    HValue* string,
                    HValue* index,
                    HValue* value) : encoding_(encoding) {
    SetOperandAt(0, context);
    SetOperandAt(1, string);
    SetOperandAt(2, index);
    SetOperandAt(3, value);
    set_representation(Representation::Tagged());
    SetChangesFlag(kStringChars);
  }

  String::Encoding encoding_;
};


class HCheckMapValue final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HCheckMapValue, HValue*, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HType CalculateInferredType() override {
    if (value()->type().IsHeapObject()) return value()->type();
    return HType::HeapObject();
  }

  HValue* value() const { return OperandAt(0); }
  HValue* map() const { return OperandAt(1); }

  HValue* Canonicalize() override;

  DECLARE_CONCRETE_INSTRUCTION(CheckMapValue)

 protected:
  int RedefinedOperandIndex() override { return 0; }

  bool DataEquals(HValue* other) override { return true; }

 private:
  HCheckMapValue(HValue* value, HValue* map)
      : HTemplateInstruction<2>(HType::HeapObject()) {
    SetOperandAt(0, value);
    SetOperandAt(1, map);
    set_representation(Representation::Tagged());
    SetFlag(kUseGVN);
    SetDependsOnFlag(kMaps);
    SetDependsOnFlag(kElementsKind);
  }
};


class HForInPrepareMap final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_WITH_CONTEXT_FACTORY_P1(HForInPrepareMap, HValue*);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* context() const { return OperandAt(0); }
  HValue* enumerable() const { return OperandAt(1); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HType CalculateInferredType() override { return HType::Tagged(); }

  DECLARE_CONCRETE_INSTRUCTION(ForInPrepareMap);

 private:
  HForInPrepareMap(HValue* context,
                   HValue* object) {
    SetOperandAt(0, context);
    SetOperandAt(1, object);
    set_representation(Representation::Tagged());
    SetAllSideEffects();
  }
};


class HForInCacheArray final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P3(HForInCacheArray, HValue*, HValue*, int);

  Representation RequiredInputRepresentation(int index) override {
    return Representation::Tagged();
  }

  HValue* enumerable() const { return OperandAt(0); }
  HValue* map() const { return OperandAt(1); }
  int idx() const { return idx_; }

  HForInCacheArray* index_cache() {
    return index_cache_;
  }

  void set_index_cache(HForInCacheArray* index_cache) {
    index_cache_ = index_cache;
  }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HType CalculateInferredType() override { return HType::Tagged(); }

  DECLARE_CONCRETE_INSTRUCTION(ForInCacheArray);

 private:
  HForInCacheArray(HValue* enumerable,
                   HValue* keys,
                   int idx) : idx_(idx) {
    SetOperandAt(0, enumerable);
    SetOperandAt(1, keys);
    set_representation(Representation::Tagged());
  }

  int idx_;
  HForInCacheArray* index_cache_;
};


class HLoadFieldByIndex final : public HTemplateInstruction<2> {
 public:
  DECLARE_INSTRUCTION_FACTORY_P2(HLoadFieldByIndex, HValue*, HValue*);

  HLoadFieldByIndex(HValue* object,
                    HValue* index) {
    SetOperandAt(0, object);
    SetOperandAt(1, index);
    SetChangesFlag(kNewSpacePromotion);
    set_representation(Representation::Tagged());
  }

  Representation RequiredInputRepresentation(int index) override {
    if (index == 1) {
      return Representation::Smi();
    } else {
      return Representation::Tagged();
    }
  }

  HValue* object() const { return OperandAt(0); }
  HValue* index() const { return OperandAt(1); }

  std::ostream& PrintDataTo(std::ostream& os) const override;  // NOLINT

  HType CalculateInferredType() override { return HType::Tagged(); }

  DECLARE_CONCRETE_INSTRUCTION(LoadFieldByIndex);

 private:
  bool IsDeletable() const override { return true; }
};

#undef DECLARE_INSTRUCTION
#undef DECLARE_CONCRETE_INSTRUCTION

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_INSTRUCTIONS_H_
