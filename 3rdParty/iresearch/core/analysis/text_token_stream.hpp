////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
/// @author Andrei Lobov
/// @author Yuriy Popov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_IQL_TEXT_TOKEN_STREAM_H
#define IRESEARCH_IQL_TEXT_TOKEN_STREAM_H

#include "analyzers.hpp"
#include "shared.hpp"
#include "token_stream.hpp"
#include "token_attributes.hpp"

NS_ROOT
NS_BEGIN(analysis)

class text_token_stream : public analyzer, util::noncopyable {
 public:
  typedef std::unordered_set<std::string> stopwords_t;
  struct options_t {
    enum case_convert_t { LOWER, NONE, UPPER };
    // lowercase tokens, match original implementation
    case_convert_t case_convert{case_convert_t::LOWER};
    stopwords_t explicit_stopwords;
    std::locale locale;
    std::string stopwordsPath{0}; // string with zero char indicates 'no value set'
    size_t min_gram{};
    size_t max_gram{};
    // needed for mark empty explicit_stopwords as valid and prevent loading from defaults
    bool explicit_stopwords_set{};
    bool accent{}; // remove accents from letters, match original implementation
    bool stemming{true}; // try to stem if possible, match original implementation
    // needed for mark empty min_gram as valid and prevent loading from defaults
    bool min_gram_set{};
    // needed for mark empty max_gram as valid and prevent loading from defaults
    bool max_gram_set{};
    bool preserve_original{}; // emit input data as a token
    // needed for mark empty preserve_original as valid and prevent loading from defaults
    bool preserve_original_set{};
  };

  struct state_t;

  class bytes_term : public irs::term_attribute {
   public:
    void clear() {
      buf_.clear();
      value_ = irs::bytes_ref::NIL;
    }

    using irs::term_attribute::value;

    void value(irs::bstring&& data) {
      buf_ = std::move(data);
      value(buf_);
    }

    void value(const irs::bytes_ref& data) {
      value_ = data;
    }

   private:
    irs::bstring buf_; // buffer for value if value cannot be referenced directly
  };

  static char const* STOPWORD_PATH_ENV_VARIABLE;

  DECLARE_ANALYZER_TYPE();

  // for use with irs::order::add<T>() and default args (static build)
  DECLARE_FACTORY(const irs::string_ref& locale);

  text_token_stream(const options_t& options, const stopwords_t& stopwords);
  virtual const irs::attribute_view& attributes() const noexcept override {
    return attrs_;
  }
  static void init(); // for triggering registration in a static build
  virtual bool next() override;
  virtual bool reset(const string_ref& data) override;

 private:
  bool next_word();
  bool next_ngram();

 private:
  irs::attribute_view attrs_;
  std::shared_ptr<state_t> state_;
  irs::offset offs_;
  irs::increment inc_;
  bytes_term term_;
};

NS_END // analysis
NS_END // ROOT

#endif
