////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_CLUSTER_CLUSTER_TRANSACTION_CONTEXT_DATA_H
#define ARANGOD_CLUSTER_CLUSTER_TRANSACTION_CONTEXT_DATA_H 1

#include "Basics/Common.h"
#include "Transaction/ContextData.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;

/// @brief transaction type
class ClusterTransactionContextData final : public transaction::ContextData {
 public:
  ClusterTransactionContextData() = default;
  ~ClusterTransactionContextData() = default;

  /// @brief pin data for the collection
  /// there is nothing to do for the RocksDB engine
  void pinData(arangodb::LogicalCollection*) override {}

  /// @brief whether or not the data for the collection is pinned
  /// note that this is always true in RocksDB
  bool isPinned(TRI_voc_cid_t) const override { return true; }
};
}

#endif
