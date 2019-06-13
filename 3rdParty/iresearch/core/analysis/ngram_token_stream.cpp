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

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document
#include <rapidjson/rapidjson/writer.h> // for rapidjson::Writer
#include <rapidjson/rapidjson/stringbuffer.h> // for rapidjson::StringBuffer

#include "ngram_token_stream.hpp"

NS_LOCAL

bool get_uint64(
    rapidjson::Document const& json,
    const irs::string_ref& name,
    uint64_t& value
) {
  if (!json.HasMember(name.c_str())) {
    return false;
  }

  const auto& attr = json[name.c_str()];

  if (!attr.IsNumber()) {
    return false;
  }

  value = attr.GetUint64();
  return true;
}

bool get_bool(
    rapidjson::Document const& json,
    const irs::string_ref& name,
    bool& value
) {
  if (!json.HasMember(name.c_str())) {
    return false;
  }

  const auto& attr = json[name.c_str()];

  if (!attr.IsBool()) {
    return false;
  }

  value = attr.GetBool();
  return true;
}

const irs::string_ref minParamName              = "min";
const irs::string_ref maxParamName              = "max";
const irs::string_ref preserveOriginalParamName = "preserveOriginal";

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "min" (number): minimum ngram size
///        "max" (number): maximum ngram size
///        "preserveOriginal" (boolean): preserve or not the original term
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  rapidjson::Document json;
  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing ngram_token_stream, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  if (rapidjson::kObjectType != json.GetType()) {
    IR_FRMT_ERROR(
      "Not a jSON object passed while constructing ngram_token_stream, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  uint64_t min, max;
  bool preserve_original;

  if (!get_uint64(json, minParamName, min)) {
    IR_FRMT_ERROR(
      "Failed to read '%s' attribute as number while constructing ngram_token_stream from jSON arguments: %s",
      minParamName.c_str(),
      args.c_str()
    );
    return nullptr;
  }

  if (!get_uint64(json, maxParamName, max)) {
    IR_FRMT_ERROR(
      "Failed to read '%s' attribute as number while constructing ngram_token_stream from jSON arguments: %s",
      maxParamName.c_str(),
      args.c_str()
    );
    return nullptr;
  }

  if (!get_bool(json, preserveOriginalParamName, preserve_original)) {
    IR_FRMT_ERROR(
      "Failed to read '%s' attribute as boolean while constructing ngram_token_stream from jSON arguments: %s",
      preserveOriginalParamName.c_str(),
      args.c_str()
    );
    return nullptr;
  }

  return irs::analysis::ngram_token_stream::make(
    irs::analysis::ngram_token_stream::options_t(size_t(min), size_t(max), preserve_original)
  );
}

///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
///////////////////////////////////////////////////////////////////////////////
bool make_json_config(const irs::analysis::ngram_token_stream::options_t& options, 
                      std::string& definition) {
  rapidjson::Document json;
  json.SetObject();
  rapidjson::Document::AllocatorType& allocator = json.GetAllocator();
  
  // ensure disambiguating casts below are safe. Casts required for clang compiler on Mac
  static_assert(sizeof(uint64_t) >= sizeof(size_t), "sizeof(uint64_t) >= sizeof(size_t)");
  //min_gram
  json.AddMember(rapidjson::Value::StringRefType(minParamName.c_str(), 
                     static_cast<rapidjson::SizeType>(minParamName.size())),
                 rapidjson::Value(static_cast<uint64_t>(options.min_gram)), 
                 allocator);

  //max_gram
  json.AddMember(rapidjson::Value::StringRefType(maxParamName.c_str(), 
                     static_cast<rapidjson::SizeType>(maxParamName.size())),
                 rapidjson::Value(static_cast<uint64_t>(options.max_gram)), 
                 allocator);

  //preserve_original
  json.AddMember(rapidjson::Value::StringRefType(preserveOriginalParamName.c_str(), 
                     static_cast<rapidjson::SizeType>(preserveOriginalParamName.size())),
                 rapidjson::Value(options.preserve_original), 
                 allocator);

  //output json to string
  rapidjson::StringBuffer buffer;
  rapidjson::Writer< rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);
  definition = buffer.GetString();
  return true;
}

REGISTER_ANALYZER_JSON(irs::analysis::ngram_token_stream, make_json);

NS_END

NS_ROOT
NS_BEGIN(analysis)

/*static*/ analyzer::ptr ngram_token_stream::make(
    const options_t& options
) {
  return std::make_shared<ngram_token_stream>(options);
}

/*static*/ void ngram_token_stream::init() {
  REGISTER_ANALYZER_JSON(ngram_token_stream, make_json); // match registration above
}

ngram_token_stream::ngram_token_stream(
    const options_t& options
) : analyzer(ngram_token_stream::type()),
    options_(options) {
  options_.min_gram = std::max(options_.min_gram, size_t(1));
  options_.max_gram = std::max(options_.max_gram, options_.min_gram);

  attrs_.emplace(offset_);
  attrs_.emplace(inc_);
  attrs_.emplace(term_);
}

//FIXME UTF-8 support
bool ngram_token_stream::next() NOEXCEPT {
  if (length_ < options_.min_gram) {
    ++begin_;

    if (data_.end() < begin_ + options_.min_gram) {
      if (emit_original_) {
        // emit the original input if it's not yet emitted
        term_.value(data_);

        assert(data_.size() <= integer_traits<uint32_t>::const_max);
        offset_.start = 0;
        offset_.end = uint32_t(data_.size());

        inc_.value = 0;

        emit_original_ = false;

        return true;
      }

      return false;
    } else if (data_.end() < begin_ + options_.max_gram) {
      assert(begin_ <= data_.end());
      length_ = size_t(std::distance(begin_, data_.end()));
    } else {
      length_ = options_.max_gram;
    }

    ++offset_.start;
  }

  assert(length_);
  term_.value(irs::bytes_ref(begin_, length_));

  assert(length_ <= integer_traits<uint32_t>::const_max);
  offset_.end = offset_.start + uint32_t(length_--);

  return true;
}

bool ngram_token_stream::reset(const irs::string_ref& value) NOEXCEPT {
  if (value.size() > integer_traits<uint32_t>::const_max) {
    // can't handle data which is longer than integer_traits<uint32_t>::const_max
    return false;
  }

  // reset term attribute
  term_.value(bytes_ref::NIL);

  // reset offset attribute
  offset_.start = integer_traits<uint32_t>::const_max;
  offset_.end = integer_traits<uint32_t>::const_max;

  // reset stream
  data_ = ref_cast<byte_type>(value);
  begin_ = data_.begin()-1;
  length_ = 0;
  emit_original_ = data_.size() > options_.max_gram && options_.preserve_original;
  assert(length_ < options_.min_gram);

  return true;
}

bool ngram_token_stream::to_string(
    const ::irs::text_format::type_id& format,
    std::string& definition) const {
  if (::irs::text_format::json == format) {
    return make_json_config(options_, definition);
  }
  return false;
}

DEFINE_ANALYZER_TYPE_NAMED(ngram_token_stream, "ngram")

NS_END // analysis
NS_END // ROOT
