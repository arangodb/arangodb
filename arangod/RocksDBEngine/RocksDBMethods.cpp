////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBMethods.h"

#include <algorithm>
#include <string>

namespace arangodb {
namespace {
// size of std::string's internal SSO buffer.. only strings
// that exceed this buffer require a dynamic memory allocation.
// note: the size of the SSO buffer is implementation-defined
// and is not guaranteed to be 15. we checked the SSO buffer
// size for all current relevant implementation though:
// - g++-11: 15
// - g++-12: 15
// - MSVC: 15
// - clang++-14 (libstdc++): 15
// - clang++-14 (libc++): 22
// TODO: replace this constant with the expression
//   std::string{}.capacity();
// once all compilers provide constexpr std::string::capacity
// implementations (clang 14 does, gcc 12 does, but gcc 11 doesn't).
constexpr size_t sizeOfString = sizeof(std::string);
constexpr size_t stringInlineBufferSize = (sizeOfString == 24 ? 22 : 15);
static_assert(stringInlineBufferSize > 0);
static_assert(stringInlineBufferSize < sizeof(std::string));
}  // namespace

size_t RocksDBMethods::indexingOverhead(bool indexingEnabled,
                                        size_t keySize) noexcept {
  if (!indexingEnabled) {
    return 0;
  }
  return indexingOverhead(keySize);
}

size_t RocksDBMethods::indexingOverhead(size_t keySize) noexcept {
  // it is ok to refer to keysize here directly, because keys are not stored as
  // std::strings inside memtables or WriteBatch objects.
  return keySize + fixedIndexingEntryOverhead;
}

size_t RocksDBMethods::lockOverhead(bool lockingEnabled,
                                    size_t keySize) noexcept {
  if (!lockingEnabled) {
    return 0;
  }
  // assumed overhead of the lock we acquired. note that RocksDB does not
  // report back here whether the current transaction had already
  // acquired the lock before. in that case it will still return ok().
  // because we do not want to track the acquired locks here in addition,
  // we simply assume here that for every invocation of this function we
  // acquire an additional lock.
  // each lock entry contains at least the string with the key. the
  // string may use SSO to store the key, but we don't want to dive into
  // the internals of std::string here. for storing the key, we assume
  // that we need to store at least the size of an std::string, or the
  // size of the key, whatever is larger. as locked keys are stored in a
  // hash table, we also need to assume overhead (as the hash table will
  // always have a load factor < 100%).
  if (keySize > stringInlineBufferSize) {
    // std::string will make a dynamic memory allocation, so we will have
    // - the size of an std::string
    // - the memory required to hold the key
    // - plus one byte for the NUL terminator
    // - 8 (assumed) extra bytes for the overhead of a dynamic allocation
    // - some (assumed) fixed overhead for each lock entry
    return sizeof(std::string) + keySize + 1 + memoryAllocationOverhead +
           fixedLockEntryOverhead;
  }
  return sizeof(std::string) + fixedLockEntryOverhead;
}

}  // namespace arangodb
