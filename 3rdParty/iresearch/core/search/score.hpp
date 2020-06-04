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

  const byte_type* c_str() const noexcept {
    return value_.c_str();
  }

  const bstring& value() const noexcept {
    return value_;
  }

  bool empty() const noexcept {
    return value_.empty();
  }

  void evaluate() const {
    assert(func_);
    (*func_)(ctx_.get(), leak());
  }

  bool prepare(const order::prepared& ord,
               const score_ctx* ctx,
               const score_f func) {
    assert(func);

    if (ord.empty()) {
      return false;
    }

    value_.resize(ord.score_size());
    ord.prepare_score(leak());

    ctx_ = memory::managed_ptr<const score_ctx>(ctx, nullptr);
    func_ = func;
    return true;
  }

  bool prepare(const order::prepared& ord,
               order::prepared::scorers&& scorers);

 private:
  byte_type* leak() const noexcept {
    return const_cast<byte_type*>(&(value_[0]));
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring value_;     // score buffer
  memory::managed_ptr<const score_ctx> ctx_{}; // arbitrary scoring context
  score_f func_{[](const score_ctx*, byte_type*){}};    // scoring function
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // score

NS_END // ROOT

#endif // IRESEARCH_SCORE_H

