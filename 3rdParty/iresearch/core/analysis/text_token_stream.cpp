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

#include <cctype> // for std::isspace(...)
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <rapidjson/rapidjson/document.h> // for rapidjson::Document, rapidjson::Value
#include <rapidjson/rapidjson/writer.h> // for rapidjson::Writer
#include <rapidjson/rapidjson/stringbuffer.h> // for rapidjson::StringBuffer
#include <utils/utf8_utils.hpp>
#include <unicode/brkiter.h> // for icu::BreakIterator

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

#include "libstemmer.h"

#include "utils/hash_utils.hpp"
#include "utils/json_utils.hpp"
#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/map_utils.hpp"
#include "utils/misc.hpp"
#include "utils/runtime_utils.hpp"
#include "utils/thread_utils.hpp"
#include "utils/utf8_path.hpp"

#include "text_token_stream.hpp"

namespace iresearch {
namespace analysis {

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

struct text_token_stream::state_t {
  struct ngram_state_t {
    const byte_type* it; // iterator
    uint32_t length{};
  };

  std::shared_ptr<icu::BreakIterator> break_iterator;
  icu::UnicodeString data;
  icu::Locale icu_locale;
  std::shared_ptr<const icu::Normalizer2> normalizer;
  const options_t& options;
  const stopwords_t& stopwords;
  std::shared_ptr<sb_stemmer> stemmer;
  std::string tmp_buf; // used by processTerm(...)
  std::shared_ptr<icu::Transliterator> transliterator;
  ngram_state_t ngram;
  bstring term_buf;
  bytes_ref term;
  uint32_t start{};
  uint32_t end{};
  state_t(const options_t& opts, const stopwords_t& stopw) :
    icu_locale("C"), options(opts), stopwords(stopw) {
    // NOTE: use of the default constructor for Locale() or
    //       use of Locale::createFromName(nullptr)
    //       causes a memory leak with Boost 1.58, as detected by valgrind
    icu_locale.setToBogus(); // set to uninitialized
  }

  bool is_search_ngram() const {
    // if min or max or preserveOriginal are set then search ngram
    return options.min_gram_set || options.max_gram_set ||
      options.preserve_original_set;
  }

  bool is_ngram_finished() const {
    return 0 == ngram.length;
  }

  void set_ngram_finished() {
    ngram.length = 0;
  }
};

} // analysis
} // ROOT

namespace {

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------
struct cached_options_t: public irs::analysis::text_token_stream::options_t {
  std::string key_;
  irs::analysis::text_token_stream::stopwords_t stopwords_;

  cached_options_t(
      irs::analysis::text_token_stream::options_t &&options, 
      irs::analysis::text_token_stream::stopwords_t&& stopwords)
    : irs::analysis::text_token_stream::options_t(std::move(options)),
      stopwords_(std::move(stopwords)){
  }
};


static std::unordered_map<irs::hashed_string_ref, cached_options_t> cached_state_by_key;
static std::mutex mutex;
static auto icu_cleanup = irs::make_finally([]()->void{
  // this call will release/free all memory used by ICU (for all users)
  // very dangerous to call if ICU is still in use by some other code
  //u_cleanup();
});

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief retrieves a set of ignored words from FS at the specified custom path
////////////////////////////////////////////////////////////////////////////////
bool get_stopwords(
    irs::analysis::text_token_stream::stopwords_t& buf,
    const std::locale& locale,
    const irs::string_ref& path = irs::string_ref::NIL) {
  auto language = irs::locale_utils::language(locale);
  irs::utf8_path stopword_path;
  auto* custom_stopword_path =
    !path.null()
    ? path.c_str()
    : irs::getenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE);

  if (custom_stopword_path) {
    bool absolute;

    stopword_path = irs::utf8_path(custom_stopword_path);

    if (!stopword_path.absolute(absolute)) {
      IR_FRMT_ERROR("Failed to determine absoluteness of path: %s",
                    stopword_path.utf8().c_str());

      return false;
    }

    if (!absolute) {
      stopword_path = irs::utf8_path(true) /= custom_stopword_path;
    }
  }
  else {
    // use CWD if the environment variable STOPWORD_PATH_ENV_VARIABLE is undefined
    stopword_path = irs::utf8_path(true);
  }

  try {
    bool result;

    if (!stopword_path.exists_directory(result) || !result
        || !(stopword_path /= language).exists_directory(result) || !result) {
      IR_FRMT_ERROR("Failed to load stopwords from path: %s", stopword_path.utf8().c_str());

      return !custom_stopword_path;
    }

    irs::analysis::text_token_stream::stopwords_t stopwords;
    auto visitor = [&stopwords, &stopword_path](
        const irs::utf8_path::native_char_t* name)->bool {
      auto path = stopword_path;
      bool result;

      path /= name;

      if (!path.exists_file(result)) {
        IR_FRMT_ERROR("Failed to identify stopword path: %s", path.utf8().c_str());

        return false;
      }

      if (!result) {
        return true; // skip non-files
      }

      std::ifstream in(path.native());

      if (!in) {
        IR_FRMT_ERROR("Failed to load stopwords from path: %s", path.utf8().c_str());

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

    if (!stopword_path.visit_directory(visitor, false)) {
      return !custom_stopword_path;
    }

    buf.insert(stopwords.begin(), stopwords.end());

    return true;
  } catch (...) {
    IR_FRMT_ERROR("Caught error while loading stopwords from path: %s", stopword_path.utf8().c_str());
    IR_LOG_EXCEPTION();
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
bool build_stopwords(const irs::analysis::text_token_stream::options_t& options,
                     irs::analysis::text_token_stream::stopwords_t& buf) {
  if (!options.explicit_stopwords.empty()) {
    // explicit stopwords always go
    buf.insert(options.explicit_stopwords.begin(), options.explicit_stopwords.end()); 
  }

  if (options.stopwordsPath.empty() || options.stopwordsPath[0] != 0) {
    // we have a custom path. let`s try loading
    // if we have stopwordsPath - do not  try default location. Nothing to do there anymore
    return get_stopwords(buf, options.locale, options.stopwordsPath); 
  }
  else if (!options.explicit_stopwords_set && options.explicit_stopwords.empty()) {
    //  no stopwordsPath, explicit_stopwords empty and not marked as valid - load from defaults
    return get_stopwords(buf, options.locale);
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer based on the supplied cache_key and options
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key,
  irs::analysis::text_token_stream::options_t&& options,
  irs::analysis::text_token_stream::stopwords_t&& stopwords
) {
  static auto generator = [](
      const irs::hashed_string_ref& key,
      cached_options_t& value
  ) noexcept {
    if (key.null()) {
      return key;
    }

    value.key_ = key;

    // reuse hash but point ref at value
    return irs::hashed_string_ref(key.hash(), value.key_);
  };
  cached_options_t* options_ptr;

  {
    SCOPED_LOCK(mutex);

    options_ptr = &(irs::map_utils::try_emplace_update_key(
      cached_state_by_key,
      generator,
      irs::make_hashed_ref(cache_key),
      std::move(options),
      std::move(stopwords)
    ).first->second);
  }

  return irs::memory::make_unique<irs::analysis::text_token_stream>(
    *options_ptr,
    options_ptr->stopwords_
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer based on the supplied cache_key
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const std::locale& locale
) {
  const auto& cache_key = irs::locale_utils::name(locale);
  {
    SCOPED_LOCK(mutex);
    auto itr = cached_state_by_key.find(
      irs::make_hashed_ref(irs::string_ref(cache_key))
    );

    if (itr != cached_state_by_key.end()) {
      return irs::memory::make_unique<irs::analysis::text_token_stream>(
        itr->second,
        itr->second.stopwords_
      );
    }
  }

  try {
    irs::analysis::text_token_stream::options_t options;
    irs::analysis::text_token_stream::stopwords_t stopwords;
    options.locale = locale; 
    
    if (!build_stopwords(options, stopwords)) {
      IR_FRMT_WARN("Failed to retrieve 'stopwords' while constructing text_token_stream with cache key: %s", 
                   cache_key.c_str());

      return nullptr;
    }

    return construct(cache_key, std::move(options), std::move(stopwords));
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing text_token_stream cache key: %s", 
                  cache_key.c_str());
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

bool process_term(
  irs::analysis::text_token_stream::state_t& state,
  icu::UnicodeString const& data
) {
  // ...........................................................................
  // normalize unicode
  // ...........................................................................
  icu::UnicodeString word;
  auto err = UErrorCode::U_ZERO_ERROR; // a value that passes the U_SUCCESS() test

  state.normalizer->normalize(data, word, err);

  if (!U_SUCCESS(err)) {
    word = data; // use non-normalized value if normalization failure
  }

  // ...........................................................................
  // case-convert unicode
  // ...........................................................................
  switch (state.options.case_convert) {
   case irs::analysis::text_token_stream::options_t::case_convert_t::LOWER:
    word.toLower(state.icu_locale); // inplace case-conversion
    break;
   case irs::analysis::text_token_stream::options_t::case_convert_t::UPPER:
    word.toUpper(state.icu_locale); // inplace case-conversion
    break;
   default:
    {} // NOOP
  };

  // ...........................................................................
  // collate value, e.g. remove accents
  // ...........................................................................
  if (state.transliterator) {
    state.transliterator->transliterate(word); // inplace translitiration
  }

  std::string& word_utf8 = state.tmp_buf;

  word_utf8.clear();
  word.toUTF8String(word_utf8);

  // ...........................................................................
  // skip ignored tokens
  // ...........................................................................
  if (state.stopwords.find(word_utf8) != state.stopwords.end()) {
    return false;
  }

  // ...........................................................................
  // find the token stem
  // ...........................................................................
  if (state.stemmer) {
    static_assert(sizeof(sb_symbol) == sizeof(char), "sizeof(sb_symbol) != sizeof(char)");
    const sb_symbol* value = reinterpret_cast<sb_symbol const*>(word_utf8.c_str());

    value = sb_stemmer_stem(state.stemmer.get(), value, (int)word_utf8.size());

    if (value) {
      static_assert(sizeof(irs::byte_type) == sizeof(sb_symbol), "sizeof(irs::byte_type) != sizeof(sb_symbol)");
      state.term = irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(value),
                                  sb_stemmer_length(state.stemmer.get()));

      return true;
    }
  }

  // ...........................................................................
  // use the value of the unstemmed token
  // ...........................................................................
  static_assert(sizeof(irs::byte_type) == sizeof(char), "sizeof(irs::byte_type) != sizeof(char)");
  state.term_buf.assign(reinterpret_cast<const irs::byte_type*>(word_utf8.c_str()), word_utf8.size());
  state.term = state.term_buf;

  return true;
}

bool make_locale_from_name(const irs::string_ref& name,
                          std::locale& locale) {
  try {
    locale = irs::locale_utils::locale(
        name, irs::string_ref::NIL,
        true);  // true == convert to unicode, required for ICU and Snowball
    // check if ICU supports locale
    auto icu_locale = icu::Locale(
      std::string(irs::locale_utils::language(locale)).c_str(),
      std::string(irs::locale_utils::country(locale)).c_str()
    );
    return !icu_locale.isBogus();
  } catch (...) {
    IR_FRMT_ERROR(
        "Caught error while constructing locale from "
        "name: %s",
        name.c_str());
    IR_LOG_EXCEPTION();
  }
  return false;
}


const irs::string_ref LOCALE_PARAM_NAME            = "locale";
const irs::string_ref CASE_CONVERT_PARAM_NAME      = "case";
const irs::string_ref STOPWORDS_PARAM_NAME         = "stopwords";
const irs::string_ref STOPWORDS_PATH_PARAM_NAME    = "stopwordsPath";
const irs::string_ref ACCENT_PARAM_NAME            = "accent";
const irs::string_ref STEMMING_PARAM_NAME          = "stemming";
const irs::string_ref EDGE_NGRAM_PARAM_NAME        = "edgeNgram";
const irs::string_ref MIN_PARAM_NAME               = "min";
const irs::string_ref MAX_PARAM_NAME               = "max";
const irs::string_ref PRESERVE_ORIGINAL_PARAM_NAME = "preserveOriginal";

const std::unordered_map<
    std::string, 
    irs::analysis::text_token_stream::options_t::case_convert_t> CASE_CONVERT_MAP = {
  { "lower", irs::analysis::text_token_stream::options_t::case_convert_t::LOWER },
  { "none", irs::analysis::text_token_stream::options_t::case_convert_t::NONE },
  { "upper", irs::analysis::text_token_stream::options_t::case_convert_t::UPPER },
};


bool parse_json_options(const irs::string_ref& args,
                        irs::analysis::text_token_stream::options_t& options) {
  rapidjson::Document json;
  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing text_token_stream, "
        "arguments: %s",
        args.c_str());

    return false;
  }

  if (json.IsString()) {
    return make_locale_from_name(args, options.locale);
  }

  if (!json.IsObject() || !json.HasMember(LOCALE_PARAM_NAME.c_str()) ||
      !json[LOCALE_PARAM_NAME.c_str()].IsString()) {
    IR_FRMT_WARN(
        "Missing '%s' while constructing text_token_stream from jSON "
        "arguments: %s",
        LOCALE_PARAM_NAME.c_str(), args.c_str());

    return false;
  }

  try {
    if (!make_locale_from_name(json[LOCALE_PARAM_NAME.c_str()].GetString(),
                              options.locale)) {
      return false;
    }
    if (json.HasMember(CASE_CONVERT_PARAM_NAME.c_str())) {
      auto& case_convert =
          json[CASE_CONVERT_PARAM_NAME.c_str()];  // optional string enum

      if (!case_convert.IsString()) {
        IR_FRMT_WARN(
            "Non-string value in '%s' while constructing text_token_stream "
            "from jSON arguments: %s",
            CASE_CONVERT_PARAM_NAME.c_str(), args.c_str());

        return false;
      }

      auto itr = CASE_CONVERT_MAP.find(case_convert.GetString());

      if (itr == CASE_CONVERT_MAP.end()) {
        IR_FRMT_WARN(
            "Invalid value in '%s' while constructing text_token_stream from "
            "jSON arguments: %s",
            CASE_CONVERT_PARAM_NAME.c_str(), args.c_str());

        return false;
      }

      options.case_convert = itr->second;
    }

    if (json.HasMember(STOPWORDS_PARAM_NAME.c_str())) {
      auto& stop_words =
          json[STOPWORDS_PARAM_NAME.c_str()];  // optional string array

      if (!stop_words.IsArray()) {
        IR_FRMT_WARN(
            "Invalid value in '%s' while constructing text_token_stream from "
            "jSON arguments: %s",
            STOPWORDS_PARAM_NAME.c_str(), args.c_str());

        return false;
      }
      options.explicit_stopwords_set =
          true;  // mark  - we have explicit list (even if it is empty)
      for (auto itr = stop_words.Begin(), end = stop_words.End(); itr != end;
           ++itr) {
        if (!itr->IsString()) {
          IR_FRMT_WARN(
              "Non-string value in '%s' while constructing text_token_stream "
              "from jSON arguments: %s",
              STOPWORDS_PARAM_NAME.c_str(), args.c_str());

          return false;
        }
        options.explicit_stopwords.emplace(itr->GetString());
      }
      if (json.HasMember(STOPWORDS_PATH_PARAM_NAME.c_str())) {
        auto& ignored_words_path =
            json[STOPWORDS_PATH_PARAM_NAME.c_str()];  // optional string

        if (!ignored_words_path.IsString()) {
          IR_FRMT_WARN(
              "Non-string value in '%s' while constructing text_token_stream "
              "from jSON arguments: %s",
              STOPWORDS_PATH_PARAM_NAME.c_str(), args.c_str());

          return false;
        }
        options.stopwordsPath = ignored_words_path.GetString();
      }
    } else if (json.HasMember(STOPWORDS_PATH_PARAM_NAME.c_str())) {
      auto& ignored_words_path =
          json[STOPWORDS_PATH_PARAM_NAME.c_str()];  // optional string

      if (!ignored_words_path.IsString()) {
        IR_FRMT_WARN(
            "Non-string value in '%s' while constructing text_token_stream "
            "from jSON arguments: %s",
            STOPWORDS_PATH_PARAM_NAME.c_str(), args.c_str());

        return false;
      }
      options.stopwordsPath = ignored_words_path.GetString();
    }

    if (json.HasMember(ACCENT_PARAM_NAME.c_str())) {
      auto& accent = json[ACCENT_PARAM_NAME.c_str()];  // optional bool

      if (!accent.IsBool()) {
        IR_FRMT_WARN(
            "Non-boolean value in '%s' while constructing text_token_stream "
            "from jSON arguments: %s",
            ACCENT_PARAM_NAME.c_str(), args.c_str());

        return false;
      }

      options.accent = accent.GetBool();
    }

    if (json.HasMember(STEMMING_PARAM_NAME.c_str())) {
      auto& stemming = json[STEMMING_PARAM_NAME.c_str()];  // optional bool

      if (!stemming.IsBool()) {
        IR_FRMT_WARN(
            "Non-boolean value in '%s' while constructing text_token_stream "
            "from jSON arguments: %s",
            STEMMING_PARAM_NAME.c_str(), args.c_str());

        return false;
      }

      options.stemming = stemming.GetBool();
    }

    if (json.HasMember(EDGE_NGRAM_PARAM_NAME.c_str())) {
      auto& ngram = json[EDGE_NGRAM_PARAM_NAME.c_str()];

      if (!ngram.IsObject()) {
        IR_FRMT_WARN(
            "Non-object value in '%s' while constructing text_token_stream "
            "from jSON arguments: %s",
            EDGE_NGRAM_PARAM_NAME.c_str(), args.c_str());

        return false;
      }

      uint64_t min;
      if (irs::get_uint64(ngram, MIN_PARAM_NAME, min)) {
        options.min_gram = min;
        options.min_gram_set = true;
      }

      uint64_t max;
      if (irs::get_uint64(ngram, MAX_PARAM_NAME, max)) {
        options.max_gram = max;
        options.max_gram_set = true;
      }

      bool preserve_original;
      if (irs::get_bool(ngram, PRESERVE_ORIGINAL_PARAM_NAME, preserve_original)) {
        options.preserve_original = preserve_original;
        options.preserve_original_set = true;
      }

      if (options.min_gram_set && options.max_gram_set) {
        return options.min_gram <= options.max_gram;
      }
    }

    return true;
  } catch (...) {
    IR_FRMT_ERROR(
        "Caught error while constructing text_token_stream from jSON "
        "arguments: %s",
        args.c_str());
    IR_LOG_EXCEPTION();
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param options reference to analyzer options storage
/// @param definition string for storing json document with config 
///////////////////////////////////////////////////////////////////////////////
bool make_json_config(
    const irs::analysis::text_token_stream::options_t& options,
    std::string& definition) {

  rapidjson::Document json;
  json.SetObject();

  rapidjson::Document::AllocatorType& allocator = json.GetAllocator();

  // locale
  {
    const auto& locale_name = irs::locale_utils::name(options.locale);
    json.AddMember(
        rapidjson::StringRef(LOCALE_PARAM_NAME.c_str(), LOCALE_PARAM_NAME.size()),
        rapidjson::Value(rapidjson::StringRef(locale_name.c_str(), 
                                              locale_name.length())),
        allocator);
  }
  // case convert
  {
    auto case_value = std::find_if(CASE_CONVERT_MAP.begin(), CASE_CONVERT_MAP.end(),
      [&options](const decltype(CASE_CONVERT_MAP)::value_type& v) {
        return v.second == options.case_convert; 
      });

    if (case_value != CASE_CONVERT_MAP.end()) {
      json.AddMember(
        rapidjson::StringRef(CASE_CONVERT_PARAM_NAME.c_str(), CASE_CONVERT_PARAM_NAME.size()),
        rapidjson::Value(case_value->first.c_str(),
                         static_cast<rapidjson::SizeType>(case_value->first.length())),
        allocator);
    } else {
      IR_FRMT_ERROR(
        "Invalid case_convert value in text analyzer options: %d",
        static_cast<int>(options.case_convert));
      return false;
    }
  }
 
  // stopwords
  if(!options.explicit_stopwords.empty() || options.explicit_stopwords_set) { 
    // explicit_stopwords_set  marks that even empty stopwords list is valid
    rapidjson::Value stopwordsArray(rapidjson::kArrayType);
    if (!options.explicit_stopwords.empty()) {
      // for simplifying comparison between properties we need deterministic order of stopwords
      std::vector<irs::string_ref> sortedWords;
      sortedWords.reserve(options.explicit_stopwords.size());
      for (const auto& stopword : options.explicit_stopwords) {
        sortedWords.emplace_back(stopword);
      }
      std::sort(sortedWords.begin(), sortedWords.end());
      for (const auto& stopword : sortedWords) {
        stopwordsArray.PushBack(
          rapidjson::StringRef(stopword.c_str(), stopword.size()),
          allocator);
      }
    }

    json.AddMember(
      rapidjson::StringRef(STOPWORDS_PARAM_NAME.c_str(), STOPWORDS_PARAM_NAME.size()),
      stopwordsArray.Move(),
      allocator);
  }

  // Accent
  json.AddMember(
    rapidjson::StringRef(ACCENT_PARAM_NAME.c_str(), ACCENT_PARAM_NAME.size()),
    rapidjson::Value(options.accent),
    allocator);

  //Stem
  json.AddMember(
    rapidjson::StringRef(STEMMING_PARAM_NAME.c_str(), STEMMING_PARAM_NAME.size()),
    rapidjson::Value(options.stemming),
    allocator);
  
  //stopwords path
  if (options.stopwordsPath.empty() || options.stopwordsPath[0] != 0 ) { 
    // if stopwordsPath is set  - output it (empty string is also valid value =  use of CWD)
    json.AddMember(
      rapidjson::StringRef(STOPWORDS_PATH_PARAM_NAME.c_str(), STOPWORDS_PATH_PARAM_NAME.size()),
      rapidjson::Value(options.stopwordsPath.c_str(),
                       static_cast<rapidjson::SizeType>(options.stopwordsPath.length())),
      allocator);
  }

  // ensure disambiguating casts below are safe. Casts required for clang compiler on Mac
  static_assert(sizeof(uint64_t) >= sizeof(size_t), "sizeof(uint64_t) >= sizeof(size_t)");

  if (options.min_gram_set || options.max_gram_set || options.preserve_original_set) {
    rapidjson::Document ngram(&allocator);
    ngram.SetObject();

    // min_gram
    if (options.min_gram_set) {
      ngram.AddMember(
        rapidjson::StringRef(MIN_PARAM_NAME.c_str(), MIN_PARAM_NAME.size()),
        rapidjson::Value(static_cast<uint64_t>(options.min_gram)),
        allocator);
    }

    // max_gram
    if (options.max_gram_set) {
      ngram.AddMember(
        rapidjson::StringRef(MAX_PARAM_NAME.c_str(), MAX_PARAM_NAME.size()),
        rapidjson::Value(static_cast<uint64_t>(options.max_gram)),
        allocator);
    }

    // preserve_original
    if (options.preserve_original_set) {
      ngram.AddMember(
        rapidjson::StringRef(PRESERVE_ORIGINAL_PARAM_NAME.c_str(), PRESERVE_ORIGINAL_PARAM_NAME.size()),
        rapidjson::Value(options.preserve_original),
        allocator);
    }

    json.AddMember(
      rapidjson::StringRef(EDGE_NGRAM_PARAM_NAME.c_str(), EDGE_NGRAM_PARAM_NAME.size()),
      ngram,
      allocator);
  }

  //output json to string
  rapidjson::StringBuffer buffer;
  rapidjson::Writer< rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);
  definition = buffer.GetString();

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
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  try {
    {
      SCOPED_LOCK(mutex);
      auto itr = cached_state_by_key.find(irs::make_hashed_ref(args));

      if (itr != cached_state_by_key.end()) {
        return irs::memory::make_unique<irs::analysis::text_token_stream>(
            itr->second, itr->second.stopwords_);
      }
    }

    irs::analysis::text_token_stream::options_t options;
    if (parse_json_options(args, options)) {
      irs::analysis::text_token_stream::stopwords_t stopwords;
      if (!build_stopwords(options, stopwords)) {
        IR_FRMT_WARN(
            "Failed to retrieve 'stopwords' from path while constructing "
            "text_token_stream from jSON arguments: %s",
            args.c_str());

        return nullptr;
      }
      return construct(args, std::move(options), std::move(stopwords));
    }
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing text_token_stream from jSON arguments: %s", 
                  args.c_str());
    IR_LOG_EXCEPTION();
  }
  return nullptr;
}


bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  irs::analysis::text_token_stream::options_t options;
  if (parse_json_options(args, options)) {
    return make_json_config(options, definition);
  } else {
    return false;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief args is a locale name
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(const irs::string_ref& args) {
  std::locale locale;
  if (make_locale_from_name(args, locale)) {
    return construct(locale);
  } else {
    return nullptr;
  }
}

bool normalize_text_config(const irs::string_ref& args,
                           std::string& definition) {
  std::locale locale;
  if (make_locale_from_name(args, locale)) {
    definition = irs::locale_utils::name(locale);
    return true;
  } 
  return false;
}

REGISTER_ANALYZER_JSON(irs::analysis::text_token_stream, make_json, 
                       normalize_json_config);
REGISTER_ANALYZER_TEXT(irs::analysis::text_token_stream, make_text, 
                       normalize_text_config);

}

namespace iresearch {
namespace analysis {

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

char const* text_token_stream::STOPWORD_PATH_ENV_VARIABLE = "IRESEARCH_TEXT_STOPWORD_PATH";

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

text_token_stream::text_token_stream(const options_t& options, const stopwords_t& stopwords)
  : attributes{{
      { irs::type<increment>::id(), &inc_       },
      { irs::type<offset>::id(), &offs_         },
      { irs::type<term_attribute>::id(), &term_ }},
      irs::type<text_token_stream>::get()},
    state_(memory::make_unique<state_t>(options, stopwords)) {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

/*static*/ void text_token_stream::init() {
  REGISTER_ANALYZER_JSON(text_token_stream, make_json,
                         normalize_json_config);  // match registration above
  REGISTER_ANALYZER_TEXT(text_token_stream, make_text,
                         normalize_text_config); // match registration above
}

/*static*/ void text_token_stream::clear_cache() {
  SCOPED_LOCK(::mutex);
  cached_state_by_key.clear();
}

/*static*/ analyzer::ptr text_token_stream::make(const irs::string_ref& locale) {
  return make_text(locale);
}

bool text_token_stream::reset(const string_ref& data) {
  if (data.size() > integer_traits<uint32_t>::const_max) {
    // can't handle data which is longer than integer_traits<uint32_t>::const_max
    return false;
  }

  // reset term attribute
  term_.value = bytes_ref::NIL;

  // reset offset attribute
  offs_.start = integer_traits<uint32_t>::const_max;
  offs_.end = integer_traits<uint32_t>::const_max;

  if (state_->icu_locale.isBogus()) {
    state_->icu_locale = icu::Locale(
      std::string(irs::locale_utils::language(state_->options.locale)).c_str(),
      std::string(irs::locale_utils::country(state_->options.locale)).c_str()
    );

    if (state_->icu_locale.isBogus()) {
      return false;
    }
  }

  auto err = UErrorCode::U_ZERO_ERROR; // a value that passes the U_SUCCESS() test

  if (!state_->normalizer) {
    // reusable object owned by ICU
    state_->normalizer.reset(
      icu::Normalizer2::getNFCInstance(err), [](const icu::Normalizer2*)->void{}
    );

    if (!U_SUCCESS(err) || !state_->normalizer) {
      state_->normalizer.reset();

      return false;
    }
  }

  if (!state_->options.accent && !state_->transliterator) {
    // transliteration rule taken verbatim from: http://userguide.icu-project.org/transforms/general
    icu::UnicodeString collationRule("NFD; [:Nonspacing Mark:] Remove; NFC"); // do not allocate statically since it causes memory leaks in ICU

    // reusable object owned by *this
    state_->transliterator.reset(icu::Transliterator::createInstance(
      collationRule, UTransDirection::UTRANS_FORWARD, err
    ));

    if (!U_SUCCESS(err) || !state_->transliterator) {
      state_->transliterator.reset();

      return false;
    }
  }

  if (!state_->break_iterator) {
    // reusable object owned by *this
    state_->break_iterator.reset(icu::BreakIterator::createWordInstance(
      state_->icu_locale, err
    ));

    if (!U_SUCCESS(err) || !state_->break_iterator) {
      state_->break_iterator.reset();

      return false;
    }
  }

  // optional since not available for all locales
  if (state_->options.stemming && !state_->stemmer) {
    // reusable object owned by *this
    state_->stemmer.reset(
      sb_stemmer_new(
        std::string(irs::locale_utils::language(state_->options.locale)).c_str(),
        nullptr // defaults to utf-8
      ),
      [](sb_stemmer* ptr)->void{ sb_stemmer_delete(ptr); }
    );
  }

  // ...........................................................................
  // convert encoding to UTF8 for use with ICU
  // ...........................................................................
  std::string data_utf8;
  irs::string_ref data_utf8_ref;
  if (irs::locale_utils::is_utf8(state_->options.locale)) {
    data_utf8_ref = data;
  } else {
    // valid conversion since 'locale_' was created with internal unicode encoding
    if (!irs::locale_utils::append_internal(data_utf8, data, state_->options.locale)) {
      return false; // UTF8 conversion failure
    }
    data_utf8_ref = data_utf8;
  }

  if (data_utf8_ref.size() > irs::integer_traits<int32_t>::const_max) {
    return false; // ICU UnicodeString signatures can handle at most INT32_MAX
  }

  state_->data = icu::UnicodeString::fromUTF8(
    icu::StringPiece(data_utf8_ref.c_str(), (int32_t)(data_utf8_ref.size()))
  );

  // ...........................................................................
  // tokenise the unicode data
  // ...........................................................................
  state_->break_iterator->setText(state_->data);

  // reset term state for ngrams
  state_->term = bytes_ref::NIL;
  state_->start = integer_traits<uint32_t>::const_max;
  state_->end = integer_traits<uint32_t>::const_max;
  state_->set_ngram_finished();
  inc_.value = 1;

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
    term_.value = state_->term;
    offs_.start = state_->start;
    offs_.end = state_->end;

    return true;
  }

  return false;
}

bool text_token_stream::next_word() {
  // ...........................................................................
  // find boundaries of the next word
  // ...........................................................................
  for (auto start = state_->break_iterator->current(),
       end = state_->break_iterator->next();
       icu::BreakIterator::DONE != end;
       start = end, end = state_->break_iterator->next()) {
    // ...........................................................................
    // skip whitespace and unsuccessful terms
    // ...........................................................................
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

  // if there are no ngrams yet then a new word started
  if (state_->is_ngram_finished()) {
    state_->ngram.it = begin;
    inc_.value = 1;
    // find the first ngram > min
    do {
      state_->ngram.it = irs::utf8_utils::next(state_->ngram.it, end);
    } while (++state_->ngram.length < state_->options.min_gram &&
             state_->ngram.it != end);
  } else {
    // not first ngram in a word
    inc_.value = 0; // staying on the current pos
    state_->ngram.it = irs::utf8_utils::next(state_->ngram.it, end);
    ++state_->ngram.length;
  }

  bool finished{};
  auto set_ngram_finished = irs::make_finally([this, &finished]()->void {
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
    static_assert(sizeof(irs::byte_type) == sizeof(char), "sizeof(irs::byte_type) != sizeof(char)");

    auto size = static_cast<uint32_t>(std::distance(begin, state_->ngram.it));
    term_buf_.assign(state_->term.c_str(), size);
    term_.value = term_buf_;
    offs_.start = state_->start;
    offs_.end = state_->start + size;

    return true;
  }

  return false;
}

} // analysis
} // ROOT
