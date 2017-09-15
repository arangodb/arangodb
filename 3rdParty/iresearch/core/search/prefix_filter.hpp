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

#ifndef IRESEARCH_PREFIX_FILTER_H
#define IRESEARCH_PREFIX_FILTER_H

#include "term_filter.hpp"

NS_ROOT

class term_selector;

class IRESEARCH_API by_prefix final : public by_term { /* public multiterm_filter<limited_term_selector> { */
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY_DEFAULT();

  by_prefix() NOEXCEPT;

  using by_term::field;

  by_prefix& field(std::string fld) {
    by_term::field(std::move(fld));
    return *this;
  }

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  by_prefix& scored_terms_limit(size_t limit) {
    scored_terms_limit_ = limit;
    return *this;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the maximum number of most frequent terms to consider for scoring
  //////////////////////////////////////////////////////////////////////////////
  size_t scored_terms_limit() const {
    return scored_terms_limit_;
  }

  virtual size_t hash() const override;

 protected:
  virtual bool equals(const filter& rhs) const override;

  virtual void collect_terms(
    term_selector& selector,
    const sub_reader& segment,
    const term_reader& field,
    seek_term_iterator& terms
  ) const;

 private:
  size_t scored_terms_limit_{1024};
}; // by_prefix

NS_END

#endif
