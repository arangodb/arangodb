////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <rocksdb/comparator.h>
#include <rocksdb/slice.h>

namespace arangodb {

class RocksDBVPackComparator final : public rocksdb::Comparator {
 public:
  RocksDBVPackComparator() = default;
  ~RocksDBVPackComparator() = default;

  /// @brief Compares any two RocksDB keys.
  /// returns  < 0 if lhs < rhs
  ///          > 0 if lhs > rhs
  ///            0 if lhs == rhs
  int Compare(rocksdb::Slice const& lhs,
              rocksdb::Slice const& rhs) const override {
    return compareIndexValues(lhs, rhs);
  }

  bool Equal(rocksdb::Slice const& lhs,
             rocksdb::Slice const& rhs) const override {
    return (compareIndexValues(lhs, rhs) == 0);
  }

  // SECTION: API compatibility
  char const* Name() const override { return "RocksDBVPackComparator"; }
  void FindShortestSeparator(std::string*,
                             rocksdb::Slice const&) const override {}
  void FindShortSuccessor(std::string*) const override {}

 private:
  /// @brief Compares two IndexValue keys or two UniqueIndexValue keys
  /// (containing VelocyPack data and more).
  int compareIndexValues(rocksdb::Slice const& lhs,
                         rocksdb::Slice const& rhs) const;
};

}  // namespace arangodb
