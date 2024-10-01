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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>
#include <memory>

namespace arangodb {
template<class Lambda>
struct lazyConstruct {
  using type = std::invoke_result_t<const Lambda&>;
  constexpr lazyConstruct(Lambda&& factory) : factory_(std::move(factory)) {}
  constexpr operator type() const
      noexcept(std::is_nothrow_invocable_v<const Lambda&>) {
    return factory_();
  }
  Lambda factory_;
};
}  // namespace arangodb
