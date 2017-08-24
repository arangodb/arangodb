//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_COST_H
#define IRESEARCH_COST_H

#include "utils/attributes.hpp"
#include <functional>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class cost
/// @brief represents an estimated cost of the query execution
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API cost : public attribute {
 public:
  typedef uint64_t cost_t;
  typedef std::function<cost_t()> cost_f;

  static const cost_t MAX = integer_traits<cost_t>::const_max;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns a value of the "cost" attribute in the specified "src"
  /// collection, or "def" value if there is no "cost" attribute in "src"
  //////////////////////////////////////////////////////////////////////////////
  static cost_t extract(const attribute_view& src, cost_t def = MAX) NOEXCEPT {
    cost::cost_t est = def;
    auto& attr = src.get<iresearch::cost>();
    if (attr) {
      est = attr->estimate();
    }
    return est;
  }

  DECLARE_ATTRIBUTE_TYPE();
  cost() = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @returns the estimation rule 
  //////////////////////////////////////////////////////////////////////////////
  const cost_f& rule() const {
    return func_;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the estimation value 
  //////////////////////////////////////////////////////////////////////////////
  cost& value(cost_t value) {
    func_ = [value](){ return value; };
    value_ = value;
    init_ = true;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the estimation rule
  //////////////////////////////////////////////////////////////////////////////
  cost& rule(const cost_f& eval) {
    func_ = eval;
    init_ = false;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief estimate the query according to the provided estimation function
  /// @return estimated cost
  //////////////////////////////////////////////////////////////////////////////
  cost_t estimate() const {
    return const_cast<cost*>(this)->estimate_impl();
  }

  void clear() {
    init_ = false;
  }

 private:
  cost_t estimate_impl() {
    if (!init_) {
      assert(func_);
      value_ = func_();
      init_ = true;
    }
    return value_;
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  /* evaluation function */
  cost_f func_;
  cost_t value_ { 0 };
  bool init_{ false };
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // cost

NS_END // ROOT

#endif // IRESEARCH_COST_H
