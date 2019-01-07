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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_MMFILES_TRANSACTION_CONTEXT_DATA_H
#define ARANGOD_MMFILES_TRANSACTION_CONTEXT_DATA_H 1

#include "Basics/Common.h"
#include "Transaction/ContextData.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class LogicalCollection;
class MMFilesDocumentDitch;

/// @brief transaction type
class MMFilesTransactionContextData final : public transaction::ContextData {
 public:
  MMFilesTransactionContextData();
  ~MMFilesTransactionContextData();

  /// @brief pin data for the collection
  void pinData(arangodb::LogicalCollection*) override;

  /// @brief whether or not the data for the collection is pinned
  bool isPinned(TRI_voc_cid_t) const override;

 private:
  std::unordered_map<TRI_voc_cid_t, MMFilesDocumentDitch*> _ditches;

  TRI_voc_cid_t _lastPinnedCid;
};

}  // namespace arangodb

#endif
