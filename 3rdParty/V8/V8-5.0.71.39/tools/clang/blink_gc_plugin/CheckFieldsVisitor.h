// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_BLINK_GC_PLUGIN_CHECK_FIELDS_VISITOR_H_
#define TOOLS_BLINK_GC_PLUGIN_CHECK_FIELDS_VISITOR_H_

#include <vector>

#include "Edge.h"

struct BlinkGCPluginOptions;
class FieldPoint;

// This visitor checks that the fields of a class are "well formed".
// - OwnPtr, RefPtr and RawPtr must not point to a GC derived types.
// - Part objects must not be GC derived types.
// - An on-heap class must never contain GC roots.
// - Only stack-allocated types may point to stack-allocated types.

class CheckFieldsVisitor : public RecursiveEdgeVisitor {
 public:
  enum Error {
    kRawPtrToGCManaged,
    kRawPtrToGCManagedWarning,
    kRefPtrToGCManaged,
    kReferencePtrToGCManaged,
    kReferencePtrToGCManagedWarning,
    kOwnPtrToGCManaged,
    kMemberToGCUnmanaged,
    kMemberInUnmanaged,
    kPtrFromHeapToStack,
    kGCDerivedPartObject
  };

  typedef std::vector<std::pair<FieldPoint*, Error> > Errors;

  explicit CheckFieldsVisitor(const BlinkGCPluginOptions& options);

  Errors& invalid_fields();

  bool ContainsInvalidFields(RecordInfo* info);

  void AtMember(Member* edge) override;
  void AtValue(Value* edge) override;
  void AtCollection(Collection* edge) override;

  static bool IsWarning(Error error);
  static bool IsRawPtrError(Error error);
  static bool IsReferencePtrError(Error error);

 private:
  Error InvalidSmartPtr(Edge* ptr);

  const BlinkGCPluginOptions& options_;
  FieldPoint* current_;
  bool stack_allocated_host_;
  bool managed_host_;
  Errors invalid_fields_;
};

#endif  // TOOLS_BLINK_GC_PLUGIN_CHECK_FIELDS_VISITOR_H_
