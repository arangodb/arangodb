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

#ifndef IRESEARCH_PREFIX_FILTER_H
#define IRESEARCH_PREFIX_FILTER_H

#include "term_filter.hpp"

NS_ROOT

class term_selector;

class IRESEARCH_API by_prefix final : public by_term {
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

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
    boost_t boost,
    const attribute_view& ctx
  ) const override;

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

  virtual size_t hash() const NOEXCEPT override;

 protected:
  virtual bool equals(const filter& rhs) const NOEXCEPT override;

 private:
  size_t scored_terms_limit_{1024};
}; // by_prefix

NS_END

#endif
