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

#include <rapidjson/rapidjson/document.h> // for rapidjson::Document

#include "delimited_token_stream.hpp"

NS_LOCAL

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

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a jSON encoded object with the following attributes:
///        "delimiter"(string): the delimiter to use for tokenization <required>
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_json(const irs::string_ref& args) {
  rapidjson::Document json;

  if (json.Parse(args.c_str(), args.size()).HasParseError()) {
    IR_FRMT_ERROR(
      "Invalid jSON arguments passed while constructing delimited_token_stream, arguments: %s", 
      args.c_str()
    );

    return nullptr;
  }

  switch (json.GetType()) {
   case rapidjson::kStringType:
    return irs::analysis::delimited_token_stream::make(json.GetString());
   case rapidjson::kObjectType:
    if (json.HasMember("delimiter") && json["delimiter"].IsString()) {
      return irs::analysis::delimited_token_stream::make(json["delimiter"].GetString());
    }
   default: {} // fall through
  }

  IR_FRMT_ERROR(
    "Missing 'delimiter' while constructing delimited_token_stream from jSON arguments: %s",
    args.c_str()
  );

  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief args is a delimiter to use for tokenization
////////////////////////////////////////////////////////////////////////////////
irs::analysis::analyzer::ptr make_text(const irs::string_ref& args) {
  return irs::memory::make_shared<irs::analysis::delimited_token_stream>(
    args
  );
}

REGISTER_ANALYZER_JSON(irs::analysis::delimited_token_stream, make_json);
REGISTER_ANALYZER_TEXT(irs::analysis::delimited_token_stream, make_text);

NS_END

NS_ROOT
NS_BEGIN(analysis)

DEFINE_ANALYZER_TYPE_NAMED(delimited_token_stream, "delimited")

delimited_token_stream::delimited_token_stream(const string_ref& delimiter)
  : analyzer(delimited_token_stream::type()),
    attrs_(4), // increment + offset + payload + term
    delim_(ref_cast<byte_type>(delimiter)) {
  attrs_.emplace(inc_);
  attrs_.emplace(offset_);
  attrs_.emplace(payload_);
  attrs_.emplace(term_);

  if (!delim_.null()) {
    delim_buf_ = delim_; // keep a local copy of the delimiter
    delim_ = delim_buf_; // update the delimter to point at the local copy
  }
}

/*static*/ analyzer::ptr delimited_token_stream::make(
    const string_ref& delimiter
) {
  return make_text(delimiter);
}

/*static*/ void delimited_token_stream::init() {
  REGISTER_ANALYZER_JSON(delimited_token_stream, make_json); // match registration above
  REGISTER_ANALYZER_TEXT(delimited_token_stream, make_text); // match registration above
}

bool delimited_token_stream::next() {
  if (data_.null()) {
    return false;
  }

  auto size = find_delimiter(data_, delim_);
  auto next = std::max(size_t(1), size + delim_.size());
  auto start = offset_.end + uint32_t(delim_.size()); // value is allowed to overflow, will only produce invalid result
  auto end = start + size;

  if (irs::integer_traits<uint32_t>::const_max < end) {
    return false; // cannot fit the next token into offset calculation
  }

  offset_.start = start;
  offset_.end = uint32_t(end);
  payload_.value = bytes_ref(data_.c_str(), size);
  term_.value(
    delim_.null()
    ? payload_.value // identity
    : eval_term(term_buf_, payload_.value)
  );
  data_ = size >= data_.size()
        ? bytes_ref::NIL
        : bytes_ref(data_.c_str() + next, data_.size() - next)
        ;

  return true;
}

bool delimited_token_stream::reset(const string_ref& data) {
  data_ = ref_cast<byte_type>(data);
  offset_.start = 0;
  offset_.end = 0 - uint32_t(delim_.size()); // counterpart to computation in next() above

  return true;
}

NS_END // analysis
NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------