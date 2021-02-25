// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_

#include "src/snapshot/deserializer.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

// Deserializes the read-only blob, creating the read-only roots and the
// Read-only object cache used by the other deserializers.
class ReadOnlyDeserializer final : public Deserializer {
 public:
  explicit ReadOnlyDeserializer(const SnapshotData* data)
      : Deserializer(data, false) {}

  // Deserialize the snapshot into an empty heap.
  void DeserializeInto(Isolate* isolate);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_DESERIALIZER_H_
