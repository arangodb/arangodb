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

#pragma once

#include "utils/memory.hpp"
#include "utils/noncopyable.hpp"

namespace irs {

// Stateful object used for computing the document score
// based on the stored state.
struct score_ctx {
  score_ctx() = default;
  score_ctx(score_ctx&&) = default;
  score_ctx& operator=(score_ctx&&) = default;

 protected:
  ~score_ctx() = default;
};

// Convenient wrapper around score_ctx, score_f and min_f.
class ScoreFunction : util::noncopyable {
  using score_f = void (*)(score_ctx* ctx, score_t* res) noexcept;
  using min_f = void (*)(score_ctx* ctx, score_t min) noexcept;

  using deleter_f = void (*)(score_ctx* ctx) noexcept;
  static void Noop(score_ctx* /*ctx*/) noexcept {}

 public:
  // TODO(MBkkt) Default score probably should do nothing instead of set 0
  static void DefaultScore(score_ctx* ctx, score_t* res) noexcept {
    IRS_ASSERT(res != nullptr);
    const auto size = reinterpret_cast<size_t>(ctx);
    std::memset(res, 0, size);
  }
  static void DefaultMin(score_ctx* /*ctx*/, score_t /*min*/) noexcept {}

  // Returns default scoring function setting `size` score buckets to 0.
  static ScoreFunction Default(size_t count) noexcept {
    static_assert(sizeof(score_ctx*) == sizeof(size_t));
    // write byte size instead of count to avoid multiply in DefaultScore call
    count *= sizeof(score_t);
    return {reinterpret_cast<score_ctx*>(count), DefaultScore, DefaultMin,
            Noop};
  }

  // Returns scoring function setting a single score bucket to `value`.
  static ScoreFunction Constant(score_t value) noexcept;
  // Returns scoring function setting `size` score buckets to `value`.
  static ScoreFunction Constant(score_t value, uint32_t count) noexcept;

  template<typename T, typename... Args>
  static auto Make(score_f score, min_f min, Args&&... args) {
    return ScoreFunction{
      new T{std::forward<Args>(args)...}, score, min,
      [](score_ctx* ctx) noexcept { delete static_cast<T*>(ctx); }};
  }

  ScoreFunction() noexcept = default;
  ScoreFunction(score_ctx& ctx, score_f score, min_f min = DefaultMin) noexcept
    : ScoreFunction{&ctx, score, min, Noop} {}
  ScoreFunction(ScoreFunction&& rhs) noexcept
    : ScoreFunction{std::exchange(rhs.ctx_, nullptr),
                    std::exchange(rhs.score_, DefaultScore),
                    std::exchange(rhs.min_, DefaultMin),
                    std::exchange(rhs.deleter_, Noop)} {}
  ScoreFunction& operator=(ScoreFunction&& rhs) noexcept {
    if (IRS_LIKELY(this != &rhs)) {
      std::swap(ctx_, rhs.ctx_);
      std::swap(score_, rhs.score_);
      std::swap(min_, rhs.min_);
      std::swap(deleter_, rhs.deleter_);
    }
    return *this;
  }
  ~ScoreFunction() noexcept { deleter_(ctx_); }

  void Reset(score_ctx& ctx, score_f score, min_f min = DefaultMin) noexcept {
    IRS_ASSERT(&ctx != ctx_ || deleter_ == Noop);
    deleter_(ctx_);
    ctx_ = &ctx;
    score_ = score;
    min_ = min;
    deleter_ = Noop;
  }

  bool IsDefault() const noexcept { return score_ == DefaultScore; }

  IRS_FORCE_INLINE void Score(score_t* res) const noexcept {
    IRS_ASSERT(score_ != nullptr);
    score_(ctx_, res);
  }

  IRS_FORCE_INLINE void Min(score_t arg) const noexcept {
    IRS_ASSERT(min_ != nullptr);
    min_(ctx_, arg);
  }

  score_t Max() const noexcept;

  // TODO(MBkkt) Remove it, use Score
  IRS_FORCE_INLINE void operator()(score_t* res) const noexcept { Score(res); }

  bool operator==(const ScoreFunction& rhs) const noexcept {
    return ctx_ == rhs.ctx_ && score_ == rhs.score_ && min_ == rhs.min_;
  }

#ifdef IRESEARCH_TEST
  [[nodiscard]] score_ctx* Ctx() const noexcept { return ctx_; }
  [[nodiscard]] score_f Func() const noexcept { return score_; }
#endif

 private:
  ScoreFunction(score_ctx* ctx, score_f score, min_f min,
                deleter_f deleter) noexcept
    : ctx_{ctx}, score_{score}, min_{min}, deleter_{deleter} {}

  score_ctx* ctx_{nullptr};
  score_f score_{DefaultScore};
  min_f min_{DefaultMin};
  deleter_f deleter_{Noop};
};

}  // namespace irs
