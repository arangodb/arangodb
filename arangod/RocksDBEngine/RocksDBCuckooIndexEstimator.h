////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include <set>
#include <map>

#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/WriteLocker.h"
#include "Basics/fasthash.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RocksDBEngine/RocksDBFormat.h"

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

template <class Key, class HashKey = HashWithSeed<Key, 0xdeadbeefdeadbeefULL>,
          class Fingerprint = HashWithSeed<Key, 0xabcdefabcdef1234ULL>,
          class HashShort = HashWithSeed<uint16_t, 0xfedcbafedcba4321ULL>, class CompKey = std::equal_to<Key>>
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

    ~Slot() {
      // Not responsible for anything
    }

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

  enum SerializeFormat : char {
    // To describe this format we use | as a seperator for readability, but it
    // is NOT a printed character in the serialized string
    // NOCOMPRESSION:
    // type|length|size|nrUsed|nrCuckood|nrTotal|niceSize|logSize|base|counters
    NOCOMPRESSION = '1'

  };

 public:
  static bool isFormatSupported(arangodb::velocypack::StringRef serialized) {
    TRI_ASSERT(serialized.size() > sizeof(_appliedSeq) + sizeof(char));
    switch (serialized[sizeof(_appliedSeq)]) {
      case SerializeFormat::NOCOMPRESSION:
        return true;
    }
    return false;
  }

  explicit RocksDBCuckooIndexEstimator(uint64_t size)
      : _randState(0x2636283625154737ULL),
        _logSize(0),
        _size(0),
        _niceSize(0),
        _sizeMask(0),
        _sizeShift(0),
        _slotAllocSize(0),
        _counterAllocSize(0),
        _base(nullptr),
        _slotBase(nullptr),
        _counters(nullptr),
        _counterBase(nullptr),
        _nrUsed(0),
        _nrCuckood(0),
        _nrTotal(0),
        _appliedSeq(0),
        _needToPersist(false) {
    // Inflate size so that we have some padding to avoid failure
    size *= 2;
    size = (size >= 1024) ? size : 1024;  // want 256 buckets minimum

    // First find the smallest power of two that is not smaller than size:
    size /= kSlotsPerBucket;
    _size = size;
    initializeDefault();
  }

  explicit RocksDBCuckooIndexEstimator(arangodb::velocypack::StringRef const serialized)
      : _randState(0x2636283625154737ULL),
        _logSize(0),
        _size(0),
        _niceSize(0),
        _sizeMask(0),
        _sizeShift(0),
        _slotAllocSize(0),
        _counterAllocSize(0),
        _base(nullptr),
        _slotBase(nullptr),
        _counters(nullptr),
        _counterBase(nullptr),
        _nrUsed(0),
        _nrCuckood(0),
        _nrTotal(0),
        _appliedSeq(0),
        _needToPersist(false) {
    switch (serialized[sizeof(_appliedSeq)]) {
      case SerializeFormat::NOCOMPRESSION: {
        deserializeUncompressed(serialized);
        break;
      }
      default: {
        LOG_TOPIC("bcd09", WARN, arangodb::Logger::ENGINES)
            << "unable to restore index estimates: invalid format found";
        // Do not construct from serialization, use other constructor instead
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "unable to restore index estimates: invalid format found");
      }
    }
  }

  ~RocksDBCuckooIndexEstimator() {
    delete[] _slotBase;
    delete[] _counterBase;
  }

  RocksDBCuckooIndexEstimator(RocksDBCuckooIndexEstimator const&) = delete;
  RocksDBCuckooIndexEstimator(RocksDBCuckooIndexEstimator&&) = delete;
  RocksDBCuckooIndexEstimator& operator=(RocksDBCuckooIndexEstimator const&) = delete;
  RocksDBCuckooIndexEstimator& operator=(RocksDBCuckooIndexEstimator&&) = delete;

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
   */
  void serialize(std::string& serialized, rocksdb::SequenceNumber maxCommitSeq) {
    // We always have to start with the commit seq, type and then the length

    // commit seq, above that is an uncommited operations
    //    rocksdb::SequenceNumber commitSeq = committableSeq();
    // must apply updates first to be valid, WAL needs to preserve
    rocksdb::SequenceNumber appliedSeq = applyUpdates(maxCommitSeq);
    TRI_ASSERT(appliedSeq <= maxCommitSeq);

    {
      // Sorry we need a consistent state, so we have to read-lock
      READ_LOCKER(locker, _lock);

      appliedSeq = std::max(appliedSeq, _appliedSeq.load(std::memory_order_acquire));
      TRI_ASSERT(appliedSeq != std::numeric_limits<rocksdb::SequenceNumber>::max());
      rocksutils::uint64ToPersistent(serialized, appliedSeq);

      // type
      serialized += SerializeFormat::NOCOMPRESSION;

      // length
      uint64_t serialLength =
          (sizeof(SerializeFormat) + sizeof(uint64_t) + sizeof(_size) +
           sizeof(_nrUsed) + sizeof(_nrCuckood) + sizeof(_nrTotal) + sizeof(_niceSize) +
           sizeof(_logSize) + (_size * kSlotSize * kSlotsPerBucket)) +
          (_size * kCounterSize * kSlotsPerBucket);

      serialized.reserve(sizeof(uint64_t) + serialLength);
      // We always prepend the length, so parsing is easier
      rocksutils::uint64ToPersistent(serialized, serialLength);

      // Add all member variables
      rocksutils::uint64ToPersistent(serialized, _size);
      rocksutils::uint64ToPersistent(serialized, _nrUsed);
      rocksutils::uint64ToPersistent(serialized, _nrCuckood);
      rocksutils::uint64ToPersistent(serialized, _nrTotal);
      rocksutils::uint64ToPersistent(serialized, _niceSize);
      rocksutils::uint64ToPersistent(serialized, _logSize);

      // Add the data blob
      // Size is as follows: nrOfBuckets * kSlotsPerBucket * SlotSize
      TRI_ASSERT((_size * kSlotSize * kSlotsPerBucket) <= _slotAllocSize);

      for (uint64_t i = 0; i < (_size * kSlotSize * kSlotsPerBucket); i += kSlotSize) {
        rocksutils::uint16ToPersistent(serialized,
                                       *(reinterpret_cast<uint16_t*>(_base + i)));
      }

      TRI_ASSERT((_size * kCounterSize * kSlotsPerBucket) <= _counterAllocSize);

      for (uint64_t i = 0; i < (_size * kCounterSize * kSlotsPerBucket); i += kCounterSize) {
        rocksutils::uint32ToPersistent(serialized,
                                       *(reinterpret_cast<uint32_t*>(_counters + i)));
      }

      bool havePendingUpdates = !_insertBuffers.empty() || !_removalBuffers.empty() ||
                                !_truncateBuffer.empty();
      _needToPersist.store(havePendingUpdates, std::memory_order_release);
    }

    _appliedSeq.store(appliedSeq, std::memory_order_release);
  }

  /// @brief only call directly during startup/recovery; otherwise buffer
  void clear() {
    WRITE_LOCKER(locker, _lock);
    // Reset Stats
    _nrTotal = 0;
    _nrCuckood = 0;
    _nrUsed = 0;

    // Reset filter content
    // Now initialize all slots in all buckets with zero data:
    for (uint32_t b = 0; b < _size; ++b) {
      for (size_t i = 0; i < kSlotsPerBucket; ++i) {
        Slot f = findSlot(b, i);
        f.injectCounter(findCounter(b, i));
        f.reset();
      }
    }

    _needToPersist.store(true, std::memory_order_release);
  }

  Result bufferTruncate(rocksdb::SequenceNumber seq) {
    Result res = basics::catchVoidToResult([&]() -> void {
      WRITE_LOCKER(locker, _lock);
      _truncateBuffer.emplace(seq);
      _needToPersist.store(true, std::memory_order_release);
    });
    return res;
  }

  double computeEstimate() {
    READ_LOCKER(locker, _lock);
    if (0 == _nrTotal) {
      TRI_ASSERT(0 == _nrUsed);
      // If we do not have any documents, we have a rather constant estimate.
      return 1.0;
    }
    TRI_ASSERT(_nrUsed <= _nrTotal);
    if (_nrUsed > _nrTotal) {
      _nrTotal = _nrUsed;  // should never happen, but will keep estimates valid
                           // for production where the above assert is disabled
    }

    return (static_cast<double>(_nrUsed) / static_cast<double>(_nrTotal));
  }

  bool lookup(Key const& k) const {
    // look up a key, return either false if no pair with key k is
    // found or true.
    uint64_t hash1 = _hasherKey(k);
    uint64_t pos1 = hashToPos(hash1);
    uint16_t fingerprint = keyToFingerprint(k);
    // We compute the second hash already here to allow the result to
    // survive a mispredicted branch in the first loop. Is this sensible?
    uint64_t hash2 = _hasherPosFingerprint(pos1, fingerprint);
    uint64_t pos2 = hashToPos(hash2);
    bool found = false;
    {
      READ_LOCKER(guard, _lock);
      findSlotNoCuckoo(pos1, pos2, fingerprint, found);
    }
    return found;
  }

  /// @brief only call directly during startup/recovery; otherwise buffer
  bool insert(Key const& k) {
    // insert the key k
    //
    // The inserted key will have its fingerprint input entered in the table. If
    // there is a collision and a fingerprint needs to be cuckooed, a certain
    // number of attempts will be made. After that, a given fingerprint may
    // simply be expunged. If something is expunged, the function will return
    // false, otherwise true.

    uint64_t hash1 = _hasherKey(k);
    uint64_t pos1 = hashToPos(hash1);
    uint16_t fingerprint = keyToFingerprint(k);
    // We compute the second hash already here to let it survive a
    // mispredicted
    // branch in the first loop:
    uint64_t hash2 = _hasherPosFingerprint(pos1, fingerprint);
    uint64_t pos2 = hashToPos(hash2);

    {
      WRITE_LOCKER(guard, _lock);
      Slot slot = findSlotCuckoo(pos1, pos2, fingerprint);
      if (slot.isEmpty()) {
        // Free slot insert ourself.
        slot.init(fingerprint);
        ++_nrUsed;
        TRI_ASSERT(_nrUsed > 0);
      } else {
        TRI_ASSERT(slot.isEqual(fingerprint));
        slot.increase();
      }
      ++_nrTotal;
      _needToPersist.store(true, std::memory_order_release);
    }

    return true;
  }

  /// @brief only call directly during startup/recovery; otherwise buffer
  bool remove(Key const& k) {
    // remove one element with key k, if one is in the table. Return true if
    // a key was removed and false otherwise.
    // look up a key, return either false if no pair with key k is
    // found or true.
    uint64_t hash1 = _hasherKey(k);
    uint64_t pos1 = hashToPos(hash1);
    uint16_t fingerprint = keyToFingerprint(k);
    // We compute the second hash already here to allow the result to
    // survive a mispredicted branch in the first loop. Is this sensible?
    uint64_t hash2 = _hasherPosFingerprint(pos1, fingerprint);
    uint64_t pos2 = hashToPos(hash2);

    bool found = false;
    {
      WRITE_LOCKER(guard, _lock);
      Slot slot = findSlotNoCuckoo(pos1, pos2, fingerprint, found);
      if (found) {
        // only decrease the total if we actually found it
        --_nrTotal;
        if (!slot.decrease()) {
          // Removed last element. Have to remove
          slot.reset();
          --_nrUsed;
        }
        _needToPersist.store(true, std::memory_order_release);
        return true;
      }
      // If we get here we assume that the element was once inserted, but
      // removed by cuckoo
      // Reduce nrCuckood;
      if (_nrCuckood > 0) {
        // not included in _nrTotal, just decrease here
        --_nrCuckood;
      }
      _needToPersist.store(true, std::memory_order_release);
    }
    return false;
  }

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
  Result bufferUpdates(rocksdb::SequenceNumber seq, std::vector<Key>&& inserts,
                       std::vector<Key>&& removals) {
    TRI_ASSERT(!inserts.empty() || !removals.empty());
    Result res = basics::catchVoidToResult([&]() -> void {
      WRITE_LOCKER(locker, _lock);
      if (!inserts.empty()) {
        _insertBuffers.emplace(seq, std::move(inserts));
      }
      if (!removals.empty()) {
        _removalBuffers.emplace(seq, std::move(removals));
      }

      _needToPersist.store(true, std::memory_order_release);
    });
    return res;
  }

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
  /// @brief call with output from committableSeq(current), and before serialize
  rocksdb::SequenceNumber applyUpdates(rocksdb::SequenceNumber commitSeq) {
    rocksdb::SequenceNumber appliedSeq = 0;
    Result res = basics::catchVoidToResult([&]() -> void {
      std::vector<Key> inserts;
      std::vector<Key> removals;

      // truncate will increase this sequence
      rocksdb::SequenceNumber ignoreSeq = 0;
      while (true) {
        bool foundTruncate = false;
        // find out if we have buffers to apply
        {
          WRITE_LOCKER(locker, _lock);

          {
            // check for a truncate marker
            auto it = _truncateBuffer.begin();  // sorted ASC
            while (it != _truncateBuffer.end() && *it <= commitSeq) {
              ignoreSeq = *it;
              TRI_ASSERT(ignoreSeq != 0);
              foundTruncate = true;
              appliedSeq = std::max(appliedSeq, ignoreSeq);
              it = _truncateBuffer.erase(it);
            }
          }
          TRI_ASSERT(ignoreSeq <= commitSeq);

          // check for inserts
          auto it = _insertBuffers.begin();  // sorted ASC
          while (it != _insertBuffers.end() && it->first <= commitSeq) {
            if (it->first <= ignoreSeq) {
              TRI_ASSERT(it->first <= appliedSeq);
              it = _insertBuffers.erase(it);
              continue;
            }
            inserts = std::move(it->second);
            TRI_ASSERT(!inserts.empty());
            appliedSeq = std::max(appliedSeq, it->first);
            _insertBuffers.erase(it);

            break;
          }

          // check for removals
          it = _removalBuffers.begin();  // sorted ASC
          while (it != _removalBuffers.end() && it->first <= commitSeq) {
            if (it->first <= ignoreSeq) {
              TRI_ASSERT(it->first <= appliedSeq);
              it = _removalBuffers.erase(it);
              continue;
            }
            removals = std::move(it->second);
            TRI_ASSERT(!removals.empty());
            appliedSeq = std::max(appliedSeq, it->first);
            _removalBuffers.erase(it);
            break;
          }
        }

        if (foundTruncate) {
          clear();  // clear estimates
        }

        // no inserts or removals left to apply, drop out of loop
        if (inserts.empty() && removals.empty()) {
          break;
        }

        // apply inserts
        for (auto const& key : inserts) {
          insert(key);
        }
        inserts.clear();

        // apply removals
        for (auto const& key : removals) {
          remove(key);
        }
        removals.clear();
      }  // </while(true)>
    });
    return appliedSeq;
  }

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

  void deserializeUncompressed(arangodb::velocypack::StringRef const& serialized) {
    // Assert that we have at least the member variables
    TRI_ASSERT(serialized.size() >=
               (sizeof(_appliedSeq)) +
                   (sizeof(SerializeFormat) + sizeof(uint64_t) + sizeof(_size) +
                    sizeof(_nrUsed) + sizeof(_nrCuckood) + sizeof(_nrTotal) +
                    sizeof(_niceSize) + sizeof(_logSize)));
    char const* current = serialized.data();

    _appliedSeq = rocksutils::uint64FromPersistent(current);
    current += sizeof(_appliedSeq);

    TRI_ASSERT(*current == SerializeFormat::NOCOMPRESSION);
    current++;  // Skip format char

    uint64_t length = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);
    // Validate that the serialized format is exactly as long as
    // we expect it to be
    TRI_ASSERT(serialized.size() == length + sizeof(_appliedSeq));

    _size = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);

    if (_size <= 256) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unable to unserialize index estimates");
    }

    _nrUsed = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);

    _nrCuckood = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);

    _nrTotal = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);

    _niceSize = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);

    _logSize = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);

    deriveSizesAndAlloc();

    // Validate that we have enough data in the serialized format.
    TRI_ASSERT(serialized.size() ==
               (sizeof(_appliedSeq) + sizeof(SerializeFormat) +
                sizeof(uint64_t) + sizeof(_size) + sizeof(_nrUsed) +
                sizeof(_nrCuckood) + sizeof(_nrTotal) + sizeof(_niceSize) +
                sizeof(_logSize) + (_size * kSlotSize * kSlotsPerBucket)) +
                   (_size * kCounterSize * kSlotsPerBucket));

    // Insert the raw data
    // Size is as follows: nrOfBuckets * kSlotsPerBucket * SlotSize
    TRI_ASSERT((_size * kSlotSize * kSlotsPerBucket) <= _slotAllocSize);

    for (uint64_t i = 0; i < (_size * kSlotSize * kSlotsPerBucket); i += kSlotSize) {
      *(reinterpret_cast<uint16_t*>(_base + i)) =
          rocksutils::uint16FromPersistent(current);
      current += kSlotSize;
    }

    TRI_ASSERT((_size * kCounterSize * kSlotsPerBucket) <= _counterAllocSize);

    for (uint64_t i = 0; i < (_size * kCounterSize * kSlotsPerBucket); i += kCounterSize) {
      *(reinterpret_cast<uint32_t*>(_counters + i)) =
          rocksutils::uint32FromPersistent(current);
      current += kCounterSize;
    }
  }

  void initializeDefault() {
    _niceSize = 256;
    _logSize = 8;
    while (_niceSize < _size) {
      _niceSize <<= 1;
      _logSize += 1;
    }

    deriveSizesAndAlloc();

    // Now initialize all slots in all buckets with zero data:
    for (uint32_t b = 0; b < _size; ++b) {
      for (size_t i = 0; i < kSlotsPerBucket; ++i) {
        Slot f = findSlot(b, i);
        f.injectCounter(findCounter(b, i));
        f.reset();
      }
    }
  }

  void deriveSizesAndAlloc() {
    _sizeMask = _niceSize - 1;
    _sizeShift = static_cast<uint32_t>((64 - _logSize) / 2);

    // give 64 bytes padding to enable 64-byte alignment
    _slotAllocSize = _size * kSlotSize * kSlotsPerBucket + 64;

    _slotBase = new char[_slotAllocSize];

    _base = reinterpret_cast<char*>(
        (reinterpret_cast<uintptr_t>(_slotBase) + 63) &
        ~((uintptr_t)0x3fu));  // to actually implement the 64-byte alignment,
                               // shift base pointer within allocated space to
                               // 64-byte boundary

    // give 64 bytes padding to enable 64-byte alignment
    _counterAllocSize = _size * kCounterSize * kSlotsPerBucket + 64;
    _counterBase = new char[_counterAllocSize];

    _counters = reinterpret_cast<char*>(
        (reinterpret_cast<uintptr_t>(_counterBase) + 63) &
        ~((uintptr_t)0x3fu));  // to actually implement the 64-byte alignment,
                               // shift base pointer within allocated space to
                               // 64-byte boundary
  }

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

  HashKey _hasherKey;        // Instance to compute the first hash function
  Fingerprint _fingerprint;  // Instance to compute a fingerprint of a key
  HashShort _hasherShort;    // Instance to compute the second hash function

  arangodb::basics::ReadWriteLock mutable _lock;
};  // namespace arangodb

}  // namespace arangodb

#endif
