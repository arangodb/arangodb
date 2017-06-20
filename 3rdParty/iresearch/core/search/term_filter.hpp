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

#ifndef IRESEARCH_TERM_FILTER_H
#define IRESEARCH_TERM_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class by_term 
/// @brief user-side term filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_term : public filter {
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY_DEFAULT();

  by_term();

  by_term& field(std::string fld) {
    fld_ = std::move(fld); 
    return *this;
  }

  const std::string& field() const { 
    return fld_; 
  }

  by_term& term(bstring&& term) {
    term_ = std::move(term);
    return *this;
  }

  by_term& term(const bytes_ref& term) {
    term_ = term;
    return *this;
  }

  by_term& term(const string_ref& term) {
    return this->term(ref_cast<byte_type>(term));
  }

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
      const index_reader& rdr,
      const order::prepared& ord,
      boost_t boost) const override;

  const bstring& term() const { 
    return term_;
  }

  virtual size_t hash() const override;

 protected:
  by_term(const type_id& type);
  virtual bool equals(const filter& rhs) const override;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string fld_;
  bstring term_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif
