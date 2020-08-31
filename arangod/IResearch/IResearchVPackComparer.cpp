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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchVPackComparer.h"
#include "IResearchViewMeta.h"

#include "Basics/VelocyPackHelper.h"

namespace  {

////////////////////////////////////////////////////////////////////////////////
/// @brief reverse multiplier to use in VPackComparer
////////////////////////////////////////////////////////////////////////////////
constexpr const int MULTIPLIER[] { -1, 1 };

}

namespace arangodb {
namespace iresearch {

VPackComparer::VPackComparer()
  : VPackComparer(IResearchViewMeta::DEFAULT()._primarySort) {
}

bool VPackComparer::less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const {
  TRI_ASSERT(_sort);
  TRI_ASSERT(_sort->size() >= _size);
  TRI_ASSERT(!lhs.empty());
  TRI_ASSERT(!rhs.empty());

  VPackSlice lhsSlice(lhs.c_str());
  VPackSlice rhsSlice(rhs.c_str());

  for (size_t i = 0; i < _size; ++i) {
    TRI_ASSERT(!lhsSlice.isNone());
    TRI_ASSERT(!rhsSlice.isNone());

    auto const res = arangodb::basics::VelocyPackHelper::compare(lhsSlice, rhsSlice, true);

    if (res) {
      return (MULTIPLIER[size_t(_sort->direction(i))] * res) < 0;
    }

    // move to the next value
    lhsSlice = VPackSlice(lhsSlice.start() + lhsSlice.byteSize());
    rhsSlice = VPackSlice(rhsSlice.start() + rhsSlice.byteSize());
  }

  return false;
}

} // iresearch
} // arangodb
