////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"

#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Context.h"
#include "Transaction/StandaloneContext.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb::replication2::replicated_state::document {
SnapshotIterator::SnapshotIterator(
    std::shared_ptr<LogicalCollection> logicalCollection)
    : _logicalCollection(std::move(logicalCollection)),
      _ctx(transaction::StandaloneContext::Create(
          _logicalCollection->vocbase())),
      _trx(_ctx, *_logicalCollection, AccessMode::Type::READ) {
  // We call begin here so that rocksMethods are initialized
  if (auto res = _trx.begin(); res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto* physicalCollection = _logicalCollection->getPhysical();
  if (physicalCollection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                                   _logicalCollection->name());
  }

  _it = physicalCollection->getReplicationIterator(
      ReplicationIterator::Ordering::Revision, _trx);
}

auto SnapshotIterator::next() -> Snapshot {
  if (!_it->hasMore()) {
    return Snapshot{};
  }
  std::size_t batchSize{0};
  VPackBuilder builder;
  builder.openArray();
  for (auto& revIterator = dynamic_cast<RevisionReplicationIterator&>(*_it);
       revIterator.hasMore() && batchSize < kBatchSizeLimit;
       revIterator.next()) {
    auto slice = revIterator.document();
    batchSize += slice.byteSize();
    builder.add(slice);
  }
  builder.close();
  return Snapshot{builder.sharedSlice()};
}
}  // namespace arangodb::replication2::replicated_state::document
