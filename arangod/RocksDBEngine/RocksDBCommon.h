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

#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>

namespace rocksdb {
class TransactionDB;
class DB;
struct ReadOptions;
class Comparator;
}

namespace arangodb {

class RocksDBOperationResult : public Result {
 public:
  explicit RocksDBOperationResult() : Result(), _keySize(0) {}

  RocksDBOperationResult(Result const& other)
      : _keySize(0) {
    cloneData(other);
  }

  RocksDBOperationResult(Result&& other) : _keySize(0) {
    cloneData(std::move(other));
  }

  uint64_t keySize() const { return _keySize; }
  void keySize(uint64_t s) { _keySize = s; }

 protected:
  uint64_t _keySize;
};

class TransactionState;
class RocksDBTransactionState;
class RocksDBMethods;
class RocksDBKeyBounds;
class RocksDBEngine;
namespace transaction {
class Methods;
}
namespace rocksutils {

//// to persistent
template <typename T>
typename std::enable_if<std::is_integral<T>::value,void>::type
toPersistent(T in, char*& out){
  in = basics::hostToLittle(in);
  using TT = typename std::decay<T>::type;
  std::memcpy(out, &in, sizeof(TT));
  out += sizeof(TT);
}

//// from persistent
template <typename T,
          typename std::enable_if<std::is_integral<typename std::remove_reference<T>::type>::value, int>::type = 0
         >
typename std::decay<T>::type fromPersistent(char const*& in){
  using TT = typename std::decay<T>::type;
  TT out;
  std::memcpy(&out, in, sizeof(TT));
  in += sizeof(TT);
  return basics::littleToHost(out);
}

//we need this overload or the template will match
template <typename T,
          typename std::enable_if<std::is_integral<typename std::remove_reference<T>::type>::value, int>::type = 1
         >
typename std::decay<T>::type fromPersistent(char *& in){
  using TT = typename std::decay<T>::type;
  TT out;
  std::memcpy(&out, in, sizeof(TT));
  in += sizeof(TT);
  return basics::littleToHost(out);
}

template <typename T, typename StringLike,
          typename std::enable_if<std::is_integral<typename std::remove_reference<T>::type>::value, int>::type = 2
         >
typename std::decay<T>::type fromPersistent(StringLike& in){
  using TT = typename std::decay<T>::type;
  TT out;
  std::memcpy(&out, in.data(), sizeof(TT));
  return basics::littleToHost(out);
}

inline uint64_t doubleToInt(double d){
  uint64_t i;
  std::memcpy(&i, &d, sizeof(i));
  return i;
}

inline double intToDouble(uint64_t i){
  double d;
  std::memcpy(&d, &i, sizeof(i));
  return d;
}

uint64_t uint64FromPersistent(char const* p);
void uint64ToPersistent(char* p, uint64_t value);
void uint64ToPersistent(std::string& out, uint64_t value);

uint32_t uint32FromPersistent(char const* p);
void uint32ToPersistent(char* p, uint32_t value);
void uint32ToPersistent(std::string& out, uint32_t value);

uint16_t uint16FromPersistent(char const* p);
void uint16ToPersistent(char* p, uint16_t value);
void uint16ToPersistent(std::string& out, uint16_t value);

RocksDBTransactionState* toRocksTransactionState(transaction::Methods* trx);
RocksDBMethods* toRocksMethods(transaction::Methods* trx);

rocksdb::TransactionDB* globalRocksDB();
RocksDBEngine* globalRocksEngine();
arangodb::Result globalRocksDBPut(
    rocksdb::Slice const& key, rocksdb::Slice const& value,
    rocksdb::WriteOptions const& = rocksdb::WriteOptions{});

arangodb::Result globalRocksDBRemove(
    rocksdb::Slice const& key,
    rocksdb::WriteOptions const& = rocksdb::WriteOptions{});

uint64_t latestSequenceNumber();

void addCollectionMapping(uint64_t, TRI_voc_tick_t, TRI_voc_cid_t);
std::pair<TRI_voc_tick_t, TRI_voc_cid_t> mapObjectToCollection(uint64_t);

/// Iterator over all keys in range and count them
std::size_t countKeyRange(rocksdb::DB*, rocksdb::ReadOptions const&,
                          rocksdb::ColumnFamilyHandle*, RocksDBKeyBounds const&);

/// @brief helper method to remove large ranges of data
/// Should mainly be used to implement the drop() call
Result removeLargeRange(rocksdb::TransactionDB* db,
                        RocksDBKeyBounds const& bounds);

std::vector<std::pair<RocksDBKey, RocksDBValue>> collectionKVPairs(
    TRI_voc_tick_t databaseId);
std::vector<std::pair<RocksDBKey, RocksDBValue>> viewKVPairs(
    TRI_voc_tick_t databaseId);

// optional switch to std::function to reduce amount of includes and to avoid
// template
// this helper is not meant for transactional usage!
template <typename T>  // T is a invokeable that takes a rocksdb::Iterator*
void iterateBounds(
    RocksDBKeyBounds const& bounds, T callback,
    rocksdb::ColumnFamilyHandle* handle,
    rocksdb::ReadOptions options = rocksdb::ReadOptions()) {
  rocksdb::Slice const end = bounds.end();
  options.iterate_upper_bound = &end;// save to use on rocksb::DB directly
  options.prefix_same_as_start = true;
  options.verify_checksums = false;
  std::unique_ptr<rocksdb::Iterator> it(globalRocksDB()->NewIterator(options, handle));
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    callback(it.get());
  }
}

}  // namespace rocksutils
}  // namespace arangodb

#endif
