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

#ifndef IRESEARCH_TOKEN_STREAMS_H
#define IRESEARCH_TOKEN_STREAMS_H

#include "token_stream.hpp"
#include "token_attributes.hpp"
#include "utils/numeric_utils.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class basic_term
/// @brief basic term_attribute implementation for string_token_stream
//////////////////////////////////////////////////////////////////////////////
class basic_term final : public term_attribute {
 public:
  void value(const bytes_ref& value) {
    value_ = value;
  }
}; // basic_term

//////////////////////////////////////////////////////////////////////////////
/// @class null_token_stream
/// @brief token_stream implementation for boolean field, a single bool term.
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API boolean_token_stream final
    : public token_stream,
      private util::noncopyable { // attrs_ non-copyable
 public:
  static const bytes_ref& value_false();
  static const bytes_ref& value_true();
  static const bytes_ref& value(bool val);

  explicit boolean_token_stream(bool value = false);
  boolean_token_stream(boolean_token_stream&& other) NOEXCEPT;

  virtual bool next() override;

  void reset(bool value) {
    value_ = value;
    in_use_ = false;
  }

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

 private:
  void init_attributes() {
    attrs_.emplace(term_);
    attrs_.emplace(inc_); // required by field_data::invert(...)
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  attribute_view attrs_;
  basic_term term_;
  increment inc_;
  bool in_use_;
  bool value_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

//////////////////////////////////////////////////////////////////////////////
/// @class string_token_stream 
/// @brief basic implementation of token_stream for simple string field.
///        it does not tokenize or analyze field, just set attributes based
///        on initial string length 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API string_token_stream final
    : public token_stream,
      private util::noncopyable { // attrs_ non-copyable
 public:
  string_token_stream();
  string_token_stream(string_token_stream&& other) NOEXCEPT;

  virtual bool next() override;

  void reset(const bytes_ref& value) { 
    value_ = value;
    in_use_ = false; 
  }

  void reset(const string_ref& value) {
    value_ = ref_cast<byte_type>(value);
    in_use_ = false;
  }

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

 private:
  void init_attributes() {
    attrs_.emplace(offset_);
    attrs_.emplace(inc_);
    attrs_.emplace(term_);
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  attribute_view attrs_;
  offset offset_;
  increment inc_;
  basic_term term_;
  bytes_ref value_;
  bool in_use_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // string_token_stream 

struct increment;
class numeric_term;

const uint32_t PRECISION_STEP_DEF = 16;
const uint32_t PRECISION_STEP_32 = 8;

//////////////////////////////////////////////////////////////////////////////
/// @class numeric_token_stream
/// @brief token_stream implementation for numeric field. based on precision
///        step it produces several terms representing ranges of the input 
///        term
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API numeric_token_stream final
    : public token_stream,
      private util::noncopyable { // attrs_ non-copyable
 public:
  numeric_token_stream();
  numeric_token_stream(numeric_token_stream&& other) NOEXCEPT;

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }
  virtual bool next() override;

  void reset(int32_t value, uint32_t step = PRECISION_STEP_DEF);
  void reset(int64_t value, uint32_t step = PRECISION_STEP_DEF);

  #ifndef FLOAT_T_IS_DOUBLE_T
  void reset(float_t value, uint32_t step = PRECISION_STEP_DEF);
  #endif

  void reset(double_t value, uint32_t step = PRECISION_STEP_DEF);
  static bytes_ref value(bstring& buf, int32_t value);
  static bytes_ref value(bstring& buf, int64_t value);

  #ifndef FLOAT_T_IS_DOUBLE_T
    static bytes_ref value(bstring& buf, float_t value);
  #endif

  static bytes_ref value(bstring& buf, double_t value);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @class numeric_term
  /// @brief term_attribute implementation for numeric_token_stream
  //////////////////////////////////////////////////////////////////////////////
  class IRESEARCH_API numeric_term final: public term_attribute {
   public:
    static bytes_ref value(bstring& buf, int32_t value) {
      decltype(val_) val;

      val.i32 = value;

      return numeric_term::value(buf, NT_INT, val, 0);
    }

    static bytes_ref value(bstring& buf, int64_t value) {
      decltype(val_) val;

      val.i64 = value;

      return numeric_term::value(buf, NT_LONG, val, 0);
    }

#ifndef FLOAT_T_IS_DOUBLE_T
    static bytes_ref value(bstring& buf, float_t value) {
      decltype(val_) val;

      val.i32 = numeric_utils::numeric_traits<float_t>::integral(value);

      return numeric_term::value(buf, NT_FLOAT, val, 0);
    }
#endif

    static bytes_ref value(bstring& buf, double_t value) {
      decltype(val_) val;

      val.i64 = numeric_utils::numeric_traits<double_t>::integral(value);

      return numeric_term::value(buf, NT_DBL, val, 0);
    }

    bool next(increment& inc);

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

    IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
    bstring data_;
    value_t val_;
    NumericType type_;
    uint32_t step_;
    uint32_t shift_;
    IRESEARCH_API_PRIVATE_VARIABLES_END

    static irs::bytes_ref value(
      bstring& buf,
      NumericType type,
      value_t val,
      uint32_t shift
    );
  }; // numeric_term

  void init_attributes() {
    attrs_.emplace(num_);
    attrs_.emplace(inc_); // required by field_data::invert(...)
  }

  attribute_view attrs_;
  numeric_term num_;
  increment inc_;
}; // numeric_token_stream 

//////////////////////////////////////////////////////////////////////////////
/// @class null_token_stream
/// @brief token_stream implementation for null field, a single null term.
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API null_token_stream final
    : public token_stream,
      private util::noncopyable { // attrs_ non-copyable
 public:
  static const bytes_ref& value_null();

  null_token_stream();
  null_token_stream(null_token_stream&& other) NOEXCEPT;

  virtual bool next() override;

  void reset() { 
    in_use_ = false; 
  }

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return attrs_;
  }

 private:
  void init_attributes() {
    attrs_.emplace(term_);
    attrs_.emplace(inc_); // required by field_data::invert(...)
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  attribute_view attrs_;
  basic_term term_;
  increment inc_;
  bool in_use_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
};

NS_END

#endif