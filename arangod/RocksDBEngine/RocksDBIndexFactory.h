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
  explicit RocksDBIndexFactory(application_features::ApplicationServer&);
  ~RocksDBIndexFactory() = default;

  /// @brief index name aliases (e.g. "persistent" => "hash", "skiplist" => "hash")
  /// used to display storage engine capabilities
  std::unordered_map<std::string, std::string> indexAliases() const override;

  /// @brief create initial system indexes
  void fillSystemIndexes(arangodb::LogicalCollection& col,
                         std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const override;

  /// @brief create indexes from a list of index definitions
  void prepareIndexes(LogicalCollection& col, arangodb::velocypack::Slice const& indexesSlice,
                      std::vector<std::shared_ptr<arangodb::Index>>& indexes) const override;
};

}  // namespace arangodb

#endif
