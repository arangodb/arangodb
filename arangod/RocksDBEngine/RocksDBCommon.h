////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_COMMON_H
#define ARANGO_ROCKSDB_ROCKSDB_COMMON_H 1

#include "Basics/Common.h"
#include "Basics/Endian.h"
#include "Basics/Result.h"
#include "Basics/RocksDBUtils.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>

#undef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
#ifdef TRI_UNALIGNED_ACCESS
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define TRI_USE_FAST_UNALIGNED_DATA_ACCESS
#endif
#endif

namespace rocksdb {
class TransactionDB;
class DB;
struct ReadOptions;
class Comparator;
class ColumnFamilyHandle;
}

namespace arangodb {

class RocksDBMethods;
class RocksDBKeyBounds;
class RocksDBEngine;
  
namespace rocksutils {

inline uint64_t doubleToInt(double d) {
  uint64_t i;
  std::memcpy(&i, &d, sizeof(i));
  return i;
}

inline double intToDouble(uint64_t i) {
  double d;
  std::memcpy(&d, &i, sizeof(i));
  return d;
}

template<typename T> 
inline T uintFromPersistent(char const* p) {
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  return *reinterpret_cast<T const*>(p);
#else
  T value = 0;
  T x = 0;
  uint8_t const* ptr = reinterpret_cast<uint8_t const*>(p);
  uint8_t const* end = ptr + sizeof(T);
  do {
    value += static_cast<T>(*ptr++) << x;
    x += 8;
  } while (ptr < end);
  return value;
#endif 
}

inline uint64_t uint64FromPersistent(char const* p) { 
  return uintFromPersistent<uint64_t>(p);
}

/// @brief encodes number little endian
template<typename T>
inline void uintToPersistent(char* p, T value) {
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  *reinterpret_cast<T*>(p) = value;
#else
  char* end = p + sizeof(T);
  do {
    *p++ = static_cast<uint8_t>(value & 0xffU);
    value >>= 8;
  } while (p < end);
#endif
}

inline void uint64ToPersistent(char* p, uint64_t value) {
  return uintToPersistent<uint64_t>(p, value);
}

template<typename T>
inline void uintToPersistent(std::string& p, T value) {
#ifdef TRI_USE_FAST_UNALIGNED_DATA_ACCESS
  p.append(reinterpret_cast<const char*>(&value), sizeof(T));
#else
  size_t len = 0;
  do {
    p.push_back(static_cast<char>(value & 0xffU));
    value >>= 8;
  } while (++len < sizeof(T));
#endif
}

inline void uint64ToPersistent(std::string& out, uint64_t value) {
  return uintToPersistent<uint64_t>(out, value);
}
  
// big endian string encoding to preserver proper ordering under
// a memcmp based comparison function
inline void uint64ToBigEndianPersistent(std::string& out, uint64_t value) {
  uintToPersistent<uint64_t>(out, basics::hostToBig(value));
}
inline uint64_t uint64FromBigEndianPersistent(char const* p) {
  return basics::bigToHost(uintFromPersistent<uint64_t>(p));
}

/// shorthand for rocksdb
rocksdb::TransactionDB* globalRocksDB();
rocksdb::ColumnFamilyHandle* defaultCF();
RocksDBEngine* globalRocksEngine();
arangodb::Result globalRocksDBPut(
    rocksdb::ColumnFamilyHandle *cf,
    rocksdb::Slice const& key, rocksdb::Slice const& value,
    rocksdb::WriteOptions const& = rocksdb::WriteOptions{});

arangodb::Result globalRocksDBRemove(
    rocksdb::ColumnFamilyHandle *cf,
    rocksdb::Slice const& key,
    rocksdb::WriteOptions const& = rocksdb::WriteOptions{});

uint64_t latestSequenceNumber();

std::pair<TRI_voc_tick_t, TRI_voc_cid_t> mapObjectToCollection(uint64_t);
std::tuple<TRI_voc_tick_t, TRI_voc_cid_t, TRI_idx_iid_t> mapObjectToIndex(uint64_t);

/// @brief count all keys in the given column family
std::size_t countKeys(rocksdb::DB*, rocksdb::ColumnFamilyHandle* cf);

/// @brief iterate over all keys in range and count them
std::size_t countKeyRange(rocksdb::DB*, RocksDBKeyBounds const&, 
                          bool prefix_same_as_start);

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
Result removeLargeRange(rocksdb::TransactionDB* db,
                        RocksDBKeyBounds const& bounds,
                        bool prefix_same_as_start);

// optional switch to std::function to reduce amount of includes and
// to avoid template
// this helper is not meant for transactional usage!
template <typename T>  // T is a invokeable that takes a rocksdb::Iterator*
void iterateBounds(RocksDBKeyBounds const& bounds, T callback,
                   rocksdb::ReadOptions options = rocksdb::ReadOptions()) {
  rocksdb::Slice const end = bounds.end();
  options.iterate_upper_bound = &end;  // save to use on rocksb::DB directly
  options.prefix_same_as_start = true;
  options.verify_checksums = false;
  std::unique_ptr<rocksdb::Iterator> it(
      globalRocksDB()->NewIterator(options, bounds.columnFamily()));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    callback(it.get());
  }
}

}  // namespace rocksutils
}  // namespace arangodb

#endif
