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

#include "Basics/Common.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringRef.h"
#include "Basics/WriteLocker.h"
#include "Basics/fasthash.h"

#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"

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
  uint64_t operator()(T const& t) const {
    // Some implementation like Fnv or xxhash looking at bytes in type T,
    // taking the seed into account.
    auto p = reinterpret_cast<void const*>(&t);
    return fasthash64(p, sizeof(T), Seed);
  }
};

template <class Key, class HashKey = HashWithSeed<Key, 0xdeadbeefdeadbeefULL>,
          class Fingerprint = HashWithSeed<Key, 0xabcdefabcdef1234ULL>,
          class HashShort = HashWithSeed<uint16_t, 0xfedcbafedcba4321ULL>,
          class CompKey = std::equal_to<Key>>
class RocksDBCuckooIndexEstimator {
  // Note that the following has to be a power of two and at least 4!
  static constexpr uint32_t SlotsPerBucket = 4;

 private:
  // Helper class to abstract away where which data is stored.
  struct Slot {
   private:
    uint16_t* _data;

   public:
    Slot(uint16_t* data) : _data(data) {}

    ~Slot() {
      // Not responsible for anything
    }

    bool operator==(const Slot& other) { return _data == other._data; }

    uint16_t* fingerprint() { return _data; }

    uint16_t* counter() { return _data + 1; }

    void reset() {
      *fingerprint() = 0;
      *counter() = 0;
    }

    bool isEqual(uint16_t fp) { return ((*fingerprint()) == fp); }

    bool isEmpty() { return (*fingerprint()) == 0; }
  };

  enum SerializeFormat : char {
    // To describe this format we use | as a seperator for readability, but it
    // is NOT a printed character in the serialized string
    // NOCOMPRESSION: type|length|size|nrUsed|nrCuckood|nrTotal|niceSize|logSize|base
    NOCOMPRESSION = '0'

  };

 public:
  RocksDBCuckooIndexEstimator(uint64_t size)
      : _randState(0x2636283625154737ULL),
        _slotSize(2 * sizeof(uint16_t)),  // Sort out offsets and alignments
        _nrUsed(0),
        _nrCuckood(0),
        _nrTotal(0),
        _maxRounds(16) {
    // Inflate size so that we have some padding to avoid failure
    size *= 2.0;
    size = (size >= 1024) ? size : 1024;  // want 256 buckets minimum

    // First find the smallest power of two that is not smaller than size:
    size /= SlotsPerBucket;
    _size = size;
    initializeDefault();
  }

  RocksDBCuckooIndexEstimator(arangodb::StringRef const serialized)
      : _randState(0x2636283625154737ULL),
        _slotSize(2 * sizeof(uint16_t)),  // Sort out offsets and alignments
        _nrUsed(0),
        _nrCuckood(0),
        _nrTotal(0),
        _maxRounds(16) {
    switch (serialized.front()) {
      case SerializeFormat::NOCOMPRESSION: {
        deserializeUncompressed(serialized);
        break;
      }
      default: {
        LOG_TOPIC(ERR, arangodb::Logger::ENGINES) << "Unable to restore the "
                                                     "index estimates. Invalid "
                                                     "format persisted.";
        initializeDefault();
      }
    }
  }

  ~RocksDBCuckooIndexEstimator() { delete[] _allocBase; }

  RocksDBCuckooIndexEstimator(RocksDBCuckooIndexEstimator const&) = delete;
  RocksDBCuckooIndexEstimator(RocksDBCuckooIndexEstimator&&) = delete;
  RocksDBCuckooIndexEstimator& operator=(RocksDBCuckooIndexEstimator const&) =
      delete;
  RocksDBCuckooIndexEstimator& operator=(RocksDBCuckooIndexEstimator&&) =
      delete;

  void serialize(std::string& serialized) const {
    // This format is always hard coded and the serialisation has to support
    // older formats
    // for backwards compatibility
    // We always have to start with the type and then the length
    serialized += SerializeFormat::NOCOMPRESSION;

    uint64_t serialLength =
        (sizeof(SerializeFormat) + sizeof(uint64_t) + sizeof(_size) + sizeof(_nrUsed) +
         sizeof(_nrCuckood) + sizeof(_nrTotal) + sizeof(_niceSize) +
         sizeof(_logSize) + (_size * _slotSize * SlotsPerBucket));

    serialized.reserve(sizeof(uint64_t) + serialLength);
    // We always prepend the length, so parsing is easier
    rocksutils::uint64ToPersistent(serialized, serialLength);

    {
      // Sorry we need a consistent state, so we have to read-lock from here on...
      READ_LOCKER(locker, _bucketLock);
      // Add all member variables
      rocksutils::uint64ToPersistent(serialized, _size);
      rocksutils::uint64ToPersistent(serialized, _nrUsed);
      rocksutils::uint64ToPersistent(serialized, _nrCuckood);
      rocksutils::uint64ToPersistent(serialized, _nrTotal);
      rocksutils::uint64ToPersistent(serialized, _niceSize);
      rocksutils::uint64ToPersistent(serialized, _logSize);

      // Add the data blob
      // Size is as follows: nrOfBuckets * SlotsPerBucket * SlotSize
      TRI_ASSERT((_size * _slotSize * SlotsPerBucket) <= _allocSize);
      
      for (uint64_t i = 0; i < (_size * _slotSize * SlotsPerBucket) / sizeof(uint16_t);
           ++i) {
        rocksutils::uint16ToPersistent(serialized, *(reinterpret_cast<uint16_t*>(_base + i * 2)));
      }
    }
  }

  void clear() {
    WRITE_LOCKER(locker, _bucketLock);
    // Reset Stats
    _nrTotal = 0;
    _nrCuckood = 0;
    _nrUsed = 0;

    // Reset filter content
    // Now initialize all slots in all buckets with zero data:
    for (uint32_t b = 0; b < _size; ++b) {
      for (size_t i = 0; i < SlotsPerBucket; ++i) {
        Slot f = findSlot(b, i);
        f.reset();
      }
    }
  }

  double computeEstimate() {
    READ_LOCKER(locker, _bucketLock);
    if (_nrTotal == 0) {
      // If we do not have any documents we have a rather constant estimate.
      return 1;
    }
    // _nrUsed; These are known to be distinct values
    // _nrCuckood; These are eventually distinct documents with unknown state
    return (double)(_nrUsed + ((double)_nrCuckood * 3 * _nrUsed / _nrTotal)) /
           _nrTotal;
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
      READ_LOCKER(guard, _bucketLock);
      findSlotNoCuckoo(pos1, pos2, fingerprint, found);
    }
    return found;
  }

  bool insert(Key& k) {
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
      WRITE_LOCKER(guard, _bucketLock);
      Slot slot = findSlotCuckoo(pos1, pos2, fingerprint);
      if (slot.isEmpty()) {
        // Free slot insert ourself.
        *slot.fingerprint() = fingerprint;
        *slot.counter() = 1;  // We are the first element
        _nrUsed++;
      } else {
        TRI_ASSERT(slot.isEqual(fingerprint));
        // TODO replace with constant uint16_t max
        if (*slot.counter() < 65536) {
          // just to avoid overflow...
          (*slot.counter())++;
        }
      }
    }
    _nrTotal++;
    return true;
  }

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
    _nrTotal--;
    {
      WRITE_LOCKER(guard, _bucketLock);
      Slot slot = findSlotNoCuckoo(pos1, pos2, fingerprint, found);
      if (found) {
        if (*slot.counter() <= 1) {
          // We remove the last one of those, free slot
          slot.reset();
          _nrUsed--;
        } else {
          // Just decrease the counter
          (*slot.counter())--;
        }
        return true;
      }
    }
    // If we get here we assume that the element was once inserted, but removed
    // by cuckoo
    // Reduce nrCuckood;
    if (_nrCuckood > 0) {
      --_nrCuckood;
    }
    return false;
  }

  uint64_t capacity() const { return _size * SlotsPerBucket; }

  uint64_t nrUsed() const { return _nrUsed; }

  uint64_t nrCuckood() const { return _nrCuckood; }

  uint64_t memoryUsage() const {
    return sizeof(RocksDBCuckooIndexEstimator) + _allocSize;
  }

 private:  // methods
  Slot findSlotNoCuckoo(uint64_t pos1, uint64_t pos2, uint16_t fp,
                        bool& found) const {
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

    for (uint64_t i = 0; i < SlotsPerBucket; ++i) {
      Slot slot = findSlot(pos1, i);
      if (slot.isEqual(fp)) {
        // Found we are done, short-circuit.
        return slot;
      }
      if (!foundEmpty && slot.isEmpty()) {
        foundEmpty = true;
        firstEmpty = slot;
      }
    }

    for (uint64_t i = 0; i < SlotsPerBucket; ++i) {
      Slot slot = findSlot(pos2, i);
      if (slot.isEqual(fp)) {
        // Found we are done, short-circuit.
        return slot;
      }
      if (!foundEmpty && slot.isEmpty()) {
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

    uint16_t counter =
        0;  // We initially write a 0 in here, because the caller will
            // Increase the counter by one
    uint8_t r = pseudoRandomChoice();
    if ((r & 1) != 0) {
      std::swap(pos1, pos2);
    }

    // Now expunge a random element from any of these slots:
    // and place our own into it.
    // We have to keep the reference to the cuckood slot here.
    r = pseudoRandomChoice();
    uint64_t i = r & (SlotsPerBucket - 1);
    firstEmpty = findSlot(pos1, i);
    uint16_t fDummy = *firstEmpty.fingerprint();
    uint16_t cDummy = *firstEmpty.counter();
    *firstEmpty.fingerprint() = fp;
    *firstEmpty.counter() = counter;
    fp = fDummy;
    counter = cDummy;

    uint64_t hash2 = _hasherPosFingerprint(pos1, fp);
    pos2 = hashToPos(hash2);

    // Now let the cuckoo fly and find a place for the poor one we just took
    // out.
    for (uint64_t i = 0; i < SlotsPerBucket; ++i) {
      Slot slot = findSlot(pos2, i);
      if (slot.isEmpty()) {
        // Yeah we found an empty place already
        *slot.fingerprint() = fp;
        *slot.counter() = counter;
        ++_nrUsed;
        return firstEmpty;
      }
    }

    // Bad luck, let us try to move to a different slot.
    for (unsigned attempt = 1; attempt < _maxRounds; attempt++) {
      std::swap(pos1, pos2);
      // Now expunge a random element from any of these slots:
      r = pseudoRandomChoice();
      uint64_t i = r & (SlotsPerBucket - 1);
      // We expunge the element at position pos1 and slot i:
      Slot slot = findSlot(pos1, i);
      if (slot == firstEmpty) {
        // We have to keep this one in place.
        // Take a different one
        i = (i + 1) % SlotsPerBucket;
        slot = findSlot(pos1, i);
      }
      fDummy = *slot.fingerprint();
      cDummy = *slot.counter();
      *slot.fingerprint() = fp;
      *slot.counter() = counter;
      fp = fDummy;
      counter = cDummy;

      hash2 = _hasherPosFingerprint(pos1, fp);
      pos2 = hashToPos(hash2);

      for (uint64_t i = 0; i < SlotsPerBucket; ++i) {
        Slot slot = findSlot(pos2, i);
        if (slot.isEmpty()) {
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
    return firstEmpty;
  }

  // Do not use the output if found == false
  Slot findSlotNoCuckoo(uint64_t pos, uint16_t fp, bool& found) const {
    found = false;
    for (uint64_t i = 0; i < SlotsPerBucket; ++i) {
      Slot slot = findSlot(pos, i);
      if (fp == *slot.fingerprint()) {
        found = true;
        return slot;
      }
    }
    return Slot{nullptr};
  }

  Slot findSlot(uint64_t pos, uint64_t slot) const {
    char* address = _base + _slotSize * (pos * SlotsPerBucket + slot);
    auto ret = reinterpret_cast<uint16_t*>(address);
    return Slot(ret);
  }

  uint64_t hashToPos(uint64_t hash) const {
    uint64_t relevantBits = (hash >> _sizeShift) & _sizeMask;
    return ((relevantBits < _size) ? relevantBits : (relevantBits - _size));
  }

  uint16_t keyToFingerprint(Key const& k) const {
    uint64_t hash = _fingerprint(k);
    uint16_t fingerprint = (uint16_t)(
        (hash ^ (hash >> 16) ^ (hash >> 32) ^ (hash >> 48)) & 0xFFFF);
    return (fingerprint ? fingerprint : 1);
  }

  uint64_t _hasherPosFingerprint(uint64_t pos, uint16_t fingerprint) const {
    return ((pos << _sizeShift) ^ _hasherShort(fingerprint));
  }

  uint8_t pseudoRandomChoice() {
    _randState = _randState * 997 + 17;  // ignore overflows
    return static_cast<uint8_t>((_randState >> 37) & 0xff);
  }

  void deserializeUncompressed(arangodb::StringRef const& serialized) {
    // Assert that we have at least the member variables
    TRI_ASSERT(serialized.size() >= (sizeof(SerializeFormat) + sizeof(uint64_t) + sizeof(_size) + sizeof(_nrUsed) +
                                     sizeof(_nrCuckood) + sizeof(_nrTotal) + sizeof(_niceSize) +
                                     sizeof(_logSize) ));
    char const* current = serialized.data();
    TRI_ASSERT(*current == SerializeFormat::NOCOMPRESSION);
    current++;  // Skip format char

    uint64_t length = rocksutils::uint64FromPersistent(current);
    current += sizeof(uint64_t);
    // Validate that the serialized format is exactly as long as we expect it to be
    TRI_ASSERT(serialized.size() == length);

    _size = rocksutils::uint64FromPersistent(current);
    current += sizeof(_size);

    _nrUsed = rocksutils::uint64FromPersistent(current);
    current += sizeof(_nrUsed);

    _nrCuckood = rocksutils::uint64FromPersistent(current);
    current += sizeof(_nrCuckood);

    _nrTotal = rocksutils::uint64FromPersistent(current);
    current += sizeof(_nrTotal);

    _niceSize = rocksutils::uint64FromPersistent(current);
    current += sizeof(_niceSize);

    _logSize = rocksutils::uint64FromPersistent(current);
    current += sizeof(_logSize);

    deriveSizesAndAlloc();


    // Validate that we have enough data in the serialized format.
    TRI_ASSERT(serialized.size() ==
               (sizeof(SerializeFormat) + sizeof( uint64_t) + sizeof(_size) + sizeof(_nrUsed) +
                sizeof(_nrCuckood) + sizeof(_nrTotal) + sizeof(_niceSize) +
                sizeof(_logSize) + (_size * _slotSize * SlotsPerBucket)));

    // Insert the raw data
    // Size is as follows: nrOfBuckets * SlotsPerBucket * SlotSize
    TRI_ASSERT((_size * _slotSize * SlotsPerBucket) <= _allocSize);

    for (uint64_t i = 0; i < (_size * _slotSize * SlotsPerBucket) / sizeof(uint16_t);
         ++i) {
      *(reinterpret_cast<uint16_t*>(_base + i * 2)) = rocksutils::uint16FromPersistent(current + (i * sizeof(uint16_t)));
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
      for (size_t i = 0; i < SlotsPerBucket; ++i) {
        Slot f = findSlot(b, i);
        f.reset();
      }
    }
  }

  void deriveSizesAndAlloc() {
    _sizeMask = _niceSize - 1;
    _sizeShift = (64 - _logSize) / 2;
    _allocSize = _size * _slotSize * SlotsPerBucket +
                 64;  // give 64 bytes padding to enable 64-byte alignment
    _allocBase = new char[_allocSize];

    _base = reinterpret_cast<char*>(
        (reinterpret_cast<uintptr_t>(_allocBase) + 63) &
        ~((uintptr_t)0x3fu));  // to actually implement the 64-byte alignment,
                               // shift base pointer within allocated space to
                               // 64-byte boundary
  }

 private:               // member variables
  uint64_t _randState;  // pseudo random state for expunging

  size_t _slotSize;  // total size of a slot

  uint64_t _logSize;    // logarithm (base 2) of number of buckets
  uint64_t _size;       // actual number of buckets
  uint64_t _niceSize;   // smallest power of 2 at least number of buckets, ==
                        // 2^_logSize
  uint64_t _sizeMask;   // used to mask out some bits from the hash
  uint32_t _sizeShift;  // used to shift the bits down to get a position
  uint64_t _allocSize;  // number of allocated bytes,
                        // == _size * SlotsPerBucket * _slotSize + 64
  char* _base;          // pointer to allocated space, 64-byte aligned
  char* _allocBase;     // base of original allocation
  uint64_t _nrUsed;     // number of pairs stored in the table
  uint64_t _nrCuckood;  // number of elements that have been removed by cuckoo
  uint64_t _nrTotal;    // number of elements included in total
  unsigned _maxRounds;  // maximum number of cuckoo rounds on insertion

  HashKey _hasherKey;        // Instance to compute the first hash function
  Fingerprint _fingerprint;  // Instance to compute a fingerprint of a key
  HashShort _hasherShort;    // Instance to compute the second hash function

  arangodb::basics::ReadWriteLock mutable _bucketLock;
};

}  // namespace arangodb

#endif
