////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_COLUMN_EXISTENCE_FILTER_H
#define IRESEARCH_COLUMN_EXISTENCE_FILTER_H

#include "filter.hpp"
#include "utils/string.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class by_column_existence
/// @brief user-side column existence filter
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API by_column_existence final : public filter {
 public:
  DECLARE_FILTER_TYPE();
  DECLARE_FACTORY();

  by_column_existence() noexcept;

  by_column_existence& field(const std::string& field) {
    field_ = field;
    return *this;
  }

  by_column_existence& field(std::string&& field) noexcept {
    field_ = std::move(field);
    return *this;
  }

  const std::string& field() const noexcept {
    return field_;
  }

  bool prefix_match() const noexcept {
    return prefix_match_;
  }

  by_column_existence& prefix_match(bool value) noexcept {
    prefix_match_ = value;
    return *this;
  }

  using filter::prepare;

  virtual filter::prepared::ptr prepare(
    const index_reader& rdr,
    const order::prepared& ord,
    boost_t boost,
    const attribute_view& ctx
  ) const override;

  virtual size_t hash() const noexcept override;

 protected:
  virtual bool equals(const filter& rhs) const noexcept override;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string field_;
  bool prefix_match_{};
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // by_column_existence

NS_END // ROOT

#endif // IRESEARCH_COLUMN_EXISTENCE_FILTER_H
