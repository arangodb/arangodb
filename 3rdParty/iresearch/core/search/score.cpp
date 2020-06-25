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

using namespace irs;

const score EMPTY_SCORE;

const byte_type* default_score(score_ctx* ctx) noexcept {
  return reinterpret_cast<byte_type*>(ctx);
}

memory::managed_ptr<score_ctx> to_managed(byte_type* value) noexcept {
  return memory::to_managed<score_ctx, false>(reinterpret_cast<score_ctx*>(value));
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
  : ctx_(to_managed(data())),
    func_(&::default_score) {
}

score::score(const order::prepared& ord)
  : buf_(ord.score_size(), 0),
    ctx_(to_managed(data())),
    func_(&::default_score) {
}

bool score::is_default() const noexcept {
  return reinterpret_cast<score_ctx*>(data()) == ctx_.get()
    && func_ == &::default_score;
}

void score::reset() noexcept {
  ctx_ = to_managed(data());
  func_ = &::default_score;
}

void reset(irs::score& score, order::prepared::scorers&& scorers) {
  switch (scorers.size()) {
    case 0: {
      score.reset();
    } break;
    case 1: {
      auto& scorer = scorers.front();
      score.reset(std::move(scorer.ctx), scorer.func);
    } break;
    case 2: {
      struct ctx : score_ctx {
        explicit ctx(order::prepared::scorers&& scorers) noexcept
          : scorers(std::move(scorers)) {
        }

        order::prepared::scorers scorers;
      };

      score.reset(
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

      score.reset(
        memory::make_unique<ctx>(std::move(scorers)),
        [](score_ctx* ctx) {
          auto& scorers = static_cast<struct ctx*>(ctx)->scorers;
          return scorers.evaluate();
      });
    } break;
  }
}

NS_END // ROOT
