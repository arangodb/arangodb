////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include "Basics/Result.h"
#include "Utils/OperationResult.h"

#include "Futures/Future.h"

namespace arangodb {
namespace futures {
// Instantiate the most common Future types to save compile time
template class Future<Unit>;
template class Future<bool>;
template class Future<int32_t>;
template class Future<uint32_t>;
template class Future<int64_t>;
template class Future<uint64_t>;
template class Future<std::string>;
template class Future<double>;

// arangodb types
template class Future<arangodb::Result>;
template class Future<arangodb::OperationResult>;
  
/// Make a complete void future
Future<Unit> makeFuture() {
  return Future<Unit>(unit);
}
}  // namespace futures
}  // namespace arangodb
