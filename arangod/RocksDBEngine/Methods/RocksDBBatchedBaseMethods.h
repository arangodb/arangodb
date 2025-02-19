////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Exceptions.h"
#include "RocksDBEngine/RocksDBMethods.h"

namespace arangodb {
class RocksDBMethodsMemoryTracker;

class RocksDBBatchedBaseMethods : public RocksDBMethods {
 public:
  explicit RocksDBBatchedBaseMethods(
      RocksDBMethodsMemoryTracker& memoryTracker);

  void resetMemoryUsage() noexcept;

 protected:
  /// @brief returns the payload size of the transaction's WriteBatch. this
  /// excludes locks and any potential indexes (i.e. WriteBatchWithIndex).
  virtual size_t currentWriteBatchSize() const noexcept = 0;

  /// @brief object used for tracking memory usage
  RocksDBMethodsMemoryTracker& _memoryTracker;
};

}  // namespace arangodb
