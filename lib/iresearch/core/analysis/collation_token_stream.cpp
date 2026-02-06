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

#include <unicode/coll.h>
#include <unicode/locid.h>

#include <string_view>

#include "collation_token_stream_encoder.hpp"
#include "utils/log.hpp"
#include "utils/vpack_utils.hpp"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace {

using namespace irs;

constexpr std::string_view LOCALE_PARAM_NAME{"locale"};

bool locale_from_slice(VPackSlice slice,
                       IRESEARCH_ICU_NAMESPACE::Locale& locale) {
  if (!slice.isString()) {
    IRS_LOG_WARN(absl::StrCat(
      "Non-string value in '", LOCALE_PARAM_NAME,
      "' while constructing collation_token_stream from VPack arguments"));

    return false;
  }

  const auto locale_name = slice.copyString();

  locale =
    IRESEARCH_ICU_NAMESPACE::Locale::createCanonical(locale_name.c_str());

  if (locale.isBogus()) {
    IRS_LOG_WARN(absl::StrCat(
      "Failed to instantiate locale from the supplied string '", locale_name,
      "' while constructing collation_token_stream from VPack arguments"));

    return false;
  }

  // validate creation of IRESEARCH_ICU_NAMESPACE::Collator
  auto err = UErrorCode::U_ZERO_ERROR;
  std::unique_ptr<IRESEARCH_ICU_NAMESPACE::Collator> collator{
    IRESEARCH_ICU_NAMESPACE::Collator::createInstance(locale, err)};

  if (!collator) {
    IRS_LOG_WARN(absl::StrCat("Can't instantiate icu::Collator from locale: ",
                              locale_name));
    return false;
  }

  // print warn message
  if (err != UErrorCode::U_ZERO_ERROR) {
    IRS_LOG(
      U_FAILURE(err) ? log::Level::kWarn : log::Level::kTrace,
      absl::StrCat("Failure while instantiation of icu::Collator from locale: ",
                   locale_name, ", ", u_errorName(err)));
  }

  return U_SUCCESS(err);
}

bool parse_vpack_options(const VPackSlice slice,
                         analysis::collation_token_stream::options_t& options) {
  if (!slice.isObject()) {
    IRS_LOG_ERROR("Slice for collation_token_stream is not an object");
    return false;
  }

  try {
    const auto locale_slice = slice.get(LOCALE_PARAM_NAME);

    if (locale_slice.isNone()) {
      IRS_LOG_ERROR(absl::StrCat(
        "Missing '", LOCALE_PARAM_NAME,
        "' while constructing collation_token_stream from VPack arguments"));

      return false;
    }

    return locale_from_slice(locale_slice, options.locale);
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while constructing collation_token_stream from VPack"));
  } catch (...) {
    IRS_LOG_ERROR(
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
    return std::make_unique<analysis::collation_token_stream>(
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
  const analysis::collation_token_stream::options_t& options,
  VPackBuilder* builder) {
  VPackObjectBuilder object{builder};

  const auto locale_name = options.locale.getName();
  builder->add(LOCALE_PARAM_NAME, VPackValue(locale_name));
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
      IRS_LOG_ERROR("Null arguments while constructing collation_token_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    return make_vpack(vpack->slice());
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while constructing collation_token_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing collation_token_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(std::string_view args, std::string& definition) {
  try {
    if (IsNull(args)) {
      IRS_LOG_ERROR("Null arguments while normalizing collation_token_stream");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    VPackBuilder builder;
    if (normalize_vpack_config(vpack->slice(), &builder)) {
      definition = builder.toString();
      return !definition.empty();
    }
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while normalizing collation_token_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing collation_token_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_JSON(irs::analysis::collation_token_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_VPACK(irs::analysis::collation_token_stream, make_vpack,
                        normalize_vpack_config);

}  // namespace

namespace irs {
namespace analysis {

constexpr size_t MAX_TOKEN_SIZE = 1 << 15;

struct collation_token_stream::state_t {
  const options_t options;
  std::unique_ptr<IRESEARCH_ICU_NAMESPACE::Collator> collator;
  byte_type term_buf[MAX_TOKEN_SIZE];

  explicit state_t(const options_t& opts) : options(opts) {}
};

void collation_token_stream::init() {
  REGISTER_ANALYZER_JSON(collation_token_stream, make_json,
                         normalize_json_config);
  REGISTER_ANALYZER_VPACK(collation_token_stream, make_vpack,
                          normalize_vpack_config);
}

void collation_token_stream::state_deleter_t::operator()(
  state_t* p) const noexcept {
  delete p;
}

collation_token_stream::collation_token_stream(const options_t& options)
  : state_{new state_t(options)}, term_eof_{true} {}

bool collation_token_stream::reset(std::string_view data) {
  if (!state_->collator) {
    auto err = UErrorCode::U_ZERO_ERROR;
    state_->collator.reset(IRESEARCH_ICU_NAMESPACE::Collator::createInstance(
      state_->options.locale, err));

    if (!U_SUCCESS(err) || !state_->collator) {
      state_->collator.reset();

      return false;
    }
  }

  if (data.size() >
      static_cast<uint32_t>(std::numeric_limits<int32_t>::max())) {
    return false;  // ICU UnicodeString signatures can handle at most INT32_MAX
  }

  const IRESEARCH_ICU_NAMESPACE::UnicodeString icu_token =
    IRESEARCH_ICU_NAMESPACE::UnicodeString::fromUTF8(
      IRESEARCH_ICU_NAMESPACE::StringPiece(data.data(),
                                           static_cast<int32_t>(data.size())));

  byte_type raw_term_buf[MAX_TOKEN_SIZE];
  static_assert(sizeof raw_term_buf == sizeof state_->term_buf);

  auto buf = state_->options.forceUtf8 ? raw_term_buf : state_->term_buf;
  int32_t term_size =
    state_->collator->getSortKey(icu_token, buf, sizeof raw_term_buf);

  // https://unicode-org.github.io/icu-docs/apidoc/released/icu4c/classicu_1_1Collator.html
  // according to ICU docs sort keys are always zero-terminated,
  // there is no reason to store terminal zero in term dictionary
  IRS_ASSERT(term_size > 0);
  --term_size;
  IRS_ASSERT(0 == buf[term_size]);
  if (term_size > static_cast<int32_t>(sizeof raw_term_buf)) {
    IRS_LOG_ERROR(
      absl::StrCat("Collated token is ", term_size,
                   " bytes length which exceeds maximum allowed length of ",
                   static_cast<int32_t>(sizeof raw_term_buf), " bytes"));
    return false;
  }
  size_t termBufIdx = static_cast<size_t>(term_size);
  if (state_->options.forceUtf8) {
    // enforce valid UTF-8 string
    IRS_ASSERT(buf == raw_term_buf);
    termBufIdx = 0;
    for (decltype(term_size) i{}; i < term_size; ++i) {
      static_assert(sizeof(raw_term_buf[i]) * (1 << CHAR_BIT) <=
                    kRecalcMap.size());
      const auto [offset, size] = kRecalcMap[raw_term_buf[i]];
      if ((termBufIdx + size) > sizeof state_->term_buf) {
        IRS_LOG_ERROR(absl::StrCat("Collated token is more than ",
                                   sizeof state_->term_buf,
                                   " bytes length after encoding."));
        return false;
      }
      IRS_ASSERT(size <= 2);
      state_->term_buf[termBufIdx++] = kBytesRecalcMap[offset];
      if (size == 2) {
        state_->term_buf[termBufIdx++] = kBytesRecalcMap[offset + 1];
      }
    }
  }

  std::get<term_attribute>(attrs_).value = {state_->term_buf, termBufIdx};
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(data.size());
  term_eof_ = false;

  return true;
}

}  // namespace analysis
}  // namespace irs
