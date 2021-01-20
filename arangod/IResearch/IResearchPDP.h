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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_PDP_H
#define ARANGOD_IRESEARCH__IRESEARCH_PDP_H 1

#include "types.hpp"
#include "Basics/Result.h"

namespace iresearch {
class parametric_description;
} // iresearch

namespace arangodb {
namespace iresearch {

constexpr irs::byte_type MAX_LEVENSHTEIN_DISTANCE = 4;
constexpr irs::byte_type MAX_DAMERAU_LEVENSHTEIN_DISTANCE = 3;

const irs::parametric_description& getParametricDescription(
  irs::byte_type max_distance,
  bool with_transpositions);

} // iresearch
} // arangodb

#endif // ARANGOD_IRESEARCH__IRESEARCH_PDP_H

