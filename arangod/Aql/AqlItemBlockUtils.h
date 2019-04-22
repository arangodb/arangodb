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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQLITEMBLOCKUTILS_H
#define ARANGOD_AQL_AQLITEMBLOCKUTILS_H

#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb {
namespace aql {
class AqlItemBlockManager;
class BlockCollector;
class InputAqlItemRow;

namespace itemBlock {

/// @brief concatenate multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
SharedAqlItemBlockPtr concatenate(AqlItemBlockManager&,
                                  std::vector<SharedAqlItemBlockPtr>& blocks);

void forRowInBlock(SharedAqlItemBlockPtr const& block,
                   std::function<void(InputAqlItemRow&&)> const& callback);

}  // namespace itemBlock

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_AQLITEMBLOCKUTILS_H
