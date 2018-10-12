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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_RANGE_FILTER_H
#define IRESEARCH_RANGE_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

NS_ROOT

enum class Bound {
  MIN, MAX
}; 

enum class Bound_Type {
  UNBOUNDED, INCLUSIVE, EXCLUSIVE
};

NS_BEGIN(detail)

template<typename T>
struct range {
  T min{};
  T max{};
  Bound_Type min_type = Bound_Type::UNBOUNDED;
  Bound_Type max_type = Bound_Type::UNBOUNDED;

  bool operator==(const range& rhs) const {
    return min == rhs.min && min_type == rhs.min_type
      && max == rhs.max && max_type == rhs.max_type;
  }

  bool operator!=(const range& rhs) const {
    return !(*this == rhs); 
  }
}; // range

NS_END // detail

class term_selector;

//////////////////////////////////////////////////////////////////////////////
/// @class by_range
/// @brief user-side term range filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_range : public filter {
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  by_range() NOEXCEPT;

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx
  ) const override;

  by_range& field(std::string fld) {
    fld_ = std::move(fld); 
    return *this;
  }

  const std::string& field() const { 
    return fld_; 
  }

  template<Bound B>
  const bstring& term() const {
    return get<B>::term(const_cast<range_t&>(rng_));
  }

  template<Bound B>
  by_range& term(bstring&& term) {
    get<B>::term(rng_) = std::move(term);

    if (Bound_Type::UNBOUNDED == get<B>::type(rng_)) {
      get<B>::type(rng_) = Bound_Type::EXCLUSIVE;
    }

    return *this;
  }

  template<Bound B>
  by_range& term(const bytes_ref& term) {
    get<B>::term(rng_) = term;

    if (term.null()) {
      get<B>::type(rng_) = Bound_Type::UNBOUNDED;
    } else if (Bound_Type::UNBOUNDED == get<B>::type(rng_)) {
      get<B>::type(rng_) = Bound_Type::EXCLUSIVE;
    }

    return *this;
  }

  template<Bound B>
  by_range& term(const string_ref& term) {
    return this->term<B>(ref_cast<byte_type>(term));
  }

  template<Bound B>
  by_range& include(bool incl) {
    get<B>::type(rng_) = incl ? Bound_Type::INCLUSIVE : Bound_Type::EXCLUSIVE;
    return *this;
  }

  template<Bound B>
  bool include() const {
    return Bound_Type::INCLUSIVE == get<B>::type(rng_);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  by_range& scored_terms_limit(size_t limit) {
    scored_terms_limit_ = limit;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit() const {
    return scored_terms_limit_;
  }

  virtual size_t hash() const NOEXCEPT override;

 protected:
  virtual bool equals(const filter& rhs) const NOEXCEPT override;

 private: 
  typedef detail::range<bstring> range_t;
  template<Bound B> struct get;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string fld_;
  range_t rng_;
  size_t scored_terms_limit_{1024};
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // by_range 

template<> struct by_range::get<Bound::MIN> {
  static bstring& term(range_t& rng) { return rng.min; }
  static const bstring& term(const range_t& rng) { return rng.min; }
  static Bound_Type& type(range_t& rng) { return rng.min_type; }
  static const Bound_Type& type(const range_t& rng) { return rng.min_type; }
}; // get<Bound::MAX>

template<> struct by_range::get<Bound::MAX> {
  static bstring& term(range_t& rng) { return rng.max; }
  static const bstring& term(const range_t& rng) { return rng.max; }
  static Bound_Type& type(range_t& rng) { return rng.max_type; }
  static const Bound_Type& type(const range_t& rng) { return rng.max_type; }
}; // get<Bound::MAX>

NS_END // ROOT

#endif // IRESEARCH_RANGE_FILTER_H
