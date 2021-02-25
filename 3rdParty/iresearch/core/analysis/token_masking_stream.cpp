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

#include <cctype> // for std::isspace(...)
#include <rapidjson/rapidjson/document.h> // for rapidjson::Document

#include "token_masking_stream.hpp"

namespace {

bool hex_decode(irs::bstring& buf, const irs::string_ref& value) {
  static const uint8_t map[256] = {
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 0 - 15
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 16 - 31
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 32 - 47
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 16, 16, 16, 16, 16, 16, // ASCII 48 - 63
    16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 64 - 79
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 80 - 95
    16, 10, 11, 12, 13, 14, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 96 - 111
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 112 - 127
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 128 - 143
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 144 - 159
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 160 - 175
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 176 - 191
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 192 - 207
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 208 - 223
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 224 - 239
    16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 240 - 255
  };

  if (value.size() & 1) {
    IR_FRMT_WARN(
      "Invalid size for hex-encoded value while HEX decoding masked token: %s",
      value.c_str()
    );

    return false;
  }

  buf.reserve(buf.size() + value.size()/2);

  for (size_t i = 0, count = value.size(); i < count; i += 2) {
    auto hi = map[size_t(value[i])];
    auto lo = map[size_t(value[i + 1])];

    if (hi >= 16 || lo >= 16) {
      IR_FRMT_WARN(
        "Invalid hex-encoded value while HEX decoding masked token: %s",
        value.c_str()
      );

      return false;
    }

    buf.append(1, (hi << 4) | lo);
  }

  return true;
}

irs::analysis::analyzer::ptr construct(const irs::string_ref& mask) {
  std::unordered_set<irs::bstring> tokens;

  for (size_t begin = 0, end = 0, length = mask.size();
       end < length;
       begin = end + 1, end = begin
      ) {
    // find first whitespace
    while (end < length && !std::isspace(mask[end])) {
      ++end;
    }

    if (end > begin) {
      irs::bstring token;
      irs::string_ref value(&mask[begin], end - begin);

      if (!hex_decode(token, value)) {
        tokens.emplace(irs::ref_cast<irs::byte_type>(value)); // interpret verbatim
      } else {
        tokens.emplace(std::move(token));
      }
    }
  }

  return irs::memory::make_shared<irs::analysis::token_masking_stream>(
    std::move(tokens)
  );
}

irs::analysis::analyzer::ptr construct(
    const rapidjson::Document::Array& mask
) {
  size_t offset = 0;
  std::unordered_set<irs::bstring> tokens;

  for (auto itr = mask.Begin(), end = mask.End(); itr != end; ++itr, ++offset) {
    if (!itr->IsString()) {
      IR_FRMT_WARN(
        "Non-string value in 'mask' at offset '" IR_SIZE_T_SPECIFIER "' while constructing token_masking_stream from jSON arguments",
        offset
      );

      return nullptr;
    }

    irs::bstring token;
    irs::string_ref value(itr->GetString());

    if (!hex_decode(token, value)) {
      tokens.emplace(irs::ref_cast<irs::byte_type>(value)); // interpret verbatim
    } else {
      tokens.emplace(std::move(token));
    }
  }

  return irs::memory::make_shared<irs::analysis::token_masking_stream>(
    std::move(tokens)
  );
}

static const irs::string_ref MASK_PARAM_NAME = "mask";

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "mask"(string-list): the HEX encoded token values to mask <required>
///        if HEX conversion fails for any token then it is matched verbatim
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing token_masking_stream, arguments: %s",
      args.c_str()
    );

    return nullptr;
  }

  switch (json.GetType()) {
   case rapidjson::kArrayType:
    return construct(json.GetArray());
   case rapidjson::kObjectType:
    if (json.HasMember(MASK_PARAM_NAME.c_str()) && json[MASK_PARAM_NAME.c_str()].IsArray()) {
      return construct(json[MASK_PARAM_NAME.c_str()].GetArray());
    }
   default: {}
  }

  IR_FRMT_ERROR(
    "Invalid '%s' while constructing token_masking_stream from jSON arguments: %s",
    MASK_PARAM_NAME.c_str(),
    args.c_str()
  );

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is whitespace delimited HEX encoded list of tokens to mask
///        if HEX conversion fails for any token then it is matched verbatim
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(const irs::string_ref& args) {
  return construct(args);
}

bool normalize_text_config(const irs::string_ref&, std::string&) {
  return false;
}

bool normalize_json_config(const irs::string_ref&, std::string&) {
  return false;
}



REGISTER_ANALYZER_JSON(irs::analysis::token_masking_stream, make_json, normalize_json_config);
REGISTER_ANALYZER_TEXT(irs::analysis::token_masking_stream, make_text, normalize_text_config);

}

namespace iresearch {
namespace analysis {

token_masking_stream::token_masking_stream(std::unordered_set<irs::bstring>&& mask)
  : attributes{{
      { irs::type<increment>::id(), &inc_       },
      { irs::type<offset>::id(), &offset_       },
      { irs::type<payload>::id(), &payload_     },
      { irs::type<term_attribute>::id(), &term_ }},
      irs::type<token_masking_stream>::get()},
    mask_(std::move(mask)),
    term_eof_(true) {
}

/*static*/ void token_masking_stream::init() {
  REGISTER_ANALYZER_JSON(token_masking_stream, make_json, normalize_json_config);  // match registration above
  REGISTER_ANALYZER_TEXT(token_masking_stream, make_text, normalize_text_config); // match registration above
}

/*static*/ analyzer::ptr token_masking_stream::make(const string_ref& mask) {
  return make_text(mask);
}

bool token_masking_stream::next() {
  if (term_eof_) {
    return false;
  }

  term_eof_ = true;

  return true;
}

bool token_masking_stream::reset(const string_ref& data) {
  offset_.start = 0;
  offset_.end = data.size();
  payload_.value = ref_cast<uint8_t>(data);
  term_.value = irs::ref_cast<irs::byte_type>(data);
  term_eof_ = mask_.find(term_.value) != mask_.end();

  return true;
}

} // analysis
} // ROOT
