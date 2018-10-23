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
  std::shared_ptr<BreakIterator> break_iterator;
  UnicodeString data;
  Locale locale;
  std::shared_ptr<const Normalizer2> normalizer;
  std::shared_ptr<sb_stemmer> stemmer;
  std::string tmp_buf; // used by processTerm(...)
  std::shared_ptr<Transliterator> transliterator;
  state_t(): locale("C") {
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
typedef std::pair<std::locale, ignored_words_t> cached_state_t;
static std::unordered_map<irs::hashed_string_ref, cached_state_t> cached_state_by_key;
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
  const std::string* path = nullptr
) {
  auto language = irs::locale_utils::language(locale);
  irs::utf8_path stopword_path;
  auto* custom_stopword_path =
    path
    ? path->c_str()
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
/// @brief create an analyzer with supplied ignored_words and cache its state
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key,
  const std::locale& locale,
  ignored_words_t&& ignored_words
) {
  cached_state_t* cached_state;

  {
    SCOPED_LOCK(mutex);

    cached_state = &(cached_state_by_key.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(irs::make_hashed_ref(cache_key, std::hash<irs::string_ref>())),
      std::forward_as_tuple(locale, std::move(ignored_words))
    ).first->second);
  }

  return irs::memory::make_unique<irs::analysis::text_token_stream>(
    cached_state->first, cached_state->second
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
        itr->second.first, itr->second.second
      );
    }
  }

  try {
    // interpret the cache_key as a locale name
    std::string locale_name(cache_key.c_str(), cache_key.size());
    auto locale = irs::locale_utils::locale(locale_name);
    ignored_words_t buf;

    if (!get_ignored_words(buf, locale)) {
      IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream with cache key: %s", cache_key.c_str());

      return nullptr;
    }

    return construct(cache_key, locale, std::move(buf));
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing text_token_stream cache key: %s", cache_key.c_str());
    IR_LOG_EXCEPTION();
  }

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer for the supplied locale and default values
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key,
  const std::locale& locale
) {
  ignored_words_t buf;

  if (!get_ignored_words(buf, locale)) {
     IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream with cache key: %s", cache_key.c_str());

     return nullptr;
  }

  return construct(cache_key, locale, std::move(buf));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer with supplied ignore_word_path
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key,
  const std::locale& locale,
  const std::string& ignored_word_path
) {
  ignored_words_t buf;

  if (!get_ignored_words(buf, locale, &ignored_word_path)) {
    IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream with cache key: '%s', ignored word path: %s", cache_key.c_str(), ignored_word_path.c_str());

    return nullptr;
  }

  return construct(cache_key, locale, std::move(buf));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create an analyzer with supplied ignored_words and ignore_word_path
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr construct(
  const irs::string_ref& cache_key,
  const std::locale& locale,
  const std::string& ignored_word_path,
  ignored_words_t&& ignored_words
) {
  if (!get_ignored_words(ignored_words, locale, &ignored_word_path)) {
    IR_FRMT_WARN("Failed to retrieve 'ignored_words' while constructing text_token_stream with cache key: '%s', ignored word path: %s", cache_key.c_str(), ignored_word_path.c_str());

    return nullptr;
  }

  return construct(cache_key, locale, std::move(ignored_words));
}

bool process_term(
  irs::analysis::text_token_stream::bytes_term& term,
  const std::unordered_set<std::string>& ignored_words,
  irs::analysis::text_token_stream::state_t& state,
  UnicodeString const& data
) {
  // ...........................................................................
  // normalize unicode
  // ...........................................................................
  UnicodeString word;
  UErrorCode err = U_ZERO_ERROR; // a value that passes the U_SUCCESS() test

  state.normalizer->normalize(data, word, err);

  if (!U_SUCCESS(err)) {
    word = data; // use non-normalized value if normalization failure
  }

  // ...........................................................................
  // case-convert unicode
  // ...........................................................................
  word.toLower(state.locale); // inplace case-conversion

  // ...........................................................................
  // collate value, e.g. remove accents
  // ...........................................................................
  state.transliterator->transliterate(word); // inplace translitiration

  std::string& word_utf8 = state.tmp_buf;

  word_utf8.clear();
  word.toUTF8String(word_utf8);

  // ...........................................................................
  // skip ignored tokens
  // ...........................................................................
  if (ignored_words.find(word_utf8) != ignored_words.end()) {
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
    static const rapidjson::Value empty;
    auto locale = irs::locale_utils::locale(json["locale"].GetString());
    auto& ignored_words = json.HasMember("ignored_words") ? json["ignored_words"] : empty;
    auto& ignored_words_path = json.HasMember("ignored_words_path") ? json["ignored_words_path"] : empty;

    if (!ignored_words.IsArray()) {
      return ignored_words_path.IsString()
        ? construct(args, locale, ignored_words_path.GetString())
        : construct(args, locale)
        ;
    }

    ignored_words_t buf;

    for (auto itr = ignored_words.Begin(), end = ignored_words.End(); itr != end; ++itr) {
      if (!itr->IsString()) {
        IR_FRMT_WARN("Non-string value in 'ignored_words' while constructing text_token_stream from jSON arguments: %s", args.c_str());

        return nullptr;
      }

      buf.emplace(itr->GetString());
    }

    return ignored_words_path.IsString()
      ? construct(args, locale, ignored_words_path.GetString(), std::move(buf))
      : construct(args, locale, std::move(buf))
      ;
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

DEFINE_ANALYZER_TYPE_NAMED(text_token_stream, "text");

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

text_token_stream::text_token_stream(
    const std::locale& locale,
    const std::unordered_set<std::string>& ignored_words
) : analyzer(text_token_stream::type()),
    attrs_(3), // offset + bytes_term + increment
    state_(memory::make_unique<state_t>()),
    ignored_words_(ignored_words) {
  attrs_.emplace(offs_);
  attrs_.emplace(term_);
  attrs_.emplace(inc_);
  locale_.country = locale_utils::country(locale);
  locale_.encoding = locale_utils::encoding(locale);
  locale_.language = locale_utils::language(locale);
  locale_.utf8 = locale_utils::utf8(locale);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

/*static*/ void text_token_stream::init() {
  REGISTER_ANALYZER_JSON(text_token_stream, make_json); // match registration above
  REGISTER_ANALYZER_TEXT(text_token_stream, make_text); // match registration above
}

/*static*/ analyzer::ptr text_token_stream::make(const std::locale& locale) {
  return construct(locale.name(), locale);
}

bool text_token_stream::reset(const string_ref& data) {
  if (state_->locale.isBogus()) {
    state_->locale = Locale(locale_.language.c_str(), locale_.country.c_str());

    if (state_->locale.isBogus()) {
      return false;
    }
  }

  UErrorCode err = U_ZERO_ERROR; // a value that passes the U_SUCCESS() test

  if (!state_->normalizer) {
    // reusable object owned by ICU
    state_->normalizer.reset(Normalizer2::getNFCInstance(err), [](const Normalizer2*)->void{});

    if (!U_SUCCESS(err) || !state_->normalizer) {
      state_->normalizer.reset();

      return false;
    }
  }

  if (!state_->transliterator) {
    // transliteration rule taken verbatim from: http://userguide.icu-project.org/transforms/general
    UnicodeString collationRule("NFD; [:Nonspacing Mark:] Remove; NFC"); // do not allocate statically since it causes memory leaks in ICU

    // reusable object owned by *this
    state_->transliterator.reset(
      Transliterator::createInstance(collationRule, UTransDirection::UTRANS_FORWARD, err)
    );

    if (!U_SUCCESS(err) || !state_->transliterator) {
      state_->transliterator.reset();

      return false;
    }
  }

  if (!state_->break_iterator) {
    // reusable object owned by *this
    state_->break_iterator.reset(BreakIterator::createWordInstance(state_->locale, err));

    if (!U_SUCCESS(err) || !state_->break_iterator) {
      state_->break_iterator.reset();

      return false;
    }
  }

  // optional since not available for all locales
  if (!state_->stemmer) {
    // reusable object owned by *this
    state_->stemmer.reset(
      sb_stemmer_new(locale_.language.c_str(), nullptr), // defaults to utf-8
      [](sb_stemmer* ptr)->void{ sb_stemmer_delete(ptr); }
    );
  }

  // ...........................................................................
  // convert encoding to UTF8 for use with ICU
  // ...........................................................................
  if (locale_.utf8) {
    if (data.size() > INT32_MAX) {
      return false; // ICU UnicodeString signatures can handle at most INT32_MAX
    }

    state_->data =
      UnicodeString::fromUTF8(StringPiece(data.c_str(), (int32_t)(data.size())));
  }
  else {
    std::string data_utf8 = boost::locale::conv::to_utf<char>(
      data.c_str(), data.c_str() + data.size(), locale_.encoding
    );

    if (data_utf8.size() > INT32_MAX) {
      return false; // ICU UnicodeString signatures can handle at most INT32_MAX
    }

    state_->data =
      UnicodeString::fromUTF8(StringPiece(data_utf8.c_str(), (int32_t)(data_utf8.size())));
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
  for (auto start = state_->break_iterator->current(), end = state_->break_iterator->next();
    BreakIterator::DONE != end;
    start = end, end = state_->break_iterator->next()) {

    // ...........................................................................
    // skip whitespace and unsuccessful terms
    // ...........................................................................
    if (state_->break_iterator->getRuleStatus() == UWordBreak::UBRK_WORD_NONE ||
        !process_term(term_, ignored_words_, *state_, state_->data.tempSubString(start, end - start))) {
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