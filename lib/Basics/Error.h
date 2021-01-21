////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

// Note that error.h uses ARANGODB_BASICS_ERROR_H!
#ifndef ARANGODB_BASICS_RESULT_ERROR_H
#define ARANGODB_BASICS_RESULT_ERROR_H

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

namespace arangodb {

// arangodb::Error is used in arangodb::Result

class Error final {
 public:
  Error(bool /*avoidCastingErrors*/) = delete;

  Error() noexcept(noexcept(std::allocator<char>()));

  // cppcheck-suppress noExplicitConstructor
  /* implicit */ Error(int errorNumber) noexcept(noexcept(std::allocator<char>()));

  Error(int errorNumber, std::string const& errorMessage);

  /**
   * @brief Construct with error number and message
   * @param  errorNumber   Said error number
   * @param  errorMessage  Said error message
   */
  Error(int errorNumber, std::string&& errorMessage) noexcept;

  Error(int errorNumber, std::string_view const& errorMessage);

  Error(int errorNumber, const char* errorMessage);

  /**
   * @brief Construct as copy
   * @param  other  To copy from
   */
  Error(Error const& other);

  /**
   * @brief Construct as clone
   * @param  other  The prototype
   */
  Error(Error&& other) noexcept;

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Refernce to ourselves
   */
  Error& operator=(Error const& other);

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Refernce to ourselves
   */
  Error& operator=(Error&& other) noexcept;

 public:
  /**
   * @brief  Nomen est omen
   * @return OK?
   */
  bool ok() const noexcept;

  /**
   * @see ok()
   */
  bool fail() const noexcept;

  /**
   * @brief  Get error number
   * @return error number
   */
  int errorNumber() const noexcept;

  /**
   * @brief  Is specific error
   * @param errorNumber Said specific error
   * @return            Equality with specific error
   */
  bool is(int errorNumber) const noexcept;

  /**
   * @see is(int errorNumber)
   */
  bool isNot(int errorNumber) const;

  /**
   * @brief  Reset to ok, error message is cleared.
   * @return            Reference to ourselves
   */
  Error& reset();

  /**
   * @brief  Reset to specific error number.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @return            Reference to ourselves
   */
  Error& reset(int errorNumber);

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  Error& reset(int errorNumber, std::string const& errorMessage);

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  Error& reset(int errorNumber, std::string&& errorMessage) noexcept;

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  Error& reset(Error const& other);

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  Error& reset(Error&& other) noexcept;

  /**
   * @brief  Get error message
   * @return Our error message
   */
  std::string errorMessage() const&;

  /**
   * @brief  Get error message
   * @return Our error message
   */
  std::string errorMessage() &&;

  template <typename S>
  void resetErrorMessage(S&& msg) {
    _errorMessage.assign(std::forward<S>(msg));
  }

  template <typename S>
  void appendErrorMessage(S&& msg) {
    if (_errorMessage.empty() && fail()) {
      _errorMessage.append(errorMessage());
    }
    _errorMessage.append(std::forward<S>(msg));
  }

 private:
  int _errorNumber;
  std::string _errorMessage;
};

}

#endif  // ARANGODB_BASICS_RESULT_ERROR_H
