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

#pragma once

#include "analyzer.hpp"
#include "token_attributes.hpp"
#include "utils/attribute_helper.hpp"
#include "utils/numeric_utils.hpp"

namespace irs {

// Convenient helper implementation providing access to "increment"
// and "term_attributes" attributes
class basic_token_stream : public analysis::analyzer {
 public:
  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  bool reset(std::string_view) final { return false; }

 protected:
  std::tuple<term_attribute, increment> attrs_;
};

// analyzer implementation for boolean field, a single bool term.
class boolean_token_stream final : public basic_token_stream,
                                   private util::noncopyable {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "boolean_token_stream";
  }

  static constexpr std::string_view value_true() noexcept {
    return {"\xFF", 1};
  }

  static constexpr std::string_view value_false() noexcept {
    return {"\x00", 1};
  }

  static constexpr std::string_view value(bool val) noexcept {
    return val ? value_true() : value_false();
  }

  bool next() noexcept final;

  void reset(bool value) noexcept {
    value_ = value;
    in_use_ = false;
  }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<boolean_token_stream>::id();
  }

 private:
  using basic_token_stream::reset;

  bool in_use_{false};
  bool value_{false};
};

// Basic implementation of token_stream for simple string field.
// it does not tokenize or analyze field, just set attributes based
// on initial string length
class string_token_stream : public analysis::TypedAnalyzer<string_token_stream>,
                            private util::noncopyable {
 public:
  static constexpr std::string_view type_name() noexcept { return "identity"; }

  bool next() noexcept final;

  attribute* get_mutable(type_info::type_id id) noexcept final {
    return irs::get_mutable(attrs_, id);
  }

  void reset(bytes_view value) noexcept {
    value_ = value;
    in_use_ = false;
  }

  bool reset(std::string_view value) noexcept final {
    value_ = ViewCast<byte_type>(value);
    in_use_ = false;
    return true;
  }

 private:
  std::tuple<offset, increment, term_attribute> attrs_;
  bytes_view value_;
  bool in_use_{false};
};

// analyzer implementation for numeric field. based on precision
// step it produces several terms representing ranges of the input
// term
class numeric_token_stream final : public basic_token_stream,
                                   private util::noncopyable {
 public:
  static constexpr uint32_t PRECISION_STEP_DEF = 16;
  static constexpr uint32_t PRECISION_STEP_32 = 8;

  static bytes_view value(bstring& buf, int32_t value);
  static bytes_view value(bstring& buf, int64_t value);

#ifndef FLOAT_T_IS_DOUBLE_T
  static bytes_view value(bstring& buf, float_t value);
#endif

  static bytes_view value(bstring& buf, double_t value);

  static constexpr std::string_view type_name() noexcept {
    return "numeric_token_stream";
  }

  bool next() final;

  void reset(int32_t value, uint32_t step = PRECISION_STEP_DEF);
  void reset(int64_t value, uint32_t step = PRECISION_STEP_DEF);

#ifndef FLOAT_T_IS_DOUBLE_T
  void reset(float_t value, uint32_t step = PRECISION_STEP_DEF);
#endif

  void reset(double_t value, uint32_t step = PRECISION_STEP_DEF);

  irs::type_info::type_id type() const noexcept final {
    return irs::type<numeric_token_stream>::id();
  }

 private:
  using basic_token_stream::reset;

  //////////////////////////////////////////////////////////////////////////////
  /// @class numeric_term
  /// @brief term_attribute implementation for numeric_token_stream
  //////////////////////////////////////////////////////////////////////////////
  class numeric_term final {
   public:
    static bytes_view value(bstring& buf, int32_t value) {
      decltype(val_) val;

      val.i32 = value;
      buf.resize(numeric_utils::numeric_traits<decltype(value)>::size());

      return numeric_term::value(buf.data(), NT_INT, val, 0);
    }

    static bytes_view value(bstring& buf, int64_t value) {
      decltype(val_) val;

      val.i64 = value;
      buf.resize(numeric_utils::numeric_traits<decltype(value)>::size());

      return numeric_term::value(buf.data(), NT_LONG, val, 0);
    }

#ifndef FLOAT_T_IS_DOUBLE_T
    static bytes_view value(bstring& buf, float_t value) {
      decltype(val_) val;

      val.i32 = numeric_utils::numeric_traits<float_t>::integral(value);
      buf.resize(numeric_utils::numeric_traits<decltype(value)>::size());

      return numeric_term::value(buf.data(), NT_FLOAT, val, 0);
    }
#endif

    static bytes_view value(bstring& buf, double_t value) {
      decltype(val_) val;

      val.i64 = numeric_utils::numeric_traits<double_t>::integral(value);
      buf.resize(numeric_utils::numeric_traits<decltype(value)>::size());

      return numeric_term::value(buf.data(), NT_DBL, val, 0);
    }

    bool next(increment& inc, bytes_view& out);

    void reset(int32_t value, uint32_t step) {
      val_.i32 = value;
      type_ = NT_INT;
      step_ = step;
      shift_ = 0;
    }

    void reset(int64_t value, uint32_t step) {
      val_.i64 = value;
      type_ = NT_LONG;
      step_ = step;
      shift_ = 0;
    }

#ifndef FLOAT_T_IS_DOUBLE_T
    void reset(float_t value, uint32_t step) {
      val_.i32 = numeric_utils::numeric_traits<float_t>::integral(value);
      type_ = NT_FLOAT;
      step_ = step;
      shift_ = 0;
    }
#endif

    void reset(double_t value, uint32_t step) {
      val_.i64 = numeric_utils::numeric_traits<double_t>::integral(value);
      type_ = NT_DBL;
      step_ = step;
      shift_ = 0;
    }

   private:
    enum NumericType { NT_LONG = 0, NT_DBL, NT_INT, NT_FLOAT };

    union value_t {
      uint64_t i64;
      uint32_t i32;
    };

    static irs::bytes_view value(byte_type* buf, NumericType type, value_t val,
                                 uint32_t shift);

    byte_type data_[numeric_utils::numeric_traits<double_t>::size()];
    value_t val_;
    NumericType type_;
    uint32_t step_;
    uint32_t shift_;
  };

  numeric_term num_;
};

// analyzer implementation for null field, a single null term.
class null_token_stream final : public basic_token_stream,
                                private util::noncopyable {
 public:
  static constexpr std::string_view type_name() noexcept {
    return "null_token_stream";
  }

  static constexpr std::string_view value_null() noexcept {
    // data pointer != nullptr or IRS_ASSERT failure in bytes_hash::insert(...)
    return {"\x00", 0};
  }

  bool next() noexcept final;

  void reset() noexcept { in_use_ = false; }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<null_token_stream>::id();
  }

 private:
  using basic_token_stream::reset;

  bool in_use_{false};
};

}  // namespace irs
