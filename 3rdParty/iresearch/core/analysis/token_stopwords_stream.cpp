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
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/Iterator.h"

#include "token_stopwords_stream.hpp"

namespace {

constexpr char HEX_DECODE_MAP[256] = {
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 0 - 15
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 16 - 31
  16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, // ASCII 32 - 47
  00,  1,  2,  3,  4,  5,  6,  7,  8,  9, 16, 16, 16, 16, 16, 16, // ASCII 48 - 63
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

bool hex_decode(std::string& buf, arangodb::velocypack::StringRef value) {
  if (value.length() & 1) {
    IR_FRMT_WARN(
      "Invalid size for hex-encoded value while HEX decoding masked token: %s",
      value.data());

    return false;
  }

  buf.reserve(buf.size() + value.length()/2);

  for (size_t i = 0, count = value.length(); i < count; i += 2) {
    auto hi = HEX_DECODE_MAP[size_t(value[i])];
    auto lo = HEX_DECODE_MAP[size_t(value[i + 1])];

    if (hi >= 16 || lo >= 16) {
      return false;
    }

    buf.append(1, (hi << 4) | lo);
  }

  return true;
}

irs::analysis::analyzer::ptr construct(const arangodb::velocypack::ArrayIterator& mask) {
  size_t offset = 0;
  irs::analysis::token_stopwords_stream::stopwords_set tokens;

  for (auto itr = mask.begin(), end = mask.end(); itr != end; ++itr, ++offset) {
    if (!(*itr).isString()) {
      IR_FRMT_WARN(
        "Non-string value in 'mask' at offset '" IR_SIZE_T_SPECIFIER "' while constructing token_stopwords_stream from jSON arguments",
        offset);

      return nullptr;
    }

    std::string token;
    auto value = (*itr).stringRef();

    if (!hex_decode(token, value)) {
      tokens.emplace(std::string(value.data(), value.length())); // interpret verbatim
    } else {
      tokens.emplace(std::move(token));
    }
  }

  return irs::memory::make_shared<irs::analysis::token_stopwords_stream>(
    std::move(tokens));
}

constexpr irs::string_ref STOPWORDS_PARAM_NAME = "stopwords";

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "mask"(string-list): the HEX encoded token values to mask <required>
///        if HEX conversion fails for any token then it is matched verbatim
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_vpack(const irs::string_ref& args) {
  arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
  switch (slice.type()) {
    case arangodb::velocypack::ValueType::Array:
      return construct(arangodb::velocypack::ArrayIterator(slice));
    case arangodb::velocypack::ValueType::Object:
    {
      auto maskSlice = slice.get(STOPWORDS_PARAM_NAME.c_str());
      if (maskSlice.isArray()) {
        return construct(arangodb::velocypack::ArrayIterator(maskSlice));
      }
    }
    [[fallthrough]];
    default: {
      IR_FRMT_ERROR(
        "Invalid vpack while constructing token_stopwords_stream from jSON arguments: %s. Array or Object was expected. Got %s",
        slice.toString().c_str(), slice.typeName());
    }
  }
  return nullptr;
}


irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while constructing token_stopwords_stream ");
      return nullptr;
    }
    auto vpack = arangodb::velocypack::Parser::fromJson(args.c_str());
    return make_vpack(irs::string_ref(reinterpret_cast<const char*>(vpack->data()), vpack->size()));
  } catch(const arangodb::velocypack::Exception& ex) {
    IR_FRMT_ERROR("Caught error '%s' while constructing token_stopwords_stream from json: %s",
                  ex.what(), args.c_str());
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing token_stopwords_stream from json: %s",
                  args.c_str());
  }
  return nullptr;
}

bool normalize_vpack_config(const irs::string_ref& args, std::string& definition) {
  arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
  switch (slice.type()) {
    case arangodb::velocypack::ValueType::Array:
    { // always normalize to object for consistency reasons
      arangodb::velocypack::Builder builder;
      builder.openObject();
      builder.add(STOPWORDS_PARAM_NAME.c_str(), slice);
      builder.close();
      definition.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
      return true;
    }
    case arangodb::velocypack::ValueType::Object:
    {
      auto maskSlice = slice.get(STOPWORDS_PARAM_NAME.c_str());
      if (maskSlice.isArray()) {
        arangodb::velocypack::Builder builder;
        builder.openObject();
        builder.add(STOPWORDS_PARAM_NAME.c_str(), maskSlice);
        builder.close();
        definition.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
        return true;
      }
    }
    [[fallthrough]];
    default: {
      IR_FRMT_ERROR(
        "Invalid vpack while normalizing token_stopwords_stream from jSON arguments: %s. Array or Object was expected. Got %s",
        slice.toString().c_str(), slice.typeName());
    }
  }
  return false;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing token_stopwords_stream");
      return false;
    }
    auto vpack = arangodb::velocypack::Parser::fromJson(args.c_str());
    std::string vpack_container;
    if (normalize_vpack_config(irs::string_ref(reinterpret_cast<const char*>(vpack->data()), vpack->size()), vpack_container)) {
      arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(vpack_container.c_str()));
      definition = slice.toString();
      return true;
    }
  } catch(const arangodb::velocypack::Exception& ex) {
    IR_FRMT_ERROR("Caught error '%s' while normalizing token_stopwords_stream from json: %s",
                  ex.what(), args.c_str());
  } catch (...) {
    IR_FRMT_ERROR("Caught error while normalizing token_stopwords_stream from json: %s",
                  args.c_str());
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::token_stopwords_stream, make_vpack, normalize_vpack_config);
REGISTER_ANALYZER_JSON(irs::analysis::token_stopwords_stream, make_json, normalize_json_config);

} // namespace

namespace iresearch {
namespace analysis {

token_stopwords_stream::token_stopwords_stream(token_stopwords_stream::stopwords_set&& stopwords)
  : analyzer{irs::type<token_stopwords_stream>::get()},
    stopwords_(std::move(stopwords)),
    term_eof_(true) {
}

/*static*/ void token_stopwords_stream::init() {
  REGISTER_ANALYZER_VPACK(irs::analysis::token_stopwords_stream, make_vpack, normalize_vpack_config);
  REGISTER_ANALYZER_JSON(token_stopwords_stream, make_json, normalize_json_config);  // match registration above
}


bool token_stopwords_stream::next() {
  if (term_eof_) {
    return false;
  }

  term_eof_ = true;

  return true;
}

bool token_stopwords_stream::reset(const string_ref& data) {
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = uint32_t(data.size());
  auto& term = std::get<term_attribute>(attrs_);
  term.value = irs::ref_cast<irs::byte_type>(data);
  term_eof_ = stopwords_.contains(data);

  return true;
}

} // namespace analysis
} // namespace iresearch
