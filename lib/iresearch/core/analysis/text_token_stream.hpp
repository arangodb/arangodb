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

#pragma once

#ifndef IRESEARCH_ICU_NAMESPACE
#define IRESEARCH_ICU_NAMESPACE icu
#endif

#include <unicode/locid.h>

#include "analyzers.hpp"
#include "shared.hpp"
#include "token_attributes.hpp"
#include "token_stream.hpp"
#include "utils/attribute_helper.hpp"

#include <absl/container/flat_hash_set.h>

namespace irs {
namespace analysis {

////////////////////////////////////////////////////////////////////////////////
/// @class text_token_stream
/// @note expects UTF-8 encoded input
////////////////////////////////////////////////////////////////////////////////
class text_token_stream final : public TypedAnalyzer<text_token_stream>,
                                private util::noncopyable {
 public:
  using stopwords_t = absl::flat_hash_set<std::string>;

  enum case_convert_t { LOWER, NONE, UPPER };

  struct options_t {
    // lowercase tokens, match original implementation
    case_convert_t case_convert{case_convert_t::LOWER};
    stopwords_t explicit_stopwords;
    IRESEARCH_ICU_NAMESPACE::Locale locale;
    std::string stopwordsPath{
      0};  // string with zero char indicates 'no value set'
    size_t min_gram{};
    size_t max_gram{};

    // needed for mark empty explicit_stopwords as valid and prevent loading
    // from defaults
    bool explicit_stopwords_set{};
    bool
      accent{};  // remove accents from letters, match original implementation
    bool stemming{
      true};  // try to stem if possible, match original implementation
    // needed for mark empty min_gram as valid and prevent loading from defaults
    bool min_gram_set{};
    // needed for mark empty max_gram as valid and prevent loading from defaults
    bool max_gram_set{};
    bool preserve_original{};  // emit input data as a token
    // needed for mark empty preserve_original as valid and prevent loading from
    // defaults
    bool preserve_original_set{};

    options_t() : locale{"C"} { locale.setToBogus(); }
  };

  struct state_t;

  static const char* STOPWORD_PATH_ENV_VARIABLE;

  static constexpr std::string_view type_name() noexcept { return "text"; }
  static void init();  // for triggering registration in a static build
  static ptr make(std::string_view locale);
  static void clear_cache();

  text_token_stream(const options_t& options, const stopwords_t& stopwords);
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }
  bool next() final;
  bool reset(std::string_view data) final;

 private:
  using attributes = std::tuple<increment, offset, term_attribute>;

  struct state_deleter_t {
    void operator()(state_t*) const noexcept;
  };

  bool next_word();
  bool next_ngram();

  bstring term_buf_;  // buffer for value if value cannot be referenced directly
  attributes attrs_;
  std::unique_ptr<state_t, state_deleter_t> state_;
};

}  // namespace analysis
}  // namespace irs
