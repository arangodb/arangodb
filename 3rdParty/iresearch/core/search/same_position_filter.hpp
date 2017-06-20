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

#ifndef IRESEARCH_SAME_POSITION_FILTER_H
#define IRESEARCH_SAME_POSITION_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class by_same_position
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_same_position : public filter {
 public:
  typedef std::pair<std::string, bstring> term_t;
  typedef std::vector<term_t> terms_t;
  typedef terms_t::iterator iterator;
  typedef terms_t::const_iterator const_iterator;

  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY_DEFAULT();
  
  // returns set of features required for filter 
  static const flags& features();

  by_same_position();

  using filter::prepare;
  
  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost) const override;

  virtual size_t hash() const override;

  by_same_position& push_back(const std::string& field, const bstring& term);
  by_same_position& push_back(const std::string& field, bstring&& term);
  by_same_position& push_back(std::string&& field, const bstring& term);
  by_same_position& push_back(std::string&& field, bstring&& term);

  iterator begin() { return terms_.begin(); }
  iterator end() { return terms_.end(); }

  const_iterator begin() const { return terms_.begin(); }
  const_iterator end() const { return terms_.end(); }

  bool empty() const { return terms_.empty(); }
  size_t size() const { return terms_.size(); }
  void clear() { terms_.clear(); }

 protected:
  virtual bool equals(const filter& rhs) const override;

 private: 
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  terms_t terms_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // by_same_position

NS_END // ROOT

#endif // IRESEARCH_SAME_POSITION_FILTER_H
