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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "NumberUtils.h"
#include "Basics/debugging.h"

namespace arangodb {
namespace NumberUtils {

/// @brief calculate the integer log2 value for the given input
/// the result is undefined when calling this with a value of 0!
uint32_t log2(uint32_t value) noexcept {
  TRI_ASSERT(value > 0);

#if (defined(__GNUC__) || defined(__clang__))
  return (8 * sizeof(unsigned long)) -
         static_cast<uint32_t>(
             __builtin_clzl(static_cast<unsigned long>(value))) -
         1;
#else
  static_assert(false, "no known way of computing log2");
#endif
}

}  // namespace NumberUtils
}  // namespace arangodb
