////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Daniel Larkin
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_INDEX_ESTIMATOR_H
#define ARANGOD_ROCKSDB_ROCKSDB_INDEX_ESTIMATOR_H 1

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/debugging.h"
#include "Basics/fasthash.h"

#include <rocksdb/types.h>
#include <velocypack/StringRef.h>

// In the following template:
//   Key is the key type, it must be copyable and movable, furthermore, Key
//     must be default constructible (without arguments) as empty and
//     must have an empty() method to indicate that the instance is
//     empty.
//     If using fasthash64 on all bytes of the object is not
//     a suitable hash function, one has to instanciate the template
//     with two hash function types as 3rd and 4th argument. If
//     std::equal_to<Key> is not implemented or does not behave correctly,
//     one has to supply a comparison class as well.
// This class is not thread-safe!

namespace arangodb {

// C++ wrapper for the hash function:
template <class T, uint64_t Seed>
class HashWithSeed {
 public:
  uint64_t operator()(T const& t) const noexcept {
    // Some implementation like Fnv or xxhash looking at bytes in type T,
    // taking the seed into account.
    auto p = reinterpret_cast<void const*>(&t);
    return fasthash64(p, sizeof(T), Seed);
  }
};

template <class Key>
class RocksDBCuckooIndexEstimator {
  // Note that the following has to be a power of two and at least 4!
  static constexpr uint32_t kSlotsPerBucket = 4;
  // total size of a slot
  static constexpr size_t kSlotSize = sizeof(uint16_t);
  // total size of a counter
  static constexpr size_t kCounterSize = sizeof(uint32_t);
  // maximum number of cuckoo rounds on insertion
  static constexpr unsigned kMaxRounds = 16;

 private:
  // Helper class to hold the finger prints.
  // Fingerprints are 64bit aligned and we
  // have 4 slots per 64bit bucket.
  struct Slot {
   private:
    uint16_t* _data;
    uint32_t* _counter;

   public:
    explicit Slot(uint16_t* data) : _data(data), _counter(nullptr) {}
    ~Slot() = default;

    bool operator==(const Slot& other) const { return _data == other._data; }

    uint16_t* fingerprint() const {
      TRI_ASSERT(_data != nullptr);
      return _data;
    }
    uint32_t* counter() const {
      TRI_ASSERT(_counter != nullptr);
      return _counter;
    }

    void reset() {
      *fingerprint() = 0;
      *counter() = 0;
    }

    bool isEqual(uint16_t fp) const { return ((*fingerprint()) == fp); }

    bool isEmpty() const { return (*fingerprint()) == 0; }

    // If this returns FALSE we have removed the
    // last element => we need to remove the fingerprint as well.
    bool decrease() {
      if (*counter() > 1) {
        (*counter())--;
        return true;
      }
      return false;
    }

    void increase() {
      if ((*counter()) < UINT32_MAX) {
        (*counter())++;
      }
    }

    void init(uint16_t fp) {
      // This is the first element
      *fingerprint() = fp;
      *counter() = 1;
    }

    void swap(uint16_t& fp, uint32_t& cnt) {
      std::swap(*fingerprint(), fp);
      std::swap(*counter(), cnt);
    }

    void injectCounter(uint32_t* cnt) { _counter = cnt; }
  };

 public:
  explicit RocksDBCuckooIndexEstimator(uint64_t size);

  explicit RocksDBCuckooIndexEstimator(arangodb::velocypack::StringRef serialized);

  ~RocksDBCuckooIndexEstimator();

  RocksDBCuckooIndexEstimator(RocksDBCuckooIndexEstimator const&) = delete;
  RocksDBCuckooIndexEstimator& operator=(RocksDBCuckooIndexEstimator const&) = delete;
  
  enum SerializeFormat : char {
    // Estimators are serialized in the following way:
    // - the first 8 bytes contain the applied seq number, little endian
    // - the next byte contains the serialization format, which is one of the 
    //   values from this enum
    // - the following bytes are format-specific payload
    //
    //   appliedSeq|format|payload

    // To describe formats we use | as a seperator for readability, but it
    // is NOT a printed character in the serialized string. 
    
    // UNCOMPRESSED:
    // serializes all instance members in a simple way, writing them all out
    // one after another. the values are not compressed and will use a lot of
    // space because even small numbers are serialized as 64bit values.
    //
    //   size|nrUsed|nrCuckood|nrTotal|niceSize|logSize|base|counters
    UNCOMPRESSED = '1',

    // COMPRESSED: 
    // first serializes everything using the UNCOMPRESSED format, and then
    // compressed it with Snappy compression into a shorter equivalent.
    // after compression, we only have a size of the compressed blob, and
    // the compressed blob itself. we end up with:
    //
    //   size|compressedBlob
    //
    // when uncompressing the compressed blob, it will contain the UNCOMPRESSED
    // serialized data structure
    COMPRESSED = '2',
  };
  
  static bool isFormatSupported(arangodb::velocypack::StringRef serialized);

  /**
   * @brief Serialize estimator for persistence, applying any buffered updates
   *
   * Format is hard-coded, and must support older formats for backwards
   * compatibility. The first 8 bytes consist of a sequence number S. All
   * updates prior to and including S are reflected in the serialization. Any
   * updates after S must be kept in the WALs and "replayed" during recovery.
   *
   * Applies any buffered updates and updates the "committed" seq/tick state.
   *
   * @param  serialized String for output
   * @param  commitSeq  Above that are still uncommited operations
   * @param  format     The serialization format to use
   */
  void serialize(std::string& serialized, 
                 rocksdb::SequenceNumber maxCommitSeq, 
                 SerializeFormat format);
  
  /// @brief only call directly during startup/recovery; otherwise buffer
  void clear();

  Result bufferTruncate(rocksdb::SequenceNumber seq);

  double computeEstimate();

  bool lookup(Key const& k) const;

  /// @brief only call directly during startup/recovery; otherwise buffer
  bool insert(Key const& k);

  /// @brief only call directly during startup/recovery; otherwise buffer
  bool remove(Key const& k);

  uint64_t capacity() const { return _size * kSlotsPerBucket; }

  // not thread safe. called only during tests
  uint64_t nrTotal() const { return _nrTotal; }

  // not thread safe. called only during tests
  uint64_t nrUsed() const { return _nrUsed; }

  // not thread safe. called only during tests
  uint64_t nrCuckood() const { return _nrCuckood; }

  bool needToPersist() const {
    return _needToPersist.load(std::memory_order_acquire);
  }

  /**
   * @brief Buffer updates to this estimator to be applied when appropriate
   *
   * Buffers updates associated with a given commit seq/tick. Will hold updates
   * until all previous blockers have been removed to ensure a consistent state
   * for sync/recovery and avoid any missed updates.
   *
   * @param  seq      The seq/tick post-commit, prior to call
   * @param  inserts  Vector of hashes to insert
   * @param  removals Vector of hashes to remove
   * @return          May return error if any functions throw (e.g. alloc)
   */
  Result bufferUpdates(rocksdb::SequenceNumber seq, 
                       std::vector<Key>&& inserts,
                       std::vector<Key>&& removals);

  /**
   * @brief Fetches the most recently set "committed" seq/tick
   *
   * This value is set during serialization.
   *
   * @return The latest seq/tick through which the estimate is valid
   */
  rocksdb::SequenceNumber appliedSeq() const {
    return _appliedSeq.load(std::memory_order_acquire);
  }

  /// @brief set the most recently set "committed" seq/tick
  /// only set when recalculating the index estimate
  void setAppliedSeq(rocksdb::SequenceNumber seq) {
    _appliedSeq.store(seq, std::memory_order_release);
    _needToPersist.store(true, std::memory_order_release);
  }

  void clearInRecovery(rocksdb::SequenceNumber seq) {
    if (seq <= _appliedSeq.load(std::memory_order_acquire)) {
      // already incorporated in estimator values
      return;
    }
    clear();
    setAppliedSeq(seq);
  }

 private:  // methods
  void appendHeader(std::string& result) const;

  void appendDataBlob(std::string& result) const;

  /// @brief call with output from committableSeq(current), and before serialize
  rocksdb::SequenceNumber applyUpdates(rocksdb::SequenceNumber commitSeq);

  uint64_t memoryUsage() const {
    return sizeof(RocksDBCuckooIndexEstimator) + _slotAllocSize + _counterAllocSize;
  }

  Slot findSlotNoCuckoo(uint64_t pos1, uint64_t pos2, uint16_t fp, bool& found) const {
    found = false;
    Slot s = findSlotNoCuckoo(pos1, fp, found);
    if (found) {
      return s;
    }
    // Not found by first hash. use second hash.
    return findSlotNoCuckoo(pos2, fp, found);
  }

  // Find a slot for this fingerprint
  // This function guarantees the following:
  // If this fingerprint is stored already, the slot will be
  // pointing to this fingerprint.
  // If this fingerprint is NOT storead already the returned slot
  // will be empty and can be filled with the fingerprint
  // In order to create an empty slot this function tries to
  // cuckoo neighboring elements, if that does not succeed
  // it deletes a random element occupying a position.
  Slot findSlotCuckoo(uint64_t pos1, uint64_t pos2, uint16_t fp) {
    Slot firstEmpty(nullptr);
    bool foundEmpty = false;

    for (uint64_t i = 0; i < kSlotsPerBucket; ++i) {
      Slot slot = findSlot(pos1, i);
      if (slot.isEqual(fp)) {
        // Found we are done, short-circuit.
        slot.injectCounter(findCounter(pos1, i));
        return slot;
      }
      if (!foundEmpty && slot.isEmpty()) {
        slot.injectCounter(findCounter(pos1, i));
        foundEmpty = true;
        firstEmpty = slot;
      }
    }

    for (uint64_t i = 0; i < kSlotsPerBucket; ++i) {
      Slot slot = findSlot(pos2, i);
      if (slot.isEqual(fp)) {
        // Found we are done, short-circuit.
        slot.injectCounter(findCounter(pos2, i));
        return slot;
      }
      if (!foundEmpty && slot.isEmpty()) {
        slot.injectCounter(findCounter(pos2, i));
        foundEmpty = true;
        firstEmpty = slot;
      }
    }

    // Value not yet inserted.

    if (foundEmpty) {
      // But we found an empty slot
      return firstEmpty;
    }

    // We also did not find an empty slot, now the cuckoo goes...

    // We initially write a 0 in here, because the caller will
    // Increase the counter by one
    uint32_t counter = 0;

    uint8_t r = pseudoRandomChoice();
    if ((r & 1) != 0) {
      std::swap(pos1, pos2);
    }

    // Now expunge a random element from any of these slots:
    // and place our own into it.
    // We have to keep the reference to the cuckood slot here.
    r = pseudoRandomChoice();
    uint64_t i = r & (kSlotsPerBucket - 1);
    firstEmpty = findSlot(pos1, i);
    firstEmpty.injectCounter(findCounter(pos1, i));
    firstEmpty.swap(fp, counter);

    uint64_t hash2 = _hasherPosFingerprint(pos1, fp);
    pos2 = hashToPos(hash2);

    // Now let the cuckoo fly and find a place for the poor one we just took
    // out.
    for (uint64_t i = 0; i < kSlotsPerBucket; ++i) {
      Slot slot = findSlot(pos2, i);
      if (slot.isEmpty()) {
        slot.injectCounter(findCounter(pos2, i));
        // Yeah we found an empty place already
        *slot.fingerprint() = fp;
        *slot.counter() = counter;
        ++_nrUsed;
        return firstEmpty;
      }
    }

    // Bad luck, let us try to move to a different slot.
    for (unsigned attempt = 1; attempt < kMaxRounds; attempt++) {
      std::swap(pos1, pos2);
      // Now expunge a random element from any of these slots:
      r = pseudoRandomChoice();
      uint64_t i = r & (kSlotsPerBucket - 1);
      // We expunge the element at position pos1 and slot i:
      Slot slot = findSlot(pos1, i);
      if (slot == firstEmpty) {
        // We have to keep this one in place.
        // Take a different one
        i = (i + 1) % kSlotsPerBucket;
        slot = findSlot(pos1, i);
      }
      slot.injectCounter(findCounter(pos1, i));
      slot.swap(fp, counter);

      hash2 = _hasherPosFingerprint(pos1, fp);
      pos2 = hashToPos(hash2);

      for (uint64_t i = 0; i < kSlotsPerBucket; ++i) {
        Slot slot = findSlot(pos2, i);
        if (slot.isEmpty()) {
          slot.injectCounter(findCounter(pos2, i));
          // Finally an empty place
          *slot.fingerprint() = fp;
          *slot.counter() = counter;
          ++_nrUsed;
          return firstEmpty;
        }
      }
    }
    // If we get here we had to remove one of the elements.
    // Let's increas the cuckoo counter
    _nrCuckood++;
    // and let's decrease the total so we don't have to recalculate later
    _nrTotal = (_nrTotal >= counter) ? (_nrTotal - counter) : 0;
    return firstEmpty;
  }

  // Do not use the output if found == false
  Slot findSlotNoCuckoo(uint64_t pos, uint16_t fp, bool& found) const {
    found = false;
    for (uint64_t i = 0; i < kSlotsPerBucket; ++i) {
      Slot slot = findSlot(pos, i);
      if (fp == *slot.fingerprint()) {
        slot.injectCounter(findCounter(pos, i));
        found = true;
        return slot;
      }
    }
    return Slot{nullptr};
  }

  Slot findSlot(uint64_t pos, uint64_t slot) const {
    TRI_ASSERT(kSlotSize * (pos * kSlotsPerBucket + slot) <= _slotAllocSize);
    char* address = _base + kSlotSize * (pos * kSlotsPerBucket + slot);
    auto ret = reinterpret_cast<uint16_t*>(address);
    return Slot(ret);
  }

  uint32_t* findCounter(uint64_t pos, uint64_t slot) const {
    TRI_ASSERT(kCounterSize * (pos * kSlotsPerBucket + slot) <= _counterAllocSize);
    char* address = _counters + kCounterSize * (pos * kSlotsPerBucket + slot);
    return reinterpret_cast<uint32_t*>(address);
  }

  uint64_t hashToPos(uint64_t hash) const {
    uint64_t relevantBits = (hash >> _sizeShift) & _sizeMask;
    return ((relevantBits < _size) ? relevantBits : (relevantBits - _size));
  }

  uint16_t keyToFingerprint(Key const& k) const {
    uint64_t hashfp = _fingerprint(k);
    uint16_t fingerprint =
        (uint16_t)((hashfp ^ (hashfp >> 16) ^ (hashfp >> 32) ^ (hashfp >> 48)) & 0xFFFF);
    return (fingerprint ? fingerprint : 1);
  }

  uint64_t _hasherPosFingerprint(uint64_t pos, uint16_t fingerprint) const {
    return ((pos << _sizeShift) ^ _hasherShort(fingerprint));
  }

  uint8_t pseudoRandomChoice() {
    _randState = _randState * 997 + 17;  // ignore overflows
    return static_cast<uint8_t>((_randState >> 37) & 0xff);
  }

  void deserialize(arangodb::velocypack::StringRef serialized);

  void deserializeUncompressedBody(arangodb::velocypack::StringRef serialized);

  void initializeDefault();

  void deriveSizesAndAlloc();

 private:               // member variables
  uint64_t _randState;  // pseudo random state for expunging

  uint64_t _logSize;    // logarithm (base 2) of number of buckets
  uint64_t _size;       // actual number of buckets
  uint64_t _niceSize;   // smallest power of 2 at least number of buckets, ==
                        // 2^_logSize
  uint64_t _sizeMask;   // used to mask out some bits from the hash
  uint32_t _sizeShift;  // used to shift the bits down to get a position
  uint64_t _slotAllocSize;     // number of allocated bytes for the slots,
                               // == _size * kSlotsPerBucket * kSlotSize + 64
  uint64_t _counterAllocSize;  // number of allocated bytes ofr the counters,
                               // == _size * kSlotsPerBucket * kCounterSize + 64
  char* _base;                 // pointer to allocated space, 64-byte aligned
  char* _slotBase;             // base of original allocation
  char* _counters;             // pointer to allocated space, 64-byte aligned
  char* _counterBase;          // base of original counter allocation
  uint64_t _nrUsed;            // number of pairs stored in the table
  uint64_t _nrCuckood;  // number of elements that have been removed by cuckoo
  uint64_t _nrTotal;    // number of elements included in total (not cuckood)

  std::atomic<rocksdb::SequenceNumber> _appliedSeq;
  std::atomic<bool> _needToPersist;

  std::multimap<rocksdb::SequenceNumber, std::vector<Key>> _insertBuffers;
  std::multimap<rocksdb::SequenceNumber, std::vector<Key>> _removalBuffers;
  std::set<rocksdb::SequenceNumber> _truncateBuffer;

  // Instance to compute the first hash function
  HashWithSeed<Key, 0xdeadbeefdeadbeefULL> _hasherKey;
  
  // Instance to compute a fingerprint of a key
  HashWithSeed<Key, 0xabcdefabcdef1234ULL> _fingerprint;

  // Instance to compute the second hash function
  HashWithSeed<uint16_t, 0xfedcbafedcba4321ULL> _hasherShort;

  arangodb::basics::ReadWriteLock mutable _lock;
}; 

using RocksDBCuckooIndexEstimatorType = RocksDBCuckooIndexEstimator<uint64_t>;

}  // namespace arangodb

#endif
