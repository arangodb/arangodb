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
#include "utils/json_utils.hpp"

NS_LOCAL

const irs::string_ref MIN_PARAM_NAME               = "min";
const irs::string_ref MAX_PARAM_NAME               = "max";
const irs::string_ref PRESERVE_ORIGINAL_PARAM_NAME = "preserveOriginal";

bool parse_json_config(const irs::string_ref& args,
                        irs::analysis::ngram_token_stream::options_t& options) {
  rapidjson::Document json;
  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
        "Invalid jSON arguments passed while constructing ngram_token_stream, "
        "arguments: %s",
        args.c_str());

    return false;
  }

  if (rapidjson::kObjectType != json.GetType()) {
    IR_FRMT_ERROR(
        "Not a jSON object passed while constructing ngram_token_stream, "
        "arguments: %s",
        args.c_str());

    return false;
  }

  uint64_t min, max;
  bool preserve_original;

  if (!get_uint64(json, MIN_PARAM_NAME, min)) {
    IR_FRMT_ERROR(
        "Failed to read '%s' attribute as number while constructing "
        "ngram_token_stream from jSON arguments: %s",
        MIN_PARAM_NAME.c_str(), args.c_str());
    return false;
  }

  if (!get_uint64(json, MAX_PARAM_NAME, max)) {
    IR_FRMT_ERROR(
        "Failed to read '%s' attribute as number while constructing "
        "ngram_token_stream from jSON arguments: %s",
        MAX_PARAM_NAME.c_str(), args.c_str());
    return false;
  }

  if (!get_bool(json, PRESERVE_ORIGINAL_PARAM_NAME, preserve_original)) {
    IR_FRMT_ERROR(
        "Failed to read '%s' attribute as boolean while constructing "
        "ngram_token_stream from jSON arguments: %s",
        PRESERVE_ORIGINAL_PARAM_NAME.c_str(), args.c_str());
    return false;
  }
  options.min_gram = min;
  options.max_gram = max;
  options.preserve_original = preserve_original;
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "min" (number): minimum ngram size
///        "max" (number): maximum ngram size
///        "preserveOriginal" (boolean): preserve or not the original term
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  
  irs::analysis::ngram_token_stream::options_t options;
  if (parse_json_config(args, options)) {
    return irs::analysis::ngram_token_stream::make(options);
  } else {
    return nullptr;
  }
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
  json.AddMember(
    rapidjson::StringRef(MIN_PARAM_NAME.c_str(), MIN_PARAM_NAME.size()),
    rapidjson::Value(static_cast<uint64_t>(options.min_gram)),
    allocator);

  //max_gram
  json.AddMember(
    rapidjson::StringRef(MAX_PARAM_NAME.c_str(), MAX_PARAM_NAME.size()),
    rapidjson::Value(static_cast<uint64_t>(options.max_gram)),
    allocator);

  //preserve_original
  json.AddMember(
    rapidjson::StringRef(PRESERVE_ORIGINAL_PARAM_NAME.c_str(), PRESERVE_ORIGINAL_PARAM_NAME.size()),
    rapidjson::Value(options.preserve_original),
    allocator);

  //output json to string
  rapidjson::StringBuffer buffer;
  rapidjson::Writer< rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);
  definition = buffer.GetString();
  return true;
}

bool normalize_json_config(const irs::string_ref& args, std::string& config) {
  irs::analysis::ngram_token_stream::options_t options;
  if (parse_json_config(args, options)) {
    return make_json_config(options, config);
  } else {
    return false;
  }
}

REGISTER_ANALYZER_JSON(irs::analysis::ngram_token_stream, make_json, 
                       normalize_json_config);

NS_END

NS_ROOT
NS_BEGIN(analysis)

/*static*/ analyzer::ptr ngram_token_stream::make(
    const options_t& options
) {
  return memory::make_shared<ngram_token_stream>(options);
}

/*static*/ void ngram_token_stream::init() {
  REGISTER_ANALYZER_JSON(ngram_token_stream, make_json, 
                         normalize_json_config); // match registration above
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
bool ngram_token_stream::next() noexcept {
  if (length_ < options_.min_gram) {
    ++begin_;
    inc_.value = 1;
    if (data_.end() < begin_ + options_.min_gram) {
      return false;
    } else if (data_.end() < begin_ + options_.max_gram) {
      assert(begin_ <= data_.end());
      length_ = size_t(std::distance(begin_, data_.end()));
    } else {
      length_ = options_.max_gram;
    }
    ++offset_.start;
  } else {
    inc_.value = 0; // staying on the current pos, just reducing ngram size
  }
  // as stream has unsigned incremet attribute
  // we cannot go back, so we must emit original
  // as first token if needed (as it starts from pos=0 in stream)
  if (emit_original_) {
    term_.value(data_);
    assert(data_.size() <= integer_traits<uint32_t>::const_max);
    offset_.end = uint32_t(data_.size());
    emit_original_ = false;
    return true;
  } 
  assert(length_);
  term_.value(irs::bytes_ref(begin_, length_));
  assert(length_ <= integer_traits<uint32_t>::const_max);
  offset_.end = offset_.start + uint32_t(length_--);
  return true;
}

bool ngram_token_stream::reset(const irs::string_ref& value) noexcept {
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

DEFINE_ANALYZER_TYPE_NAMED(ngram_token_stream, "ngram")

NS_END // analysis
NS_END // ROOT
