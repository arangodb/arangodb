// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_
#define V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_

#include <unordered_set>

#include "src/snapshot/roots-serializer.h"

namespace v8 {
namespace internal {

class HeapObject;
class SnapshotByteSink;

class V8_EXPORT_PRIVATE ReadOnlySerializer : public RootsSerializer {
 public:
  explicit ReadOnlySerializer(Isolate* isolate);
  ~ReadOnlySerializer() override;

  void SerializeReadOnlyRoots();

  // Completes the serialization of the read-only object cache and serializes
  // any deferred objects.
  void FinalizeSerialization();

  // If |obj| can be serialized in the read-only snapshot then add it to the
  // read-only object cache if not already present and emit a
  // ReadOnlyObjectCache bytecode into |sink|. Returns whether this was
  // successful.
  bool SerializeUsingReadOnlyObjectCache(SnapshotByteSink* sink,
                                         HeapObject obj);

 private:
  void SerializeObject(HeapObject o) override;
  bool MustBeDeferred(HeapObject object) override;

#ifdef DEBUG
  std::unordered_set<HeapObject, Object::Hasher> serialized_objects_;
#endif
  DISALLOW_COPY_AND_ASSIGN(ReadOnlySerializer);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_READ_ONLY_SERIALIZER_H_
