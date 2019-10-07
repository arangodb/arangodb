// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ALLOCATION_SITE_H_
#define V8_OBJECTS_ALLOCATION_SITE_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class AllocationSite : public Struct, public NeverReadOnlySpaceObject {
 public:
  static const uint32_t kMaximumArrayBytesToPretransition = 8 * 1024;
  static const double kPretenureRatio;
  static const int kPretenureMinimumCreated = 100;

  // Values for pretenure decision field.
  enum PretenureDecision {
    kUndecided = 0,
    kDontTenure = 1,
    kMaybeTenure = 2,
    kTenure = 3,
    kZombie = 4,
    kLastPretenureDecisionValue = kZombie
  };

  const char* PretenureDecisionName(PretenureDecision decision);

  // Contains either a Smi-encoded bitfield or a boilerplate. If it's a Smi the
  // AllocationSite is for a constructed Array.
  DECL_ACCESSORS(transition_info_or_boilerplate, Object)
  DECL_ACCESSORS(boilerplate, JSObject)
  DECL_INT_ACCESSORS(transition_info)

  // nested_site threads a list of sites that represent nested literals
  // walked in a particular order. So [[1, 2], 1, 2] will have one
  // nested_site, but [[1, 2], 3, [4]] will have a list of two.
  DECL_ACCESSORS(nested_site, Object)

  // Bitfield containing pretenuring information.
  DECL_INT32_ACCESSORS(pretenure_data)

  DECL_INT32_ACCESSORS(pretenure_create_count)
  DECL_ACCESSORS(dependent_code, DependentCode)

  // heap->allocation_site_list() points to the last AllocationSite which form
  // a linked list through the weak_next property. The GC might remove elements
  // from the list by updateing weak_next.
  DECL_ACCESSORS(weak_next, Object)

  inline void Initialize();

  // Checks if the allocation site contain weak_next field;
  inline bool HasWeakNext() const;

  // This method is expensive, it should only be called for reporting.
  bool IsNested();

  // transition_info bitfields, for constructed array transition info.
  class ElementsKindBits : public BitField<ElementsKind, 0, 5> {};
  class DoNotInlineBit : public BitField<bool, 5, 1> {};
  // Unused bits 6-30.

  // Bitfields for pretenure_data
  class MementoFoundCountBits : public BitField<int, 0, 26> {};
  class PretenureDecisionBits : public BitField<PretenureDecision, 26, 3> {};
  class DeoptDependentCodeBit : public BitField<bool, 29, 1> {};
  STATIC_ASSERT(PretenureDecisionBits::kMax >= kLastPretenureDecisionValue);

  // Increments the mementos found counter and returns true when the first
  // memento was found for a given allocation site.
  inline bool IncrementMementoFoundCount(int increment = 1);

  inline void IncrementMementoCreateCount();

  PretenureFlag GetPretenureMode() const;

  void ResetPretenureDecision();

  inline PretenureDecision pretenure_decision() const;
  inline void set_pretenure_decision(PretenureDecision decision);

  inline bool deopt_dependent_code() const;
  inline void set_deopt_dependent_code(bool deopt);

  inline int memento_found_count() const;
  inline void set_memento_found_count(int count);

  inline int memento_create_count() const;
  inline void set_memento_create_count(int count);

  // The pretenuring decision is made during gc, and the zombie state allows
  // us to recognize when an allocation site is just being kept alive because
  // a later traversal of new space may discover AllocationMementos that point
  // to this AllocationSite.
  inline bool IsZombie() const;

  inline bool IsMaybeTenure() const;

  inline void MarkZombie();

  inline bool MakePretenureDecision(PretenureDecision current_decision,
                                    double ratio, bool maximum_size_scavenge);

  inline bool DigestPretenuringFeedback(bool maximum_size_scavenge);

  inline ElementsKind GetElementsKind() const;
  inline void SetElementsKind(ElementsKind kind);

  inline bool CanInlineCall() const;
  inline void SetDoNotInlineCall();

  inline bool PointsToLiteral() const;

  template <AllocationSiteUpdateMode update_or_check =
                AllocationSiteUpdateMode::kUpdate>
  static bool DigestTransitionFeedback(Handle<AllocationSite> site,
                                       ElementsKind to_kind);

  DECL_PRINTER(AllocationSite)
  DECL_VERIFIER(AllocationSite)

  DECL_CAST(AllocationSite)
  static inline bool ShouldTrack(ElementsKind boilerplate_elements_kind);
  static bool ShouldTrack(ElementsKind from, ElementsKind to);
  static inline bool CanTrack(InstanceType type);

// Layout description.
// AllocationSite has to start with TransitionInfoOrboilerPlateOffset
// and end with WeakNext field.
#define ALLOCATION_SITE_FIELDS(V)                     \
  V(kTransitionInfoOrBoilerplateOffset, kPointerSize) \
  V(kNestedSiteOffset, kPointerSize)                  \
  V(kDependentCodeOffset, kPointerSize)               \
  V(kCommonPointerFieldEndOffset, 0)                  \
  V(kPretenureDataOffset, kInt32Size)                 \
  V(kPretenureCreateCountOffset, kInt32Size)          \
  /* Size of AllocationSite without WeakNext field */ \
  V(kSizeWithoutWeakNext, 0)                          \
  V(kWeakNextOffset, kPointerSize)                    \
  /* Size of AllocationSite with WeakNext field */    \
  V(kSizeWithWeakNext, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, ALLOCATION_SITE_FIELDS)

  static const int kStartOffset = HeapObject::kHeaderSize;

  class BodyDescriptor;

 private:
  inline bool PretenuringDecisionMade() const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationSite);
};

class AllocationMemento : public Struct {
 public:
  static const int kAllocationSiteOffset = HeapObject::kHeaderSize;
  static const int kSize = kAllocationSiteOffset + kPointerSize;

  DECL_ACCESSORS(allocation_site, Object)

  inline bool IsValid() const;
  inline AllocationSite* GetAllocationSite() const;
  inline Address GetAllocationSiteUnchecked() const;

  DECL_PRINTER(AllocationMemento)
  DECL_VERIFIER(AllocationMemento)

  DECL_CAST(AllocationMemento)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AllocationMemento);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_ALLOCATION_SITE_H_
