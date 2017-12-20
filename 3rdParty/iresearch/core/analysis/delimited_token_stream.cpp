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

NS_END

NS_ROOT
NS_BEGIN(analysis)

DEFINE_ANALYZER_TYPE_NAMED(delimited_token_stream, "delimited");
REGISTER_ANALYZER_TEXT(delimited_token_stream, delimited_token_stream::make);

delimited_token_stream::delimited_token_stream(const string_ref& args)
  : analyzer(delimited_token_stream::type()),
    delim_(ref_cast<byte_type>(args)) {
  attrs_.emplace(offset_);
  attrs_.emplace(payload_);
  attrs_.emplace(term_);

  if (!delim_.null()) {
    delim_buf_ = delim_; // keep a local copy of the delimiter
    delim_ = delim_buf_; // update the delimter to point at the local copy
  }
}

/*static*/ analyzer::ptr delimited_token_stream::make(const string_ref& args) {
  PTR_NAMED(delimited_token_stream, ptr, args);

  return ptr;
}

bool delimited_token_stream::next() {
  if (data_.null()) {
    return false;
  }

  auto size = find_delimiter(data_, delim_);
  auto next = std::max(size_t(1), size + delim_.size());

  offset_.start = offset_.end + delim_.size();
  offset_.end = offset_.start + size;
  payload_.value = bytes_ref(data_.c_str(), size);
  term_.value(
    delim_.null()
    ? payload_.value // identity
    : eval_term(term_buf_, payload_.value)
  );
  data_ = size >= data_.size()
        ? bytes_ref::nil
        : bytes_ref(data_.c_str() + next, data_.size() - next)
        ;

  return true;
}

bool delimited_token_stream::reset(const string_ref& data) {
  data_ = ref_cast<byte_type>(data);
  offset_.start = 0;
  offset_.end = 0 - delim_.size();

  return true;
}

NS_END // analysis
NS_END // ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// ----------------------------------------------------------------------------