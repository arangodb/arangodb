// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SERIALIZER_COMMON_H_
#define V8_SNAPSHOT_SERIALIZER_COMMON_H_

#include "src/address-map.h"
#include "src/external-reference-table.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Isolate;

class ExternalReferenceEncoder {
 public:
  explicit ExternalReferenceEncoder(Isolate* isolate);

  uint32_t Encode(Address key) const;

  const char* NameOfAddress(Isolate* isolate, Address address) const;

 private:
  AddressToIndexHashMap* map_;
#ifdef DEBUG
  ExternalReferenceTable* table_;
#endif  // DEBUG

  DISALLOW_COPY_AND_ASSIGN(ExternalReferenceEncoder);
};

class HotObjectsList {
 public:
  HotObjectsList() : index_(0) {
    for (int i = 0; i < kSize; i++) circular_queue_[i] = NULL;
  }

  void Add(HeapObject* object) {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    circular_queue_[index_] = object;
    index_ = (index_ + 1) & kSizeMask;
  }

  HeapObject* Get(int index) {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    DCHECK_NOT_NULL(circular_queue_[index]);
    return circular_queue_[index];
  }

  static const int kNotFound = -1;

  int Find(HeapObject* object) {
    DCHECK(!AllowHeapAllocation::IsAllowed());
    for (int i = 0; i < kSize; i++) {
      if (circular_queue_[i] == object) return i;
    }
    return kNotFound;
  }

  static const int kSize = 8;

 private:
  STATIC_ASSERT(IS_POWER_OF_TWO(kSize));
  static const int kSizeMask = kSize - 1;
  HeapObject* circular_queue_[kSize];
  int index_;

  DISALLOW_COPY_AND_ASSIGN(HotObjectsList);
};

// The Serializer/Deserializer class is a common superclass for Serializer and
// Deserializer which is used to store common constants and methods used by
// both.
class SerializerDeserializer : public ObjectVisitor {
 public:
  static void Iterate(Isolate* isolate, ObjectVisitor* visitor);

  // No reservation for large object space necessary.
  // We also handle map space differenly.
  STATIC_ASSERT(MAP_SPACE == CODE_SPACE + 1);
  static const int kNumberOfPreallocatedSpaces = CODE_SPACE + 1;
  static const int kNumberOfSpaces = LAST_SPACE + 1;

 protected:
  static bool CanBeDeferred(HeapObject* o);

  void RestoreExternalReferenceRedirectors(List<AccessorInfo*>* accessor_infos);

  // ---------- byte code range 0x00..0x7f ----------
  // Byte codes in this range represent Where, HowToCode and WhereToPoint.
  // Where the pointed-to object can be found:
  // The static assert below will trigger when the number of preallocated spaces
  // changed. If that happens, update the bytecode ranges in the comments below.
  STATIC_ASSERT(5 == kNumberOfSpaces);
  enum Where {
    // 0x00..0x04  Allocate new object, in specified space.
    kNewObject = 0x00,
    // 0x08..0x0c  Reference to previous object from space.
    kBackref = 0x08,
    // 0x10..0x14  Reference to previous object from space after skip.
    kBackrefWithSkip = 0x10,

    // 0x05       Root array item.
    kRootArray = 0x05,
    // 0x06        Object in the partial snapshot cache.
    kPartialSnapshotCache = 0x06,
    // 0x07        External reference referenced by id.
    kExternalReference = 0x07,

    // 0x0d        Object provided in the attached list.
    kAttachedReference = 0x0d,
    // 0x0e        Builtin code referenced by index.
    kBuiltin = 0x0e,

    // 0x0f        Misc, see below (incl. 0x2f, 0x4f, 0x6f).
    // 0x15..0x1f  Misc, see below (incl. 0x35..0x3f, 0x55..0x5f, 0x75..0x7f).
  };

  static const int kWhereMask = 0x1f;
  static const int kSpaceMask = 7;
  STATIC_ASSERT(kNumberOfSpaces <= kSpaceMask + 1);

  // How to code the pointer to the object.
  enum HowToCode {
    // Straight pointer.
    kPlain = 0,
    // A pointer inlined in code. What this means depends on the architecture.
    kFromCode = 0x20
  };

  static const int kHowToCodeMask = 0x20;

  // Where to point within the object.
  enum WhereToPoint {
    // Points to start of object
    kStartOfObject = 0,
    // Points to instruction in code object or payload of cell.
    kInnerPointer = 0x40
  };

  static const int kWhereToPointMask = 0x40;

  // ---------- Misc ----------
  // Skip.
  static const int kSkip = 0x0f;
  // Do nothing, used for padding.
  static const int kNop = 0x2f;
  // Move to next reserved chunk.
  static const int kNextChunk = 0x4f;
  // Deferring object content.
  static const int kDeferred = 0x6f;
  // Alignment prefixes 0x15..0x17
  static const int kAlignmentPrefix = 0x15;
  // A tag emitted at strategic points in the snapshot to delineate sections.
  // If the deserializer does not find these at the expected moments then it
  // is an indication that the snapshot and the VM do not fit together.
  // Examine the build process for architecture, version or configuration
  // mismatches.
  static const int kSynchronize = 0x18;
  // Repeats of variable length.
  static const int kVariableRepeat = 0x19;
  // Raw data of variable length.
  static const int kVariableRawData = 0x1a;
  // Internal reference encoded as offsets of pc and target from code entry.
  static const int kInternalReference = 0x1b;
  static const int kInternalReferenceEncoded = 0x1c;
  // Used for the source code of the natives, which is in the executable, but
  // is referred to from external strings in the snapshot.
  static const int kNativesStringResource = 0x1d;
  // Used for the source code for compiled stubs, which is in the executable,
  // but is referred to from external strings in the snapshot.
  static const int kExtraNativesStringResource = 0x1e;
  // Used for embedder-provided serialization data for internal fields.
  static const int kInternalFieldsData = 0x1f;

  // 8 hot (recently seen or back-referenced) objects with optional skip.
  static const int kNumberOfHotObjects = 8;
  STATIC_ASSERT(kNumberOfHotObjects == HotObjectsList::kSize);
  // 0x38..0x3f
  static const int kHotObject = 0x38;
  // 0x58..0x5f
  static const int kHotObjectWithSkip = 0x58;
  static const int kHotObjectMask = 0x07;

  // 0x35..0x37, 0x55..0x57, 0x75..0x7f unused.

  // ---------- byte code range 0x80..0xff ----------
  // First 32 root array items.
  static const int kNumberOfRootArrayConstants = 0x20;
  // 0x80..0x9f
  static const int kRootArrayConstants = 0x80;
  // 0xa0..0xbf
  static const int kRootArrayConstantsWithSkip = 0xa0;
  static const int kRootArrayConstantsMask = 0x1f;

  // 32 common raw data lengths.
  static const int kNumberOfFixedRawData = 0x20;
  // 0xc0..0xdf
  static const int kFixedRawData = 0xc0;
  static const int kOnePointerRawData = kFixedRawData;
  static const int kFixedRawDataStart = kFixedRawData - 1;

  // 16 repeats lengths.
  static const int kNumberOfFixedRepeat = 0x10;
  // 0xe0..0xef
  static const int kFixedRepeat = 0xe0;
  static const int kFixedRepeatStart = kFixedRepeat - 1;

  // 0xf0..0xff unused.

  // ---------- special values ----------
  static const int kAnyOldSpace = -1;

  // Sentinel after a new object to indicate that double alignment is needed.
  static const int kDoubleAlignmentSentinel = 0;

  // ---------- member variable ----------
  HotObjectsList hot_objects_;
};

class SerializedData {
 public:
  class Reservation {
   public:
    explicit Reservation(uint32_t size)
        : reservation_(ChunkSizeBits::encode(size)) {}

    uint32_t chunk_size() const { return ChunkSizeBits::decode(reservation_); }
    bool is_last() const { return IsLastChunkBits::decode(reservation_); }

    void mark_as_last() { reservation_ |= IsLastChunkBits::encode(true); }

   private:
    uint32_t reservation_;
  };

  SerializedData(byte* data, int size)
      : data_(data), size_(size), owns_data_(false) {}
  SerializedData() : data_(NULL), size_(0), owns_data_(false) {}

  ~SerializedData() {
    if (owns_data_) DeleteArray<byte>(data_);
  }

  uint32_t GetMagicNumber() const { return GetHeaderValue(kMagicNumberOffset); }

  class ChunkSizeBits : public BitField<uint32_t, 0, 31> {};
  class IsLastChunkBits : public BitField<bool, 31, 1> {};

  static uint32_t ComputeMagicNumber(ExternalReferenceTable* table) {
    uint32_t external_refs = table->size();
    return 0xC0DE0000 ^ external_refs;
  }

 protected:
  void SetHeaderValue(int offset, uint32_t value) {
    uint32_t* address = reinterpret_cast<uint32_t*>(data_ + offset);
    memcpy(reinterpret_cast<uint32_t*>(address), &value, sizeof(value));
  }

  uint32_t GetHeaderValue(int offset) const {
    uint32_t value;
    memcpy(&value, reinterpret_cast<int*>(data_ + offset), sizeof(value));
    return value;
  }

  void AllocateData(int size);

  static uint32_t ComputeMagicNumber(Isolate* isolate) {
    return ComputeMagicNumber(ExternalReferenceTable::instance(isolate));
  }

  void SetMagicNumber(Isolate* isolate) {
    SetHeaderValue(kMagicNumberOffset, ComputeMagicNumber(isolate));
  }

  static const int kMagicNumberOffset = 0;

  byte* data_;
  int size_;
  bool owns_data_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SERIALIZER_COMMON_H_
