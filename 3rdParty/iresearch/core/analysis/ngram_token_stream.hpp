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
  struct options_t {
    options_t() : min_gram(0), max_gram(0), preserve_original(true) {}
    options_t(size_t min, size_t max, bool original) {
      min_gram = min;
      max_gram = max;
      preserve_original = original;
    }

    size_t min_gram;
    size_t max_gram;
    bool preserve_original; // emit input data as a token
  };

  DECLARE_ANALYZER_TYPE();

  DECLARE_FACTORY(const options_t& options);

  static void init(); // for trigering registration in a static build

  //ngram_token_stream(size_t n, bool preserve_original)
  //  : ngram_token_stream(n, n, preserve_original) {
 //}

  ngram_token_stream(const options_t& options);

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

  virtual bool next() NOEXCEPT override;
  virtual bool reset(const string_ref& data) NOEXCEPT override;

  size_t min_gram() const NOEXCEPT { return options_.min_gram; }
  size_t max_gram() const NOEXCEPT { return options_.max_gram; }
  bool preserve_original() const NOEXCEPT { return options_.preserve_original; }

 private:
  class term_attribute final: public irs::term_attribute {
   public:
    void value(const bytes_ref& value) { value_ = value; }
  };

  options_t options_;
  attribute_view attrs_;
  bytes_ref data_; // data to process
  increment inc_;
  offset offset_;
  term_attribute term_;
  const byte_type* begin_{};
  size_t length_{};
  bool emit_original_{ false };
}; // ngram_token_stream


NS_END
NS_END

#endif // IRESEARCH_NGRAM_TOKEN_STREAM_H
