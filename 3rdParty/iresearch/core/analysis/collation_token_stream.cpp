////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "collation_token_stream.hpp"

#include <unicode/locid.h>
#include <unicode/coll.h>

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "utils/locale_utils.hpp"
#include "utils/vpack_utils.hpp"

namespace {

using namespace irs;

constexpr VPackStringRef LOCALE_PARAM_NAME{"locale"};

bool parse_vpack_options(
    const VPackSlice slice,
    analysis::collation_token_stream::options_t& options) {

  if (!slice.isObject() && !slice.isString()) {
    IR_FRMT_ERROR(
      "Slice for collation_token_stream is not an object or string");
    return false;
  }

  try {
    switch (slice.type()) {
      case VPackValueType::String:
        return locale_utils::icu_locale(get_string<string_ref>(slice), options.locale);  // required
      case VPackValueType::Object:
      {
        auto param_name_slice = slice.get(LOCALE_PARAM_NAME);
        if (param_name_slice.isString()) {
          if (!locale_utils::icu_locale(get_string<string_ref>(param_name_slice), options.locale)) {
            return false;
          }

          return true;
        }
      }
      [[fallthrough]];
      default:
        IR_FRMT_ERROR(
          "Missing '%s' while constructing collation_token_stream"
          "from VPack arguments",
          LOCALE_PARAM_NAME.data());
    }
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing collation_token_stream from VPack",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing collation_token_stream from "
      "VPack arguments");
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "locale"(string): the locale to use for stemming <required>
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  analysis::collation_token_stream::options_t options;
  if (parse_vpack_options(slice, options)) {
    return memory::make_unique<analysis::collation_token_stream>(std::move(options));
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
/// @param options reference to analyzer options storage
/// @param definition string for storing json document with config
///////////////////////////////////////////////////////////////////////////////
bool make_vpack_config(
    const analysis::collation_token_stream::options_t& options,
    VPackBuilder* builder) {

  VPackObjectBuilder object(builder);
  {
    // locale
    const auto& locale_name = locale_utils::name(options.locale);
    builder->add(LOCALE_PARAM_NAME, VPackValue(locale_name));
  }

  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* builder) {
  analysis::collation_token_stream::options_t options;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a language to use for normalizing
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_text(const string_ref& args) {
  try {
    analysis::collation_token_stream::options_t options;

    if (locale_utils::icu_locale(args, options.locale)) {// interpret 'args' as a locale name
      return memory::make_unique<analysis::collation_token_stream>(
          std::move(options));
    }
  } catch (...) {
    std::string err_msg = static_cast<std::string>(args);
    IR_FRMT_ERROR(
      "Caught error while constructing collation_token_stream TEXT arguments: %s",
      err_msg.c_str());
  }

  return nullptr;
}

bool normalize_text_config(const string_ref& args,
                           std::string& definition) {
  std::locale locale;
  if (locale_utils::icu_locale(args, locale)){
    definition = locale_utils::name(locale);
    return true;
  }
  return false;
}

analysis::analyzer::ptr make_json(const string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while constructing collation_token_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    return make_vpack(vpack->slice());
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing collation_token_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing collation_token_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(const string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing collation_token_stream");
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
      "Caught error '%s' while normalizing collation_token_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while normalizing text_token_normalizing_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_JSON(analysis::collation_token_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_TEXT(analysis::collation_token_stream, make_text,
                       normalize_text_config);
REGISTER_ANALYZER_VPACK(analysis::collation_token_stream, make_vpack,
                       normalize_vpack_config);

}

namespace iresearch {
namespace analysis {

constexpr size_t MAX_TOKEN_SIZE = 1 << 15;

struct collation_token_stream::state_t {
  icu::Locale icu_locale;
  const options_t options;
  std::unique_ptr<icu::Collator> collator;
  std::string utf8_buf;
  byte_type term_buf[MAX_TOKEN_SIZE];

  state_t(const options_t& opts)
    : icu_locale("C"),
      options(opts) {
    // NOTE: use of the default constructor for Locale() or
    //       use of Locale::createFromName(nullptr)
    //       causes a memory leak with Boost 1.58, as detected by valgrind
    icu_locale.setToBogus(); // set to uninitialized
  }
};

/*static*/ void collation_token_stream::init() {
  REGISTER_ANALYZER_JSON(collation_token_stream, make_json,
                         normalize_json_config);
  REGISTER_ANALYZER_TEXT(collation_token_stream, make_text,
                         normalize_text_config);
  REGISTER_ANALYZER_VPACK(collation_token_stream, make_vpack,
                         normalize_vpack_config);
}

void collation_token_stream::state_deleter_t::operator()(state_t* p) const noexcept {
  delete p;
}

/*static*/ analyzer::ptr collation_token_stream::make(
    const string_ref& locale) {
  return make_text(locale);
}

collation_token_stream::collation_token_stream(
    const options_t& options)
  : analyzer{irs::type<collation_token_stream>::get()},
    state_{new state_t(options)},
    term_eof_{true} {
}

bool collation_token_stream::reset(const string_ref& data) {
  if (state_->icu_locale.isBogus()) {
    state_->icu_locale = icu::Locale(
      std::string(locale_utils::language(state_->options.locale)).c_str(),
      std::string(locale_utils::country(state_->options.locale)).c_str());

    if (state_->icu_locale.isBogus()) {
      return false;
    }
  }

  if (!state_->collator) {
    auto err = UErrorCode::U_ZERO_ERROR;
    state_->collator.reset(icu::Collator::createInstance(state_->icu_locale, err));

    if (!U_SUCCESS(err) || !state_->collator) {
      state_->collator.reset();

      return false;
    }
  }

  // ...........................................................................
  // convert encoding to UTF8 for use with ICU
  // ...........................................................................
  string_ref data_utf8_ref;
  if (locale_utils::is_utf8(state_->options.locale)) {
    data_utf8_ref = data;
  } else {
    // valid conversion since 'locale_' was created with internal unicode encoding
    if (!locale_utils::append_internal(state_->utf8_buf, data, state_->options.locale)) {
      return false; // UTF8 conversion failure
    }
    data_utf8_ref = state_->utf8_buf;
  }

  if (data_utf8_ref.size() > static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return false; // ICU UnicodeString signatures can handle at most INT32_MAX
  }

  const icu::UnicodeString icu_token = icu::UnicodeString::fromUTF8(
    icu::StringPiece(data_utf8_ref.c_str(), static_cast<int32_t>(data_utf8_ref.size())));

  int32_t term_size = state_->collator->getSortKey(
    icu_token, state_->term_buf, sizeof state_->term_buf);

  // https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/classicu_1_1Collator.html
  // according to ICU docs sort keys are always zero-terminated,
  // there is no reason to store terminal zero in term dictionary
  assert(term_size > 0);

  --term_size;
  assert(0 == state_->term_buf[term_size]);

  if (term_size > static_cast<int32_t>(sizeof state_->term_buf)) {
    IR_FRMT_ERROR(
      "Collated token is %d bytes length which exceeds maximum allowed length of %d bytes",
      term_size, static_cast<int32_t>(sizeof state_->term_buf));
    return false;
  }

  std::get<term_attribute>(attrs_).value = {
    state_->term_buf, static_cast<size_t>(term_size) };
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());
  std::get<payload>(attrs_).value = ref_cast<uint8_t>(data);
  term_eof_ = false;

  return true;
}

} // analysis
} // iresearch
