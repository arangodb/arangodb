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

#ifndef IRESEARCH_PHRASE_FILTER_H
#define IRESEARCH_PHRASE_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"
#include <map>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class by_phrase
/// @brief user-side phrase filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_phrase : public filter {
 public:
  // positions and terms
  typedef std::map<size_t, bstring> terms_t;
  typedef terms_t::const_iterator const_iterator;
  typedef terms_t::iterator iterator;
  typedef terms_t::value_type term_t;
 
  // returns set of features required for filter 
  static const flags& required();

  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY_DEFAULT();

  by_phrase();
  
  by_phrase& field(std::string fld) {
    fld_ = std::move(fld); 
    return *this;
  }

  const std::string& field() const { return fld_; }

  // inserts term to the specified position 
  by_phrase& insert(size_t pos, const bytes_ref& term) {
    phrase_[pos] = term; 
    return *this;
  }
  
  by_phrase& insert(size_t pos, const string_ref& term) {
    return insert(pos, ref_cast<byte_type>(term));
  }
  
  by_phrase& insert(size_t pos, bstring&& term) {
    phrase_[pos] = std::move(term);
    return *this;
  }

  // inserts term to the end of the phrase with 
  // the specified offset from the last term
  by_phrase& push_back(const bytes_ref& term, size_t offs = 0) {
    return insert(next_pos() + offs, term);
  }
  
  by_phrase& push_back(const string_ref& term, size_t offs = 0) {
    return push_back(ref_cast<byte_type>(term), offs);
  }
  
  by_phrase& push_back(bstring&& term, size_t offs = 0) {
    return insert(next_pos() + offs, std::move(term));
  }

  bstring& operator[](size_t pos) { return phrase_[pos]; } 
  const bstring& operator[](size_t pos) const {
    return phrase_.at(pos); 
  }

  bool empty() const { return phrase_.empty(); }
  size_t size() const { return phrase_.size(); }

  const_iterator begin() const { return phrase_.begin(); }
  const_iterator end() const { return phrase_.end(); }
  
  iterator begin() { return phrase_.begin(); }
  iterator end() { return phrase_.end(); }

  boost::boost_t phrase_boost() const { return phrase_boost_; }
  by_phrase& phrase_boost(boost::boost_t phrase_boost) {
    phrase_boost_ = phrase_boost;
    return *this;
  }

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost) const override;
  
  virtual size_t hash() const override;

 protected:
  virtual bool equals(const filter& rhs) const override;
 
 private:
  size_t next_pos() const {
    return phrase_.empty() ? 0 : 1 + phrase_.rbegin()->first;
  }
  
  size_t first_pos() const {
    return phrase_.empty() ? 0 : phrase_.begin()->first;
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string fld_;
  terms_t phrase_;
  boost::boost_t phrase_boost_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // by_phrase

NS_END // ROOT

#endif
