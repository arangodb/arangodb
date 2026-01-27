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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#ifndef IRESEARCH_ICU_NAMESPACE
#define IRESEARCH_ICU_NAMESPACE icu
#endif

#include <unicode/locid.h>

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @class collation_token_stream
/// @brief an analyser capable of converting UTF-8 encoded input into a sortable
///        token as per specified locale
/// @note expects UTF-8 encoded input
////////////////////////////////////////////////////////////////////////////////
class collation_token_stream final
  : public TypedAnalyzer<collation_token_stream>,
    private util::noncopyable {
 public:
  struct options_t {
    IRESEARCH_ICU_NAMESPACE::Locale locale;
    bool forceUtf8;

    options_t() : locale{"C"}, forceUtf8{true} { locale.setToBogus(); }
  };

  static constexpr std::string_view type_name() noexcept { return "collation"; }
  static void init();  // for trigering registration in a static build

  explicit collation_token_stream(const options_t& options);

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }
  bool next() noexcept final {
    const auto eof = !term_eof_;
    term_eof_ = true;
    return eof;
  }
  bool reset(std::string_view data) final;

 private:
  struct state_t;
  struct state_deleter_t {
    void operator()(state_t*) const noexcept;
  };

  using attributes =
    std::tuple<increment, offset,
               term_attribute>;  // token value with evaluated quotes

  attributes attrs_;
  std::unique_ptr<state_t, state_deleter_t> state_;
  bool term_eof_;
};

}  // namespace analysis
}  // namespace irs
