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

#ifndef IRESEARCH_COLLATION_TOKEN_STREAM_H
#define IRESEARCH_COLLATION_TOKEN_STREAM_H

#include <unicode/locid.h>

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

namespace iresearch {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @class collation_token_stream
/// @brief an analyser capable of converting UTF-8 encoded input into a sortable
///        token as per specified locale
/// @note expects UTF-8 encoded input
////////////////////////////////////////////////////////////////////////////////
class collation_token_stream final
  : public analyzer,
    private util::noncopyable {
 public:
  struct options_t {
    icu::Locale locale;

    options_t() : locale{"C"} {
      locale.setToBogus();
    }
  };

  static constexpr string_ref type_name() noexcept { 
    return "collation";
  }
  static void init(); // for trigering registration in a static build

  explicit collation_token_stream(const options_t& options);

  virtual attribute* get_mutable(
      irs::type_info::type_id type) noexcept override final {
    return irs::get_mutable(attrs_, type);
  }
  virtual bool next() noexcept override {
    const auto eof = !term_eof_;
    term_eof_ = true;
    return eof;
  }
  virtual bool reset(const irs::string_ref& data) override;

 private:
  struct state_t;
  struct state_deleter_t {
    void operator()(state_t*) const noexcept;
  };

  using attributes = std::tuple<
    increment,
    offset,
    term_attribute>; // token value with evaluated quotes

  attributes attrs_;
  std::unique_ptr<state_t, state_deleter_t> state_;
  bool term_eof_;
}; // collation_token_stream

} // analysis
} // iresearch

#endif // IRESEARCH_COLLATION_TOKEN_STREAM_H
