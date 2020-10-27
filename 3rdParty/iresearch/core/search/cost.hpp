////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COST_H
#define IRESEARCH_COST_H

#include <functional>

#include "utils/attribute_provider.hpp"
#include "utils/attributes.hpp"

namespace iresearch {

//////////////////////////////////////////////////////////////////////////////
/// @class cost
/// @brief represents an estimated cost of the query execution
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API cost final : public attribute {
 public:
  using cost_t = uint64_t;
  using cost_f = std::function<cost_t()> ;

  static constexpr string_ref type_name() noexcept {
    return "iresearch::cost";
  }

  static constexpr cost_t MAX = integer_traits<cost_t>::const_max;

  cost() = default;

  explicit cost(cost_t value) noexcept
    : value_(value),
      init_(true) {
  }

  explicit cost(cost_f&& func) noexcept(std::is_nothrow_move_constructible_v<cost_f>)
    : func_(std::move(func)),
      init_(false) {
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @returns a value of the "cost" attribute in the specified "src"
  /// collection, or "def" value if there is no "cost" attribute in "src"
  //////////////////////////////////////////////////////////////////////////////
  template<typename Provider>
  static cost_t extract(const Provider& src, cost_t def = MAX) noexcept {
    cost::cost_t est = def;
    auto* attr = irs::get<cost>(src);
    if (attr) {
      est = attr->estimate();
    }
    return est;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the estimation value 
  //////////////////////////////////////////////////////////////////////////////
  void value(cost_t value) noexcept {
    value_ = value;
    init_ = true;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the estimation rule
  //////////////////////////////////////////////////////////////////////////////
  void rule(cost_f&& eval) {
    assert(eval);
    func_ = std::move(eval);
    init_ = false;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief estimate the query according to the provided estimation function
  /// @return estimated cost
  //////////////////////////////////////////////////////////////////////////////
  cost_t estimate() const {
    if (!init_) {
      assert(func_);
      value_ = func_();
      init_ = true;
    }
    return value_;
  }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  cost_f func_{[]{ return 0; }}; // evaluation function
  mutable cost_t value_{ 0 };
  mutable bool init_{ true };
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // cost

} // ROOT

#endif // IRESEARCH_COST_H
