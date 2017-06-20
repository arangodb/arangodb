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

#include "shared.hpp"
#include "token_streams.hpp"
#include "token_attributes.hpp"
#include "utils/bit_utils.hpp"

NS_LOCAL

iresearch::bstring const& value_empty() {
  static const iresearch::bstring value(0, iresearch::byte_type(0));
  return value;
}

iresearch::bstring const& value_false() {
  static const iresearch::bstring value(1, iresearch::byte_type(0));
  return value;
}

iresearch::bstring const& value_true() {
  static const iresearch::bstring value(1, iresearch::byte_type(~0));
  return value;
}

NS_END

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @class basic_term 
/// @brief basic term_attribute implementation for string_token_stream
//////////////////////////////////////////////////////////////////////////////
class basic_term final : public term_attribute {
 public:
  DECLARE_FACTORY_DEFAULT();
  basic_term(): value_(nullptr) {}

  virtual void clear() override {
    value_ = nullptr;
  }

  void value( const bytes_ref* value ) {
    value_ = value;
  }

  virtual const bytes_ref& value() const {
    return nullptr == value_ ? bytes_ref::nil : *value_;
  }

 private:
  const bytes_ref* value_;
}; // basic_term
DEFINE_FACTORY_DEFAULT(basic_term);

// -----------------------------------------------------------------------------
// --SECTION--                               boolean_token_stream implementation
// -----------------------------------------------------------------------------

boolean_token_stream::boolean_token_stream(bool value /*= false*/) 
  : attrs_(1), // increment
    in_use_(false), 
    value_(value) {
  term_ = attrs_.emplace<basic_term>().get();
  attrs_.emplace<increment>(); // required by field_data::invert(...)
}

boolean_token_stream::boolean_token_stream(
  boolean_token_stream&& other
) NOEXCEPT
  : attrs_(std::move(other.attrs_)),
    term_(std::move(other.term_)),
    in_use_(std::move(other.in_use_)),
    value_(std::move(other.value_)) {
}

bool boolean_token_stream::next() {
  if (in_use_) {
    return false;
  }

  in_use_ = true;
  term_->value(value_ ? &value_true() : &value_false());

  return true;
}

/*static*/ const bytes_ref& boolean_token_stream::value_false() {
  static const bytes_ref value(::value_false());

  return value;
}

/*static*/ const bytes_ref& boolean_token_stream::value_true() {
  static const bytes_ref value(::value_true());

  return value;
}

// -----------------------------------------------------------------------------
// --SECTION--                                string_token_stream implementation
// -----------------------------------------------------------------------------

string_token_stream::string_token_stream()
  : attrs_(3), // offset + basic_term + increment
    in_use_(false) {
  offset_ = attrs_.emplace<offset>().get();
  term_ = attrs_.emplace<basic_term>().get();
  attrs_.emplace<increment>();
}

string_token_stream::string_token_stream(string_token_stream&& other) NOEXCEPT
  : attrs_(std::move(other.attrs_)),
    offset_(std::move(other.offset_)),
    term_(std::move(other.term_)),
    value_(std::move(other.value_)),
    in_use_(std::move(other.in_use_)) {
}

bool string_token_stream::next() {
  if (in_use_) {
    return false;
  }

  term_->value(&value_);
  offset_->start = 0;
  offset_->end = static_cast<uint32_t>(value_.size());
  return (in_use_ = true);
}

//////////////////////////////////////////////////////////////////////////////
/// @class numeric_term 
/// @brief term_attribute implementation for numeric_token_stream  
//////////////////////////////////////////////////////////////////////////////
class numeric_term final : public term_attribute {
 public:
  DECLARE_FACTORY_DEFAULT();

  bool next(increment& inc) {
    auto bits = type_ > NT_DBL
        ? bits_required<int32_t>()
        : bits_required<int64_t>();

    if (shift_ >= bits) {
      return false;
    }

    data_ref_ = value(data_, type_, val_, shift_);
    shift_ += step_;
    inc.value = step_ == shift_ ? 1 : 0;

    return true;
  }

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

  virtual void clear() override {
    data_ref_ = bytes_ref::nil;
  }

  virtual const bytes_ref& value() const override {
    return data_ref_;
  }

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

 private:
  enum NumericType { NT_LONG = 0, NT_DBL, NT_INT, NT_FLOAT };

  bstring data_;
  bytes_ref data_ref_;
  union {
    uint64_t i64;
    uint32_t i32;
  } val_;
  NumericType type_;
  uint32_t step_;
  uint32_t shift_;

  static bytes_ref value(
      bstring& buf,
      NumericType type,
      decltype(val_) val,
      uint32_t shift
  ) {
    switch (type) {
     case NT_LONG: {
      typedef numeric_utils::numeric_traits<int64_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i64, &(buf[0]), shift));
     }
     case NT_DBL: {
      typedef numeric_utils::numeric_traits<double_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i64, &(buf[0]), shift));
     }
     case NT_INT: {
      typedef numeric_utils::numeric_traits<int32_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i32, &(buf[0]), shift));
     }
     case NT_FLOAT: { 
      typedef numeric_utils::numeric_traits<float_t> traits_t;
      oversize(buf, traits_t::size());

      return bytes_ref(&(buf[0]), traits_t::encode(val.i32, &(buf[0]), shift));
     }
    }

    return bytes_ref::nil;
  }
}; // numeric_term
DEFINE_FACTORY_DEFAULT(numeric_term);

// -----------------------------------------------------------------------------
// --SECTION--                                       numeric_term implementation
// -----------------------------------------------------------------------------

numeric_token_stream::numeric_token_stream() 
  : attrs_(2) { // numeric_term + increment
  num_ = attrs_.emplace<numeric_term>().get();
  inc_ = attrs_.emplace<increment>().get();
}

numeric_token_stream::numeric_token_stream(
  numeric_token_stream&& other
) NOEXCEPT
  : attrs_(std::move(other.attrs_)),
    num_(std::move(other.num_)), 
    inc_(std::move(other.inc_)) {
}

const attribute_store& numeric_token_stream::attributes() const NOEXCEPT {
  return attrs_;
}

bool numeric_token_stream::next() {
  return num_->next(*inc_);
}

void numeric_token_stream::reset(
    int32_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_->reset(value, step); 
}

void numeric_token_stream::reset(
    int64_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_->reset(value, step); 
}

#ifndef FLOAT_T_IS_DOUBLE_T
void numeric_token_stream::reset(
    float_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_->reset(value, step); 
}
#endif

void numeric_token_stream::reset(
    double_t value, 
    uint32_t step /* = PRECISION_STEP_DEF */) { 
  num_->reset(value, step); 
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

null_token_stream::null_token_stream():
  attrs_(2), // basic_term + increment
  in_use_(false) {
  term_ = attrs_.emplace<basic_term>().get();
  attrs_.emplace<increment>(); // required by field_data::invert(...)
}

null_token_stream::null_token_stream(null_token_stream&& other) NOEXCEPT
  : attrs_(std::move(other.attrs_)),
    term_(std::move(other.term_)),
    in_use_(std::move(other.in_use_)) {
}

bool null_token_stream::next() {
  if (in_use_) {
    return false;
  }

  in_use_ = true;
  term_->value(&value_null());

  return true;
}

/*static*/ const bytes_ref& null_token_stream::value_null() {
  // data pointer != nullptr or assert failure in bytes_hash::insert(...)
  static const bytes_ref value(::value_empty());

  return value;
}

NS_END