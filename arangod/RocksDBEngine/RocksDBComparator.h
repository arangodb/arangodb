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
/// @author Jan Steemann
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_COMPARATOR_H
#define ARANGO_ROCKSDB_ROCKSDB_COMPARATOR_H 1

#include "Basics/Common.h"

#include <rocksdb/comparator.h>
#include <rocksdb/slice.h>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBComparator final : public rocksdb::Comparator {
 public:
  RocksDBComparator();
  ~RocksDBComparator();

  int Compare(rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const;

  char const* Name() const;

  void FindShortestSeparator(std::string*, rocksdb::Slice const&) const {}
  void FindShortSuccessor(std::string*) const {}

 private:
  int compareType(rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const;
  int compareDatabases(rocksdb::Slice const& lhs,
                       rocksdb::Slice const& rhs) const;
  int compareCollections(rocksdb::Slice const& lhs,
                         rocksdb::Slice const& rhs) const;
  int compareIndices(rocksdb::Slice const& lhs,
                     rocksdb::Slice const& rhs) const;
  int compareDocuments(rocksdb::Slice const& lhs,
                       rocksdb::Slice const& rhs) const;
  int compareIndexValues(rocksdb::Slice const& lhs,
                         rocksdb::Slice const& rhs) const;
  int compareUniqueIndexValues(rocksdb::Slice const& lhs,
                               rocksdb::Slice const& rhs) const;
  int compareViews(rocksdb::Slice const& lhs, rocksdb::Slice const& rhs) const;
  int compareCrossReferences(rocksdb::Slice const& lhs,
                             rocksdb::Slice const& rhs) const;
  int compareCrossReferenceCollections(rocksdb::Slice const& lhs,
                                       rocksdb::Slice const& rhs) const;
  int compareCrossReferenceIndices(rocksdb::Slice const& lhs,
                                   rocksdb::Slice const& rhs) const;
  int compareCrossReferenceViews(rocksdb::Slice const& lhs,
                                 rocksdb::Slice const& rhs) const;
  int compareLexicographic(rocksdb::Slice const& lhs,
                           rocksdb::Slice const& rhs) const;

  int compareIndexedValues(VPackSlice const& lhs, VPackSlice const& rhs) const;
  VPackSlice extractIndexedValues(rocksdb::Slice const& key) const;

 private:
  const std::string _name;
};

}  // namespace arangodb

#endif
