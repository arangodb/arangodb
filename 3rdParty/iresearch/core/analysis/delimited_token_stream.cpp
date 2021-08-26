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

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"
#include "utils/vpack_utils.hpp"

namespace {

irs::bytes_ref eval_term(irs::bstring& buf, const irs::bytes_ref& data) {
  if (!data.size() || '"' != data[0]) {
    return data; // not a quoted term (even if quotes inside
  }

  buf.clear();

  bool escaped = false;
  size_t start = 1;

  for(size_t i = 1, count = data.size(); i < count; ++i) {
    if ('"' == data[i]) {
      if (escaped && start == i) { // an escaped quote
        escaped = false;

        continue;
      }

      if (escaped) {
        escaped = false;

        break; // mismatched quote
      }

      buf.append(&data[start], i - start);
      escaped = true;
      start = i + 1;
    }
  }

  return start != 1 && start == data.size() ? irs::bytes_ref(buf) : data; // return identity for mismatched quotes
}

size_t find_delimiter(const irs::bytes_ref& data, const irs::bytes_ref& delim) {
  if (delim.null()) {
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
      break; // no more delimiters in data
    }

    if (0 == memcmp(data.c_str() + i, delim.c_str(), delim.size())
        && (i || delim.size())) { // do not match empty delim at data start
      return i; // delimiter match takes precedence over '"' match
    }

    if ('"' == data[i]) {
      quoted = true;
    }
  }

  return data.size();
}

constexpr VPackStringRef DELIMITER_PARAM_NAME {"delimiter"};

bool parse_vpack_options(const VPackSlice slice, std::string& delimiter) {

  if (!slice.isObject() && !slice.isString()) {
    IR_FRMT_ERROR(
      "Slice for delimited_token_stream is not an object or string");
    return false;
  }

  switch (slice.type()) {
    case VPackValueType::String:
      delimiter = irs::get_string<irs::string_ref>(slice);
      return true;
    case VPackValueType::Object:
      if (slice.hasKey(DELIMITER_PARAM_NAME)) {
        auto delim_type_slice = slice.get(DELIMITER_PARAM_NAME);
        if (!delim_type_slice.isString()) {
          IR_FRMT_WARN(
            "Invalid type '%s' (string expected) for delimited_token_stream from "
            "VPack arguments",
            DELIMITER_PARAM_NAME.data());
          return false;
        }
        delimiter = irs::get_string<irs::string_ref>(delim_type_slice);
        return true;
      }
    default: {}  // fall through
  }

  IR_FRMT_ERROR(
    "Missing '%s' while constructing delimited_token_stream from VPack arguments",
    DELIMITER_PARAM_NAME.data());

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

irs::analysis::analyzer::ptr make_vpack(const irs::string_ref& args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  return make_vpack(slice);
}
///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
/// @param delimiter reference to analyzer options storage
/// @param definition string for storing json document with config 
///////////////////////////////////////////////////////////////////////////////
bool make_vpack_config(const std::string& delimiter, VPackBuilder* vpack_builder) {
  VPackObjectBuilder object(vpack_builder);
  {
    // delimiter
    vpack_builder->add(DELIMITER_PARAM_NAME, VPackValue(delimiter));
  }

  return true;
}

bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* vpack_builder) {
  std::string delimiter;
  if (parse_vpack_options(slice, delimiter)) {
    return make_vpack_config(delimiter, vpack_builder);
  } else {
    return false;
  }
}

bool normalize_vpack_config(const irs::string_ref& args, std::string& definition) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  VPackBuilder builder;
  bool res = normalize_vpack_config(slice, &builder);
  if (res) {
    definition.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
  }
  return res;
}

irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while constructing delimited_token_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    return make_vpack(vpack->slice());
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing delimited_token_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing delimited_token_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing delimited_token_stream");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    VPackBuilder vpack_builder;
    if (normalize_vpack_config(vpack->slice(), &vpack_builder)) {
      definition = vpack_builder.toString();
      return !definition.empty();
    }
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while normalizing delimited_token_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while normalizing delimited_token_stream from JSON");
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a delimiter to use for tokenization
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(const irs::string_ref& args) {
  return irs::memory::make_unique<irs::analysis::delimited_token_stream>(args);
}

bool normalize_text_config(const irs::string_ref& delimiter, std::string& definition) {
  definition = delimiter;
  return true;
}

REGISTER_ANALYZER_VPACK(irs::analysis::delimited_token_stream, make_vpack, normalize_vpack_config);
REGISTER_ANALYZER_JSON(irs::analysis::delimited_token_stream, make_json, normalize_json_config);
REGISTER_ANALYZER_TEXT(irs::analysis::delimited_token_stream, make_text, normalize_text_config);

}

namespace iresearch {
namespace analysis {

delimited_token_stream::delimited_token_stream(const string_ref& delimiter)
  : analyzer(irs::type<delimited_token_stream>::get()),
    delim_(ref_cast<byte_type>(delimiter)) {
  if (!delim_.null()) {
    delim_buf_ = delim_; // keep a local copy of the delimiter
    delim_ = delim_buf_; // update the delimter to point at the local copy
  }
}

/*static*/ analyzer::ptr delimited_token_stream::make(const string_ref& delimiter) {
  return make_text(delimiter);
}

/*static*/ void delimited_token_stream::init() {
  REGISTER_ANALYZER_VPACK(delimited_token_stream, make_vpack, normalize_vpack_config); // match registration above
  REGISTER_ANALYZER_JSON(delimited_token_stream, make_json, normalize_json_config); // match registration above
  REGISTER_ANALYZER_TEXT(delimited_token_stream, make_text, normalize_text_config); // match registration above
}

bool delimited_token_stream::next() {
  if (data_.null()) {
    return false;
  }

  auto& offset = std::get<irs::offset>(attrs_);

  auto size = find_delimiter(data_, delim_);
  auto next = std::max(size_t(1), size + delim_.size());
  auto start = offset.end + uint32_t(delim_.size()); // value is allowed to overflow, will only produce invalid result
  auto end = start + size;

  if (std::numeric_limits<uint32_t>::max() < end) {
    return false; // cannot fit the next token into offset calculation
  }

  auto& payload = std::get<irs::payload>(attrs_);
  auto& term = std::get<term_attribute>(attrs_);

  offset.start = start;
  offset.end = uint32_t(end);
  payload.value = bytes_ref(data_.c_str(), size);
  term.value =  delim_.null() ? payload.value
                              : eval_term(term_buf_, payload.value);
  data_ = size >= data_.size() ? bytes_ref::NIL
                               : bytes_ref(data_.c_str() + next, data_.size() - next);

  return true;
}

bool delimited_token_stream::reset(const string_ref& data) {
  data_ = ref_cast<byte_type>(data);

  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = 0 - uint32_t(delim_.size()); // counterpart to computation in next() above

  return true;
}

} // analysis
} // ROOT
