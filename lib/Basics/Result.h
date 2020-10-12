////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_RESULT_H
#define ARANGODB_BASICS_RESULT_H 1

#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>

namespace arangodb {
class Result final {
 public:
  Result(bool /*avoidCastingErrors*/) = delete;

  Result() noexcept(noexcept(std::allocator<char>()));

  // cppcheck-suppress noExplicitConstructor
  /* implicit */ Result(int errorNumber) noexcept(noexcept(std::allocator<char>()));

  Result(int errorNumber, std::string const& errorMessage);

  /**
   * @brief Construct with error number and message
   * @param  errorNumber   Said error number
   * @param  errorMessage  Said error message
   */
  Result(int errorNumber, std::string&& errorMessage) noexcept;

  Result(int errorNumber, std::string_view const& errorMessage);

  Result(int errorNumber, const char* errorMessage);

  /**
   * @brief Construct as copy
   * @param  other  To copy from
   */
  Result(Result const& other);

  /**
   * @brief Construct as clone
   * @param  other  The prototype
   */
  Result(Result&& other) noexcept;

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Refernce to ourselves
   */
  Result& operator=(Result const& other);

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Refernce to ourselves
   */
  Result& operator=(Result&& other) noexcept;

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
  Result& reset();

  /**
   * @brief  Reset to specific error number.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @return            Reference to ourselves
   */
  Result& reset(int errorNumber);

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  Result& reset(int errorNumber, std::string const& errorMessage);

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  Result& reset(int errorNumber, std::string&& errorMessage) noexcept;

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  Result& reset(Result const& other);

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  Result& reset(Result&& other) noexcept;

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

}  // namespace arangodb

/**
 * @brief  Print to output stream
 * @return Said output steam
 */
std::ostream& operator<<(std::ostream& out, arangodb::Result const& result);

#endif
