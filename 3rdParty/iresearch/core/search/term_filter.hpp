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
  DECLARE_FACTORY();

  by_term() noexcept;

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
    boost_t boost,
    const attribute_view& ctx
  ) const override;

  const bstring& term() const { 
    return term_;
  }

  virtual size_t hash() const noexcept override;

 protected:
  explicit by_term(const type_id& type) noexcept
    : filter(type) {
  }
  virtual bool equals(const filter& rhs) const noexcept override;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string fld_;
  bstring term_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif
