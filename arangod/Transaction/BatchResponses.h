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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_TRANSACTION_BATCHRESPONSES_H
#define ARANGOD_TRANSACTION_BATCHRESPONSES_H

#include <stdint.h>
#include <memory>

#include <Basics/Result.h>
#include <Utils/OperationOptions.h>
#include <Utils/OperationResult.h>
#include <VocBase/ManagedDocumentResult.h>
#include <velocypack/Buffer.h>

namespace arangodb {
namespace velocypack {
template <typename T>
class Buffer;
}

namespace batch {

using VPackBufferPtr = std::shared_ptr<velocypack::Buffer<uint8_t>>;

struct SingleDocRemoveResponse {
  Result result;
  std::string key;
  TRI_voc_rid_t rid;
  std::unique_ptr<ManagedDocumentResult> old;
};

struct RemoveResponse {
  // Real collection name (never shard name)
  std::string collection;
  std::deque<SingleDocRemoveResponse> data;

  OperationResult moveToOperationResult(OperationOptions const& options,
                                        bool isBabies);

 private:
  SingleDocRemoveResponse popFrontData();
};
}
}

#endif  // ARANGOD_TRANSACTION_BATCHRESPONSES_H
