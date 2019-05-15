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

  if (!get_uint64(json, "min", min)) {
    IR_FRMT_ERROR(
      "Failed to read 'min' attribute as number while constructing ngram_token_stream from jSON arguments: %s",
      args.c_str()
    );
    return nullptr;
  }

  if (!get_uint64(json, "max", max)) {
    IR_FRMT_ERROR(
      "Failed to read 'max' attribute as number while constructing ngram_token_stream from jSON arguments: %s",
      args.c_str()
    );
    return nullptr;
  }

  if (!get_bool(json, "preserveOriginal", preserve_original)) {
    IR_FRMT_ERROR(
      "Failed to read 'preserveOriginal' attribute as boolean while constructing ngram_token_stream from jSON arguments: %s",
      args.c_str()
    );
    return nullptr;
  }

  return irs::analysis::ngram_token_stream::make(
    size_t(min), size_t(max), preserve_original
  );
}

REGISTER_ANALYZER_JSON(irs::analysis::ngram_token_stream, make_json);

NS_END

NS_ROOT
NS_BEGIN(analysis)

/*static*/ analyzer::ptr ngram_token_stream::make(
    size_t min, size_t max, bool preserve_original
) {
  return std::make_shared<ngram_token_stream>(min, max, preserve_original);
}

/*static*/ void ngram_token_stream::init() {
  REGISTER_ANALYZER_JSON(ngram_token_stream, make_json); // match registration above
}

ngram_token_stream::ngram_token_stream(
    size_t min_gram,
    size_t max_gram,
    bool preserve_original
) : analyzer(ngram_token_stream::type()),
    min_gram_(min_gram),
    max_gram_(max_gram),
    preserve_original_(preserve_original) {
  min_gram_ = std::max(min_gram_, size_t(1));
  max_gram_ = std::max(max_gram_, min_gram_);

  attrs_.emplace(offset_);
  attrs_.emplace(inc_);
  attrs_.emplace(term_);
}

//FIXME UTF-8 support
bool ngram_token_stream::next() NOEXCEPT {
  if (length_ < min_gram_) {
    ++begin_;

    if (data_.end() < begin_ + min_gram_) {
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
    } else if (data_.end() < begin_ + max_gram_) {
      assert(begin_ <= data_.end());
      length_ = size_t(std::distance(begin_, data_.end()));
    } else {
      length_ = max_gram_;
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
  emit_original_ = data_.size() > max_gram_ && preserve_original_;
  assert(length_ < min_gram_);

  return true;
}

DEFINE_ANALYZER_TYPE_NAMED(ngram_token_stream, "ngram")

NS_END // analysis
NS_END // ROOT
