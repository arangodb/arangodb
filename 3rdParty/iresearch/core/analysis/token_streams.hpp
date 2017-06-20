//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_TOKEN_STREAMS_H
#define IRESEARCH_TOKEN_STREAMS_H

#include "token_stream.hpp"
#include "utils/numeric_utils.hpp"

NS_ROOT

struct offset;
class basic_term;

//////////////////////////////////////////////////////////////////////////////
/// @class null_token_stream
/// @brief token_stream implementation for boolean field, a single bool term.
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API boolean_token_stream final:
  public token_stream, private util::noncopyable { // attrs_ non-copyable
 public:
  explicit boolean_token_stream(bool value = false);
  boolean_token_stream(boolean_token_stream&& other) NOEXCEPT;

  virtual bool next() override;
  void reset(bool value) { 
    value_ = value;
    in_use_ = false; 
  }
  virtual const attribute_store& attributes() const NOEXCEPT override {
    return attrs_;
  }
  static const bytes_ref& value_false();
  static const bytes_ref& value_true();
 private:
  attribute_store attrs_;
  basic_term* term_;
  bool in_use_;
  bool value_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class string_token_stream 
/// @brief basic implementation of token_stream for simple string field.
///        it does not tokenize or analyze field, just set attributes based
///        on initial string length 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API string_token_stream final:
  public token_stream, private util::noncopyable { // attrs_ non-copyable
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

  virtual const attribute_store& attributes() const NOEXCEPT override {
    return attrs_;
  }

 private:
  attribute_store attrs_;
  offset* offset_;
  basic_term* term_;
  bytes_ref value_;
  bool in_use_;
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
class IRESEARCH_API numeric_token_stream final:
  public token_stream, private util::noncopyable { // attrs_ non-copyable
 public:
  numeric_token_stream();
  numeric_token_stream(numeric_token_stream&& other) NOEXCEPT;

  virtual const attribute_store& attributes() const NOEXCEPT override;
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
  attribute_store attrs_;
  numeric_term* num_;
  increment* inc_;
}; // numeric_token_stream 

//////////////////////////////////////////////////////////////////////////////
/// @class null_token_stream
/// @brief token_stream implementation for null field, a single null term.
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API null_token_stream final:
  public token_stream, private util::noncopyable { // attrs_ non-copyable
 public:
  null_token_stream();
  null_token_stream(null_token_stream&& other) NOEXCEPT;

  virtual bool next() override;
  void reset() { 
    in_use_ = false; 
  }
  virtual const attribute_store& attributes() const NOEXCEPT override {
    return attrs_;
  }
  static const bytes_ref& value_null();

 private:
  attribute_store attrs_;
  basic_term* term_;
  bool in_use_;
};

NS_END

#endif