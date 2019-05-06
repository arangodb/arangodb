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

#ifndef IRESEARCH_NGRAM_TOKEN_STREAM_H
#define IRESEARCH_NGRAM_TOKEN_STREAM_H

#include "analyzers.hpp"
#include "token_attributes.hpp"

NS_ROOT
NS_BEGIN(analysis)

////////////////////////////////////////////////////////////////////////////////
/// @class ngram_token_stream
/// @brief produces ngram from a specified input in a range of 
//         [min_gram;max_gram]. Can optionally preserve the original input.
////////////////////////////////////////////////////////////////////////////////
class ngram_token_stream: public analyzer, util::noncopyable {
 public:
  DECLARE_ANALYZER_TYPE();

  DECLARE_FACTORY(size_t min_gram, size_t max_gram, bool preserve_original);

  static void init(); // for trigering registration in a static build

  ngram_token_stream(size_t n, bool preserve_original)
    : ngram_token_stream(n, n, preserve_original) {
  }

  ngram_token_stream(size_t min_gram, size_t max_gram, bool preserve_original);

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

  virtual bool next() NOEXCEPT override;

  virtual bool reset(const string_ref& data) NOEXCEPT override;

  size_t min_gram() const NOEXCEPT { return min_gram_; }

  size_t max_gram() const NOEXCEPT { return max_gram_; }

  bool preserve_original() const NOEXCEPT { return preserve_original_; }

 private:
  class term_attribute final: public irs::term_attribute {
   public:
    void value(const bytes_ref& value) { value_ = value; }
  };

  attribute_view attrs_;
  size_t min_gram_;
  size_t max_gram_;
  bytes_ref data_; // data to process
  increment inc_;
  offset offset_;
  term_attribute term_;
  const byte_type* begin_{};
  size_t length_{};

  bool preserve_original_{ false }; // emit input data as a token
  bool emit_original_{ false };
}; // ngram_token_stream


NS_END
NS_END

#endif // IRESEARCH_NGRAM_TOKEN_STREAM_H
