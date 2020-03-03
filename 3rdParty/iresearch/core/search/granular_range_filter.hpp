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

#ifndef IRESEARCH_GRANULAR_RANGE_FILTER_H
#define IRESEARCH_GRANULAR_RANGE_FILTER_H

#include "range_filter.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class by_granular_range
/// @brief user-side term range filter for granularity-enabled terms
///        when indexing, the lower the value for attributes().get<position>()
///        the higher the granularity of the term value
///        the lower granularity terms are <= higher granularity terms
///        NOTE: it is assumed that granularity level gaps are identical for
///              all terms, i.e. the behavour for the following is undefined:
///              termA@0 + termA@2 + termA@5 + termA@10
///              termB@0 + termB@2 + termB@6 + termB@10
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_granular_range: public filter {
 public:
  // granularity levels and terms
  typedef bytes_ref::char_type granularity_level_t;
  typedef std::map<granularity_level_t, bstring> terms_t;
  typedef terms_t::const_iterator const_iterator;
  typedef terms_t::iterator iterator;
  typedef terms_t::key_type level_t;
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  by_granular_range();

  using filter::prepare;

  template<Bound B>
  bool empty() const { return get<B>::term(rng_).empty(); }

  template<Bound B>
  size_t size() const { return get<B>::term(rng_).size(); }

  template<Bound B>
  const_iterator begin() const { return get<B>::term(rng_).begin(); }

  template<Bound B>
  const_iterator end() const { return get<B>::term(rng_).end(); }

  template<Bound B>
  iterator begin() { return get<B>::term(rng_).begin(); }

  template<Bound B>
  iterator end() { return get<B>::term(rng_).end(); }

  const std::string& field() const { return fld_; }

  by_granular_range& field(std::string fld);

  virtual size_t hash() const noexcept override;

  template<Bound B>
  bool include() const { return BoundType::INCLUSIVE == get<B>::type(rng_); }

  template<Bound B>
  by_granular_range& include(bool incl) {
    get<B>::type(rng_) = incl ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
    return *this;
  }

  // use the most precise value for 'granularity_level'
  template<Bound B>
  by_granular_range& insert(bstring&& term) {
    return insert<B>((std::numeric_limits<level_t>::min)(), std::move(term));
  }

  // use the most precise value for 'granularity_level'
  template<Bound B>
  by_granular_range& insert(const bytes_ref& term) {
    return insert<B>((std::numeric_limits<level_t>::min)(), term);
  }

  // use the most precise value for 'granularity_level'
  template<Bound B>
  by_granular_range& insert(const string_ref& term) {
    return insert<B>((std::numeric_limits<level_t>::min)(), ref_cast<byte_type>(term));
  }

  // sequential 'granularity_level' value, cannot use 'iresearch::increment' since it can be 0
  template<Bound B>
  by_granular_range& insert(numeric_token_stream& term) {
    auto& attributes = term.attributes();
    auto& term_attr = attributes.get<term_attribute>();

    for (level_t level = (std::numeric_limits<level_t>::min)(); term.next(); ++level) {
      insert<B>(level, term_attr->value());
    }

    return *this;
  }

  // the lower the value of 'granularity_level' the more precise the term
  template<Bound B>
  by_granular_range& insert(
    const level_t& granularity_level, bstring&& term
  ) {
    insert(get<B>::term(rng_), granularity_level, std::move(term));

    if (BoundType::UNBOUNDED == get<B>::type(rng_)) {
      get<B>::type(rng_) = BoundType::EXCLUSIVE;
    }

    return *this;
  }

  // the lower the value of 'granularity_level' the more precise the term
  template<Bound B>
  by_granular_range& insert(
    const level_t& granularity_level, const bytes_ref& term
  ) {
    assert(!term.null());

    if (BoundType::UNBOUNDED == get<B>::type(rng_)) {
      get<B>::type(rng_) = BoundType::EXCLUSIVE;
    }

    insert(get<B>::term(rng_), granularity_level, term);

    return *this;
  }

  // the lower the value of 'granularity_level' the more precise the term
  template<Bound B>
  by_granular_range& insert(
    const level_t& granularity_level, const string_ref& term
  ) {
    return insert<B>(granularity_level, ref_cast<byte_type>(term));
  }

  virtual filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost,
      const attribute_view& ctx
  ) const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  by_granular_range& scored_terms_limit(size_t limit) {
    scored_terms_limit_ = limit;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit() const { return scored_terms_limit_; }

  // the lower the value of 'granularity_level' the more precise the term
  template<Bound B>
  bstring& term(level_t granularity_level) {
    if (BoundType::UNBOUNDED == get<B>::type(rng_)) {
      get<B>::type(rng_) = BoundType::EXCLUSIVE;
    }

    return insert(get<B>::term(rng_), granularity_level);
  }

  // the lower the value of 'granularity_level' the more precise the term
  template<Bound B>
  const bstring& term(level_t granularity_level) const {
    return get<B>::term(rng_).at(granularity_level);
  }

 protected:
  virtual bool equals(const filter& rhs) const noexcept override;

 private:
  typedef range<terms_t> range_t;
  template<Bound B> struct get;

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string fld_;
  range_t rng_;
  size_t scored_terms_limit_{1024};
  IRESEARCH_API_PRIVATE_VARIABLES_END

  bstring& insert(terms_t& terms, const level_t& granularity_level);

  bstring& insert(terms_t& terms, const level_t& granularity_level, bstring&& term);

  bstring& insert(terms_t& terms, const level_t& granularity_level, const bytes_ref& term);
};

template<> struct by_granular_range::get<Bound::MIN> {
  static terms_t& term(range_t& rng) { return rng.min; }
  static const terms_t& term(const range_t& rng) { return rng.min; }
  static BoundType& type(range_t& rng) { return rng.min_type; }
  static const BoundType& type(const range_t& rng) { return rng.min_type; }
}; // get<Bound::MIN>

template<> struct by_granular_range::get<Bound::MAX> {
  static terms_t& term(range_t& rng) { return rng.max; }
  static const terms_t& term(const range_t& rng) { return rng.max; }
  static BoundType& type(range_t& rng) { return rng.max_type; }
  static const BoundType& type(const range_t& rng) { return rng.max_type; }
}; // get<Bound::MAX>

NS_END // ROOT

#endif
