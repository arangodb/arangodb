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

#include "libstemmer.h"
#include "rapidjson/rapidjson/document.h" // for rapidjson::Document
#include <rapidjson/rapidjson/writer.h> // for rapidjson::Writer
#include <rapidjson/rapidjson/stringbuffer.h> // for rapidjson::StringBuffer
#include "unicode/locid.h"
#include "utils/locale_utils.hpp"

#include "text_token_stemming_stream.hpp"

namespace {


bool make_locale_from_name(const irs::string_ref& name,
                          std::locale& locale) {
  try {
    locale = irs::locale_utils::locale(
        name, irs::string_ref::NIL,
        true);  // true == convert to unicode, required for ICU and Snowball
    // check if ICU supports locale
    auto icu_locale =
        icu::Locale(std::string(irs::locale_utils::language(locale)).c_str(),
                    std::string(irs::locale_utils::country(locale)).c_str());
    return !icu_locale.isBogus();
  } catch (...) {
    IR_FRMT_ERROR(
        "Caught error while constructing locale from "
        "name: %s",
        name.c_str());
  }
  return false;
}

const irs::string_ref LOCALE_PARAM_NAME = "locale";

bool parse_json_config(const irs::string_ref& args, std::locale& locale) {
  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing "
        "text_token_stemming_stream, arguments: %s",
        args.c_str());

    return false;
  }

  try {
    switch (json.GetType()) {
      case rapidjson::kStringType:
        return make_locale_from_name(json.GetString(), locale);
      case rapidjson::kObjectType:
        if (json.HasMember(LOCALE_PARAM_NAME.c_str()) &&
            json[LOCALE_PARAM_NAME.c_str()].IsString()) {
          return make_locale_from_name(
              json[LOCALE_PARAM_NAME.c_str()].GetString(), locale);
        }
      [[fallthrough]];
      default:
        IR_FRMT_ERROR(
            "Missing '%s' while constructing text_token_stemming_stream from "
            "jSON arguments: %s",
            LOCALE_PARAM_NAME.c_str(), args.c_str());
    }
  } catch (...) {
    IR_FRMT_ERROR(
        "Caught error while constructing text_token_stemming_stream from jSON "
        "arguments: %s",
        args.c_str());
  }

  return false;
}
    ////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "locale"(string): the locale to use for stemming <required>
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  std::locale locale;
  if (parse_json_config(args, locale)) {
    return irs::memory::make_shared<irs::analysis::text_token_stemming_stream>(locale);
  } else {
    return nullptr;
  }
}

///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param locale reference to analyzer`s locale
/// @param definition string for storing json document with config 
///////////////////////////////////////////////////////////////////////////////
bool make_json_config( const std::locale& locale,  std::string& definition) {
  rapidjson::Document json;
  json.SetObject();

  rapidjson::Document::AllocatorType& allocator = json.GetAllocator();

  // locale
  {
    const auto& locale_name = irs::locale_utils::name(locale);
    json.AddMember(
        rapidjson::StringRef(LOCALE_PARAM_NAME.c_str(), LOCALE_PARAM_NAME.size()),
        rapidjson::Value(rapidjson::StringRef(locale_name.c_str(), locale_name.length())),
        allocator);
  }
  //output json to string
  rapidjson::StringBuffer buffer;
  rapidjson::Writer< rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);
  definition = buffer.GetString();
  return true;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  std::locale options;
  if (parse_json_config(args, options)) {
    return make_json_config(options, definition);
  } else {
    return false;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a language to use for stemming
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(const irs::string_ref& args) {
  try {
    std::locale locale;
    if (make_locale_from_name(args, locale)) {
      return irs::memory::make_shared<irs::analysis::text_token_stemming_stream>(locale);
    }
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing text_token_stemming_stream TEXT arguments: %s",
      args.c_str()
    );
  }

  return nullptr;
}

bool normalize_text_config(const irs::string_ref& args, std::string& definition) {
  std::locale locale;
  if (make_locale_from_name(args, locale)) {
    definition = irs::locale_utils::name(locale);
    return true;
  }
  return false;
}

REGISTER_ANALYZER_JSON(irs::analysis::text_token_stemming_stream, make_json, 
                       normalize_json_config);
REGISTER_ANALYZER_TEXT(irs::analysis::text_token_stemming_stream, make_text, 
                       normalize_text_config);

}

namespace iresearch {
namespace analysis {

text_token_stemming_stream::text_token_stemming_stream(const std::locale& locale)
  : analyzer{irs::type<text_token_stemming_stream>::get()},
    locale_(locale),
    term_eof_(true) {
}

/*static*/ void text_token_stemming_stream::init() {
  REGISTER_ANALYZER_JSON(text_token_stemming_stream, make_json, 
                         normalize_json_config); // match registration above
  REGISTER_ANALYZER_TEXT(text_token_stemming_stream, make_text,
                         normalize_text_config); // match registration above
}

/*static*/ analyzer::ptr text_token_stemming_stream::make(const string_ref& locale) {
  return make_text(locale);
}

bool text_token_stemming_stream::next() {
  if (term_eof_) {
    return false;
  }

  term_eof_ = true;

  return true;
}

bool text_token_stemming_stream::reset(const irs::string_ref& data) {
  if (!stemmer_) {
    stemmer_.reset(
      sb_stemmer_new(
        std::string(irs::locale_utils::language(locale_)).c_str(), nullptr // defaults to utf-8
      ),
      [](sb_stemmer* ptr)->void{ sb_stemmer_delete(ptr); }
    );
  }

  auto& term = std::get<term_attribute>(attrs_);

  term.value = irs::bytes_ref::NIL; // reset
  term_buf_.clear();
  term_eof_ = true;

  // convert to UTF8 for use with 'stemmer_'
  irs::string_ref term_buf_ref;
  if (irs::locale_utils::is_utf8(locale_)) {
    term_buf_ref = data;
  } else {
    // valid conversion since 'locale_' was created with internal unicode encoding
    if (!irs::locale_utils::append_internal(term_buf_, data, locale_)) {
      IR_FRMT_ERROR(
        "Failed to parse UTF8 value from token: %s",
        data.c_str());

      return false;
    }

    term_buf_ref = term_buf_;
  }

  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());

  std::get<payload>(attrs_).value = ref_cast<uint8_t>(data);
  term_eof_ = false;

  // ...........................................................................
  // find the token stem
  // ...........................................................................
  if (stemmer_) {
    if (term_buf_ref.size() > std::numeric_limits<int>::max()) {
      IR_FRMT_WARN(
        "Token size greater than the supported maximum size '%d', truncating token: %s",
        std::numeric_limits<int>::max(), data.c_str());
      term_buf_ref = {term_buf_ref, std::numeric_limits<int>::max() };
    }

    static_assert(sizeof(sb_symbol) == sizeof(char), "sizeof(sb_symbol) != sizeof(char)");
    const auto* value = reinterpret_cast<sb_symbol const*>(term_buf_ref.c_str());

    value = sb_stemmer_stem(stemmer_.get(), value, static_cast<int>(term_buf_ref.size()));

    if (value) {
      static_assert(sizeof(irs::byte_type) == sizeof(sb_symbol), "sizeof(irs::byte_type) != sizeof(sb_symbol)");
      term.value = irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(value),
                                   sb_stemmer_length(stemmer_.get()));

      return true;
    }
  }

  // ...........................................................................
  // use the value of the unstemmed token
  // ...........................................................................
  static_assert(sizeof(irs::byte_type) == sizeof(char), "sizeof(irs::byte_type) != sizeof(char)");
  term.value = irs::ref_cast<irs::byte_type>(term_buf_);

  return true;
}


} // analysis
} // ROOT
