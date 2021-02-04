////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_LEVENSHTEIN_DEFAULT_PDP_H
#define IRESEARCH_LEVENSHTEIN_DEFAULT_PDP_H

#include "shared.hpp"

namespace iresearch {

class parametric_description;

////////////////////////////////////////////////////////////////////////////////
/// @brief default parametric description provider, returns parametric
///        description according to specified arguments.
/// @note supports building descriptions for distances in range of [0..4] with
///       the exception for distance 4: can only build description wihtout
///       transpositions
////////////////////////////////////////////////////////////////////////////////
IRESEARCH_API const parametric_description& default_pdp(
  byte_type max_distance,
  bool with_transpositions);

}

#endif // IRESEARCH_LEVENSHTEIN_DEFAULT_PDP_H
