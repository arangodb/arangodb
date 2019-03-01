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

struct sb_stemmer; // forward declaration

NS_ROOT
NS_BEGIN(analysis)

////////////////////////////////////////////////////////////////////////////////
/// @brief an analyser capable of stemming the text, treated as a single token,
///        for supported languages
////////////////////////////////////////////////////////////////////////////////
class text_token_stemming_stream: public analyzer, util::noncopyable {
 public:
  DECLARE_ANALYZER_TYPE();

  // for use with irs::order::add<T>() and default args (static build)
  DECLARE_FACTORY(const string_ref& locale);

  text_token_stemming_stream(const irs::string_ref& locale);
  virtual const irs::attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }
  static void init(); // for trigering registration in a static build
  virtual bool next() override;
  virtual bool reset(const irs::string_ref& data) override;

  private:
   class term_attribute final: public irs::term_attribute {
    public:
     using irs::term_attribute::value;
     void value(const irs::bytes_ref& value) { value_ = value; }
   };

   irs::attribute_view attrs_;
   irs::increment inc_;
   std::locale locale_;
   irs::offset offset_;
   irs::payload payload_; // raw token value
   std::shared_ptr<sb_stemmer> stemmer_;
   term_attribute term_; // token value with evaluated quotes
   std::string term_buf_; // buffer for the last evaluated term
   bool term_eof_;
};

NS_END // analysis
NS_END // ROOT

#endif