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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "search/score_function.hpp"

#include <absl/base/casts.h>

namespace irs {
namespace {

void Constant1(score_ctx* ctx, score_t* res) noexcept {
  IRS_ASSERT(res != nullptr);
  const auto boost = reinterpret_cast<uintptr_t>(ctx);
  std::memcpy(res, &boost, sizeof(score_t));
}

struct ConstantCtx {
  score_t value;
  uint32_t count;
};

void ConstantN(score_ctx* ctx, score_t* res) noexcept {
  IRS_ASSERT(res != nullptr);
  const auto score_ctx = absl::bit_cast<ConstantCtx>(ctx);
  std::fill_n(res, score_ctx.count, score_ctx.value);
}

}  // namespace

ScoreFunction ScoreFunction::Constant(score_t value) noexcept {
  static_assert(sizeof(score_t) <= sizeof(uintptr_t));
  uintptr_t boost = 0;
  std::memcpy(&boost, &value, sizeof(score_t));
  static_assert(sizeof(score_ctx*) == sizeof(uintptr_t));
  return {reinterpret_cast<score_ctx*>(boost), Constant1, DefaultMin, Noop};
}

ScoreFunction ScoreFunction::Constant(score_t value, uint32_t count) noexcept {
  if (0 == count) {
    return {};
  } else if (1 == count) {
    return Constant(value);
  } else {
    return {absl::bit_cast<score_ctx*>(ConstantCtx{value, count}), ConstantN,
            DefaultMin, Noop};
  }
}

score_t ScoreFunction::Max() const noexcept {
  if (score_ == ScoreFunction::DefaultScore) {
    return 0.f;
  } else if (score_ == Constant1 || score_ == ConstantN) {
    score_t score;
    Score(&score);
    return score;
  }
  return std::numeric_limits<score_t>::max();
}

}  // namespace irs
