////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @brief an analyzer capable of breaking up delimited text into tokens
///        separated by a set of strings.
////////////////////////////////////////////////////////////////////////////////
class MultiDelimitedAnalyser : public TypedAnalyzer<MultiDelimitedAnalyser>,
                               private util::noncopyable {
 public:
  struct Options {
    std::vector<bstring> delimiters;
  };

  static constexpr std::string_view type_name() noexcept {
    return "multi_delimiter";
  }
  static void init();

  static analyzer::ptr Make(Options&&);

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs, type);
  }

  bool reset(std::string_view input) final {
    data = ViewCast<byte_type>(input);
    start = data.data();
    return true;
  }

 protected:
  using Attributes = std::tuple<increment, offset, term_attribute>;
  const byte_type* start{};
  bytes_view data{};
  Attributes attrs;
};

}  // namespace analysis
}  // namespace irs
