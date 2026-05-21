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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "segmentation_token_stream.hpp"

#include <frozen/unordered_map.h>

#include <boost/text/case_mapping.hpp>
#include <boost/text/word_break.hpp>
#include <string_view>

#include "utils/hash_utils.hpp"
#include "utils/utf8_character_utils.hpp"
#include "utils/vpack_utils.hpp"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace {

using namespace irs;

constexpr std::string_view kCaseConvertParamName{"case"};
constexpr std::string_view kBreakParamName{"break"};

constexpr frozen::unordered_map<
  std::string_view,
  analysis::segmentation_token_stream::options_t::case_convert_t, 3>
  kCaseConvertMap = {
    {"lower",
     analysis::segmentation_token_stream::options_t::case_convert_t::LOWER},
    {"none",
     analysis::segmentation_token_stream::options_t::case_convert_t::NONE},
    {"upper",
     analysis::segmentation_token_stream::options_t::case_convert_t::UPPER},
};

constexpr frozen::unordered_map<
  std::string_view,
  analysis::segmentation_token_stream::options_t::word_break_t, 3>
  kBreakConvertMap = {
    {"all", analysis::segmentation_token_stream::options_t::word_break_t::ALL},
    {"alpha",
     analysis::segmentation_token_stream::options_t::word_break_t::ALPHA},
    {"graphic",
     analysis::segmentation_token_stream::options_t::word_break_t::GRAPHIC},
};

bool parse_vpack_options(
  const VPackSlice slice,
  analysis::segmentation_token_stream::options_t& options) {
  if (!slice.isObject()) {
    IRS_LOG_ERROR("Slice for segmentation_token_stream is not an object");
    return false;
  }
  if (auto case_convert_slice = slice.get(kCaseConvertParamName);
      !case_convert_slice.isNone()) {
    if (!case_convert_slice.isString()) {
      IRS_LOG_WARN(
        absl::StrCat("Invalid type '", kCaseConvertParamName,
                     "' (string expected) for segmentation_token_stream from "
                     "VPack arguments"));
      return false;
    }
    auto case_convert = case_convert_slice.stringView();
    auto itr = kCaseConvertMap.find(
      std::string_view(case_convert.data(), case_convert.size()));

    if (itr == kCaseConvertMap.end()) {
      IRS_LOG_WARN(
        absl::StrCat("Invalid value in '", kCaseConvertParamName,
                     "' for segmentation_token_stream from VPack arguments"));
      return false;
    }
    options.case_convert = itr->second;
  }
  if (auto break_type_slice = slice.get(kBreakParamName);
      !break_type_slice.isNone()) {
    if (!break_type_slice.isString()) {
      IRS_LOG_WARN(
        absl::StrCat("Invalid type '", kBreakParamName,
                     "' (string expected) for segmentation_token_stream from "
                     "VPack arguments"));
      return false;
    }
    auto break_type = break_type_slice.stringView();
    auto itr = kBreakConvertMap.find(
      std::string_view(break_type.data(), break_type.size()));

    if (itr == kBreakConvertMap.end()) {
      IRS_LOG_WARN(
        absl::StrCat("Invalid value in '", kBreakParamName,
                     "' for segmentation_token_stream from VPack arguments"));
      return false;
    }
    options.word_break = itr->second;
  }
  return true;
}

bool make_vpack_config(
  const analysis::segmentation_token_stream::options_t& options,
  VPackBuilder* builder) {
  VPackObjectBuilder object(builder);
  {
    auto it = std::find_if(kCaseConvertMap.begin(), kCaseConvertMap.end(),
                           [v = options.case_convert](
                             const decltype(kCaseConvertMap)::value_type& m) {
                             return m.second == v;
                           });
    if (it != kCaseConvertMap.end()) {
      builder->add(kCaseConvertParamName, VPackValue(it->first));
    } else {
      IRS_LOG_WARN(absl::StrCat(
        "Invalid value in '", kCaseConvertParamName,
        "' for normalizing segmentation_token_stream from Value is: ",
        options.case_convert));
      return false;
    }
  }
  {
    auto it = std::find_if(kBreakConvertMap.begin(), kBreakConvertMap.end(),
                           [v = options.word_break](
                             const decltype(kBreakConvertMap)::value_type& m) {
                             return m.second == v;
                           });
    if (it != kBreakConvertMap.end()) {
      builder->add(kBreakParamName, VPackValue(it->first));
    } else {
      IRS_LOG_WARN(absl::StrCat(
        "Invalid value in '", kBreakParamName,
        "' for normalizing segmentation_token_stream from Value is: ",
        options.word_break));
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a Vpack slice object with the following attributes:
///        "case"(string enum): modify token case
///        "break"(string enum): word breaking method
////////////////////////////////////////////////////////////////////////////////
analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  try {
    analysis::segmentation_token_stream::options_t options;
    if (!parse_vpack_options(slice, options)) {
      return nullptr;
    }
    return std::make_unique<analysis::segmentation_token_stream>(
      std::move(options));
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught error '", ex.what(),
      "' while constructing segmentation_token_stream from VPack arguments"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing segmentation_token_stream from VPack "
      "arguments");
  }
  return nullptr;
}

analysis::analyzer::ptr make_vpack(std::string_view args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  return make_vpack(slice);
}

bool normalize_vpack_config(const VPackSlice slice,
                            VPackBuilder* vpack_builder) {
  analysis::segmentation_token_stream::options_t options;
  try {
    if (parse_vpack_options(slice, options)) {
      return make_vpack_config(options, vpack_builder);
    } else {
      return false;
    }
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(absl::StrCat(
      "Caught error '", ex.what(),
      "' while normalizing segmentation_token_stream from VPack arguments"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing segmentation_token_stream from VPack "
      "arguments");
  }
  return false;
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
        "Null arguments while constructing segmentation_token_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    return make_vpack(vpack->slice());
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while constructing segmentation_token_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing segmentation_token_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(std::string_view args, std::string& definition) {
  try {
    if (IsNull(args)) {
      IRS_LOG_ERROR(
        "Null arguments while normalizing segmentation_token_stream");
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
                   "' while normalizing segmentation_token_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing segmentation_token_stream from JSON");
  }
  return false;
}

using word_break_t =
  analysis::segmentation_token_stream::options_t::word_break_t;

template<typename Iterator>
bool accept_token(Iterator begin, Iterator end, word_break_t wb) {
  switch (wb) {
    case word_break_t::ALL:
      return true;
    case word_break_t::GRAPHIC:
      return std::find_if_not(begin, end, utf8_utils::CharIsWhiteSpace) != end;
    case word_break_t::ALPHA:
      return std::find_if(begin, end, utf8_utils::CharIsAlphanumeric) != end;
  }
}

}  // namespace

namespace irs::analysis {

using namespace boost::text;

using data_t =
  decltype(as_graphemes(std::string_view{}.begin(), std::string_view{}.end()));
using iterator_t = decltype(next_word_break(data_t{}, data_t{}.begin()));

struct segmentation_token_stream::state_t {
  data_t data;
  iterator_t begin;
  iterator_t end;
};

void segmentation_token_stream::state_deleter_t::operator()(
  state_t* p) const noexcept {
  delete p;
}

REGISTER_ANALYZER_VPACK(segmentation_token_stream, make_vpack,
                        normalize_vpack_config);
REGISTER_ANALYZER_JSON(segmentation_token_stream, make_json,
                       normalize_json_config);

void segmentation_token_stream::init() {
  REGISTER_ANALYZER_VPACK(segmentation_token_stream, make_vpack,
                          normalize_vpack_config);  // match registration above
  REGISTER_ANALYZER_JSON(segmentation_token_stream, make_json,
                         normalize_json_config);  // match registration above
}

segmentation_token_stream::segmentation_token_stream(
  segmentation_token_stream::options_t&& options)
  : state_{new state_t()}, options_{options} {}

bool segmentation_token_stream::next() {
  while (true) {
    const auto gr_begin = state_->begin;
    const auto gr_end = next_word_break(state_->data, gr_begin);

    const auto begin = gr_begin.base();
    const auto end = gr_end.base();

    const auto length =
      static_cast<size_t>(std::distance(begin.base(), end.base()));

    if (length == 0) {  // eof
      return false;
    }

    auto& offset = std::get<irs::offset>(attrs_);
    IRS_ASSERT(offset.end + length <= std::numeric_limits<uint32_t>::max());

    offset.start = offset.end;
    offset.end += static_cast<uint32_t>(length);
    state_->begin = gr_end;

    if (!accept_token(begin, end, options_.word_break)) {
      continue;
    }

    switch (auto& term = std::get<term_attribute>(attrs_);
            options_.case_convert) {
      case options_t::case_convert_t::NONE:
        IRS_ASSERT(length);
        // on *nix base returns pointer on msvc it return iterator
        term.value = {reinterpret_cast<const byte_type*>(&(*begin.base())),
                      length};
        break;
      case options_t::case_convert_t::LOWER:
        term_buf_.clear();
        to_lower(begin, begin, end, from_utf32_back_inserter(term_buf_));
        term.value = irs::ViewCast<byte_type>(std::string_view{term_buf_});
        break;
      case options_t::case_convert_t::UPPER:
        term_buf_.clear();
        to_upper(begin, begin, end, from_utf32_back_inserter(term_buf_));
        term.value = irs::ViewCast<byte_type>(std::string_view{term_buf_});
        break;
    }

    return true;
  }
}

bool segmentation_token_stream::reset(std::string_view data) {
  state_->data = as_graphemes(data.begin(), data.end());
  state_->begin = state_->data.begin();
  state_->end = state_->data.end();

  return true;
}

}  // namespace irs::analysis
