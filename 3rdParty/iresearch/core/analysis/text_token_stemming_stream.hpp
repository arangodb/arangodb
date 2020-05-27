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

#ifndef IRESEARCH_TEXT_TOKEN_STEMMING_STREAM_H
#define IRESEARCH_TEXT_TOKEN_STEMMING_STREAM_H

#include "analyzers.hpp"
#include "token_attributes.hpp"
#include "utils/frozen_attributes.hpp"

struct sb_stemmer; // forward declaration

NS_ROOT
NS_BEGIN(analysis)

////////////////////////////////////////////////////////////////////////////////
/// @brief an analyser capable of stemming the text, treated as a single token,
///        for supported languages
////////////////////////////////////////////////////////////////////////////////
class text_token_stemming_stream final
  : public frozen_attributes<4, analyzer>,
    private util::noncopyable {
 public:
  static constexpr string_ref type_name() noexcept { return "stem"; }
  static void init(); // for trigering registration in a static build
  static ptr make(const irs::string_ref& locale);

  explicit text_token_stemming_stream(const std::locale& locale);
  virtual bool next() override;
  virtual bool reset(const irs::string_ref& data) override;

  private:
   increment inc_;
   std::locale locale_;
   offset offset_;
   payload payload_; // raw token value
   std::shared_ptr<sb_stemmer> stemmer_;
   term_attribute term_; // token value with evaluated quotes
   std::string term_buf_; // buffer for the last evaluated term
   bool term_eof_;
};

NS_END // analysis
NS_END // ROOT

#endif
