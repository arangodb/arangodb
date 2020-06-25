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
////////////////////////////////////////////////////////////////////////////////

#include "score.hpp"
#include "shared.hpp"

NS_LOCAL

const irs::score EMPTY_SCORE;

const irs::byte_type* no_score(irs::score_ctx*) noexcept {
  return irs::bytes_ref::EMPTY.c_str();
}

NS_END

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                            score
// ----------------------------------------------------------------------------

/*static*/ const irs::score& score::no_score() noexcept {
  return EMPTY_SCORE;
}

score::score() noexcept
  : func_(&::no_score) {
}

score::score(const order::prepared& ord)
  : buf_(ord.score_size(), 0),
    func_(&::no_score) {
}

bool score::empty() const noexcept {
  return func_ == &::no_score;
}

void prepare_score(irs::score& score, order::prepared::scorers&& scorers) {
  switch (scorers.size()) {
    case 0: {
      score.prepare(nullptr, &::no_score);
    } break;
    case 1: {
      auto& scorer = scorers.front();
      score.prepare(std::move(scorer.ctx), scorer.func);
    } break;
    case 2: {
      struct ctx : score_ctx {
        explicit ctx(order::prepared::scorers&& scorers) noexcept
          : scorers(std::move(scorers)) {
        }

        order::prepared::scorers scorers;
      };

      score.prepare(
        memory::make_unique<ctx>(std::move(scorers)),
        [](score_ctx* ctx) {
          auto& scorers = static_cast<struct ctx*>(ctx)->scorers;
          auto& front = scorers.front();
          auto& back = scorers.back();
          (*front.func)(front.ctx.get());
          (*back.func)(back.ctx.get());
          return scorers.score_buf();
      });
    } break;
    default: {
      struct ctx : score_ctx {
        explicit ctx(order::prepared::scorers&& scorers) noexcept
          : scorers(std::move(scorers)) {
        }

        order::prepared::scorers scorers;
      };

      score.prepare(
        memory::make_unique<ctx>(std::move(scorers)),
        [](score_ctx* ctx) {
          auto& scorers = static_cast<struct ctx*>(ctx)->scorers;
          return scorers.evaluate();
      });
    } break;
  }
}

NS_END // ROOT
