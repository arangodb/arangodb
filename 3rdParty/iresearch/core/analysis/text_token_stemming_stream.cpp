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

#include "text_token_stemming_stream.hpp"

#include "libstemmer.h"
#include "unicode/locid.h"
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"
#include "utils/locale_utils.hpp"
#include "utils/vpack_utils.hpp"

namespace {

using namespace irs;

const VPackStringRef LOCALE_PARAM_NAME {"locale"};

bool parse_vpack_options(const VPackSlice slice, std::locale& locale) {

  if (!slice.isObject() && !slice.isString()) {
    IR_FRMT_ERROR("Slice for delimited_token_stream is not an object or string");
    return false;
  }

  try {
    switch (slice.type()) {
      case VPackValueType::String:
        return locale_utils::icu_locale(get_string<string_ref>(slice), locale);
      case VPackValueType::Object:
      {
        auto param_name_slice = slice.get(LOCALE_PARAM_NAME);
        if (!param_name_slice.isNone() &&
            param_name_slice.isString()) {
          string_ref param_name = get_string<string_ref>(param_name_slice);
          return locale_utils::icu_locale(param_name, locale);
        }
      }

      [[fallthrough]];
      default:
        IR_FRMT_ERROR(
          "Missing '%s' while constructing text_token_stemming_stream from "
          "VPack arguments",
          LOCALE_PARAM_NAME.data());
    }
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing text_token_stemming_stream from VPack",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing text_token_stemming_stream from VPack arguments");
  }

  return false;
}
    ////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "locale"(string): the locale to use for stemming <required>
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  std::locale locale;
  if (parse_vpack_options(slice, locale)) {
    return memory::make_unique<analysis::text_token_stemming_stream>(locale);
  } else {
    return nullptr;
  }
}

analysis::analyzer::ptr make_vpack(const string_ref& args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  return make_vpack(slice);
}
///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param locale reference to analyzer`s locale
/// @param definition string for storing json document with config 
///////////////////////////////////////////////////////////////////////////////
bool make_vpack_config(const std::locale& locale, VPackBuilder* builder) {

  VPackObjectBuilder object(builder);
  {
     // locale
    const auto& locale_name = locale_utils::name(locale);
    builder->add(LOCALE_PARAM_NAME, VPackValue(locale_name));
  }
  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* builder) {
  std::locale options;
  if (parse_vpack_options(slice, options)) {
    return make_vpack_config(options, builder);
  } else {
    return false;
  }
}

bool normalize_vpack_config(const string_ref& args, std::string& config) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  VPackBuilder builder;
  if (normalize_vpack_config(slice, &builder)) {
    config.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
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
////////////////////////////////////////////////////////////////////////////////
/// @brief args is a language to use for stemming
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_text(const string_ref& args) {
  try {
    std::locale locale;
    if (locale_utils::icu_locale(args, locale)) {
      return memory::make_unique<analysis::text_token_stemming_stream>(locale);
    }
  } catch (...) {
    std::string err_msg = static_cast<std::string>(args);
    IR_FRMT_ERROR(
      "Caught error while constructing text_token_stemming_stream TEXT arguments: %s",
      err_msg.c_str());
  }

  return nullptr;
}

bool normalize_text_config(const string_ref& args, std::string& definition) {
  std::locale locale;
  if (locale_utils::icu_locale(args, locale)) {
    definition = locale_utils::name(locale);
    return true;
  }
  return false;
}

REGISTER_ANALYZER_JSON(analysis::text_token_stemming_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_TEXT(analysis::text_token_stemming_stream, make_text,
                       normalize_text_config);
REGISTER_ANALYZER_VPACK(analysis::text_token_stemming_stream, make_vpack,
                       normalize_vpack_config);

}

namespace iresearch {
namespace analysis {

text_token_stemming_stream::text_token_stemming_stream(const std::locale& locale)
  : analyzer{irs::type<text_token_stemming_stream>::get()},
    locale_{locale},
    stemmer_{nullptr, &sb_stemmer_delete},
    term_eof_{true} {
}

/*static*/ void text_token_stemming_stream::init() {
  REGISTER_ANALYZER_JSON(text_token_stemming_stream, make_json, 
                         normalize_json_config); // match registration above
  REGISTER_ANALYZER_TEXT(text_token_stemming_stream, make_text,
                         normalize_text_config); // match registration above
  REGISTER_ANALYZER_VPACK(analysis::text_token_stemming_stream, make_vpack,
                         normalize_vpack_config); // match registration above
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

bool text_token_stemming_stream::reset(const string_ref& data) {
  if (!stemmer_) {
    stemmer_.reset(
      sb_stemmer_new(
        std::string(locale_utils::language(locale_)).c_str(),
        nullptr)); // defaults to utf-8
  }

  auto& term = std::get<term_attribute>(attrs_);

  term.value = bytes_ref::NIL; // reset
  term_buf_.clear();
  term_eof_ = true;

  // convert to UTF8 for use with 'stemmer_'
  string_ref term_buf_ref;
  if (locale_utils::is_utf8(locale_)) {
    term_buf_ref = data;
  } else {
    // valid conversion since 'locale_' was created with internal unicode encoding
    if (!locale_utils::append_internal(term_buf_, data, locale_)) {
      IR_FRMT_ERROR(
        "Failed to parse UTF8 value from token");

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
    if (term_buf_ref.size() > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
      IR_FRMT_WARN(
        "Token size greater than the supported maximum size '%d', truncating token: %s",
        std::numeric_limits<int>::max(), data.c_str());
      term_buf_ref = {term_buf_ref, std::numeric_limits<int>::max() };
    }

    static_assert(sizeof(sb_symbol) == sizeof(char), "sizeof(sb_symbol) != sizeof(char)");
    const auto* value = reinterpret_cast<sb_symbol const*>(term_buf_ref.c_str());

    value = sb_stemmer_stem(stemmer_.get(), value, static_cast<int>(term_buf_ref.size()));

    if (value) {
      static_assert(sizeof(byte_type) == sizeof(sb_symbol), "sizeof(irs::byte_type) != sizeof(sb_symbol)");
      term.value = bytes_ref(reinterpret_cast<const byte_type*>(value),
                                   sb_stemmer_length(stemmer_.get()));

      return true;
    }
  }

  // ...........................................................................
  // use the value of the unstemmed token
  // ...........................................................................
  static_assert(sizeof(byte_type) == sizeof(char), "sizeof(irs::byte_type) != sizeof(char)");
  term.value = ref_cast<byte_type>(term_buf_);

  return true;
}


} // analysis
} // ROOT
