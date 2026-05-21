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

#include "text_token_normalizing_stream.hpp"

#if defined(_MSC_VER)
#pragma warning(disable : 4512)
#endif

#include <unicode/normalizer2.h>  // for icu::Normalizer2

#if defined(_MSC_VER)
#pragma warning(default : 4512)
#endif

#include <frozen/unordered_map.h>
#include <unicode/translit.h>  // for icu::Transliterator

#include <string_view>

#include "utils/hash_utils.hpp"
#include "utils/vpack_utils.hpp"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace irs {
namespace analysis {

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

struct normalizing_token_stream::state_t {
  IRESEARCH_ICU_NAMESPACE::UnicodeString data;
  IRESEARCH_ICU_NAMESPACE::UnicodeString token;
  std::string term_buf;
  const IRESEARCH_ICU_NAMESPACE::Normalizer2*
    normalizer;  // reusable object owned by ICU
  std::unique_ptr<IRESEARCH_ICU_NAMESPACE::Transliterator> transliterator;
  const options_t options;

  explicit state_t(const options_t& opts) : normalizer{}, options{opts} {}
};

}  // namespace analysis
}  // namespace irs

namespace {

using namespace irs;

constexpr std::string_view LOCALE_PARAM_NAME{"locale"};
constexpr std::string_view CASE_CONVERT_PARAM_NAME{"case"};
constexpr std::string_view ACCENT_PARAM_NAME{"accent"};

constexpr frozen::unordered_map<
  std::string_view, analysis::normalizing_token_stream::case_convert_t, 3>
  CASE_CONVERT_MAP = {
    {"lower", analysis::normalizing_token_stream::LOWER},
    {"none", analysis::normalizing_token_stream::NONE},
    {"upper", analysis::normalizing_token_stream::UPPER},
};

bool locale_from_slice(VPackSlice slice,
                       IRESEARCH_ICU_NAMESPACE::Locale& locale) {
  if (!slice.isString()) {
    IRS_LOG_WARN(
      absl::StrCat("Non-string value in '", LOCALE_PARAM_NAME,
                   "' while constructing text_token_normalizing_stream from "
                   "VPack arguments"));

    return false;
  }

  const auto locale_name = slice.copyString();

  locale = IRESEARCH_ICU_NAMESPACE::Locale::createFromName(locale_name.c_str());

  if (!locale.isBogus()) {
    locale = IRESEARCH_ICU_NAMESPACE::Locale{
      locale.getLanguage(), locale.getCountry(), locale.getVariant()};
  }

  if (locale.isBogus()) {
    IRS_LOG_WARN(absl::StrCat(
      "Failed to instantiate locale from the supplied string '", locale_name,
      "' while constructing text_token_normalizing_stream from VPack "
      "arguments"));

    return false;
  }

  return true;
}

bool parse_vpack_options(
  const VPackSlice slice,
  analysis::normalizing_token_stream::options_t& options) {
  if (!slice.isObject()) {
    IRS_LOG_ERROR("Slice for text_token_normalizing_stream is not an object");
    return false;
  }

  try {
    const auto locale_slice = slice.get(LOCALE_PARAM_NAME);

    if (!locale_slice.isNone()) {
      if (!locale_from_slice(locale_slice, options.locale)) {
        return false;
      }

      // optional string enum
      if (auto case_convert_slice = slice.get(CASE_CONVERT_PARAM_NAME);
          !case_convert_slice.isNone()) {
        if (!case_convert_slice.isString()) {
          IRS_LOG_WARN(
            absl::StrCat("Non-string value in '", CASE_CONVERT_PARAM_NAME,
                         "' while constructing text_token_normalizing_stream "
                         "from VPack arguments"));

          return false;
        }

        auto itr = CASE_CONVERT_MAP.find(case_convert_slice.stringView());

        if (itr == CASE_CONVERT_MAP.end()) {
          IRS_LOG_WARN(
            absl::StrCat("Invalid value in '", CASE_CONVERT_PARAM_NAME,
                         "' while constructing text_token_normalizing_stream "
                         "from VPack arguments"));

          return false;
        }

        options.case_convert = itr->second;
      }

      // optional bool
      if (auto accent_slice = slice.get(ACCENT_PARAM_NAME);
          !accent_slice.isNone()) {
        if (!accent_slice.isBool()) {
          IRS_LOG_WARN(
            absl::StrCat("Non-boolean value in '", ACCENT_PARAM_NAME,
                         "' while constructing text_token_normalizing_stream "
                         "from VPack arguments"));

          return false;
        }

        options.accent = accent_slice.getBool();
      }

      return true;
    }

    IRS_LOG_ERROR(
      absl::StrCat("Missing '", LOCALE_PARAM_NAME,
                   "' while constructing text_token_normalizing_stream from "
                   "VPack arguments"));
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught error '", ex.what(),
      "' while constructing text_token_normalizing_stream from VPack"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing text_token_normalizing_stream from "
      "VPack arguments");
  }
  return false;
}
////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "locale"(string): the locale to use for stemming <required>
///        "case"(string enum): modify token case using "locale"
///        "accent"(bool): leave accents
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  analysis::normalizing_token_stream::options_t options;
  if (parse_vpack_options(slice, options)) {
    return std::make_unique<analysis::normalizing_token_stream>(
      std::move(options));
  } else {
    return nullptr;
  }
}

analysis::analyzer::ptr make_vpack(std::string_view args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  return make_vpack(slice);
}
///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param options reference to analyzer options storage
/// @param definition string for storing json document with config
///////////////////////////////////////////////////////////////////////////////
bool make_vpack_config(
  const analysis::normalizing_token_stream::options_t& options,
  VPackBuilder* builder) {
  VPackObjectBuilder object(builder);
  {
    // locale
    const auto& locale_name = options.locale.getBaseName();
    builder->add(LOCALE_PARAM_NAME, VPackValue(locale_name));

    // case convert
    auto case_value =
      std::find_if(CASE_CONVERT_MAP.begin(), CASE_CONVERT_MAP.end(),
                   [&options](const decltype(CASE_CONVERT_MAP)::value_type& v) {
                     return v.second == options.case_convert;
                   });
    if (case_value != CASE_CONVERT_MAP.end()) {
      builder->add(CASE_CONVERT_PARAM_NAME, VPackValue(case_value->first));
    } else {
      IRS_LOG_ERROR(
        absl::StrCat("Invalid case_convert value in text analyzer options: ",
                     static_cast<int>(options.case_convert)));
      return false;
    }

    // Accent
    builder->add(ACCENT_PARAM_NAME, VPackValue(options.accent));
  }

  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* builder) {
  analysis::normalizing_token_stream::options_t options;
  if (parse_vpack_options(slice, options)) {
    return make_vpack_config(options, builder);
  } else {
    return false;
  }
}

bool normalize_vpack_config(std::string_view args, std::string& config) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  VPackBuilder builder;
  if (normalize_vpack_config(slice, &builder)) {
    config.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
    return true;
  }
  return false;
}

analysis::analyzer::ptr make_json(std::string_view args) {
  try {
    if (IsNull(args)) {
      IRS_LOG_ERROR(
        "Null arguments while constructing text_token_normalizing_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    return make_vpack(vpack->slice());
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught error '", ex.what(),
      "' while constructing text_token_normalizing_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing text_token_normalizing_stream from "
      "JSON");
  }
  return nullptr;
}

bool normalize_json_config(std::string_view args, std::string& definition) {
  try {
    if (IsNull(args)) {
      IRS_LOG_ERROR(
        "Null arguments while normalizing text_token_normalizing_stream");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    VPackBuilder builder;
    if (normalize_vpack_config(vpack->slice(), &builder)) {
      definition = builder.toString();
      return !definition.empty();
    }
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught error '", ex.what(),
      "' while normalizing text_token_normalizing_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing text_token_normalizing_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_JSON(analysis::normalizing_token_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_VPACK(analysis::normalizing_token_stream, make_vpack,
                        normalize_vpack_config);

}  // namespace

namespace irs {
namespace analysis {

void normalizing_token_stream::state_deleter_t::operator()(
  state_t* p) const noexcept {
  delete p;
}

normalizing_token_stream::normalizing_token_stream(const options_t& options)
  : state_{new state_t{options}}, term_eof_{true} {}

void normalizing_token_stream::init() {
  REGISTER_ANALYZER_JSON(normalizing_token_stream, make_json,
                         normalize_json_config);  // match registration above
  REGISTER_ANALYZER_VPACK(normalizing_token_stream, make_vpack,
                          normalize_vpack_config);  // match registration above
}

bool normalizing_token_stream::next() {
  if (term_eof_) {
    return false;
  }

  term_eof_ = true;

  return true;
}

bool normalizing_token_stream::reset(std::string_view data) {
  auto err =
    UErrorCode::U_ZERO_ERROR;  // a value that passes the U_SUCCESS() test

  if (!state_->normalizer) {
    // reusable object owned by ICU
    state_->normalizer =
      IRESEARCH_ICU_NAMESPACE::Normalizer2::getNFCInstance(err);

    if (!U_SUCCESS(err) || !state_->normalizer) {
      state_->normalizer = nullptr;

      return false;
    }
  }

  if (!state_->options.accent && !state_->transliterator) {
    // transliteration rule taken verbatim from:
    // http://userguide.icu-project.org/transforms/general do not allocate
    // statically since it causes memory leaks in ICU
    const IRESEARCH_ICU_NAMESPACE::UnicodeString collationRule(
      "NFD; [:Nonspacing Mark:] Remove; NFC");

    // reusable object owned by *this
    state_->transliterator.reset(
      IRESEARCH_ICU_NAMESPACE::Transliterator::createInstance(
        collationRule, UTransDirection::UTRANS_FORWARD, err));

    if (!U_SUCCESS(err) || !state_->transliterator) {
      state_->transliterator.reset();

      return false;
    }
  }

  // convert input string for use with ICU
  if (data.size() >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return false;
  }

  state_->data = IRESEARCH_ICU_NAMESPACE::UnicodeString::fromUTF8(
    IRESEARCH_ICU_NAMESPACE::StringPiece{data.data(),
                                         static_cast<int32_t>(data.size())});

  // normalize unicode
  state_->normalizer->normalize(state_->data, state_->token, err);

  if (!U_SUCCESS(err)) {
    // use non-normalized value if normalization failure
    state_->token = std::move(state_->data);
  }

  // case-convert unicode
  switch (state_->options.case_convert) {
    case LOWER:
      state_->token.toLower(state_->options.locale);  // inplace case-conversion
      break;
    case UPPER:
      state_->token.toUpper(state_->options.locale);  // inplace case-conversion
      break;
    case NONE:
      break;
  }

  // collate value, e.g. remove accents
  if (state_->transliterator) {
    state_->transliterator->transliterate(
      state_->token);  // inplace translitiration
  }

  state_->term_buf.clear();
  state_->token.toUTF8String(state_->term_buf);

  // use the normalized value
  static_assert(sizeof(byte_type) == sizeof(char));
  std::get<term_attribute>(attrs_).value =
    irs::ViewCast<byte_type>(std::string_view{state_->term_buf});
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());
  term_eof_ = false;

  return true;
}

}  // namespace analysis
}  // namespace irs
