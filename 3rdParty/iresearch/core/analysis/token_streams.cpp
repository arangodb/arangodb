////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "token_streams.hpp"
#include "utils/bit_utils.hpp"
#include "utils/string_utils.hpp"

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                               boolean_token_stream implementation
// -----------------------------------------------------------------------------

boolean_token_stream::boolean_token_stream(bool value /*= false*/) noexcept
  : in_use_(false),
    value_(value) {
}

bool boolean_token_stream::next() noexcept {
  const auto in_use = in_use_;
  in_use_ = true;
  std::get<term_attribute>(attrs_).value = ref_cast<byte_type>(value(value_));
  return !in_use;
}

// -----------------------------------------------------------------------------
// --SECTION--                                string_token_stream implementation
// -----------------------------------------------------------------------------

string_token_stream::string_token_stream() noexcept
   : in_use_(false) {
}

bool string_token_stream::next() noexcept {
  const auto in_use = in_use_;
  std::get<term_attribute>(attrs_).value = value_;
  auto& offset = std::get<irs::offset>(attrs_);
  offset.start = 0;
  offset.end = static_cast<uint32_t>(value_.size());
  value_ = irs::bytes_ref::NIL;
  in_use_ = true;
  return !in_use;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       numeric_term implementation
// -----------------------------------------------------------------------------

bytes_ref numeric_token_stream::numeric_term::value(
    byte_type* buf, NumericType type,
    value_t val, uint32_t shift) {
  switch (type) {
    case NT_LONG: {
      using traits_t = numeric_utils::numeric_traits<int64_t> ;
      static_assert(traits_t::size() <=
        IRESEARCH_COUNTOF(decltype(numeric_token_stream::numeric_term::data_){}));

      return {buf, traits_t::encode(val.i64, buf, shift)};
    }
    case NT_DBL: {
      using traits_t = numeric_utils::numeric_traits<double_t>;
      static_assert(traits_t::size() <=
        IRESEARCH_COUNTOF(decltype(numeric_token_stream::numeric_term::data_){}));

      return {buf, traits_t::encode(val.i64, buf, shift)};
    }
    case NT_INT: {
      using traits_t = numeric_utils::numeric_traits<int32_t>;
      static_assert(traits_t::size() <=
        IRESEARCH_COUNTOF(decltype(numeric_token_stream::numeric_term::data_){}));

      return {buf, traits_t::encode(val.i32, buf, shift)};
    }
    case NT_FLOAT: {
      using traits_t = numeric_utils::numeric_traits<float_t>;
      static_assert(traits_t::size() <=
        IRESEARCH_COUNTOF(decltype(numeric_token_stream::numeric_term::data_){}));

      return {buf, traits_t::encode(val.i32, buf, shift)};
    }
  }

  return bytes_ref::NIL;
}

bool numeric_token_stream::numeric_term::next(increment& inc, bytes_ref& out) {
  constexpr uint32_t INCREMENT_VALUE[] { 0, 1 };
  constexpr uint32_t BITS_REQUIRED[] {
    bits_required<int64_t>(),
    bits_required<int32_t>() };

  if (shift_ >= BITS_REQUIRED[type_ > NT_DBL]) {
    return false;
  }

  out = value(data_, type_, val_, shift_);
  shift_ += step_;
  inc.value = INCREMENT_VALUE[step_ == shift_];

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                               numeric_token_stream implementation
// -----------------------------------------------------------------------------

bool numeric_token_stream::next() {
  return num_.next(
    std::get<increment>(attrs_),
    std::get<term_attribute>(attrs_).value);
}

void numeric_token_stream::reset(
    int32_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}

void numeric_token_stream::reset(
    int64_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}

#ifndef FLOAT_T_IS_DOUBLE_T
void numeric_token_stream::reset(
    float_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}
#endif

void numeric_token_stream::reset(
    double_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_.reset(value, step);
}

/*static*/ bytes_ref numeric_token_stream::value(bstring& buf, int32_t value) {
  return numeric_term::value(buf, value);
}

/*static*/ bytes_ref numeric_token_stream::value(bstring& buf, int64_t value) {
  return numeric_term::value(buf, value);
}

#ifndef FLOAT_T_IS_DOUBLE_T
  /*static*/ bytes_ref numeric_token_stream::value(bstring& buf, float_t value) {
    return numeric_term::value(buf, value);
  }
#endif

/*static*/ bytes_ref numeric_token_stream::value(bstring& buf, double_t value) {
  return numeric_term::value(buf, value);
}

// -----------------------------------------------------------------------------
// --SECTION--                                  null_token_stream implementation
// -----------------------------------------------------------------------------

bool null_token_stream::next() noexcept {
  const auto in_use = in_use_;
  in_use_ = true;
  std::get<term_attribute>(attrs_).value = irs::ref_cast<byte_type>(value_null());
  return !in_use;
}

}
