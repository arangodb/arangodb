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

NS_END

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                                            score
// ----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(iresearch::score)

/*static*/ const irs::score& score::no_score() noexcept {
  return EMPTY_SCORE;
}

score::score() noexcept
  : func_([](const score_ctx*, byte_type*){}) {
}


bool score::prepare(const order::prepared& ord,
                    order::prepared::scorers&& scorers) {
  if (ord.empty()) {
    value_.resize(0);
    func_ = [](const score_ctx*, byte_type*){};

    return false;
  }

  value_.resize(ord.score_size());
  ord.prepare_score(leak());

  switch (scorers.size()) {
    case 0: {
      ctx_ = nullptr;
      func_ = [](const score_ctx*, byte_type*){};
    } break;
    case 1: {
      auto& scorer = scorers[0];

      if (scorer.offset) {
        struct ctx : score_ctx {
          ctx(score_f func, score_ctx_ptr&& context, size_t offset) noexcept
            : func(func), context(std::move(context)), offset(offset) {
          }

          score_f func;
          score_ctx_ptr context;
          size_t offset;
        };

        ctx_ = memory::make_managed<const score_ctx>(new ctx(scorer.func, std::move(scorer.ctx), scorer.offset));
        func_ = [](const score_ctx* ctx, byte_type* score) {
          auto& scorer = *static_cast<const struct ctx*>(ctx);
          (*scorer.func)(scorer.context.get(), score + scorer.offset);
        };
      } else {
        ctx_ = memory::make_managed<const score_ctx>(std::move(scorer.ctx));
        func_ = scorer.func;
      }
    } break;
    case 2: {
      struct ctx : score_ctx {
        explicit ctx(order::prepared::scorers&& scorers) noexcept
          : scorers(std::move(scorers)) {
        }

        order::prepared::scorers scorers;
      };

      const bool has_offset = bool(scorers[0].offset);
      ctx_ = memory::make_managed<const score_ctx>(new ctx(std::move(scorers)));

      if (has_offset) {
        func_ = [](const score_ctx* ctx, byte_type* score) {
          auto& scorers = static_cast<const struct ctx*>(ctx)->scorers;
          (*scorers[0].func)(scorers[0].ctx.get(), score + scorers[0].offset);
          (*scorers[1].func)(scorers[1].ctx.get(), score + scorers[1].offset);
        };
      } else {
         func_ = [](const score_ctx* ctx, byte_type* score) {
          auto& scorers = static_cast<const struct ctx*>(ctx)->scorers;
          (*scorers[0].func)(scorers[0].ctx.get(), score);
          (*scorers[1].func)(scorers[1].ctx.get(), score + scorers[1].offset);
        };
      }
    } break;
    default: {
      struct ctx : score_ctx {
        explicit ctx(order::prepared::scorers&& scorers) noexcept
          : scorers(std::move(scorers)) {
        }

        order::prepared::scorers scorers;
      };

      ctx_ = memory::make_managed<const score_ctx>(new ctx(std::move(scorers)));
      func_ = [](const score_ctx* ctx, byte_type* score) {
        auto& scorers = static_cast<const struct ctx*>(ctx)->scorers;
        scorers.score(score);
      };
    } break;
  }

  return true;
}

NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
