////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "score.hpp"

NS_LOCAL

const irs::score EMPTY_SCORE;

NS_END

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                            score
// ----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(iresearch::score)

/*static*/ const irs::score& score::no_score() NOEXCEPT {
  return EMPTY_SCORE;
}

score::score() NOEXCEPT
  : func_([](byte_type*){}) {
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------