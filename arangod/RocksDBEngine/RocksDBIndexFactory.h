////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_INDEX_FACTORY_H
#define ARANGOD_ROCKSDB_ROCKSDB_INDEX_FACTORY_H 1

#include "Indexes/IndexFactory.h"

namespace arangodb {

class RocksDBIndexFactory final : public IndexFactory {
 public:
  RocksDBIndexFactory() : IndexFactory() {}

  ~RocksDBIndexFactory() {}

  int enhanceIndexDefinition(arangodb::velocypack::Slice const definition,
                             arangodb::velocypack::Builder& enhanced,
                             bool isCreation,
                             bool isCoordinator) const override;

  std::shared_ptr<arangodb::Index> prepareIndexFromSlice(
      arangodb::velocypack::Slice info, bool generateKey,
      LogicalCollection* col, bool isClusterConstructor) const override;

  void fillSystemIndexes(arangodb::LogicalCollection* col,
                         std::vector<std::shared_ptr<arangodb::Index>>&
                             systemIndexes) const override;

  std::vector<std::string> supportedIndexes() const override;
};
}

#endif
