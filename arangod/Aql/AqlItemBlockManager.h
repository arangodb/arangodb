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

#ifndef ARANGOD_AQL_AQL_ITEM_BLOCK_MANAGER_H
#define ARANGOD_AQL_AQL_ITEM_BLOCK_MANAGER_H 1

#include "Basics/Common.h"
#include "Aql/types.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
struct ResourceMonitor;

class AqlItemBlockManager {
 public:
  /// @brief create the manager
  explicit AqlItemBlockManager(ResourceMonitor*);

  /// @brief destroy the manager
  ~AqlItemBlockManager();

 public:
  /// @brief request a block with the specified size
  AqlItemBlock* requestBlock(size_t, RegisterId);

  /// @brief return a block to the manager
  void returnBlock(AqlItemBlock*&);

 private:
  ResourceMonitor* _resourceMonitor;

  /// @brief last block handed back to the manager
  /// this is the block that may be recycled
  AqlItemBlock* _last;
};
}
}

#endif
