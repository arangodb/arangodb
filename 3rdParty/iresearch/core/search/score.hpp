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
/// @author Vasiliy Nabatchikov
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
  typedef std::function<void(byte_type*)> score_f;

  DECLARE_ATTRIBUTE_TYPE();

  static const irs::score& no_score() NOEXCEPT;

  static const irs::score& extract(const attribute_view& attrs) NOEXCEPT {
    const irs::score* score = attrs.get<irs::score>().get();
    return score ? *score : no_score();
  }

  score() NOEXCEPT;

  const byte_type* c_str() const NOEXCEPT {
    return value_.c_str();
  }

  const bstring& value() const NOEXCEPT {
    return value_;
  }

  bool empty() const NOEXCEPT {
    return value_.empty();
  }

  void evaluate() const {
    assert(func_);
    func_(leak());
  }

  bool prepare(const order::prepared& ord, score_f&& func) {
    if (ord.empty()) {
      return false;
    }

    value_.resize(ord.score_size());
    ord.prepare_score(leak());

    func_ = std::move(func);
    return true;
  }

 private:
  byte_type* leak() const NOEXCEPT {
    return const_cast<byte_type*>(&(value_[0]));
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  bstring value_;
  score_f func_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // score

NS_END // ROOT

#endif // IRESEARCH_SCORE_H

