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

namespace {

using namespace irs;

sort::ptr make_json(const string_ref& /*args*/) {
  return memory::make_unique<boost_sort>();
}

struct volatile_boost_score_ctx : score_ctx {
  volatile_boost_score_ctx(
      byte_type* score_buf,
      const filter_boost* volatile_boost,
      boost_t boost) noexcept
    : score_buf(score_buf),
      boost(boost),
      volatile_boost(volatile_boost) {
    assert(volatile_boost);
  }

  byte_type* score_buf;
  boost_t boost;
  const filter_boost* volatile_boost;
};

struct prepared final : prepared_sort_basic<boost_t> {
  const irs::flags& features() const override {
    return irs::flags::empty_instance();
  }

  score_function prepare_scorer(
      const sub_reader&,
      const term_reader&,
      const byte_type*,
      byte_type* score_buf,
      const irs::attribute_provider& attrs,
      irs::boost_t boost) const override {
    auto* volatile_boost = irs::get<irs::filter_boost>(attrs);

    if (!volatile_boost) {
      sort::score_cast<boost_t>(score_buf) = boost;

      return {
        reinterpret_cast<score_ctx*>(score_buf),
        [](irs::score_ctx* ctx) noexcept -> const byte_type* {
          return reinterpret_cast<byte_type*>(ctx);
        }
      };
    }

    return {
      memory::make_unique<volatile_boost_score_ctx>(score_buf, volatile_boost, boost),
      [](irs::score_ctx* ctx) noexcept -> const byte_type* {
        auto& state = *reinterpret_cast<volatile_boost_score_ctx*>(ctx);
        sort::score_cast<boost_t>(state.score_buf) = state.volatile_boost->value*state.boost;

        return state.score_buf;
      }
    };
  }
};

}

namespace iresearch {

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

}
