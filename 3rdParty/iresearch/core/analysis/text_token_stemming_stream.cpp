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

#include <libstemmer.h>

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"
#include "utils/vpack_utils.hpp"

namespace {

using namespace irs;

constexpr VPackStringRef LOCALE_PARAM_NAME {"locale"};

bool locale_from_slice(VPackSlice slice, icu::Locale& locale) {
  if (!slice.isString()) {
    IR_FRMT_WARN(
      "Non-string value in '%s' while constructing "
      "text_token_stemming_stream from VPack arguments",
      LOCALE_PARAM_NAME.data());

    return false;
  }

  const auto locale_name = get_string<std::string>(slice);

  locale = icu::Locale::createFromName(locale_name.c_str());

  if (!locale.isBogus()) {
    locale = icu::Locale{locale.getLanguage()};
  }

  if (locale.isBogus()) {
    IR_FRMT_WARN(
      "Failed to instantiate locale from the supplied string '%s'"
      "while constructing text_token_stemming_stream from VPack arguments",
      locale_name.c_str());

    return false;
  }

  // validate creation of sb_stemmer, defaults to utf-8
  stemmer_ptr stemmer = make_stemmer_ptr(locale.getLanguage(), nullptr);

  if (!stemmer) {
    IR_FRMT_WARN(
      "Failed to instantiate sb_stemmer from locale '%s' "
      "while constructing stemming_token_stream from VPack arguments",
      locale_name.c_str());
  }

  return true;
}

bool parse_vpack_options(const VPackSlice slice, irs::analysis::stemming_token_stream::options_t& opts) {
  if (!slice.isObject()) {
    IR_FRMT_ERROR("Slice for text_token_stemming_stream  is not an object");
    return false;
  }

  try {
    const auto locale_slice = slice.get(LOCALE_PARAM_NAME);

    if (locale_slice.isNone()) {
      IR_FRMT_ERROR(
        "Missing '%s' while constructing text_token_stemming_stream from "
        "VPack arguments",
        LOCALE_PARAM_NAME.data());

      return false;
    }

    return locale_from_slice(locale_slice, opts.locale);
  } catch (const std::exception& ex) {
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
  analysis::stemming_token_stream::options_t opts;

  if (parse_vpack_options(slice, opts)) {
    return memory::make_unique<analysis::stemming_token_stream>(opts);
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
bool make_vpack_config(const analysis::stemming_token_stream::options_t& opts, VPackBuilder* builder) {
  VPackObjectBuilder object(builder);
  {
    // locale
    const auto* locale_name = opts.locale.getName();
    builder->add(LOCALE_PARAM_NAME, VPackValue(locale_name));
  }
  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* builder) {
  analysis::stemming_token_stream::options_t opts;

  if (parse_vpack_options(slice, opts)) {
    return make_vpack_config(opts, builder);
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

REGISTER_ANALYZER_JSON(analysis::stemming_token_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_VPACK(analysis::stemming_token_stream, make_vpack,
                       normalize_vpack_config);

}

namespace iresearch {
namespace analysis {

stemming_token_stream::stemming_token_stream(const options_t& options)
  : analyzer{irs::type<stemming_token_stream>::get()},
    options_{options},
    term_eof_{true} {
}

/*static*/ void stemming_token_stream::init() {
  REGISTER_ANALYZER_JSON(stemming_token_stream, make_json,
                         normalize_json_config); // match registration above
  REGISTER_ANALYZER_VPACK(analysis::stemming_token_stream, make_vpack,
                         normalize_vpack_config); // match registration above
}

bool stemming_token_stream::next() {
  if (term_eof_) {
    return false;
  }

  term_eof_ = true;

  return true;
}

bool stemming_token_stream::reset(const string_ref& data) {
  if (!stemmer_) {
    // defaults to utf-8
    stemmer_ = make_stemmer_ptr(options_.locale.getLanguage(), nullptr);
  }

  auto& term = std::get<term_attribute>(attrs_);

  term.value = bytes_ref::NIL; // reset

  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());

  term_eof_ = false;

  // find the token stem
  string_ref utf8_data{data};

  if (stemmer_) {
    if (utf8_data.size() > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
      return false;
    }

    static_assert(sizeof(sb_symbol) == sizeof(char));
    const auto* value = reinterpret_cast<const sb_symbol*>(utf8_data.c_str());

    value = sb_stemmer_stem(stemmer_.get(), value, static_cast<int>(utf8_data.size()));

    if (value) {
      static_assert(sizeof(byte_type) == sizeof(sb_symbol));
      term.value = bytes_ref(reinterpret_cast<const byte_type*>(value),
                                   sb_stemmer_length(stemmer_.get()));

      return true;
    }
  }

  // use the value of the unstemmed token
  static_assert(sizeof(byte_type) == sizeof(char));
  term.value = ref_cast<byte_type>(utf8_data);

  return true;
}


} // analysis
} // ROOT
