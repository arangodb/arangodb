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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @brief an analyzer capable of breaking up delimited text into tokens as per
///        RFC4180 (without starting new records on newlines)
////////////////////////////////////////////////////////////////////////////////
class delimited_token_stream final
  : public TypedAnalyzer<delimited_token_stream>,
    private util::noncopyable {
 public:
  static constexpr std::string_view type_name() noexcept { return "delimiter"; }
  static void init();  // for trigering registration in a static build
  static ptr make(std::string_view delimiter);

  explicit delimited_token_stream(std::string_view delimiter);
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }
  bool next() final;
  bool reset(std::string_view data) final;

 private:
  using attributes = std::tuple<increment,
                                offset,  // token value with evaluated quotes
                                term_attribute>;

  bytes_view data_;
  bytes_view delim_;
  bstring delim_buf_;
  bstring term_buf_;  // buffer for the last evaluated term
  attributes attrs_;
};

}  // namespace analysis
}  // namespace irs
