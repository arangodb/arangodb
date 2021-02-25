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
////////////////////////////////////////////////////////////////////////////////

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document
#include <rapidjson/rapidjson/writer.h> // for rapidjson::Writer
#include <rapidjson/rapidjson/stringbuffer.h> // for rapidjson::StringBuffer

#include "ngram_token_stream.hpp"
#include "utils/json_utils.hpp"
#include "utils/utf8_utils.hpp"

namespace {

const irs::string_ref MIN_PARAM_NAME               = "min";
const irs::string_ref MAX_PARAM_NAME               = "max";
const irs::string_ref PRESERVE_ORIGINAL_PARAM_NAME = "preserveOriginal";
const irs::string_ref STREAM_TYPE_PARAM_NAME       = "streamType";
const irs::string_ref START_MARKER_PARAM_NAME      = "startMarker";
const irs::string_ref END_MARKER_PARAM_NAME        = "endMarker";

const std::unordered_map<std::string, irs::analysis::ngram_token_stream_base::InputType> STREAM_TYPE_CONVERT_MAP = {
  { "binary", irs::analysis::ngram_token_stream_base::InputType::Binary },
  { "utf8", irs::analysis::ngram_token_stream_base::InputType::UTF8 }};

bool parse_json_config(const irs::string_ref& args,
                        irs::analysis::ngram_token_stream_base::Options& options) {
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
  auto stream_bytes_type = irs::analysis::ngram_token_stream_base::InputType::Binary;
  std::string start_marker, end_marker;

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

  if (json.HasMember(START_MARKER_PARAM_NAME.c_str())) {
    auto& start_marker_json = json[START_MARKER_PARAM_NAME.c_str()];
    if (start_marker_json.IsString()) {
      start_marker = start_marker_json.GetString();
    } else {
      IR_FRMT_ERROR(
          "Failed to read '%s' attribute as string while constructing "
          "ngram_token_stream from jSON arguments: %s",
          START_MARKER_PARAM_NAME.c_str(), args.c_str());
      return false;
    }
  }

  if (json.HasMember(END_MARKER_PARAM_NAME.c_str())) {
    auto& end_marker_json = json[END_MARKER_PARAM_NAME.c_str()];
    if (end_marker_json.IsString()) {
      end_marker = end_marker_json.GetString();
    } else {
      IR_FRMT_ERROR(
          "Failed to read '%s' attribute as string while constructing "
          "ngram_token_stream from jSON arguments: %s",
          END_MARKER_PARAM_NAME.c_str(), args.c_str());
      return false;
    }
  }

  if (json.HasMember(STREAM_TYPE_PARAM_NAME.c_str())) {
      auto& stream_type_json =
          json[STREAM_TYPE_PARAM_NAME.c_str()];

      if (!stream_type_json.IsString()) {
        IR_FRMT_WARN(
            "Non-string value in '%s' while constructing ngram_token_stream "
            "from jSON arguments: %s",
            STREAM_TYPE_PARAM_NAME.c_str(), args.c_str());
        return false;
      }
      auto itr = STREAM_TYPE_CONVERT_MAP.find(stream_type_json.GetString());
      if (itr == STREAM_TYPE_CONVERT_MAP.end()) {
        IR_FRMT_WARN(
            "Invalid value in '%s' while constructing ngram_token_stream from "
            "jSON arguments: %s",
            STREAM_TYPE_PARAM_NAME.c_str(), args.c_str());
        return false;
      }
      stream_bytes_type = itr->second;
  }

  min = std::max(min, decltype(min)(1));
  max = std::max(max, min);

  options.min_gram = min;
  options.max_gram = max;
  options.preserve_original = preserve_original;
  options.start_marker = irs::ref_cast<irs::byte_type>(start_marker);
  options.end_marker = irs::ref_cast<irs::byte_type>(end_marker);
  options.stream_bytes_type = stream_bytes_type;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "min" (number): minimum ngram size
///        "max" (number): maximum ngram size
///        "preserveOriginal" (boolean): preserve or not the original term
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  irs::analysis::ngram_token_stream_base::Options options;
  if (parse_json_config(args, options)) {
    switch (options.stream_bytes_type) {
      case irs::analysis::ngram_token_stream_base::InputType::Binary:
        return irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>::make(options);
      case irs::analysis::ngram_token_stream_base::InputType::UTF8:
        return irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8>::make(options);
    }
  }

  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
///////////////////////////////////////////////////////////////////////////////
bool make_json_config(const irs::analysis::ngram_token_stream_base::Options& options,
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

  // stream type
  {
    auto stream_type_value = std::find_if(STREAM_TYPE_CONVERT_MAP.begin(), STREAM_TYPE_CONVERT_MAP.end(),
      [&options](const decltype(STREAM_TYPE_CONVERT_MAP)::value_type& v) {
        return v.second == options.stream_bytes_type;
      });

    if (stream_type_value != STREAM_TYPE_CONVERT_MAP.end()) {
      json.AddMember(
        rapidjson::StringRef(STREAM_TYPE_PARAM_NAME.c_str(), STREAM_TYPE_PARAM_NAME.size()),
        rapidjson::StringRef(stream_type_value->first.c_str(), stream_type_value->first.length()),
        allocator);
    } else {
      IR_FRMT_ERROR(
        "Invalid %s value in ngram analyzer options: %d",
        STREAM_TYPE_PARAM_NAME.c_str(),
        static_cast<int>(options.stream_bytes_type));
      return false;
    }
  }

  // start_marker
  if (!options.start_marker.empty()) {
    json.AddMember(
      rapidjson::StringRef(START_MARKER_PARAM_NAME.c_str(), START_MARKER_PARAM_NAME.size()),
      rapidjson::StringRef(reinterpret_cast<const char*>(options.start_marker.c_str()), options.start_marker.length()),
      allocator);
  }

  // end_marker
  if (!options.end_marker.empty()) {
    json.AddMember(
      rapidjson::StringRef(END_MARKER_PARAM_NAME.c_str(), END_MARKER_PARAM_NAME.size()),
      rapidjson::StringRef(reinterpret_cast<const char*>(options.end_marker.c_str()), options.end_marker.length()),
      allocator);
  }

  //output json to string
  rapidjson::StringBuffer buffer;
  rapidjson::Writer< rapidjson::StringBuffer> writer(buffer);
  json.Accept(writer);
  definition = buffer.GetString();
  return true;
}

bool normalize_json_config(const irs::string_ref& args, std::string& config) {
  irs::analysis::ngram_token_stream_base::Options options;
  if (parse_json_config(args, options)) {
    return make_json_config(options, config);
  } else {
    return false;
  }
}


REGISTER_ANALYZER_JSON(irs::analysis::ngram_token_stream_base, make_json, normalize_json_config);

}

namespace iresearch {
namespace analysis {

template<irs::analysis::ngram_token_stream_base::InputType StreamType>
/*static*/ analyzer::ptr ngram_token_stream<StreamType>::make(
    const ngram_token_stream_base::Options& options
) {
  return std::make_shared<ngram_token_stream<StreamType>>(options);
}

/*static*/ void ngram_token_stream_base::init() {
  REGISTER_ANALYZER_JSON(ngram_token_stream_base, make_json,
                         normalize_json_config); // match registration above
}


ngram_token_stream_base::ngram_token_stream_base(
    const ngram_token_stream_base::Options& options) 
  : attributes{{
      { irs::type<increment>::id(), &inc_       },
      { irs::type<offset>::id(), &offset_       },
      { irs::type<term_attribute>::id(), &term_ }},
      irs::type<ngram_token_stream_base>::get()},
    options_(options),
    start_marker_empty_(options.start_marker.empty()),
    end_marker_empty_(options.end_marker.empty()) {
  options_.min_gram = std::max(options_.min_gram, size_t(1));
  options_.max_gram = std::max(options_.max_gram, options_.min_gram);
}

template<irs::analysis::ngram_token_stream_base::InputType StreamType>
ngram_token_stream<StreamType>::ngram_token_stream(
    const ngram_token_stream_base::Options& options
) : ngram_token_stream_base(options) {
  IRS_ASSERT(StreamType == options_.stream_bytes_type);
}

void ngram_token_stream_base::emit_original() noexcept {
  switch (emit_original_) {
    case EmitOriginal::WithoutMarkers:
      term_.value = data_;
      assert(data_.size() <= integer_traits<uint32_t>::const_max);
      offset_.end = uint32_t(data_.size());
      emit_original_ = EmitOriginal::None;
      inc_.value = next_inc_val_;
      break;
    case EmitOriginal::WithEndMarker:
      marked_term_buffer_.clear();
      IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.end_marker.size() + data_.size()));
      marked_term_buffer_.append(data_.begin(), data_end_);
      marked_term_buffer_.append(options_.end_marker.begin(), options_.end_marker.end());
      term_.value = marked_term_buffer_;
      assert(marked_term_buffer_.size() <= integer_traits<uint32_t>::const_max);
      offset_.start = 0;
      offset_.end = uint32_t(data_.size());
      emit_original_ = EmitOriginal::None; // end marker is emitted last, so we are done emitting original
      inc_.value = next_inc_val_;
      break;
    case EmitOriginal::WithStartMarker:
      marked_term_buffer_.clear();
      IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.start_marker.size() + data_.size()));
      marked_term_buffer_.append(options_.start_marker.begin(), options_.start_marker.end());
      marked_term_buffer_.append(data_.begin(), data_end_);
      term_.value = marked_term_buffer_;
      assert(marked_term_buffer_.size() <= integer_traits<uint32_t>::const_max);
      offset_.start = 0;
      offset_.end = uint32_t(data_.size());
      emit_original_ = options_.end_marker.empty()? EmitOriginal::None : EmitOriginal::WithEndMarker;
      inc_.value = next_inc_val_;
      break;
    default:
      IRS_ASSERT(false); // should not be called when None
      break;
  }
  next_inc_val_ = 0;
}

bool ngram_token_stream_base::reset(const irs::string_ref& value) noexcept {
  if (value.size() > integer_traits<uint32_t>::const_max) {
    // can't handle data which is longer than integer_traits<uint32_t>::const_max
    return false;
  }

  // reset term attribute
  term_.value = bytes_ref::NIL;

  // reset offset attribute
  offset_.start = integer_traits<uint32_t>::const_max;
  offset_.end = integer_traits<uint32_t>::const_max;

  // reset stream
  data_ = ref_cast<byte_type>(value);
  begin_ = data_.begin();
  ngram_end_ = begin_;
  data_end_ = data_.end();
  offset_.start = 0;
  length_ = 0;
  if (options_.preserve_original) {
    if (!start_marker_empty_) {
      emit_original_ = EmitOriginal::WithStartMarker;
    } else if (!end_marker_empty_) {
      emit_original_ = EmitOriginal::WithEndMarker;
    } else {
      emit_original_ = EmitOriginal::WithoutMarkers;
    }
  } else {
    emit_original_ = EmitOriginal::None;
  }
  next_inc_val_ = 1;
  assert(length_ < options_.min_gram);
  const size_t max_marker_size = std::max(options_.start_marker.size(), options_.end_marker.size());
  if (max_marker_size > 0) {
    // we have at least one marker. As we need to append marker to ngram and provide term
    // value as continious buffer, we can`t return pointer to some byte inside input stream
    // but rather we return pointer to buffer with copied values of ngram and marker
    // For sake of performance we allocate requested memory right now
    size_t buffer_size = options_.preserve_original ? data_.size() : std::min(data_.size(), options_.max_gram);
    buffer_size += max_marker_size;
    if (buffer_size > marked_term_buffer_.capacity()) { // until c++20 this check is needed to avoid shrinking
      marked_term_buffer_.reserve(buffer_size);
    }
  }
  return true;
}

template<irs::analysis::ngram_token_stream_base::InputType StreamType>
bool ngram_token_stream<StreamType>::next_symbol(const byte_type*& it) const noexcept {
  IRS_ASSERT(it);
  if (it < data_end_) {
    if constexpr (StreamType == InputType::Binary) {
      ++it;
    } else if constexpr (StreamType == InputType::UTF8) {
      it = irs::utf8_utils::next(it, data_end_);
    }
    return true;
  }
  return false;
}

template<irs::analysis::ngram_token_stream_base::InputType StreamType>
bool ngram_token_stream<StreamType>::next() noexcept {
  while (begin_ < data_end_) {
    if (length_ < options_.max_gram && next_symbol(ngram_end_)) {
      // we have next ngram from current position
      ++length_;
      if (length_ >= options_.min_gram) {
        IRS_ASSERT(begin_ <= ngram_end_);
        assert(static_cast<size_t>(std::distance(begin_, ngram_end_)) <= integer_traits<uint32_t>::const_max);
        const auto ngram_byte_len = static_cast<uint32_t>(std::distance(begin_, ngram_end_));
        if (EmitOriginal::None == emit_original_ || 0 != offset_.start || ngram_byte_len != data_.size()) {
          offset_.end = offset_.start + ngram_byte_len;
          inc_.value = next_inc_val_;
          next_inc_val_ = 0;
          if ((0 != offset_.start || start_marker_empty_) && (end_marker_empty_ || ngram_end_ != data_end_)) {
            term_.value = irs::bytes_ref(begin_, ngram_byte_len);
          } else if (0 == offset_.start && !start_marker_empty_) {
            marked_term_buffer_.clear();
            IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.start_marker.size() + ngram_byte_len));
            marked_term_buffer_.append(options_.start_marker.begin(), options_.start_marker.end());
            marked_term_buffer_.append(begin_, ngram_byte_len);
            term_.value = marked_term_buffer_;
            assert(marked_term_buffer_.size() <= integer_traits<uint32_t>::const_max);
            if (ngram_byte_len == data_.size() && !end_marker_empty_) {
              // this term is whole original stream and we have end marker, so we need to emit
              // this term again with end marker just like original, so pretend we need to emit original
              emit_original_ = EmitOriginal::WithEndMarker;
            }
          } else {
            IRS_ASSERT(!end_marker_empty_ && ngram_end_ == data_end_);
            marked_term_buffer_.clear();
            IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.end_marker.size() + ngram_byte_len));
            marked_term_buffer_.append(begin_, ngram_byte_len);
            marked_term_buffer_.append(options_.end_marker.begin(), options_.end_marker.end());
            term_.value = marked_term_buffer_;
          }
        } else {
          // if ngram covers original stream we need to process it specially
          emit_original();
        }
        return true;
      }
    } else {
      // need to move to next position
      if (EmitOriginal::None == emit_original_) {
        if (next_symbol(begin_)) {
          next_inc_val_ = 1;
          length_ = 0;
          ngram_end_ = begin_;
          offset_.start = static_cast<uint32_t>(std::distance(data_.begin(), begin_));
        } else {
          return false; // stream exhausted
        }
      } else {
        // as stream has unsigned incremet attribute
        // we cannot go back, so we must emit original before we leave start pos in stream
        // (as it starts from pos=0 in stream)
        emit_original();
        return true;
      }
    }
  }
  return false;
}


} // analysis
} // ROOT

// Making library export see template instantinations
template class irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>;
template class irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8>;
