////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "MMFilesIndex.h"

#include "Basics/LocalTaskQueue.h"

using namespace arangodb;

void MMFilesIndex::batchInsert(transaction::Methods& trx,
                        std::vector<std::pair<LocalDocumentId, arangodb::velocypack::Slice>> const& documents,
                        std::shared_ptr<arangodb::basics::LocalTaskQueue> queue) {
  for (auto const& it : documents) {
    Result status = insert(trx, it.first, it.second, OperationMode::normal);
    if (status.errorNumber() != TRI_ERROR_NO_ERROR) {
      queue->setStatus(status.errorNumber());
      break;
    }
  }
}
