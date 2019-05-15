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

#include "AqlItemBlockHelper.h"

std::ostream& std::operator<<(
    std::ostream& out, ::arangodb::aql::AqlItemBlock const& block) {
  for (size_t i = 0; i < block.size(); i++) {
    for (arangodb::aql::RegisterCount j = 0; j < block.getNrRegs(); j++) {
      out << block.getValue(i, j).slice().toJson();
      if (j + 1 != block.getNrRegs()) out << ", ";
    }
    if (i + 1 != block.size()) out << std::endl;
  }

  out << std::endl;

  return out;
}
