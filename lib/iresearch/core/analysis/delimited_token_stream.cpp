////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "delimited_token_stream.hpp"

#include <string_view>

#include "utils/vpack_utils.hpp"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/velocypack-aliases.h"

namespace {

irs::bytes_view eval_term(irs::bstring& buf, irs::bytes_view data) {
  if (!data.size() || '"' != data[0]) {
    return data;  // not a quoted term (even if quotes inside
  }

  buf.clear();

  bool escaped = false;
  size_t start = 1;

  for (size_t i = 1, count = data.size(); i < count; ++i) {
    if ('"' == data[i]) {
      if (escaped && start == i) {  // an escaped quote
        escaped = false;

        continue;
      }

      if (escaped) {
        break;  // mismatched quote
      }

      buf.append(&data[start], i - start);
      escaped = true;
      start = i + 1;
    }
  }

  return start != 1 && start == data.size()
           ? irs::bytes_view(buf)
           : data;  // return identity for mismatched quotes
}

size_t find_delimiter(irs::bytes_view data, irs::bytes_view delim) {
  if (irs::IsNull(delim)) {
    return data.size();
  }

  bool quoted = false;

  for (size_t i = 0, count = data.size(); i < count; ++i) {
    if (quoted) {
      if ('"' == data[i]) {
        quoted = false;
      }

      continue;
    }

    if (data.size() - i < delim.size()) {
      break;  // no more delimiters in data
    }

    if (0 == memcmp(data.data() + i, delim.data(), delim.size()) &&
        (i || delim.size())) {  // do not match empty delim at data start
      return i;  // delimiter match takes precedence over '"' match
    }

    if ('"' == data[i]) {
      quoted = true;
    }
  }

  return data.size();
}

constexpr std::string_view DELIMITER_PARAM_NAME{"delimiter"};

bool parse_vpack_options(const VPackSlice slice, std::string& delimiter) {
  if (!slice.isObject() && !slice.isString()) {
    IRS_LOG_ERROR(
      "Slice for delimited_token_stream is not an object or string");
    return false;
  }

  switch (slice.type()) {
    case VPackValueType::String:
      delimiter = slice.stringView();
      return true;
    case VPackValueType::Object:
      if (auto delim_type_slice = slice.get(DELIMITER_PARAM_NAME);
          !delim_type_slice.isNone()) {
        if (!delim_type_slice.isString()) {
          IRS_LOG_WARN(
            absl::StrCat("Invalid type '", DELIMITER_PARAM_NAME,
                         "' (string expected) for delimited_token_stream from "
                         "VPack arguments"));
          return false;
        }
        delimiter = delim_type_slice.stringView();
        return true;
      }
      break;
    default:
      break;
  }

  IRS_LOG_ERROR(absl::StrCat(
    "Missing '", DELIMITER_PARAM_NAME,
    "' while constructing delimited_token_stream from VPack arguments"));

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "delimiter"(string): the delimiter to use for tokenization <required>
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  std::string delimiter;
  if (parse_vpack_options(slice, delimiter)) {
    return irs::analysis::delimited_token_stream::make(delimiter);
  } else {
    return nullptr;
  }
}

irs::analysis::analyzer::ptr make_vpack(std::string_view args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  return make_vpack(slice);
}
///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param delimiter reference to analyzer options storage
/// @param definition string for storing json document with config
///////////////////////////////////////////////////////////////////////////////
bool make_vpack_config(std::string_view delimiter,
                       VPackBuilder* vpack_builder) {
  VPackObjectBuilder object(vpack_builder);
  {
    // delimiter
    vpack_builder->add(DELIMITER_PARAM_NAME, VPackValue(delimiter));
  }

  return true;
}

bool normalize_vpack_config(const VPackSlice slice,
                            VPackBuilder* vpack_builder) {
  std::string delimiter;
  if (parse_vpack_options(slice, delimiter)) {
    return make_vpack_config(delimiter, vpack_builder);
  } else {
    return false;
  }
}

bool normalize_vpack_config(std::string_view args, std::string& definition) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.data()));
  VPackBuilder builder;
  bool res = normalize_vpack_config(slice, &builder);
  if (res) {
    definition.assign(builder.slice().startAs<char>(),
                      builder.slice().byteSize());
  }
  return res;
}

irs::analysis::analyzer::ptr make_json(std::string_view args) {
  try {
    if (irs::IsNull(args)) {
      IRS_LOG_ERROR("Null arguments while constructing delimited_token_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    return make_vpack(vpack->slice());
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while constructing delimited_token_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while constructing delimited_token_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(std::string_view args, std::string& definition) {
  try {
    if (irs::IsNull(args)) {
      IRS_LOG_ERROR("Null arguments while normalizing delimited_token_stream");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.data(), args.size());
    VPackBuilder vpack_builder;
    if (normalize_vpack_config(vpack->slice(), &vpack_builder)) {
      definition = vpack_builder.toString();
      return !definition.empty();
    }
  } catch (const VPackException& ex) {
    IRS_LOG_ERROR(
      absl::StrCat("Caught error '", ex.what(),
                   "' while normalizing delimited_token_stream from JSON"));
  } catch (...) {
    IRS_LOG_ERROR(
      "Caught error while normalizing delimited_token_stream from JSON");
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a delimiter to use for tokenization
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(std::string_view args) {
  return std::make_unique<irs::analysis::delimited_token_stream>(args);
}

bool normalize_text_config(std::string_view delimiter,
                           std::string& definition) {
  definition = delimiter;
  return true;
}

REGISTER_ANALYZER_VPACK(irs::analysis::delimited_token_stream, make_vpack,
                        normalize_vpack_config);
REGISTER_ANALYZER_JSON(irs::analysis::delimited_token_stream, make_json,
                       normalize_json_config);
REGISTER_ANALYZER_TEXT(irs::analysis::delimited_token_stream, make_text,
                       normalize_text_config);

}  // namespace

namespace irs {
namespace analysis {

delimited_token_stream::delimited_token_stream(std::string_view delimiter)
  : delim_(ViewCast<byte_type>(delimiter)) {
  if (!irs::IsNull(delim_)) {
    delim_buf_ = delim_;  // keep a local copy of the delimiter
    delim_ = delim_buf_;  // update the delimter to point at the local copy
  }
}

analyzer::ptr delimited_token_stream::make(std::string_view delimiter) {
  return make_text(delimiter);
}

void delimited_token_stream::init() {
  REGISTER_ANALYZER_VPACK(delimited_token_stream, make_vpack,
                          normalize_vpack_config);  // match registration above
  REGISTER_ANALYZER_JSON(delimited_token_stream, make_json,
                         normalize_json_config);  // match registration above
  REGISTER_ANALYZER_TEXT(delimited_token_stream, make_text,
                         normalize_text_config);  // match registration above
}

bool delimited_token_stream::next() {
  if (irs::IsNull(data_)) {
    return false;
  }

  auto& offset = std::get<irs::offset>(attrs_);

  auto size = find_delimiter(data_, delim_);
  auto next = std::max(size_t(1), size + delim_.size());
  auto start =
    offset.end + uint32_t(delim_.size());  // value is allowed to overflow, will
                                           // only produce invalid result
  auto end = start + size;

  if (std::numeric_limits<uint32_t>::max() < end) {
    return false;  // cannot fit the next token into offset calculation
  }

  auto& term = std::get<term_attribute>(attrs_);

  offset.start = start;
  offset.end = uint32_t(end);
  term.value = irs::IsNull(delim_)
                 ? bytes_view{data_.data(), size}
                 : eval_term(term_buf_, bytes_view(data_.data(), size));
  data_ = size >= data_.size()
            ? bytes_view{}
            : bytes_view{data_.data() + next, data_.size() - next};

  return true;
}

bool delimited_token_stream::reset(std::string_view data) {
  data_ = ViewCast<byte_type>(data);

  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end =
    0 - uint32_t(delim_.size());  // counterpart to computation in next() above

  return true;
}

}  // namespace analysis
}  // namespace irs
