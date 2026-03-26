////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "analysis/token_attributes.hpp"
#include "filter.hpp"
#include "types.hpp"
#include "utils/type_id.hpp"

#include <absl/container/flat_hash_map.h>

namespace irs {

struct proxy_query_cache;

// Proxy filter designed to cache results of underlying real filter and
// provide fast replaying same filter on consequent calls.
// It is up to caller to control validity of the supplied cache. If the index
// was changed caller must discard cache and request a new one via make_cache.
// Scoring cache is not supported yet.
class proxy_filter final : public filter {
 public:
  using cache_ptr = std::shared_ptr<proxy_query_cache>;

  prepared::ptr prepare(const PrepareContext& ctx) const final;

  template<typename Impl, typename Base = Impl, typename... Args>
  std::pair<Base&, cache_ptr> set_filter(IResourceManager& memory,
                                         Args&&... args) {
    static_assert(std::is_base_of_v<filter, Base>);
    static_assert(std::is_base_of_v<Base, Impl>);
    auto& real =
      cache_filter(memory, std::make_unique<Impl>(std::forward<Args>(args)...));
    return {DownCast<Base>(real), cache_};
  }

  void set_cache(cache_ptr cache) noexcept { cache_ = std::move(cache); }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<proxy_filter>::id();
  }

 private:
  filter& cache_filter(IResourceManager& memory, filter::ptr&& ptr);

  mutable cache_ptr cache_;
};

}  // namespace irs
