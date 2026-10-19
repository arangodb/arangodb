////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>

#include "Indexes/IndexType.h"

namespace arangodb {

class Index;
class LogicalCollection;
class IndexId;

namespace velocypack {
class Slice;
}  // namespace velocypack

// one switch-on-type entry point per engine, replacing a virtual per type
struct IIndexFactory {
  virtual ~IIndexFactory() = default;

  virtual std::shared_ptr<Index> createIndex(
      IndexType type, LogicalCollection& collection,
      velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const = 0;

  // the arangosearch link is added by a feature at startup (see
  // IndexFactory::setLinkCreator), not implemented by the engines directly -
  // this keeps RocksDBIndexFactory/ClusterIndexFactory free of a compile-time
  // dependency on iresearch
  virtual std::shared_ptr<Index> createIResearchLink(
      LogicalCollection& collection, velocypack::Slice definition, IndexId id,
      bool isClusterConstructor) const = 0;
};

}  // namespace arangodb
