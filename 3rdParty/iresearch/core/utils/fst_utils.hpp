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

#ifndef IRESEARCH_FST_UTILS_H
#define IRESEARCH_FST_UTILS_H

#include "store/data_output.hpp"
#include "store/data_input.hpp"

#include "noncopyable.hpp"

#include "fst_decl.hpp"
#include "fst_string_weight.h"

NS_ROOT

struct byte_weight_output : data_output, private util::noncopyable {
  virtual void close() override {}

  virtual void write_byte(byte_type b) override final {
    weight.PushBack(b);
  }

  virtual void write_bytes(const byte_type* b, size_t len) override final {
    weight.PushBack(b, b + len);
  }

  byte_weight weight;
}; // byte_weight_output

struct byte_weight_input final : data_input, private util::noncopyable {
 public:
  byte_weight_input() = default;

  explicit byte_weight_input(byte_weight&& weight) NOEXCEPT
    : weight_(std::move(weight)),
      begin_(weight_.begin()),
      end_(weight_.end()) {
  }

  virtual byte_type read_byte() override {
    assert(!eof());
    const auto b = *begin_;
    ++begin_;
    return b;
  }

  virtual size_t read_bytes(byte_type* b, size_t size) override {
    const size_t pos = std::distance(weight_.begin(), begin_); // current position
    const size_t read = std::min(size, weight_.Size() - pos);  // number of elements to read

    std::memcpy(b, weight_.c_str() + pos, read*sizeof(byte_type));

    return read;
  }

  virtual bool eof() const override {
    return begin_ == end_;
  }

  virtual size_t length() const override {
    return weight_.Size();
  }

  virtual size_t file_pointer() const override {
    return std::distance(weight_.begin(), begin_);
  }

  void reset() {
    begin_ = weight_.begin();
  }

 private:
  byte_weight weight_;
  byte_weight::iterator begin_{ weight_.begin() };
  const byte_weight::iterator end_{ weight_.end() };
}; // byte_weight_input

NS_END // ROOT

#endif
