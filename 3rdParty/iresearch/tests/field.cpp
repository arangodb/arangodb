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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "field.hpp"

#include "store/data_output.hpp"
#include "store/store_utils.hpp"

#include "analysis/analyzer.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                              field implementation
// -----------------------------------------------------------------------------

field::field(field&& rhs) NOEXCEPT
  : name_(std::move(rhs.name_)),
    boost_(rhs.boost_) {
}

field& field::operator=(field&& rhs) NOEXCEPT {
  if (this != &rhs) {
    name_ = std::move(rhs.name_);
    boost_ = rhs.boost_;
  }

  return *this;
}

field::~field() { }

// -----------------------------------------------------------------------------
// --SECTION--                                         long_field implementation
// -----------------------------------------------------------------------------

long_field::long_field(long_field&& other) NOEXCEPT
  : stream_(std::move(other.stream_)), value_(std::move(other.value_)) {
}

bool long_field::write(data_output& out) const {
  write_zvlong(out, value_);
  return true;
}

token_stream& long_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

const flags& long_field::features() const {
  static const flags features{ granularity_prefix::type() }; // for by_granular_range
  return features;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         int_field implementation
// -----------------------------------------------------------------------------

int_field::int_field(int_field&& other) NOEXCEPT
  : stream_(std::move(other.stream_)), value_(std::move(other.value_)) {
}

bool int_field::write(data_output& out) const {
  write_zvint(out, value_);
  return true;
}

token_stream& int_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

const flags& int_field::features() const {
  static const flags features{ granularity_prefix::type() }; // for by_granular_range
  return features;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       double_field implementation
// -----------------------------------------------------------------------------

double_field::double_field(double_field&& other) NOEXCEPT
  : stream_(std::move(other.stream_)), value_(std::move(other.value_)) {
}

token_stream& double_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

bool double_field::write(data_output& out) const {
  write_zvdouble(out, value_);
  return true;
}

const flags& double_field::features() const {
  static const flags features{ granularity_prefix::type() }; // for by_granular_range
  return features;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        float_field implementation
// -----------------------------------------------------------------------------

float_field::float_field(float_field&& other) NOEXCEPT
  : stream_(std::move(other.stream_)), value_(std::move(other.value_)) {
}

bool float_field::write(data_output& out) const {
  write_zvfloat(out, value_);
  return true;
}

token_stream& float_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

const flags& float_field::features() const {
  static const flags features{ granularity_prefix::type() }; // for by_granular_range
  return features;
}

// -----------------------------------------------------------------------------
// --SECTION--                                       string_field implementation
// -----------------------------------------------------------------------------

string_field::string_field(string_field&& other) NOEXCEPT
  : stream_(std::move(other.stream_)), value_(std::move(other.value_)) {
}

bool string_field::write(data_output& out) const {
  write_string(out, value_);
  return true;
}

token_stream& string_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

const flags& string_field::features() const {
  return flags::empty_instance();
}

// -----------------------------------------------------------------------------
// --SECTION--                                       binary_field implementation
// -----------------------------------------------------------------------------

binary_field::binary_field(binary_field&& other) NOEXCEPT
  : stream_(std::move(other.stream_)), value_(std::move(other.value_)) {
}

void binary_field::value(const bstring& value) {
  value_ = value;
}

void binary_field::value(bstring&& value) {
  value_ = std::move(value);
}

bool binary_field::write(data_output& out) const {
  write_string(out, value_);
  return true;
}

token_stream& binary_field::get_tokens() const {
  stream_.reset(value_);
  return stream_;
}

const flags& binary_field::features() const {
  return flags::empty_instance();
}

NS_END