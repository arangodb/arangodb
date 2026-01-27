////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "analyzers.hpp"
#include "shared.hpp"
#include "token_attributes.hpp"
#include "token_stream.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {
namespace analysis {
class segmentation_token_stream final
  : public TypedAnalyzer<segmentation_token_stream>,
    private util::noncopyable {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "segmentation";
  }
  static void init();  // for triggering registration in a static build

  struct options_t {
    enum class case_convert_t { LOWER, NONE, UPPER };
    enum class word_break_t {
      ALL,      // All UAX29 words are reported
      GRAPHIC,  // Report only words with graphic characters
      ALPHA     // Report only words with alphanumeric characters
    };

    // lowercase tokens, match default values in text analyzer
    case_convert_t case_convert{case_convert_t::LOWER};
    word_break_t word_break{word_break_t::ALPHA};
  };

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }
  explicit segmentation_token_stream(options_t&& opts);
  bool next() final;
  bool reset(std::string_view data) final;

 private:
  using attributes = std::tuple<increment, offset, term_attribute>;

  struct state_t;
  struct state_deleter_t {
    void operator()(state_t*) const noexcept;
  };

  std::unique_ptr<state_t, state_deleter_t> state_;
  options_t options_;
  std::string
    term_buf_;  // buffer for value if value cannot be referenced directly
  attributes attrs_;
};
}  // namespace analysis
}  // namespace irs
