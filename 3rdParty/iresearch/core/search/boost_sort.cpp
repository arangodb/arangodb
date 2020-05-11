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

#include "boost_sort.hpp"

NS_LOCAL

using namespace irs;

sort::ptr make_json(const string_ref& /*args*/) {
  return memory::make_shared<boost_sort>();
}

struct boost_score_ctx : score_ctx {
  explicit boost_score_ctx(boost_t boost)
    : boost(boost) {
  }

  boost_t boost;
};

struct volatile_boost_score_ctx : boost_score_ctx {
  explicit volatile_boost_score_ctx(
      const filter_boost* volatile_boost,
      boost_t boost) noexcept
    : boost_score_ctx(boost),
      volatile_boost(volatile_boost) {
    assert(volatile_boost);
  }

  const filter_boost* volatile_boost;
};

struct prepared final : prepared_sort_basic<boost_t> {
  const irs::flags& features() const override {
    return irs::flags::empty_instance();
  }

  std::pair<irs::score_ctx_ptr, irs::score_f> prepare_scorer(
      const irs::sub_reader&,
      const irs::term_reader&,
      const irs::byte_type*,
      const irs::attribute_provider& attrs,
      irs::boost_t boost) const override {

    auto* volatile_boost = irs::get<irs::filter_boost>(attrs);

    if (!volatile_boost) {
      return {
        irs::score_ctx_ptr(new boost_score_ctx(boost)), // FIXME can avoid allocation
        [](const irs::score_ctx* ctx, irs::byte_type* score) noexcept {
          auto& state = *reinterpret_cast<const boost_score_ctx*>(ctx);
          sort::score_cast<boost_t>(score) = state.boost;
        }
      };
    }

    return {
      irs::score_ctx_ptr(new volatile_boost_score_ctx(volatile_boost, boost)), // FIXME can avoid allocation
      [](const irs::score_ctx* ctx, irs::byte_type* score) noexcept {
        auto& state = *reinterpret_cast<const volatile_boost_score_ctx*>(ctx);
        sort::score_cast<boost_t>(score) = state.volatile_boost->value*state.boost;
      }
    };
  }
};

NS_END

NS_ROOT

DEFINE_FACTORY_DEFAULT(irs::boost_sort);

/*static*/ void boost_sort::init() {
  REGISTER_SCORER_JSON(boost_sort, make_json);
}

boost_sort::boost_sort() noexcept
  : sort(irs::type<boost_sort>::get()) {
}

sort::prepared::ptr boost_sort::prepare() const {
  // FIXME can avoid allocation
  return memory::make_unique<::prepared>();
}

NS_END
