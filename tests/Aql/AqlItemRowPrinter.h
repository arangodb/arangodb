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

#ifndef TESTS_AQL_AQLITEMROWPRINTER_H
#define TESTS_AQL_AQLITEMROWPRINTER_H

#include <iosfwd>
#include <type_traits>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ShadowAqlItemRow;

template <class RowType, class = std::enable_if_t<std::is_same<RowType, InputAqlItemRow>::value || std::is_same<RowType, ShadowAqlItemRow>::value>>
std::ostream& operator<<(std::ostream& stream, RowType const& row);

}  // namespace aql
}  // namespace arangodb

#endif  // TESTS_AQL_AQLITEMROWPRINTER_H
