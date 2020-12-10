////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef IRESEARCH_TEXT_TOKEN_NORMALIZING_STREAM_H
#define IRESEARCH_TEXT_TOKEN_NORMALIZING_STREAM_H

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

namespace iresearch {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @brief an analyser capable of normalizing the text, treated as a single
///        token, i.e. case conversion and accent removal
////////////////////////////////////////////////////////////////////////////////
class text_token_normalizing_stream
  : public frozen_attributes<4, analyzer>,
    util::noncopyable {
 public:
  struct options_t {
    enum case_convert_t { LOWER, NONE, UPPER };
    std::locale locale;
    case_convert_t case_convert{case_convert_t::NONE}; // no extra normalization
    bool accent{true}; // no extra normalization
  };

  struct state_t;

  static constexpr string_ref type_name() noexcept { return "norm"; }
  static void init(); // for trigering registration in a static build
  static ptr make(const string_ref& locale);

  explicit text_token_normalizing_stream(const options_t& options);
  virtual bool next() override;
  virtual bool reset(const irs::string_ref& data) override;

 private:
  irs::increment inc_;
  irs::offset offset_;
  irs::payload payload_; // raw token value
  std::shared_ptr<state_t> state_;
  irs::term_attribute term_; // token value with evaluated quotes
  bool term_eof_;
};

} // analysis
} // ROOT

#endif
