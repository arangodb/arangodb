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

#include "text_token_stream.hpp"

#include <unicode/brkiter.h> // for icu::BreakIterator
#include <absl/container/node_hash_map.h>
#include <frozen/unordered_map.h>
#include <libstemmer.h>

#include <cctype> // for std::isspace(...)
#include <fstream>
#include <mutex>
#include <string_view>

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"
#include "velocypack/vpack.h"
#include "utils/hash_utils.hpp"
#include "utils/log.hpp"
#include "utils/map_utils.hpp"
#include "utils/misc.hpp"
#include "utils/runtime_utils.hpp"
#include "utils/thread_utils.hpp"
#include "utils/utf8_utils.hpp"
#include "utils/utf8_path.hpp"
#include "utils/file_utils.hpp"
#include "utils/vpack_utils.hpp"
#include "utils/snowball_stemmer.hpp"

#if defined(_MSC_VER)
  #pragma warning(disable: 4512)
#endif

#include <unicode/normalizer2.h> // for icu::Normalizer2

#if defined(_MSC_VER)
  #pragma warning(default: 4512)
#endif

#include <unicode/translit.h> // for icu::Transliterator

#if defined(_MSC_VER)
  #pragma warning(disable: 4229)
#endif

#include <unicode/uclean.h> // for u_cleanup

#if defined(_MSC_VER)
  #pragma warning(default: 4229)
#endif

namespace iresearch {
namespace analysis {

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

struct icu_objects {
  bool valid() const noexcept {
    // 'break_iterator' indicates that 'icu_objects' struct initialized
    return nullptr != break_iterator;
  }

  void clear() noexcept {
    transliterator.reset();
    break_iterator.reset();
    normalizer = nullptr;
    stemmer.reset();
  }

  std::unique_ptr<icu::Transliterator> transliterator;
  std::unique_ptr<icu::BreakIterator> break_iterator;
  const icu::Normalizer2* normalizer{}; // reusable object owned by ICU
  stemmer_ptr stemmer;
};

struct text_token_stream::state_t : icu_objects {
  struct ngram_state_t {
    const byte_type* it{nullptr}; // iterator
    uint32_t length{0};
  };

  icu::UnicodeString data;
  icu::UnicodeString token;
  const options_t& options;
  const stopwords_t& stopwords;
  bstring term_buf;
  std::string tmp_buf; // used by processTerm(...)
  ngram_state_t ngram;
  bytes_ref term;
  uint32_t start{};
  uint32_t end{};

  state_t(const options_t& opts, const stopwords_t& stopw)
    : options(opts),
      stopwords(stopw) {
  }

  bool is_search_ngram() const {
    // if min or max or preserveOriginal are set then search ngram
    return options.min_gram_set || options.max_gram_set ||
      options.preserve_original_set;
  }

  bool is_ngram_finished() const {
    return 0 == ngram.length;
  }

  void set_ngram_finished() noexcept {
    ngram.length = 0;
  }
};

} // analysis
} // ROOT

namespace {

using namespace irs;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
struct cached_options_t: public analysis::text_token_stream::options_t {
  std::string key_;
  analysis::text_token_stream::stopwords_t stopwords_;

  cached_options_t(
      analysis::text_token_stream::options_t &&options,
      analysis::text_token_stream::stopwords_t&& stopwords)
    : analysis::text_token_stream::options_t(std::move(options)),
      stopwords_(std::move(stopwords)){
  }
};

absl::node_hash_map<hashed_string_ref, cached_options_t> cached_state_by_key;
std::mutex mutex;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a set of ignored words from FS at the specified custom path
////////////////////////////////////////////////////////////////////////////////
bool get_stopwords(
    analysis::text_token_stream::stopwords_t& buf,
    string_ref language,
    const string_ref& path = string_ref::NIL) {
  utf8_path stopword_path;

  auto* custom_stopword_path = !path.null()
    ? path.c_str()
    : irs::getenv(analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE);

  if (custom_stopword_path) {
    stopword_path.assign(custom_stopword_path);
    file_utils::ensure_absolute(stopword_path);
  } else {
    utf8_path::string_type cwd;
    file_utils::read_cwd(cwd);

    // use CWD if the environment variable STOPWORD_PATH_ENV_VARIABLE is undefined
    stopword_path = std::move(cwd);
  }

  try {
    bool result;
    stopword_path /= std::string_view(language);

    if (!file_utils::exists_directory(result, stopword_path.c_str()) || !result) {
      if (custom_stopword_path) {
        IR_FRMT_ERROR("Failed to load stopwords from path: %s", stopword_path.u8string().c_str());
        return false;
      } else {
        IR_FRMT_TRACE("Failed to load stopwords from default path: %s. "
                      "Analyzer will continue without stopwords",
                      stopword_path.u8string().c_str());
        return true;
      }
    }

    analysis::text_token_stream::stopwords_t stopwords;
    auto visitor = [&stopwords, &stopword_path](auto name)->bool {
      bool result;
      const auto path = stopword_path / name;

      if (!file_utils::exists_file(result, path.c_str())) {
        IR_FRMT_ERROR("Failed to identify stopword path: %s", path.u8string().c_str());

        return false;
      }

      if (!result) {
        return true; // skip non-files
      }

      std::ifstream in(path.native());

      if (!in) {
        IR_FRMT_ERROR("Failed to load stopwords from path: %s", path.u8string().c_str());

        return false;
      }

      for (std::string line; std::getline(in, line);) {
        size_t i = 0;

        // find first whitespace
        for (size_t length = line.size(); i < length && !std::isspace(line[i]); ++i);

        // skip lines starting with whitespace
        if (i > 0) {
          stopwords.insert(line.substr(0, i));
        }
      }

      return true;
    };

    if (!file_utils::visit_directory(stopword_path.c_str(), visitor, false)) {
      return !custom_stopword_path;
    }

    buf.insert(stopwords.begin(), stopwords.end());

    return true;
  } catch (...) {
    IR_FRMT_ERROR("Caught error while loading stopwords from path: %s", stopword_path.u8string().c_str());
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a set of stopwords for options
/// load rules:
/// 'explicit_stopwords' + 'stopwordsPath' = load from both
/// 'explicit_stopwords' only - load from 'explicit_stopwords'
/// 'stopwordsPath' only - load from 'stopwordsPath'
///  none (empty explicit_Stopwords  and flg explicit_stopwords_set not set) - load from default location
////////////////////////////////////////////////////////////////////////////////
bool build_stopwords(const analysis::text_token_stream::options_t& options,
                     analysis::text_token_stream::stopwords_t& buf) {
  if (!options.explicit_stopwords.empty()) {
    // explicit stopwords always go
    buf.insert(options.explicit_stopwords.begin(), options.explicit_stopwords.end());
  }

  if (options.stopwordsPath.empty() || options.stopwordsPath[0] != 0) {
    // we have a custom path. let`s try loading
    // if we have stopwordsPath - do not  try default location. Nothing to do there anymore
    return get_stopwords(buf, options.locale.getLanguage(), options.stopwordsPath);
  }
  else if (!options.explicit_stopwords_set && options.explicit_stopwords.empty()) {
    //  no stopwordsPath, explicit_stopwords empty and not marked as valid - load from defaults
    return get_stopwords(buf, options.locale.getLanguage());
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer based on the supplied cache_key and options
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr construct(
    const string_ref& cache_key,
    analysis::text_token_stream::options_t&& options,
    analysis::text_token_stream::stopwords_t&& stopwords) {
  auto generator = [](
      const hashed_string_ref& key,
      cached_options_t& value) noexcept {
    if (key.null()) {
      return key;
    }

    value.key_ = key;

    // reuse hash but point ref at value
    return hashed_string_ref(key.hash(), value.key_);
  };

  cached_options_t* options_ptr;

  {
    auto lock = make_lock_guard(mutex);

    options_ptr = &(map_utils::try_emplace_update_key(
      cached_state_by_key,
      generator,
      make_hashed_ref(cache_key),
      std::move(options),
      std::move(stopwords)).first->second);
  }

  return memory::make_unique<analysis::text_token_stream>(
      *options_ptr,
      options_ptr->stopwords_);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer based on the supplied cache_key
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr construct(icu::Locale&& locale) {
  if (locale.isBogus()) {
    return nullptr;
  }

  {
    auto lock = make_lock_guard(mutex);
    auto itr = cached_state_by_key.find(
      make_hashed_ref(string_ref(locale.getName())));

    if (itr != cached_state_by_key.end()) {
      return memory::make_unique<analysis::text_token_stream>(
        itr->second,
        itr->second.stopwords_);
    }
  }

  try {
    analysis::text_token_stream::options_t options;
    analysis::text_token_stream::stopwords_t stopwords;
    options.locale = locale;

    if (!build_stopwords(options, stopwords)) {
      IR_FRMT_WARN("Failed to retrieve 'stopwords' while constructing text_token_stream with cache key: %s",
        options.locale.getName());

      return nullptr;
    }

    return construct(options.locale.getName(), std::move(options), std::move(stopwords));
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing text_token_stream cache key: %s",
      locale.getName());
  }

  return nullptr;
}

bool process_term(
    analysis::text_token_stream::state_t& state,
    icu::UnicodeString&& data) {

  // normalize unicode
  auto err = UErrorCode::U_ZERO_ERROR; // a value that passes the U_SUCCESS() test

  state.normalizer->normalize(data, state.token, err);

  if (!U_SUCCESS(err)) {
    state.token = std::move(data); // use non-normalized value if normalization failure
  }

  // case-convert unicode
  switch (state.options.case_convert) {
   case analysis::text_token_stream::LOWER:
    state.token.toLower(state.options.locale); // inplace case-conversion
    break;
   case analysis::text_token_stream::UPPER:
    state.token.toUpper(state.options.locale); // inplace case-conversion
    break;
   default:
    {} // NOOP
  };

  // collate value, e.g. remove accents
  if (state.transliterator) {
    state.transliterator->transliterate(state.token); // inplace translitiration
  }

  std::string& word_utf8 = state.tmp_buf;

  word_utf8.clear();
  state.token.toUTF8String(word_utf8);

  // skip ignored tokens
  if (state.stopwords.contains(word_utf8)) {
    return false;
  }

  // find the token stem
  if (state.stemmer) {
    static_assert(sizeof(sb_symbol) == sizeof(char));
    const sb_symbol* value = reinterpret_cast<sb_symbol const*>(word_utf8.c_str());

    value = sb_stemmer_stem(state.stemmer.get(), value, (int)word_utf8.size());

    if (value) {
      static_assert(sizeof(byte_type) == sizeof(sb_symbol));
      state.term = bytes_ref(reinterpret_cast<const byte_type*>(value),
                             sb_stemmer_length(state.stemmer.get()));

      return true;
    }
  }

  // use the value of the unstemmed token
  static_assert(sizeof(byte_type) == sizeof(char));
  state.term_buf.assign(reinterpret_cast<const byte_type*>(word_utf8.c_str()), word_utf8.size());
  state.term = state.term_buf;

  return true;
}

constexpr std::string_view LOCALE_PARAM_NAME            {"locale"};
constexpr std::string_view CASE_CONVERT_PARAM_NAME      {"case"};
constexpr std::string_view STOPWORDS_PARAM_NAME         {"stopwords"};
constexpr std::string_view STOPWORDS_PATH_PARAM_NAME    {"stopwordsPath"};
constexpr std::string_view ACCENT_PARAM_NAME            {"accent"};
constexpr std::string_view STEMMING_PARAM_NAME          {"stemming"};
constexpr std::string_view EDGE_NGRAM_PARAM_NAME        {"edgeNgram"};
constexpr std::string_view MIN_PARAM_NAME               {"min"};
constexpr std::string_view MAX_PARAM_NAME               {"max"};
constexpr std::string_view PRESERVE_ORIGINAL_PARAM_NAME {"preserveOriginal"};

const frozen::unordered_map<
    string_ref,
    analysis::text_token_stream::case_convert_t, 3> CASE_CONVERT_MAP = {
  { "lower", analysis::text_token_stream::case_convert_t::LOWER },
  { "none", analysis::text_token_stream::case_convert_t::NONE },
  { "upper", analysis::text_token_stream::case_convert_t::UPPER },
};

bool init_from_options(const analysis::text_token_stream::options_t& options,
                       analysis::icu_objects* objects,
                       bool print_errors) {

  auto err = UErrorCode::U_ZERO_ERROR; // a value that passes the U_SUCCESS() test

  // reusable object owned by ICU
  objects->normalizer = icu::Normalizer2::getNFCInstance(err);

  if (!U_SUCCESS(err) || !objects->normalizer) {
    objects->normalizer = nullptr;

    if (print_errors) {
      IR_FRMT_WARN(
        "Warning while instantiation icu::Normalizer2 for text_token_stream from locale '%s' : '%s'",
        options.locale.getName(), u_errorName(err));
    }

    return false;
  }

  if (!options.accent) {
    // transliteration rule taken verbatim from: http://userguide.icu-project.org/transforms/general
    const icu::UnicodeString collationRule("NFD; [:Nonspacing Mark:] Remove; NFC"); // do not allocate statically since it causes memory leaks in ICU

    // reusable object owned by *this
    objects->transliterator.reset(icu::Transliterator::createInstance(
      collationRule, UTransDirection::UTRANS_FORWARD, err));

    if (!U_SUCCESS(err) || !objects->transliterator) {
      objects->transliterator.reset();

      if (print_errors) {
        IR_FRMT_WARN(
          "Warning while instantiation icu::Transliterator for text_token_stream from locale '%s' : '%s'",
          options.locale.getName(), u_errorName(err));
      }

      return false;
    }
  }

  // reusable object owned by *this
  objects->break_iterator.reset(
    icu::BreakIterator::createWordInstance(options.locale, err));

  if (!U_SUCCESS(err) || !objects->break_iterator) {
    objects->break_iterator.reset();

    if (print_errors) {
      IR_FRMT_WARN(
        "Warning while instantiation icu::BreakIterator for text_token_stream from locale '%s' : '%s'",
        options.locale.getName(), u_errorName(err));
    }

    return false;
  }

  // optional since not available for all locales
  if (options.stemming) {
    // reusable object owned by *this
    objects->stemmer = make_stemmer_ptr(options.locale.getLanguage(), nullptr); // defaults to utf-8

    if (!objects->stemmer && print_errors) {
      IR_FRMT_WARN(
        "Failed to create stemmer for text_token_stream from locale '%s'",
        options.locale.getName());
    }
  }

  return true;
}

bool locale_from_string(std::string locale_name, icu::Locale& locale) {
  locale = icu::Locale::createFromName(locale_name.c_str());

  if (!locale.isBogus()) {
    locale = icu::Locale{
      locale.getLanguage(),
      locale.getCountry(),
      locale.getVariant() };
  }

  if (locale.isBogus()) {
    IR_FRMT_WARN(
      "Failed to instantiate locale from the supplied string '%s'"
      "while constructing text_token_stream from VPack arguments",
      locale_name.c_str());

    return false;
  }

  return true;
}

bool locale_from_slice(VPackSlice slice, icu::Locale& locale) {
  if (!slice.isString()) {
    IR_FRMT_WARN(
      "Non-string value in '%s' while constructing "
      "text_token_stream from VPack arguments",
      LOCALE_PARAM_NAME.data());

    return false;
  }

  return locale_from_string(get_string<std::string>(slice), locale);
}

bool parse_vpack_options(const VPackSlice slice,
                         analysis::text_token_stream::options_t& options) {
  if (!slice.isObject() || !slice.hasKey(LOCALE_PARAM_NAME) ||
      !slice.get(LOCALE_PARAM_NAME).isString()) {
    IR_FRMT_WARN(
      "Missing '%s' while constructing text_token_stream from VPack ",
      LOCALE_PARAM_NAME.data());

    return false;
  }

  try {
    if (!locale_from_slice(slice.get(LOCALE_PARAM_NAME), options.locale)) {
      return false;
    }

    if (slice.hasKey(CASE_CONVERT_PARAM_NAME)) {
      auto case_convert_slice = slice.get(CASE_CONVERT_PARAM_NAME);  // optional string enum

      if (!case_convert_slice.isString()) {
        IR_FRMT_WARN(
          "Non-string value in '%s' while constructing text_token_stream "
          "from VPack arguments",
          CASE_CONVERT_PARAM_NAME.data());

        return false;
      }

      auto itr = CASE_CONVERT_MAP.find(get_string<string_ref>(case_convert_slice));

      if (itr == CASE_CONVERT_MAP.end()) {
        IR_FRMT_WARN(
          "Invalid value in '%s' while constructing text_token_stream from "
          "VPack arguments",
          CASE_CONVERT_PARAM_NAME.data());

        return false;
      }

      options.case_convert = itr->second;
    }

    if (slice.hasKey(STOPWORDS_PARAM_NAME)) {
      auto stop_words_slice = slice.get(STOPWORDS_PARAM_NAME);  // optional string array
      if (!stop_words_slice.isArray()) {
        IR_FRMT_WARN(
          "Invalid value in '%s' while constructing text_token_stream from "
          "VPack arguments",
          STOPWORDS_PARAM_NAME.data());

        return false;
      }
      options.explicit_stopwords_set = true;  // mark  - we have explicit list (even if it is empty)
      for (const auto& itr : VPackArrayIterator(stop_words_slice)) {
        if (!itr.isString()) {
          IR_FRMT_WARN(
            "Non-string value in '%s' while constructing text_token_stream "
            "from VPack arguments",
            STOPWORDS_PARAM_NAME.data());

          return false;
        }
        options.explicit_stopwords.emplace(get_string<std::string>(itr));
      }
    }

    if (slice.hasKey(STOPWORDS_PATH_PARAM_NAME)) {
        auto ignored_words_path_slice = slice.get(STOPWORDS_PATH_PARAM_NAME);  // optional string

        if (!ignored_words_path_slice.isString()) {
          IR_FRMT_WARN(
            "Non-string value in '%s' while constructing text_token_stream "
            "from VPack arguments",
            STOPWORDS_PATH_PARAM_NAME.data());

          return false;
        }
        options.stopwordsPath = get_string<std::string>(ignored_words_path_slice);
    }

    if (slice.hasKey(ACCENT_PARAM_NAME)) {
      auto accent_slice = slice.get(ACCENT_PARAM_NAME);  // optional bool

      if (!accent_slice.isBool()) {
        IR_FRMT_WARN(
          "Non-boolean value in '%s' while constructing text_token_stream "
          "from VPack arguments",
          ACCENT_PARAM_NAME.data());

        return false;
      }
      options.accent = accent_slice.getBool();
    }

    if (slice.hasKey(STEMMING_PARAM_NAME)) {
      auto stemming_slice = slice.get(STEMMING_PARAM_NAME);  // optional bool

      if (!stemming_slice.isBool()) {
        IR_FRMT_WARN(
          "Non-boolean value in '%s' while constructing text_token_stream "
          "from VPack arguments",
          STEMMING_PARAM_NAME.data());

        return false;
      }

      options.stemming = stemming_slice.getBool();
    }

    if (slice.hasKey(EDGE_NGRAM_PARAM_NAME)) {
      auto ngram_slice = slice.get(EDGE_NGRAM_PARAM_NAME);

      if (!ngram_slice.isObject()) {
        IR_FRMT_WARN(
          "Non-object value in '%s' while constructing text_token_stream "
          "from VPack arguments",
          EDGE_NGRAM_PARAM_NAME.data());

        return false;
      }

      if(ngram_slice.hasKey(MIN_PARAM_NAME) &&
         ngram_slice.get(MIN_PARAM_NAME).isNumber<decltype (options.min_gram)>()) {

        options.min_gram = ngram_slice.get(MIN_PARAM_NAME).getNumber<decltype (options.min_gram)>();
        options.min_gram_set = true;
      }

      if(ngram_slice.hasKey(MAX_PARAM_NAME) &&
         ngram_slice.get(MAX_PARAM_NAME).isNumber<decltype (options.min_gram)>()) {

        options.max_gram = ngram_slice.get(MAX_PARAM_NAME).getNumber<decltype (options.min_gram)>();
        options.max_gram_set = true;
      }

      if(ngram_slice.hasKey(PRESERVE_ORIGINAL_PARAM_NAME) &&
         ngram_slice.get(PRESERVE_ORIGINAL_PARAM_NAME).isBool()) {

        options.preserve_original = ngram_slice.get(PRESERVE_ORIGINAL_PARAM_NAME).getBool();
        options.preserve_original_set = true;
      }


      if (options.min_gram_set && options.max_gram_set) {
        return options.min_gram <= options.max_gram;
      }
    }

    analysis::icu_objects obj;
    init_from_options(options, &obj, true);

    return true;
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing text_token_stream from VPack",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing text_token_stream from VPack arguments");
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param options reference to analyzer options storage
/// @param definition string for storing json document with config
///////////////////////////////////////////////////////////////////////////////
bool make_vpack_config(
    const analysis::text_token_stream::options_t& options,
    VPackBuilder* builder) {

  VPackObjectBuilder object(builder);
  {
    // locale
    const auto locale_name = options.locale.getBaseName();
    builder->add(LOCALE_PARAM_NAME, VPackValue(locale_name));

    // case convert
    auto case_value = std::find_if(CASE_CONVERT_MAP.begin(), CASE_CONVERT_MAP.end(),
      [&options](const decltype(CASE_CONVERT_MAP)::value_type& v) {
        return v.second == options.case_convert;
      });

    if (case_value != CASE_CONVERT_MAP.end()) {
      builder->add(CASE_CONVERT_PARAM_NAME, VPackValue(case_value->first));
    } else {
      IR_FRMT_ERROR(
        "Invalid case_convert value in text analyzer options: %d",
        static_cast<int>(options.case_convert));
      return false;
    }

    // stopwords
    if(!options.explicit_stopwords.empty() || options.explicit_stopwords_set) {
      // explicit_stopwords_set  marks that even empty stopwords list is valid
      std::vector<string_ref> sortedWords;
      if (!options.explicit_stopwords.empty()) {
        // for simplifying comparison between properties we need deterministic order of stopwords
        sortedWords.reserve(options.explicit_stopwords.size());
        for (const auto& stopword : options.explicit_stopwords) {
          // cppcheck-suppress useStlAlgorithm
          sortedWords.emplace_back(stopword);
        }
        std::sort(sortedWords.begin(), sortedWords.end());
      }
      {
        VPackArrayBuilder array(builder, STOPWORDS_PARAM_NAME.data());

        for (const auto& stopword : sortedWords) {
            builder->add(VPackValue(stopword));
        }
      }
    }

    // Accent
    builder->add(ACCENT_PARAM_NAME, VPackValue(options.accent));

    //Stem
    builder->add(STEMMING_PARAM_NAME, VPackValue(options.stemming));

    //stopwords path
    if (options.stopwordsPath.empty() || options.stopwordsPath[0] != 0 ) {
      // if stopwordsPath is set  - output it (empty string is also valid value =  use of CWD)
      builder->add(STOPWORDS_PATH_PARAM_NAME, VPackValue(options.stopwordsPath));
    }
  }

  // ensure disambiguating casts below are safe. Casts required for clang compiler on Mac
  static_assert(sizeof(uint64_t) >= sizeof(size_t), "sizeof(uint64_t) >= sizeof(size_t)");

  if (options.min_gram_set || options.max_gram_set || options.preserve_original_set) {

    VPackObjectBuilder sub_object(builder, EDGE_NGRAM_PARAM_NAME.data());

    // min_gram
    if (options.min_gram_set) {
      builder->add(MIN_PARAM_NAME, VPackValue(static_cast<uint64_t>(options.min_gram)));
    }

    // max_gram
    if (options.max_gram_set) {
      builder->add(MAX_PARAM_NAME, VPackValue(static_cast<uint64_t>(options.max_gram)));
    }

    // preserve_original
    if (options.preserve_original_set) {
      builder->add(PRESERVE_ORIGINAL_PARAM_NAME, VPackValue(options.preserve_original));
    }
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "locale"(string): locale of the analyzer <required>
///        "case"(string enum): modify token case using "locale"
///        "accent"(bool): leave accents
///        "stemming"(bool): use stemming
///        "stopwords([string...]): set of words to ignore
///        "stopwordsPath"(string): custom path, where to load stopwords
///        "min" (number): minimum ngram size
///        "max" (number): maximum ngram size
///        "preserveOriginal" (boolean): preserve or not the original term
///  if none of stopwords and stopwordsPath specified, stopwords are loaded from default location
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  try {
    const string_ref slice_ref(slice.startAs<char>(), slice.byteSize());
    {
      auto lock = make_lock_guard(mutex);
      auto itr = cached_state_by_key.find(make_hashed_ref(slice_ref));

      if (itr != cached_state_by_key.end()) {
        return memory::make_unique<analysis::text_token_stream>(
            itr->second, itr->second.stopwords_);
      }
    }

    analysis::text_token_stream::options_t options;
    if (parse_vpack_options(slice, options)) {
      analysis::text_token_stream::stopwords_t stopwords;
      if (!build_stopwords(options, stopwords)) {
        IR_FRMT_WARN(
          "Failed to retrieve 'stopwords' from path while constructing "
          "text_token_stream from VPack arguments");

        return nullptr;
      }
      return construct(slice_ref, std::move(options), std::move(stopwords));
    }
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing text_token_stream from VPack arguments");
  }
  return nullptr;
}

analysis::analyzer::ptr make_vpack(const string_ref& args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  return make_vpack(slice);
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* vpack_builder) {
  analysis::text_token_stream::options_t options;
  if (parse_vpack_options(slice, options)) {
    return make_vpack_config(options, vpack_builder);
  } else {
    return false;
  }
}

bool normalize_vpack_config(const string_ref& args, std::string& definition) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  VPackBuilder builder;
  bool res = normalize_vpack_config(slice, &builder);
  if (res) {
    definition.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a locale name
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_text(const string_ref& args) {
  icu::Locale locale;

  if (locale_from_string(static_cast<std::string>(args), locale)) {
    return construct(std::move(locale));
  } else {
    return nullptr;
  }
}

bool normalize_text_config(const string_ref& args,
                           std::string& definition) {
  icu::Locale locale;

  if (locale_from_string(static_cast<std::string>(args), locale)) {
    definition = locale.getBaseName();
    return true;
  }

  return false;
}

analysis::analyzer::ptr make_json(const string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while constructing text_token_normalizing_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    return make_vpack(vpack->slice());
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing text_token_normalizing_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing text_token_normalizing_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(const string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing text_token_normalizing_stream");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    VPackBuilder builder;
    if (normalize_vpack_config(vpack->slice(), &builder)) {
      definition = builder.toString();
      return !definition.empty();
    }
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while normalizing text_token_normalizing_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while normalizing text_token_normalizing_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_VPACK(analysis::text_token_stream, make_vpack,
                       normalize_vpack_config);
REGISTER_ANALYZER_JSON(analysis::text_token_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_TEXT(analysis::text_token_stream, make_text,
                       normalize_text_config);
}

namespace iresearch {
namespace analysis {

void text_token_stream::state_deleter_t::operator()(state_t* p) const noexcept {
  delete p;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

char const* text_token_stream::STOPWORD_PATH_ENV_VARIABLE = "IRESEARCH_TEXT_STOPWORD_PATH";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

text_token_stream::text_token_stream(
    const options_t& options,
    const stopwords_t& stopwords)
  : analyzer{ irs::type<text_token_stream>::get() },
    state_{new state_t{options, stopwords}}
{
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

/*static*/ void text_token_stream::init() {
  REGISTER_ANALYZER_VPACK(analysis::text_token_stream, make_vpack,
                         normalize_vpack_config); // match registration above
  REGISTER_ANALYZER_JSON(text_token_stream, make_json,
                         normalize_json_config);  // match registration above
  REGISTER_ANALYZER_TEXT(text_token_stream, make_text,
                         normalize_text_config); // match registration above
}

/*static*/ void text_token_stream::clear_cache() {
  // cppcheck-suppress unreadVariable
  auto lock = make_lock_guard(::mutex);
  cached_state_by_key.clear();
}

/*static*/ analyzer::ptr text_token_stream::make(const string_ref& locale) {
  return make_text(locale);
}

bool text_token_stream::reset(const string_ref& data) {
  if (data.size() > std::numeric_limits<uint32_t>::max()) {
    // can't handle data which is longer than std::numeric_limits<uint32_t>::max()
    return false;
  }

  // reset term attribute
  std::get<term_attribute>(attrs_).value = bytes_ref::NIL;

  // reset offset attribute
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = std::numeric_limits<uint32_t>::max();
  offset.end = std::numeric_limits<uint32_t>::max();

  if (!state_->valid() && !init_from_options(state_->options, state_.get(), false)) {
    state_->clear();
    return false;
  }

  // Create ICU UnicodeString
  if (data.size() > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return false;
  }

  state_->data = icu::UnicodeString::fromUTF8(
    icu::StringPiece{data.c_str(), static_cast<int32_t>(data.size())});

  // tokenise the unicode data
  state_->break_iterator->setText(state_->data);

  // reset term state for ngrams
  state_->term = bytes_ref::NIL;
  state_->start = std::numeric_limits<uint32_t>::max();
  state_->end = std::numeric_limits<uint32_t>::max();
  state_->set_ngram_finished();
  std::get<increment>(attrs_).value = 1;

  return true;
}

bool text_token_stream::next() {
  if (state_->is_search_ngram()) {
    while (true) {
      // if a word has not started for ngrams yet
      if (state_->is_ngram_finished() && !next_word()) {
        return false;
      }
      // get next ngram taking into account min and max
      if (next_ngram()) {
        return true;
      }
    }
  } else if (next_word()) {
    std::get<term_attribute>(attrs_).value = state_->term;

    auto& offset = std::get<irs::offset>(attrs_);
    offset.start = state_->start;
    offset.end = state_->end;

    return true;
  }

  return false;
}

bool text_token_stream::next_word() {
  // find boundaries of the next word
  for (auto start = state_->break_iterator->current(),
       end = state_->break_iterator->next();
       icu::BreakIterator::DONE != end;
       start = end, end = state_->break_iterator->next()) {
    // skip whitespace and unsuccessful terms
    if (UWordBreak::UBRK_WORD_NONE == state_->break_iterator->getRuleStatus()
        || !process_term(*state_, state_->data.tempSubString(start, end - start))) {
      continue;
    }

    state_->start = start;
    state_->end = end;
    return true;
  }

  return false;
}

bool text_token_stream::next_ngram() {
  auto begin = state_->term.begin();
  auto end = state_->term.end();
  assert(begin != end);

  auto& inc = std::get<increment>(attrs_);

  // if there are no ngrams yet then a new word started
  if (state_->is_ngram_finished()) {
    state_->ngram.it = begin;
    inc.value = 1;
    // find the first ngram > min
    do {
      state_->ngram.it = utf8_utils::next(state_->ngram.it, end);
    } while (++state_->ngram.length < state_->options.min_gram &&
             state_->ngram.it != end);
  } else {
    // not first ngram in a word
    inc.value = 0; // staying on the current pos
    state_->ngram.it = utf8_utils::next(state_->ngram.it, end);
    ++state_->ngram.length;
  }

  bool finished{};
  // cppcheck-suppress unreadVariable
  auto set_ngram_finished = make_finally([this, &finished]()noexcept->void {
    if (finished) {
      state_->set_ngram_finished();
    }
  });

  // if a word has finished
  if (state_->ngram.it == end) {
    // no unwatched ngrams in a word
    finished = true;
  }

  // if length > max
  if (state_->options.max_gram_set &&
      state_->ngram.length > state_->options.max_gram) {
    // no unwatched ngrams in a word
    finished = true;
    if (state_->options.preserve_original) {
      state_->ngram.it = end;
    } else {
      return false;
    }
  }

  // if length >= min or preserveOriginal
  if (state_->ngram.length >= state_->options.min_gram ||
      state_->options.preserve_original) {
    // ensure disambiguating casts below are safe. Casts required for clang compiler on Mac
    static_assert(sizeof(byte_type) == sizeof(char));

    auto size = static_cast<uint32_t>(std::distance(begin, state_->ngram.it));
    term_buf_.assign(state_->term.c_str(), size);
    std::get<term_attribute>(attrs_).value = term_buf_;

    auto& offset = std::get<irs::offset>(attrs_);
    offset.start = state_->start;
    offset.end = state_->start + size;

    return true;
  }

  return false;
}

} // analysis
} // ROOT
