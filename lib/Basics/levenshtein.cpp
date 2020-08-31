////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "levenshtein.h"

#include <algorithm>
#include <numeric>

////////////////////////////////////////////////////////////////////////////////
/// @brief calculate the levenshtein distance of the two strings
////////////////////////////////////////////////////////////////////////////////

int TRI_Levenshtein(std::string const& lhs, std::string const& rhs) {
  int const lhsLength = static_cast<int>(lhs.size());
  int const rhsLength = static_cast<int>(rhs.size());

  int* col = new int[lhsLength + 1];
  int start = 1;
  // fill with initial values
  std::iota(col + start, col + lhsLength + 1, start);

  for (int x = start; x <= rhsLength; ++x) {
    col[0] = x;
    int last = x - start;
    for (int y = start; y <= lhsLength; ++y) {
      int const save = col[y];
      col[y] = (std::min)({
          col[y] + 1,                                // deletion
          col[y - 1] + 1,                            // insertion
          last + (lhs[y - 1] == rhs[x - 1] ? 0 : 1)  // substitution
      });
      last = save;
    }
  }

  // fetch final value
  int result = col[lhsLength];
  // free memory
  delete[] col;

  return result;
}
