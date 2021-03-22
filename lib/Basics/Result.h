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

#include "Basics/ErrorCode.h"
#include "Basics/ResultError.h"

namespace arangodb {
class Result final {
 public:
  Result(bool /*avoidCastingErrors*/) = delete;
  Result(int /*avoidCastingErrors*/) = delete;

  Result() noexcept = default;

  // cppcheck-suppress noExplicitConstructor
  /* implicit */ Result(ErrorCode errorNumber);

  Result(ErrorCode errorNumber, std::string const& errorMessage);

  /**
   * @brief Construct with error number and message
   * @param  errorNumber   Said error number
   * @param  errorMessage  Said error message
   */
  Result(ErrorCode errorNumber, std::string&& errorMessage);

  Result(ErrorCode errorNumber, std::string_view errorMessage);

  Result(ErrorCode errorNumber, const char* errorMessage);

  /**
   * @brief Construct as copy
   * @param  other  To copy from
   */
  Result(Result const& other);

  /**
   * @brief Construct as clone
   * @param  other  The prototype
   */
  Result(Result&& other) noexcept = default;

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Reference to ourselves
   */
  auto operator=(Result const& other) -> Result&;

  /**
   * @brief Assignment operator
   * @param  other  To assign from
   * @return        Reference to ourselves
   */
  auto operator=(Result&& other) noexcept -> Result& = default;

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
  [[nodiscard]] auto errorNumber() const noexcept -> ErrorCode;

  /**
   * @brief  Is specific error
   * @param errorNumber Said specific error
   * @return            Equality with specific error
   */
  [[nodiscard]] auto is(ErrorCode errorNumber) const noexcept -> bool;

  /**
   * @see is(int errorNumber)
   */
  [[nodiscard]] auto isNot(ErrorCode errorNumber) const noexcept -> bool;

  /**
   * @brief  Reset to ok, error message is cleared.
   * @return            Reference to ourselves
   */
  auto reset() noexcept -> Result&;

  /**
   * @brief  Reset to specific error number.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @return            Reference to ourselves
   */
  auto reset(ErrorCode errorNumber) -> Result&;

  /**
   * @brief  Reset to specific error number with message.
   *         If ok, error message is cleared.
   * @param errorNumber Said specific error number
   * @param errorMessage Said specific error message
   * @return            Reference to ourselves
   */
  auto reset(ErrorCode errorNumber, std::string_view errorMessage) -> Result&;
  auto reset(ErrorCode errorNumber, const char* errorMessage) -> Result&;
  auto reset(ErrorCode errorNumber, std::string&& errorMessage) -> Result&;

  /**
   * @brief  Reset to other error.
   * @param  other  Said specific error
   * @return        Reference to ourselves
   */
  auto reset(Result const& other) -> Result&;
  auto reset(Result&& other) noexcept -> Result&;

  /**
   * @brief  Get error message
   * @return Our error message
   */
  [[nodiscard]] auto errorMessage() const& noexcept -> std::string_view;
  [[nodiscard]] auto errorMessage() && noexcept -> std::string;

  template <typename F, std::enable_if_t<std::is_invocable_r_v<void, F, arangodb::result::Error&>, int> = 0>
  auto withError(F&& f) -> Result& {
    if (_error != nullptr) {
      std::forward<F>(f)(*_error);
    }

    return *this;
  }

  template <typename F, std::enable_if_t<std::is_invocable_r_v<arangodb::result::Error, F, arangodb::result::Error const&>, int> = 0>
  auto mapError(F&& f) -> Result {
    if (_error != nullptr) {
      return Result{errorNumber(), std::forward<F>(f)(*_error)};
    }

    return *this;
  }

 private:
  std::unique_ptr<arangodb::result::Error> _error = nullptr;
};

}  // namespace arangodb

/**
 * @brief  Print to output stream
 * @return Said output stream
 */
auto operator<<(std::ostream& out, arangodb::Result const& result) -> std::ostream&;

#endif
