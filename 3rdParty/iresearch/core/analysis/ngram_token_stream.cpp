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

#include "ngram_token_stream.hpp"

#include <frozen/unordered_map.h>
#include <frozen/string.h>

#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"
#include "velocypack/velocypack-aliases.h"

#include "utils/hash_utils.hpp"
#include "utils/vpack_utils.hpp"
#include "utils/utf8_utils.hpp"

#include <string_view>

namespace {

constexpr std::string_view MIN_PARAM_NAME               {"min"};
constexpr std::string_view MAX_PARAM_NAME               {"max"};
constexpr std::string_view PRESERVE_ORIGINAL_PARAM_NAME {"preserveOriginal"};
constexpr std::string_view STREAM_TYPE_PARAM_NAME       {"streamType"};
constexpr std::string_view START_MARKER_PARAM_NAME      {"startMarker"};
constexpr std::string_view END_MARKER_PARAM_NAME        {"endMarker"};

constexpr frozen::unordered_map<irs::string_ref, irs::analysis::ngram_token_stream_base::InputType, 2> STREAM_TYPE_CONVERT_MAP = {
  { "binary", irs::analysis::ngram_token_stream_base::InputType::Binary },
  { "utf8", irs::analysis::ngram_token_stream_base::InputType::UTF8 }};

bool parse_vpack_options(const VPackSlice slice,
                        irs::analysis::ngram_token_stream_base::Options& options) {
  if (!slice.isObject()) {
    IR_FRMT_ERROR(
      "Slice for ngram_token_stream is not an object");
    return false;
  }

  uint64_t min = 0, max = 0;
  bool preserve_original;
  auto stream_bytes_type = irs::analysis::ngram_token_stream_base::InputType::Binary;
  irs::string_ref start_marker, end_marker;

  //min
  auto min_type_slice = slice.get(MIN_PARAM_NAME);
  if (min_type_slice.isNone()) {
    IR_FRMT_ERROR(
      "Failed to read '%s' attribute as number while constructing "
      "ngram_token_stream from VPack arguments",
      MIN_PARAM_NAME.data());
    return false;
  }

  if (!min_type_slice.isNumber()) {
    IR_FRMT_WARN(
      "Invalid type '%s' (unsigned int expected) for ngram_token_stream from "
      "VPack arguments",
      MIN_PARAM_NAME.data());
    return false;
  }
  min = min_type_slice.getNumber<decltype (min)>();

  // max
  auto max_type_slice = slice.get(MAX_PARAM_NAME);
  if (max_type_slice.isNone()) {
    IR_FRMT_ERROR(
      "Failed to read '%s' attribute as number while constructing "
      "ngram_token_stream from VPack arguments",
      MAX_PARAM_NAME.data());
    return false;
  }
  if (!max_type_slice.isNumber()) {
    IR_FRMT_WARN(
      "Invalid type '%s' (unsigned int expected) for ngram_token_stream from "
      "VPack arguments",
      MAX_PARAM_NAME.data());
    return false;
  }
  max = max_type_slice.getNumber<decltype (max)>();

  min = std::max(min, decltype(min)(1));
  max = std::max(max, min);

  options.min_gram = min;
  options.max_gram = max;

  //preserve original
  auto preserve_type_slice = slice.get(PRESERVE_ORIGINAL_PARAM_NAME);
  if (preserve_type_slice.isNone()) {
    IR_FRMT_ERROR(
      "Failed to read '%s' attribute as boolean while constructing "
      "ngram_token_stream from VPack arguments",
      PRESERVE_ORIGINAL_PARAM_NAME.data());
    return false;
  }
  if (!preserve_type_slice.isBool()) {
    IR_FRMT_WARN(
      "Invalid type '%b' (bool expected) for ngram_token_stream from "
      "VPack arguments",
      PRESERVE_ORIGINAL_PARAM_NAME.data());
    return false;
  }
  preserve_original = preserve_type_slice.getBool();
  options.preserve_original = preserve_original;

  //start marker
  if (slice.hasKey(START_MARKER_PARAM_NAME)) {
    auto start_marker_type_slice = slice.get(START_MARKER_PARAM_NAME);
    if (!start_marker_type_slice.isString()) {
      IR_FRMT_WARN(
        "Invalid type '%s' (string expected) for ngram_token_stream from "
        "VPack arguments",
        START_MARKER_PARAM_NAME.data());
      return false;
    }
    start_marker = irs::get_string<irs::string_ref>(start_marker_type_slice);
  }
  options.start_marker = irs::ref_cast<irs::byte_type>(start_marker);

  // end marker
  if (slice.hasKey(END_MARKER_PARAM_NAME)) {
    auto end_marker_type_slice = slice.get(END_MARKER_PARAM_NAME);
    if (!end_marker_type_slice.isString()) {
      IR_FRMT_WARN(
        "Invalid type '%s' (string expected) for ngram_token_stream from "
        "VPack arguments",
        END_MARKER_PARAM_NAME.data());
      return false;
    }
    end_marker = irs::get_string<irs::string_ref>(end_marker_type_slice);
  }
  options.end_marker = irs::ref_cast<irs::byte_type>(end_marker);

  //stream bytes
  if(slice.hasKey(STREAM_TYPE_PARAM_NAME)) {
    auto stream_type_slice = slice.get(STREAM_TYPE_PARAM_NAME);
    if (!stream_type_slice.isString()) {
      IR_FRMT_WARN(
        "Non-string value in '%s' while constructing ngram_token_stream "
        "from VPack arguments",
        STREAM_TYPE_PARAM_NAME.data());
      return false;
    }
    auto stream_type = stream_type_slice.stringRef();
    auto itr = STREAM_TYPE_CONVERT_MAP.find(irs::string_ref(stream_type.data(),
                                                            stream_type.size()));
    if (itr == STREAM_TYPE_CONVERT_MAP.end()) {
      IR_FRMT_WARN(
        "Invalid value in '%s' while constructing ngram_token_stream from "
        "VPack arguments",
        STREAM_TYPE_PARAM_NAME.data());
      return false;
    }
    stream_bytes_type = itr->second;
  }
  options.stream_bytes_type = stream_bytes_type;

  return true;
}

bool make_vpack_config(
    const irs::analysis::ngram_token_stream_base::Options& options,
    VPackBuilder* builder) {

  // ensure disambiguating casts below are safe. Casts required for clang compiler on Mac
  static_assert(sizeof(uint64_t) >= sizeof(size_t), "sizeof(uint64_t) >= sizeof(size_t)");

  VPackObjectBuilder object(builder);
  {
    //min_gram
    builder->add(MIN_PARAM_NAME, VPackValue(options.min_gram));

    //max_gram
    builder->add(MAX_PARAM_NAME, VPackValue(options.max_gram));

    //preserve_original
    builder->add(PRESERVE_ORIGINAL_PARAM_NAME, VPackValue(options.preserve_original));

    // stream type
    auto stream_type_value = std::find_if(STREAM_TYPE_CONVERT_MAP.begin(), STREAM_TYPE_CONVERT_MAP.end(),
      [&options](const decltype(STREAM_TYPE_CONVERT_MAP)::value_type& v) {
        return v.second == options.stream_bytes_type;
      });

    if (stream_type_value != STREAM_TYPE_CONVERT_MAP.end()) {
        builder->add(STREAM_TYPE_PARAM_NAME, VPackValue(stream_type_value->first));
    } else {
      IR_FRMT_ERROR(
        "Invalid %s value in ngram analyzer options: %d",
        STREAM_TYPE_PARAM_NAME.data(),
        static_cast<int>(options.stream_bytes_type));
      return false;
    }

    // start_marker
    builder->add(START_MARKER_PARAM_NAME, VPackValue(irs::ref_cast<char>(options.start_marker)));
    // end_marker
    builder->add(END_MARKER_PARAM_NAME, VPackValue(irs::ref_cast<char>(options.end_marker)));
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "min" (number): minimum ngram size
///        "max" (number): maximum ngram size
///        "preserveOriginal" (boolean): preserve or not the original term
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_vpack(const VPackSlice slice) {
  irs::analysis::ngram_token_stream_base::Options options;
  if (parse_vpack_options(slice, options)) {
    switch (options.stream_bytes_type) {
      case irs::analysis::ngram_token_stream_base::InputType::Binary:
        return irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::Binary>::make(options);
      case irs::analysis::ngram_token_stream_base::InputType::UTF8:
        return irs::analysis::ngram_token_stream<irs::analysis::ngram_token_stream_base::InputType::UTF8>::make(options);
    }
  }

  return nullptr;
}

irs::analysis::analyzer::ptr make_vpack(const irs::string_ref& args) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  return make_vpack(slice);
}
///////////////////////////////////////////////////////////////////////////////
/// @brief builds analyzer config from internal options in json format
///////////////////////////////////////////////////////////////////////////////
bool normalize_vpack_config(const VPackSlice slice, VPackBuilder* vpack_builder) {
  irs::analysis::ngram_token_stream_base::Options options;
  if (parse_vpack_options(slice, options)) {
    return make_vpack_config(options, vpack_builder);
  } else {
    return false;
  }
}

bool normalize_vpack_config(const irs::string_ref& args, std::string& config) {
  VPackSlice slice(reinterpret_cast<const uint8_t*>(args.c_str()));
  VPackBuilder builder;
  if (normalize_vpack_config(slice, &builder)) {
    config.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
    return true;
  }
  return false;
}

irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR(
        "Null arguments while constructing ngram_token_stream");
      return nullptr;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    return make_vpack(vpack->slice());
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while constructing ngram_token_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while constructing ngram_token_stream from JSON");
  }
  return nullptr;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing ngram_token_stream");
      return false;
    }
    auto vpack = VPackParser::fromJson(args.c_str(), args.size());
    VPackBuilder builder;
    if (normalize_vpack_config(vpack->slice(), &builder)) {
      definition = builder.toString();
      return !definition.empty();
    }
  } catch(const VPackException& ex) {
    IR_FRMT_ERROR(
      "Caught error '%s' while normalizing ngram_token_stream from JSON",
      ex.what());
  } catch (...) {
    IR_FRMT_ERROR(
      "Caught error while normalizing ngram_token_stream from JSON");
  }
  return false;
}

REGISTER_ANALYZER_VPACK(irs::analysis::ngram_token_stream_base, make_vpack, normalize_vpack_config);
REGISTER_ANALYZER_JSON(irs::analysis::ngram_token_stream_base, make_json, normalize_json_config);

}

namespace iresearch {
namespace analysis {

template<irs::analysis::ngram_token_stream_base::InputType StreamType>
/*static*/ analyzer::ptr ngram_token_stream<StreamType>::make(
    const ngram_token_stream_base::Options& options) {
  return std::make_unique<ngram_token_stream<StreamType>>(options);
}

/*static*/ void ngram_token_stream_base::init() {

  REGISTER_ANALYZER_VPACK(ngram_token_stream_base, make_vpack,
                            normalize_vpack_config); // match registration above
  REGISTER_ANALYZER_JSON(ngram_token_stream_base, make_json,
                            normalize_json_config); // match registration above
}

ngram_token_stream_base::ngram_token_stream_base(
    const ngram_token_stream_base::Options& options)
  : analyzer{ irs::type<ngram_token_stream_base>::get() },
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
  auto& term = std::get<term_attribute>(attrs_);
  auto& offset = std::get<irs::offset>(attrs_);
  auto& inc = std::get<increment>(attrs_);

  switch (emit_original_) {
    case EmitOriginal::WithoutMarkers:
      term.value = data_;
      assert(data_.size() <= std::numeric_limits<uint32_t>::max());
      offset.end = uint32_t(data_.size());
      emit_original_ = EmitOriginal::None;
      inc.value = next_inc_val_;
      break;
    case EmitOriginal::WithEndMarker:
      marked_term_buffer_.clear();
      IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.end_marker.size() + data_.size()));
      marked_term_buffer_.append(data_.begin(), data_end_);
      marked_term_buffer_.append(options_.end_marker.begin(), options_.end_marker.end());
      term.value = marked_term_buffer_;
      assert(marked_term_buffer_.size() <= std::numeric_limits<uint32_t>::max());
      offset.start = 0;
      offset.end = uint32_t(data_.size());
      emit_original_ = EmitOriginal::None; // end marker is emitted last, so we are done emitting original
      inc.value = next_inc_val_;
      break;
    case EmitOriginal::WithStartMarker:
      marked_term_buffer_.clear();
      IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.start_marker.size() + data_.size()));
      marked_term_buffer_.append(options_.start_marker.begin(), options_.start_marker.end());
      marked_term_buffer_.append(data_.begin(), data_end_);
      term.value = marked_term_buffer_;
      assert(marked_term_buffer_.size() <= std::numeric_limits<uint32_t>::max());
      offset.start = 0;
      offset.end = uint32_t(data_.size());
      emit_original_ = options_.end_marker.empty()? EmitOriginal::None : EmitOriginal::WithEndMarker;
      inc.value = next_inc_val_;
      break;
    default:
      IRS_ASSERT(false); // should not be called when None
      break;
  }
  next_inc_val_ = 0;
}

bool ngram_token_stream_base::reset(const irs::string_ref& value) noexcept {
  if (value.size() > std::numeric_limits<uint32_t>::max()) {
    // can't handle data which is longer than std::numeric_limits<uint32_t>::max()
    return false;
  }

  auto& term = std::get<term_attribute>(attrs_);
  auto& offset = std::get<irs::offset>(attrs_);

  // reset term attribute
  term.value = bytes_ref::NIL;

  // reset offset attribute
  offset.start = std::numeric_limits<uint32_t>::max();
  offset.end = std::numeric_limits<uint32_t>::max();

  // reset stream
  data_ = ref_cast<byte_type>(value);
  begin_ = data_.begin();
  ngram_end_ = begin_;
  data_end_ = data_.end();
  offset.start = 0;
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
  auto& term = std::get<term_attribute>(attrs_);
  auto& offset = std::get<irs::offset>(attrs_);
  auto& inc = std::get<increment>(attrs_);

  while (begin_ < data_end_) {
    if (length_ < options_.max_gram && next_symbol(ngram_end_)) {
      // we have next ngram from current position
      ++length_;
      if (length_ >= options_.min_gram) {
        IRS_ASSERT(begin_ <= ngram_end_);
        assert(static_cast<size_t>(std::distance(begin_, ngram_end_)) <= std::numeric_limits<uint32_t>::max());
        const auto ngram_byte_len = static_cast<uint32_t>(std::distance(begin_, ngram_end_));
        if (EmitOriginal::None == emit_original_ || 0 != offset.start || ngram_byte_len != data_.size()) {
          offset.end = offset.start + ngram_byte_len;
          inc.value = next_inc_val_;
          next_inc_val_ = 0;
          if ((0 != offset.start || start_marker_empty_) && (end_marker_empty_ || ngram_end_ != data_end_)) {
            term.value = irs::bytes_ref(begin_, ngram_byte_len);
          } else if (0 == offset.start && !start_marker_empty_) {
            marked_term_buffer_.clear();
            IRS_ASSERT(marked_term_buffer_.capacity() >= (options_.start_marker.size() + ngram_byte_len));
            marked_term_buffer_.append(options_.start_marker.begin(), options_.start_marker.end());
            marked_term_buffer_.append(begin_, ngram_byte_len);
            term.value = marked_term_buffer_;
            assert(marked_term_buffer_.size() <= std::numeric_limits<uint32_t>::max());
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
            term.value = marked_term_buffer_;
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
          offset.start = static_cast<uint32_t>(std::distance(data_.begin(), begin_));
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
