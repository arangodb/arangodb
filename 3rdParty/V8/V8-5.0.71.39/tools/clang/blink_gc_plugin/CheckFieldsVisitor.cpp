// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "CheckFieldsVisitor.h"

#include <cassert>

#include "BlinkGCPluginOptions.h"
#include "RecordInfo.h"

CheckFieldsVisitor::CheckFieldsVisitor(const BlinkGCPluginOptions& options)
    : options_(options),
      current_(0),
      stack_allocated_host_(false) {
}

CheckFieldsVisitor::Errors& CheckFieldsVisitor::invalid_fields() {
  return invalid_fields_;
}

bool CheckFieldsVisitor::ContainsInvalidFields(RecordInfo* info) {
  stack_allocated_host_ = info->IsStackAllocated();
  managed_host_ = stack_allocated_host_ ||
                  info->IsGCAllocated() ||
                  info->IsNonNewable() ||
                  info->IsOnlyPlacementNewable();
  for (RecordInfo::Fields::iterator it = info->GetFields().begin();
       it != info->GetFields().end();
       ++it) {
    context().clear();
    current_ = &it->second;
    current_->edge()->Accept(this);
  }
  return !invalid_fields_.empty();
}

void CheckFieldsVisitor::AtMember(Member* edge) {
  if (managed_host_)
    return;
  // A member is allowed to appear in the context of a root.
  for (Context::iterator it = context().begin();
       it != context().end();
       ++it) {
    if ((*it)->Kind() == Edge::kRoot)
      return;
  }
  invalid_fields_.push_back(std::make_pair(current_, kMemberInUnmanaged));
}

void CheckFieldsVisitor::AtValue(Value* edge) {
  // TODO: what should we do to check unions?
  if (edge->value()->record()->isUnion())
    return;

  if (!stack_allocated_host_ && edge->value()->IsStackAllocated()) {
    invalid_fields_.push_back(std::make_pair(current_, kPtrFromHeapToStack));
    return;
  }

  if (!Parent() &&
      edge->value()->IsGCDerived() &&
      !edge->value()->IsGCMixin()) {
    invalid_fields_.push_back(std::make_pair(current_, kGCDerivedPartObject));
    return;
  }

  // If in a stack allocated context, be fairly insistent that T in Member<T>
  // is GC allocated, as stack allocated objects do not have a trace()
  // that separately verifies the validity of Member<T>.
  //
  // Notice that an error is only reported if T's definition is in scope;
  // we do not require that it must be brought into scope as that would
  // prevent declarations of mutually dependent class types.
  //
  // (Note: Member<>'s constructor will at run-time verify that the
  // pointer it wraps is indeed heap allocated.)
  if (stack_allocated_host_ && Parent() && Parent()->IsMember() &&
      edge->value()->HasDefinition() && !edge->value()->IsGCAllocated()) {
    invalid_fields_.push_back(std::make_pair(current_,
                                             kMemberToGCUnmanaged));
    return;
  }

  if (!Parent() || !edge->value()->IsGCAllocated())
    return;

  // In transition mode, disallow  OwnPtr<T>, RawPtr<T> to GC allocated T's,
  // also disallow T* in stack-allocated types.
  if (options_.enable_oilpan) {
    if (Parent()->IsOwnPtr() ||
        Parent()->IsRawPtrClass() ||
        (stack_allocated_host_ && Parent()->IsRawPtr())) {
      invalid_fields_.push_back(std::make_pair(
          current_, InvalidSmartPtr(Parent())));
      return;
    }
    if (options_.warn_raw_ptr && Parent()->IsRawPtr()) {
      if (static_cast<RawPtr*>(Parent())->HasReferenceType()) {
        invalid_fields_.push_back(std::make_pair(
            current_, kReferencePtrToGCManagedWarning));
      } else {
        invalid_fields_.push_back(std::make_pair(
            current_, kRawPtrToGCManagedWarning));
      }
    }
    return;
  }

  if (Parent()->IsRawPtr() || Parent()->IsOwnPtr()) {
    invalid_fields_.push_back(std::make_pair(
        current_, InvalidSmartPtr(Parent())));
    return;
  }

  if (Parent()->IsRefPtr() && !edge->value()->IsGCRefCounted()) {
    invalid_fields_.push_back(std::make_pair(
        current_, InvalidSmartPtr(Parent())));
    return;
  }
}

void CheckFieldsVisitor::AtCollection(Collection* edge) {
  if (edge->on_heap() && Parent() && Parent()->IsOwnPtr())
    invalid_fields_.push_back(std::make_pair(current_, kOwnPtrToGCManaged));
}

bool CheckFieldsVisitor::IsWarning(Error error) {
  if (error == kRawPtrToGCManagedWarning)
    return true;
  if (error == kReferencePtrToGCManagedWarning)
    return true;
  return false;
}

bool CheckFieldsVisitor::IsRawPtrError(Error error) {
  return (error == kRawPtrToGCManaged ||
          error == kRawPtrToGCManagedWarning);
}

bool CheckFieldsVisitor::IsReferencePtrError(Error error) {
  return (error == kReferencePtrToGCManaged ||
          error == kReferencePtrToGCManagedWarning);
}

CheckFieldsVisitor::Error CheckFieldsVisitor::InvalidSmartPtr(Edge* ptr) {
  if (ptr->IsRawPtr()) {
    if (static_cast<RawPtr*>(ptr)->HasReferenceType())
      return kReferencePtrToGCManaged;
    else
      return kRawPtrToGCManaged;
  }
  if (ptr->IsRefPtr())
    return kRefPtrToGCManaged;
  if (ptr->IsOwnPtr())
    return kOwnPtrToGCManaged;
  assert(false && "Unknown smart pointer kind");
}
