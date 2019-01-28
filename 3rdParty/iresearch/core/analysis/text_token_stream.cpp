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
////////////////////////////////////////////////////////////////////////////////

#include <cctype>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <boost/locale/encoding.hpp>
#include <rapidjson/rapidjson/document.h> // for rapidjson::Document, rapidjson::Value
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
#include "utils/locale_utils.hpp"
#include "utils/log.hpp"
#include "utils/misc.hpp"
#include "utils/runtime_utils.hpp"
#include "utils/thread_utils.hpp"
#include "utils/utf8_path.hpp"

#include "text_token_stream.hpp"

NS_ROOT
NS_BEGIN(analysis)

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

struct text_token_stream::state_t {
  std::shared_ptr<icu::BreakIterator> break_iterator;
  icu::UnicodeString data;
  icu::Locale locale;
  const struct {
    std::string country;
    std::string encoding;
    std::string language;
    bool utf8;
  } locale_parts;
  std::shared_ptr<const icu::Normalizer2> normalizer;
  const options_t& options;
  std::shared_ptr<sb_stemmer> stemmer;
  std::string tmp_buf; // used by processTerm(...)
  std::shared_ptr<icu::Transliterator> transliterator;
  state_t(const options_t& opts)
    : locale("C"),
      locale_parts({
        irs::locale_utils::country(opts.locale),
        irs::locale_utils::encoding(opts.locale),
        irs::locale_utils::language(opts.locale),
        irs::locale_utils::utf8(opts.locale)
      }),
      options(opts) {
    // NOTE: use of the default constructor for Locale() or
    //       use of Locale::createFromName(nullptr)
    //       causes a memory leak with Boost 1.58, as detected by valgrind
    locale.setToBogus(); // set to uninitialized
  }
};

NS_END // analysis
NS_END // ROOT

NS_LOCAL

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

typedef std::unordered_set<std::string> ignored_words_t;
static std::unordered_map<irs::hashed_string_ref, irs::analysis::text_token_stream::options_t> cached_state_by_key;
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
bool get_ignored_words(
  ignored_words_t& buf,
  const std::locale& locale,
  const irs::string_ref& path = irs::string_ref::NIL
) {
  auto language = irs::locale_utils::language(locale);
  irs::utf8_path stopword_path;
  auto* custom_stopword_path =
    !path.null()
    ? path.c_str()
    : irs::getenv(irs::analysis::text_token_stream::STOPWORD_PATH_ENV_VARIABLE)
    ;

  if (custom_stopword_path) {
    bool absolute;

    stopword_path = irs::utf8_path(custom_stopword_path);

    if (!stopword_path.absolute(absolute)) {
      IR_FRMT_ERROR("Failed to determine absoluteness of path: %s", stopword_path.utf8().c_str());

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

      return false;
    }

    ignored_words_t ignored_words;
    auto visitor = [&ignored_words, &stopword_path](
        const irs::utf8_path::native_char_t* name
    )->bool {
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
          ignored_words.insert(line.substr(0, i));
        }
      }

      return true;
    };

    if (!stopword_path.visit_directory(visitor, false)) {
      return false;
    }

    buf.insert(ignored_words.begin(), ignored_words.end());

    return true;
  } catch (...) {
    IR_FRMT_ERROR("Caught error while loading stopwords from path: %s", stopword_path.utf8().c_str());
    IR_LOG_EXCEPTION();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer based on the supplied cache_key and options
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key,
  irs::analysis::text_token_stream::options_t&& options
) {
  irs::analysis::text_token_stream::options_t* options_ptr;

  {
    SCOPED_LOCK(mutex);

    options_ptr = &(cached_state_by_key.emplace(
      irs::make_hashed_ref(cache_key, std::hash<irs::string_ref>()),
      std::move(options)
    ).first->second);
  }

  return irs::memory::make_unique<irs::analysis::text_token_stream>(
    *options_ptr
  );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer based on the supplied cache_key
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key
) {
  {
    SCOPED_LOCK(mutex);
    auto itr = cached_state_by_key.find(
      irs::make_hashed_ref(cache_key, std::hash<irs::string_ref>())
    );

    if (itr != cached_state_by_key.end()) {
      return irs::memory::make_unique<irs::analysis::text_token_stream>(
        itr->second
      );
    }
  }

  try {
    // interpret the cache_key as a locale name
    std::string locale_name(cache_key.c_str(), cache_key.size());
    irs::analysis::text_token_stream::options_t options;

    options.locale = irs::locale_utils::locale(locale_name);

    if (!get_ignored_words(options.ignored_words, options.locale)) {
      IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream with cache key: %s", cache_key.c_str());

      return nullptr;
    }

    return construct(cache_key, std::move(options));
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing text_token_stream cache key: %s", cache_key.c_str());
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

bool process_term(
  irs::analysis::text_token_stream::bytes_term& term,
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
    word.toLower(state.locale); // inplace case-conversion
    break;
   case irs::analysis::text_token_stream::options_t::case_convert_t::UPPER:
    word.toUpper(state.locale); // inplace case-conversion
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
  if (state.options.ignored_words.find(word_utf8) != state.options.ignored_words.end()) {
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
      term.value(irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(value), sb_stemmer_length(state.stemmer.get())));

      return true;
    }
  }

  // ...........................................................................
  // use the value of the unstemmed token
  // ...........................................................................
  static_assert(sizeof(irs::byte_type) == sizeof(char), "sizeof(irs::byte_type) != sizeof(char)");
  term.value(irs::bstring(irs::ref_cast<irs::byte_type>(word_utf8).c_str(), word_utf8.size()));

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "locale"(string): locale of the analyzer <required>
///        "ignored_words([string...]): set of words to ignore (missing == use default list)
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()
      || !json.IsObject()
      || !json.HasMember("locale")
      || !json["locale"].IsString()
     ) {
    IR_FRMT_WARN("Missing 'locale' while constructing text_token_stream from jSON arguments: %s", args.c_str());

    return nullptr;
  }

  try {
    typedef irs::analysis::text_token_stream::options_t options_t;
    options_t options;

    options.locale = irs::locale_utils::locale(json["locale"].GetString()); // required

    if (json.HasMember("case_convert")) {
      auto& case_convert = json["case_convert"]; // optional string enum

      if (!case_convert.IsString()) {
        IR_FRMT_WARN("Non-string value in 'case_convert' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      static const std::unordered_map<std::string, options_t::case_convert_t> case_convert_map = {
        { "lower", options_t::case_convert_t::LOWER },
        { "none", options_t::case_convert_t::NONE },
        { "upper", options_t::case_convert_t::UPPER },
      };
      auto itr = case_convert_map.find(case_convert.GetString());

      if (itr == case_convert_map.end()) {
        IR_FRMT_WARN("Invalid value in 'case_convert' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      options.case_convert = itr->second;
    }

    // load stopwords
    // 'ignored_words' + 'ignored_words_path' = load from both
    // 'ignored_words' only - load from 'ignored_words'
    // 'ignored_words_path' only - load from 'ignored_words_path'
    // none - load from default location
    if (json.HasMember("ignored_words")) {
      auto& ignored_words = json["ignored_words"]; // optional string array

      if (!ignored_words.IsArray()) {
        IR_FRMT_WARN("Invalid value in 'ignored_words' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      for (auto itr = ignored_words.Begin(), end = ignored_words.End();
           itr != end;
           ++itr) {
        if (!itr->IsString()) {
          IR_FRMT_WARN("Non-string value in 'ignored_words' while constructing text_token_stream from jSON arguments: %s", args.c_str());

          return nullptr;
        }

        options.ignored_words.emplace(itr->GetString());
      }

      if (json.HasMember("ignored_words_path")) {
        auto& ignored_words_path = json["ignored_words_path"]; // optional string

        if (!ignored_words_path.IsString()) {
          IR_FRMT_WARN("Non-string value in 'ignored_words_path' while constructing text_token_stream from jSON arguments: %s", args.c_str());

          return nullptr;
        }

        if (!get_ignored_words(options.ignored_words, options.locale, ignored_words_path.GetString())) {
          IR_FRMT_WARN("Failed to retrieve 'ignored_words' from path while constructing text_token_stream from jSON arguments: %s", args.c_str());

          return nullptr;
        }
      }
    } else if (json.HasMember("ignored_words_path")) {
      auto& ignored_words_path = json["ignored_words_path"]; // optional string

      if (!ignored_words_path.IsString()) {
        IR_FRMT_WARN("Non-string value in 'ignored_words_path' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      if (!get_ignored_words(options.ignored_words, options.locale, ignored_words_path.GetString())) {
        IR_FRMT_WARN("Failed to retrieve 'ignored_words' from path while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }
    } else {
      if (!get_ignored_words(options.ignored_words, options.locale)) {
        IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }
    }

    if (json.HasMember("no_accent")) {
      auto& no_accent = json["no_accent"]; // optional bool

      if (!no_accent.IsBool()) {
        IR_FRMT_WARN("Non-boolean value in 'no_accent' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      options.no_accent = no_accent.GetBool();
    }

    if (json.HasMember("no_stem")) {
      auto& no_stem = json["no_stem"]; // optional bool

      if (!no_stem.IsBool()) {
        IR_FRMT_WARN("Non-boolean value in 'no_stem' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      options.no_stem = no_stem.GetBool();
    }

    return construct(args, std::move(options));
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing text_token_stream from jSON arguments: %s", args.c_str());
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a locale name
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(const irs::string_ref& args) {
  return construct(args);
}

REGISTER_ANALYZER_JSON(irs::analysis::text_token_stream, make_json);
REGISTER_ANALYZER_TEXT(irs::analysis::text_token_stream, make_text);

NS_END

NS_ROOT
NS_BEGIN(analysis)

// -----------------------------------------------------------------------------
// --SECTION--                                                  static variables
// -----------------------------------------------------------------------------

char const* text_token_stream::STOPWORD_PATH_ENV_VARIABLE = "IRESEARCH_TEXT_STOPWORD_PATH";

// -----------------------------------------------------------------------------
// --SECTION--                                                  static functions
// -----------------------------------------------------------------------------

DEFINE_ANALYZER_TYPE_NAMED(text_token_stream, "text")

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

text_token_stream::text_token_stream(const options_t& options)
  : analyzer(text_token_stream::type()),
    attrs_(3), // offset + bytes_term + increment
    state_(memory::make_unique<state_t>(options)) {
  attrs_.emplace(offs_);
  attrs_.emplace(term_);
  attrs_.emplace(inc_);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

/*static*/ void text_token_stream::init() {
  REGISTER_ANALYZER_JSON(text_token_stream, make_json); // match registration above
  REGISTER_ANALYZER_TEXT(text_token_stream, make_text); // match registration above
}

/*static*/ analyzer::ptr text_token_stream::make(const std::locale& locale) {
  options_t options;

  options.locale = locale;

  if (!get_ignored_words(options.ignored_words, options.locale)) {
     IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream for locale: %s", options.locale.name().c_str());

     return nullptr;
  }

  return construct(locale.name(), std::move(options));
}

bool text_token_stream::reset(const string_ref& data) {
  if (state_->locale.isBogus()) {
    state_->locale = icu::Locale(
      state_->locale_parts.language.c_str(),
      state_->locale_parts.country.c_str()
    );

    if (state_->locale.isBogus()) {
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

  if (state_->options.no_accent && !state_->transliterator) {
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
      state_->locale, err
    ));

    if (!U_SUCCESS(err) || !state_->break_iterator) {
      state_->break_iterator.reset();

      return false;
    }
  }

  // optional since not available for all locales
  if (!state_->options.no_stem && !state_->stemmer) {
    // reusable object owned by *this
    state_->stemmer.reset(
      sb_stemmer_new(state_->locale_parts.language.c_str(), nullptr), // defaults to utf-8
      [](sb_stemmer* ptr)->void{ sb_stemmer_delete(ptr); }
    );
  }

  // ...........................................................................
  // convert encoding to UTF8 for use with ICU
  // ...........................................................................
  if (state_->locale_parts.utf8) {
    if (data.size() > INT32_MAX) {
      return false; // ICU UnicodeString signatures can handle at most INT32_MAX
    }

    state_->data = icu::UnicodeString::fromUTF8(
      icu::StringPiece(data.c_str(), (int32_t)(data.size()))
    );
  }
  else {
    std::string data_utf8 = boost::locale::conv::to_utf<char>(
      data.c_str(), data.c_str() + data.size(), state_->locale_parts.encoding
    );

    if (data_utf8.size() > INT32_MAX) {
      return false; // ICU UnicodeString signatures can handle at most INT32_MAX
    }

    state_->data = icu::UnicodeString::fromUTF8(
      icu::StringPiece(data_utf8.c_str(), (int32_t)(data_utf8.size()))
    );
  }

  // ...........................................................................
  // tokenise the unicode data
  // ...........................................................................
  state_->break_iterator->setText(state_->data);

  return true;
}

bool text_token_stream::next() {
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
        || !process_term(term_, *state_, state_->data.tempSubString(start, end - start))) {
      continue;
    }

    offs_.start = start;
    offs_.end = end;
    return true;
  }

  return false;
}

NS_END // analysis
NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------