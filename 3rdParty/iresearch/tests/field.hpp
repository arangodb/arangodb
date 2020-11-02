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

#ifndef IRESEARCH_FIELD_H
#define IRESEARCH_FIELD_H

#include "analysis/token_streams.hpp"

namespace iresearch {

struct data_output;

//////////////////////////////////////////////////////////////////////////////
/// @class field 
/// @brief base class for field implementations
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API field {
 public:
  field() = default;
  virtual ~field();

  field(field&& rhs) noexcept;
  field& operator=(field&& rhs) noexcept;

  field(const field&) = default;
  field& operator=(const field&) = default;

  const std::string& name() const { return name_; }
  void name(std::string&& name) { name_ = std::move(name); }
  void name(const std::string& name) { name_ = name; }

  void boost(float_t value) { boost_ = value; }
  float_t boost() const { return boost_; }

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::string name_;
  float_t boost_{ 1.f };
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // field

//////////////////////////////////////////////////////////////////////////////
/// @class long_field 
/// @brief provides capabilities for storing & indexing int64_t values 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API long_field : public field, private util::noncopyable {
 public:
  typedef int64_t value_t;

  long_field() = default;
  long_field(long_field&& other) noexcept;

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(data_output& out) const;
  token_stream& get_tokens() const;
  const flags& features() const;

 private:
  mutable numeric_token_stream stream_;
  int64_t value_{};
}; // long_field 

//////////////////////////////////////////////////////////////////////////////
/// @class int_field
/// @brief provides capabilities for storing & indexing int32_t values 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API int_field : public field, private util::noncopyable {
 public:
  typedef int32_t value_t;

  int_field() = default;
  int_field(int_field&& other) noexcept;

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(data_output& out) const;
  token_stream& get_tokens() const;
  const flags& features() const;

 private:
  mutable numeric_token_stream stream_;
  int32_t value_{};
}; // int_field 

//////////////////////////////////////////////////////////////////////////////
/// @class double_field
/// @brief provides capabilities for storing & indexing double_t values 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API double_field : public field, private util::noncopyable {
 public:
  typedef double_t value_t;

  double_field() = default;
  double_field(double_field&& other) noexcept;

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(data_output& out) const;
  token_stream& get_tokens() const;
  const flags& features() const;

 private:
  mutable numeric_token_stream stream_;
  double_t value_{};
}; // double_field

//////////////////////////////////////////////////////////////////////////////
/// @class float_field
/// @brief provides capabilities for storing & indexing double_t values 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API float_field : public field, private util::noncopyable {
 public:
  typedef float_t value_t;

  float_field() = default;
  float_field(float_field&& other) noexcept;

  void value(value_t value) { value_ = value; }
  value_t value() const { return value_; }

  bool write(data_output& out) const;
  token_stream& get_tokens() const;
  const flags& features() const;

 private:
  mutable numeric_token_stream stream_;
  float_t value_{};
}; // float_field 

//////////////////////////////////////////////////////////////////////////////
/// @class string_field
/// @brief provides capabilities for storing & indexing string values 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API string_field : public field, private util::noncopyable {
 public:
  string_field() = default;
  string_field(string_field&& other) noexcept;

  const std::string& value() const { return value_; }
  void value(std::string&& value) { value_ = std::move(value); }
  void value(const std::string& value) { value_ = value; }

  template<typename Iterator>
  void value(Iterator first, Iterator last) {
    value_ = std::string(first, last);
  }
  
  bool write(data_output& out) const;
  token_stream& get_tokens() const;
  const flags& features() const;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  mutable string_token_stream stream_;
  std::string value_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // string_field

//////////////////////////////////////////////////////////////////////////////
/// @class binary_field 
/// @brief provides capabilities for storing & indexing binary values 
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API binary_field : public field, private util::noncopyable {
 public:
  binary_field() = default;
  binary_field(binary_field&& other) noexcept;

  const bstring& value() const { return value_; }
  void value(const bstring& value);
  void value(bstring&& value);

  template<typename Iterator>
  void value(Iterator first, Iterator last) {
    value(bstring(first, last));
  }
  
  bool write(data_output& out) const;
  token_stream& get_tokens() const;
  const flags& features() const;

 private:
  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  mutable string_token_stream stream_;
  bstring value_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // binary_field

}

#endif