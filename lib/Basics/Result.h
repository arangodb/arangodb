////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/Error.h"

namespace arangodb {
class Result final {
 public:
  Result(bool /*avoidCastingErrors*/) = delete;

  Result() noexcept = default;

  // cppcheck-suppress noExplicitConstructor
  /* implicit */ Result(int errorNumber);

  Result(int errorNumber, std::string const& errorMessage);

  /**
   * @brief Construct with error number and message
   * @param  errorNumber   Said error number
   * @param  errorMessage  Said error message
   */
  Result(int errorNumber, std::string&& errorMessage);

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
   * @return        Refernece to ourselves
   */
  auto operator=(Result const& other) -> Result&;

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Reference to ourselves
   */
  auto operator=(Result&& other) noexcept -> Result&;

 public:
  /**
   * @brief  Nomen est omen
   * @return OK?
   */
  [[nodiscard]] auto ok() const noexcept -> bool;

  /**
   * @see ok()
   */
  [[nodiscard]] auto fail() const noexcept -> bool;

  /**
   * @brief  Get error number
   * @return error number
   */
  [[nodiscard]] auto errorNumber() const noexcept -> int;

  /**
   * @brief  Is specific error
   * @param errorNumber Said specific error
   * @return            Equality with specific error
   */
  [[nodiscard]] auto is(int errorNumber) const noexcept -> bool;

  /**
   * @see is(int errorNumber)
   */
  [[nodiscard]] auto isNot(int errorNumber) const -> bool;

  /**
   * @brief  Reset to ok, error message is cleared.
   * @return            Reference to ourselves
   */
  auto reset() -> Result&;

  /**
   * @brief  Reset to specific error number.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @return            Reference to ourselves
   */
  auto reset(int errorNumber) -> Result&;

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  auto reset(int errorNumber, std::string const& errorMessage) -> Result&;

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  auto reset(int errorNumber, std::string&& errorMessage) noexcept -> Result&;

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  auto reset(Result const& other) -> Result&;

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  auto reset(Result&& other) noexcept -> Result&;

  /**
   * @brief  Get error message
   * @return Our error message
   */
  [[nodiscard]] auto errorMessage() const& -> std::string;

  /**
   * @brief  Get error message
   * @return Our error message
   */
  auto errorMessage() && -> std::string;

  template <typename S>
  void resetErrorMessage(S&& msg) {
    // TODO This message doesn't make sense any longer, remove it
    if (_error != nullptr) {
      _error->resetErrorMessage(std::forward<S>(msg));
    }
  }

  template <typename S>
  void appendErrorMessage(S&& msg) {
    // TODO This message doesn't make sense any longer, remove it
    _error->appendErrorMessage(std::forward<S>(msg));
  }

 private:
  std::unique_ptr<arangodb::result::Error> _error = nullptr;
};

}  // namespace arangodb

/**
 * @brief  Print to output stream
 * @return Said output stream
 */
std::ostream& operator<<(std::ostream& out, arangodb::Result const& result);

#endif
