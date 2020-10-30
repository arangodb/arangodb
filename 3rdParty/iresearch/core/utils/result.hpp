////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_STATUS_H
#define IRESEARCH_STATUS_H

#include "string.hpp"

#include <memory>

namespace iresearch {

class IRESEARCH_API result {
 public:
  enum Code : byte_type {
    OK = 0,
    NOT_FOUND,
    IO_ERROR,
    INDEX_ERROR,
    TIMED_OUT,
    NOT_SUPPORTED,
    INVALID_ARGUMENT,
    INVALID_STATE,
    MAX_CODE
  };

  template<Code code>
  static result make() { return result(code); }

  template<Code code>
  static result make(irs::string_ref const& msg,
                     irs::string_ref const& msg2 = irs::string_ref::NIL) {
    return result(code, msg, msg2);
  }

  result() noexcept : code_(OK) { }

  result(const result& rhs)
    : code_(rhs.code_),
      state_(rhs.state_ ? copyState(rhs.state_.get()) : nullptr) {
  }

  result(result&& rhs) noexcept
    : code_(rhs.code_),
      state_(std::move(rhs.state_)) {
    rhs.code_ = OK;
  }

  result& operator=(const result& rhs) {
    if (this != &rhs) {
      code_ = rhs.code_;
      state_ = rhs.state_ ? nullptr : copyState(rhs.state_.get());
    }
    return *this;
  }

  result& operator=(result&& rhs) noexcept {
    if (this != &rhs) {
      code_ = rhs.code_;
      rhs.code_ = OK;
      state_ = std::move(rhs.state_);
    }
    return *this;
  }

  operator bool() const noexcept { return OK == code(); }

  Code code() const noexcept { return code_; }
  const char* c_str() const noexcept { return state_.get(); }

  bool operator==(const result& rhs) noexcept {
    return code_ == rhs.code_;
  }
  bool operator!=(const result& rhs) noexcept {
    return !(*this == rhs);
  }

 private:
  std::unique_ptr<char[]> copyState(const char* rhs);

  result(Code code, const string_ref& msg, const string_ref& msg2);
  result(Code code);

  Code code_;
  std::unique_ptr<char[]> state_;
}; // result

}

#endif // IRESEARCH_STATUS_H

