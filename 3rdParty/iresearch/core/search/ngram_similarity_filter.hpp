////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_NGRAM_SIMILARITY_FILTER_H
#define IRESEARCH_NGRAM_SIMILARITY_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

NS_ROOT


//////////////////////////////////////////////////////////////////////////////
/// @class by_ngram_similarity
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_ngram_similarity : public filter {
 public:
  typedef bstring term_t;
  typedef std::vector<term_t> terms_t;
  typedef terms_t::iterator iterator;
  typedef terms_t::const_iterator const_iterator;

  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  // returns set of features required for filter 
  static const flags& features();

  by_ngram_similarity();

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx
  ) const override;

  virtual size_t hash() const noexcept override;

  double threshold() const noexcept { return threshold_; }

  by_ngram_similarity& threshold(double d) {
    assert(d >= 0.);
    assert(d <= 1.);
    threshold_ = d;
    return *this;
  }
  
  by_ngram_similarity& field(std::string fld) {
    fld_ = std::move(fld); 
    return *this;
  }
   
  const std::string& field() const noexcept { return fld_; }

  by_ngram_similarity& push_back(const bstring& term) {
    ngrams_.emplace_back(term);
    return *this;
  }

  by_ngram_similarity& push_back(bstring&& term) {
    ngrams_.push_back(std::move(term));
    return *this;
  }

  by_ngram_similarity& push_back(const bytes_ref& term) {
    ngrams_.emplace_back(term);
    return *this;
  }

  by_ngram_similarity& push_back(const string_ref& term) {
    ngrams_.emplace_back(ref_cast<byte_type>(term));
    return *this;
  }

  iterator begin() { return ngrams_.begin(); }
  iterator end() { return ngrams_.end(); }

  const_iterator begin() const { return ngrams_.begin(); }
  const_iterator end() const { return ngrams_.end(); }

  bool empty() const { return ngrams_.empty(); }
  size_t size() const { return ngrams_.size(); }
  void clear() { ngrams_.clear(); }

 protected:
  virtual bool equals(const filter& rhs) const noexcept override;

 private: 
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  terms_t ngrams_;
  std::string fld_;
  double_t threshold_{1.};
  IRESEARCH_API_PRIVATE_VARIABLES_END

}; // by_ngram_similarity
NS_END // ROOT

#endif // IRESEARCH_NGRAM_SIMILARITY_FILTER_H