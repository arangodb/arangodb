////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <variant>

#include "Inspection/Types.h"

namespace arangodb::sepp {

struct StoppingCriterion {
  struct Runtime {
    std::uint64_t ms;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, Runtime& o) {
      return f.apply(o.ms);
    }
  };

  struct NumberOfOperations {
    std::uint64_t count;

    template<class Inspector>
    friend inline auto inspect(Inspector& f, NumberOfOperations& o) {
      return f.apply(o.count);
    }
  };

  using type = std::variant<Runtime, NumberOfOperations>;

  template<class Inspector>
  friend inline auto inspect(Inspector& f, StoppingCriterion::type& o) {
    namespace insp = arangodb::inspection;
    return f.variant(o).unqualified().alternatives(
        insp::type<Runtime>("runtime"),
        insp::type<NumberOfOperations>("operations"));
  }
};
}  // namespace arangodb::sepp