////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STORAGE_ENGINE_REPLICATION_ITERATOR_H
#define ARANGOD_STORAGE_ENGINE_REPLICATION_ITERATOR_H 1

#include "Basics/Common.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/vocbase.h"

namespace arangodb {
class Index;
class LogicalCollection;

namespace transaction {
class Methods;
}

/// @brief a base class to iterate over the collection. An iterator is requested
/// at the collection itself
class ReplicationIterator {
 public:
  enum Ordering { Key, Revision };

  ReplicationIterator(ReplicationIterator const&) = delete;
  ReplicationIterator& operator=(ReplicationIterator const&) = delete;
  ReplicationIterator() = delete;

  ReplicationIterator(LogicalCollection&);

  virtual ~ReplicationIterator() = default;

  virtual Ordering order() const = 0;

  /// @brief return the underlying collection
  LogicalCollection& collection() const;

  virtual bool hasMore() const = 0;
  virtual void reset() = 0;

 protected:
  LogicalCollection& _collection;
};

class RevisionReplicationIterator : public ReplicationIterator {
 public:
  RevisionReplicationIterator(LogicalCollection&);

  Ordering order() const override;

  virtual RevisionId revision() const = 0;
  virtual VPackSlice document() const = 0;

  virtual void next() = 0;
  virtual void seek(RevisionId) = 0;
};

}  // namespace arangodb
#endif
