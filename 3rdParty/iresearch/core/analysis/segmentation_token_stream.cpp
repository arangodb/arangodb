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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "unicorn/segment.hpp"
#include "unicorn/string.hpp"
#include <frozen/unordered_map.h>
#include "velocypack/Slice.h"
#include "velocypack/Builder.h"
#include "velocypack/Parser.h"

#include "utils/hash_utils.hpp"
#include "segmentation_token_stream.hpp"

namespace {

constexpr irs::string_ref CASE_CONVERT_PARAM_NAME  = "case";
constexpr irs::string_ref BREAK_PARAM_NAME         = "break";

const frozen::unordered_map<
    irs::string_ref,
    irs::analysis::segmentation_token_stream::options_t::case_convert_t, 3> CASE_CONVERT_MAP = {
  { "lower", irs::analysis::segmentation_token_stream::options_t::case_convert_t::LOWER },
  { "none", irs::analysis::segmentation_token_stream::options_t::case_convert_t::NONE },
  { "upper", irs::analysis::segmentation_token_stream::options_t::case_convert_t::UPPER },
};

const frozen::unordered_map<
    irs::string_ref,
    irs::analysis::segmentation_token_stream::options_t::word_break_t, 3> BREAK_CONVERT_MAP = {
  { "all", irs::analysis::segmentation_token_stream::options_t::word_break_t::ALL },
  { "alpha", irs::analysis::segmentation_token_stream::options_t::word_break_t::ALPHA },
  { "graphic", irs::analysis::segmentation_token_stream::options_t::word_break_t::GRAPHIC },
};

bool parse_vpack_options(const irs::string_ref& args,
                         irs::analysis::segmentation_token_stream::options_t& options) {
  arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
  if (!slice.isObject()) {
    IR_FRMT_ERROR("Slice for segmentation_token_stream is not an object: %s",
                  slice.toString().c_str());
    return false;
  }
  if (slice.hasKey(CASE_CONVERT_PARAM_NAME.c_str())) {
    auto case_convert_slice = slice.get(CASE_CONVERT_PARAM_NAME.c_str());
    if (!case_convert_slice.isString()) {
      IR_FRMT_WARN(
          "Invalid type '%s' (string expected) for segmentation_token_stream from"
          " Vpack arguments: %s",
          CASE_CONVERT_PARAM_NAME.c_str(), slice.toString().c_str());
      return false;
    }
    auto case_convert = case_convert_slice.stringRef();
    auto itr = CASE_CONVERT_MAP.find(irs::string_ref(case_convert.data(),
                                     case_convert.size()));

    if (itr == CASE_CONVERT_MAP.end()) {
      IR_FRMT_WARN(
          "Invalid value in '%s' for segmentation_token_stream from"
          " Vpack arguments: %s",
          CASE_CONVERT_PARAM_NAME.c_str(), slice.toString().c_str());
      return false;
    }
    options.case_convert = itr->second;
  }
  if (slice.hasKey(BREAK_PARAM_NAME.c_str())) {
    auto break_type_slice = slice.get(BREAK_PARAM_NAME.c_str());
    if (!break_type_slice.isString()) {
      IR_FRMT_WARN(
          "Invalid type '%s' (string expected) for segmentation_token_stream from "
          "Vpack arguments: %s",
          BREAK_PARAM_NAME.c_str(), slice.toString().c_str());
      return false;
    }
    auto break_type = break_type_slice.stringRef();
    auto itr = BREAK_CONVERT_MAP.find(irs::string_ref(break_type.data(),
                                                      break_type.size()));

    if (itr == BREAK_CONVERT_MAP.end()) {
      IR_FRMT_WARN(
          "Invalid value in '%s' for segmentation_token_stream from "
          "Vpack arguments: %s",
          BREAK_PARAM_NAME.c_str(), slice.toString().c_str());
      return false;
    }
    options.word_break = itr->second;
  }
  return true;
}

bool make_vpack_config(const irs::analysis::segmentation_token_stream::options_t& options,
  std::string& definition) {
  arangodb::velocypack::Builder builder;
  {
    arangodb::velocypack::ObjectBuilder object(&builder);
    {
      auto it = std::find_if(
          CASE_CONVERT_MAP.begin(), CASE_CONVERT_MAP.end(),
          [v = options.case_convert](const decltype(CASE_CONVERT_MAP)::value_type& m) {
            return m.second == v;
          });
      if (it != CASE_CONVERT_MAP.end()) {
        builder.add(CASE_CONVERT_PARAM_NAME.c_str(),
                    arangodb::velocypack::Value(it->first.c_str()));
      } else {
         IR_FRMT_WARN(
              "Invalid value in '%s' for normalizing segmentation_token_stream from "
              "Value is : %d",
              CASE_CONVERT_PARAM_NAME.c_str(), options.case_convert);
         return false;
      }
    }
    {
      auto it = std::find_if(
          BREAK_CONVERT_MAP.begin(), BREAK_CONVERT_MAP.end(),
          [v = options.word_break](const decltype(BREAK_CONVERT_MAP)::value_type& m) {
            return m.second == v;
          });
      if (it != BREAK_CONVERT_MAP.end()) {
        builder.add(BREAK_PARAM_NAME.c_str(), arangodb::velocypack::Value(it->first.c_str()));
      } else {
         IR_FRMT_WARN(
              "Invalid value in '%s' for normalizing segmentation_token_stream from "
              "Value is : %d",
              BREAK_PARAM_NAME.c_str(), options.word_break);
         return false;
      }
    }
  }
  definition.assign(builder.slice().startAs<char>(), builder.slice().byteSize());
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a Vpack slice object with the following attributes:
///        "case"(string enum): modify token case
///        "break"(string enum): word breaking method
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_vpack(const irs::string_ref& args) {
  try {
    irs::analysis::segmentation_token_stream::options_t options;
    if (!parse_vpack_options(args, options)) {
      return nullptr;
    }
    return irs::memory::make_unique<irs::analysis::segmentation_token_stream>(
        std::move(options));
  } catch(const arangodb::velocypack::Exception& ex) {
    arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
    IR_FRMT_ERROR("Caught error '%s' while constructing segmentation_token_stream from Vpack arguments: %s",
                  ex.what(), slice.toString().c_str());
  } catch (...) {
    arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
    IR_FRMT_ERROR("Caught error while constructing segmentation_token_stream from Vpack arguments: %s",
                  slice.toString().c_str());
  }
  return nullptr;
}

bool normalize_vpack_config(const irs::string_ref& args, std::string& definition) {
  irs::analysis::segmentation_token_stream::options_t options;
  try {
    if (parse_vpack_options(args, options)) {
      return make_vpack_config(options, definition);
    } else {
      return false;
    }
  } catch(const arangodb::velocypack::Exception& ex) {
    arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
    IR_FRMT_ERROR("Caught error '%s' while normalizing segmentation_token_stream from Vpack arguments: %s",
                  ex.what(), slice.toString().c_str());
  } catch (...) {
    arangodb::velocypack::Slice slice(reinterpret_cast<uint8_t const*>(args.c_str()));
    IR_FRMT_ERROR("Caught error while normalizing segmentation_token_stream from Vpack arguments: %s",
                  slice.toString().c_str());
  }
  return false;
}

irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while constructing segmentation_token_stream");
      return nullptr;
    }
    auto vpack = arangodb::velocypack::Parser::fromJson(args.c_str());
    return make_vpack(
        irs::string_ref(reinterpret_cast<const char*>(vpack->data()), vpack->size()));
  } catch(const arangodb::velocypack::Exception& ex) {
    IR_FRMT_ERROR("Caught error '%s' while constructing segmentation_token_stream from json: %s",
                  ex.what(), args.c_str());
  } catch (...) {
    IR_FRMT_ERROR("Caught error while constructing segmentation_token_stream from json: %s",
                  args.c_str());
  }
  return nullptr;
}

bool normalize_json_config(const irs::string_ref& args, std::string& definition) {
  try {
    if (args.null()) {
      IR_FRMT_ERROR("Null arguments while normalizing segmentation_token_stream");
      return false;
    }
    auto vpack = arangodb::velocypack::Parser::fromJson(args.c_str());
    std::string vpack_container;
    if (normalize_vpack_config(
        irs::string_ref(reinterpret_cast<const char*>(vpack->data()), vpack->size()),
        vpack_container)) {
      arangodb::velocypack::Slice slice(
          reinterpret_cast<uint8_t const*>(vpack_container.c_str()));
      definition = slice.toString();
      return true;
    }
  } catch(const arangodb::velocypack::Exception& ex) {
    IR_FRMT_ERROR("Caught error '%s' while normalizing segmentation_token_stream from json: %s",
                  ex.what(), args.c_str());
  } catch (...) {
    IR_FRMT_ERROR("Caught error while normalizing segmentation_token_stream from json: %s",
                  args.c_str());
  }
  return false;
}

} // namespace

namespace iresearch {
namespace analysis {

struct segmentation_token_stream::state_t {
  RS::Unicorn::WordIterator<char> begin;
  RS::Unicorn::WordIterator<char> end;
};

REGISTER_ANALYZER_VPACK(segmentation_token_stream, make_vpack,
                        normalize_vpack_config);
REGISTER_ANALYZER_JSON(segmentation_token_stream, make_json,
                        normalize_json_config);


/*static*/ void segmentation_token_stream::init() {
  REGISTER_ANALYZER_VPACK(segmentation_token_stream, make_vpack,
                          normalize_vpack_config);  // match registration above
  REGISTER_ANALYZER_JSON(segmentation_token_stream, make_json,
                         normalize_json_config);  // match registration above
}

segmentation_token_stream::segmentation_token_stream(
    segmentation_token_stream::options_t&& options)
  : analyzer{ irs::type<segmentation_token_stream>::get() },
    state_(memory::make_unique<state_t>()), options_(options) {
}

bool segmentation_token_stream::next() {
  if (state_->begin != state_->end) {
    assert(state_->begin->first.offset() <=
           std::numeric_limits<uint32_t>::max());
    assert(state_->begin->second.offset() <=
           std::numeric_limits<uint32_t>::max());
    std::get<1>(attrs_).start =
        static_cast<uint32_t>(state_->begin->first.offset());
    std::get<1>(attrs_).end =
        static_cast<uint32_t>(state_->begin->second.offset());
    switch (options_.case_convert) {
      case options_t::case_convert_t::NONE:
        std::get<2>(attrs_).value =
            irs::ref_cast<irs::byte_type>(RS::Unicorn::u_view(*state_->begin++));
        break;
      case options_t::case_convert_t::LOWER:
        term_buf_ =
            RS::Unicorn::str_lowercase(RS::Unicorn::u_view(*state_->begin++));
        std::get<2>(attrs_).value =
            irs::ref_cast<irs::byte_type>(irs::string_ref(term_buf_));
        break;
      case options_t::case_convert_t::UPPER:
        term_buf_ =
            RS::Unicorn::str_uppercase(RS::Unicorn::u_view(*state_->begin++));
        std::get<2>(attrs_).value =
            irs::ref_cast<irs::byte_type>(irs::string_ref(term_buf_));
        break;
    }
    return true;
  }
  return false;
}

bool segmentation_token_stream::reset(const string_ref& data) {
  auto flags = RS::Unicorn::Segment::alpha;
  switch (options_.word_break) {
    case options_t::word_break_t::ALL:
      flags = RS::Unicorn::Segment::unicode;
      break;
    case options_t::word_break_t::GRAPHIC:
      flags = RS::Unicorn::Segment::graphic;
      break;
    case options_t::word_break_t::ALPHA:
    [[fallthrough]];
    default:
      assert(options_.word_break == options_t::word_break_t::ALPHA);
      break;
  }
  auto range =  RS::Unicorn::word_range(static_cast<std::string_view>(data), flags);
  state_->begin = range.begin();
  state_->end = range.end();
  return true;
}

} // namespace analysis
} // namespace iresearch
