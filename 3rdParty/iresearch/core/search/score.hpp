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

#ifndef IRESEARCH_SCORE_H
#define IRESEARCH_SCORE_H

#include "sort.hpp"
#include "utils/attributes.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class score
/// @brief represents a score related for the particular document
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API score : public attribute {
 public:
  static constexpr string_ref type_name() noexcept {
    return "iresearch::score";
  }

  static const irs::score& no_score() noexcept;

  template<typename Provider>
  static const score& get(const Provider& attrs) {
    const auto* score = irs::get<irs::score>(attrs);
    return score ? *score : no_score();
  }

  score() noexcept;
  explicit score(const order::prepared& ord);

  bool empty() const noexcept;

  [[nodiscard]] const byte_type* evaluate() const {
    assert(func_);
    return (*func_)(ctx_.get());
  }

  void prepare(score_ctx_ptr&& ctx, const score_f func) noexcept {
    assert(func);
    ctx_ = memory::to_managed<score_ctx>(std::move(ctx));
    func_ = func;
  }

  void prepare(score_ctx* ctx, const score_f func) noexcept {
    assert(func);
    ctx_ = memory::to_managed<score_ctx, false>(ctx);
    func_ = func;
  }

  byte_type* realloc(const order::prepared& order) {
    buf_.resize(order.score_size());
    return const_cast<byte_type*>(buf_.data());
  }

  byte_type* data() const noexcept {
    return const_cast<byte_type*>(buf_.c_str());
  }

  size_t size() const noexcept {
    return buf_.size();
  }

  void clear() noexcept {
    assert(!buf_.empty());
    std::memset(const_cast<byte_type*>(buf_.data()), 0, buf_.size());
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring buf_;
  memory::managed_ptr<score_ctx> ctx_; // arbitrary scoring context
  score_f func_; // scoring function
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // score

IRESEARCH_API void prepare_score(
  irs::score& score, order::prepared::scorers&& scorers);

NS_END // ROOT

#endif // IRESEARCH_SCORE_H

