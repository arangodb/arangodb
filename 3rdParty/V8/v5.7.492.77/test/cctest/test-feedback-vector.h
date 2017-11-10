// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TEST_FEEDBACK_VECTOR_H_
#define V8_TEST_FEEDBACK_VECTOR_H_

#include "src/objects.h"


namespace v8 {
namespace internal {

// Helper class that allows to write tests in a slot size independent manner.
// Use helper.slot(X) to get X'th slot identifier.
class FeedbackVectorHelper {
 public:
  explicit FeedbackVectorHelper(Handle<TypeFeedbackVector> vector)
      : vector_(vector) {
    int slot_count = vector->slot_count();
    slots_.reserve(slot_count);
    TypeFeedbackMetadataIterator iter(vector->metadata());
    while (iter.HasNext()) {
      FeedbackVectorSlot slot = iter.Next();
      slots_.push_back(slot);
    }
  }

  Handle<TypeFeedbackVector> vector() { return vector_; }

  // Returns slot identifier by numerical index.
  FeedbackVectorSlot slot(int index) const { return slots_[index]; }

  // Returns the number of slots in the feedback vector.
  int slot_count() const { return static_cast<int>(slots_.size()); }

 private:
  Handle<TypeFeedbackVector> vector_;
  std::vector<FeedbackVectorSlot> slots_;
};

template <typename Spec>
Handle<TypeFeedbackVector> NewTypeFeedbackVector(Isolate* isolate, Spec* spec) {
  Handle<TypeFeedbackMetadata> metadata =
      TypeFeedbackMetadata::New(isolate, spec);
  return TypeFeedbackVector::New(isolate, metadata);
}


}  // namespace internal
}  // namespace v8

#endif
