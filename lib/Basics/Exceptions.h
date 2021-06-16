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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <exception>
#include <new>
#include <string>
#include <utility>

#include "Basics/Result.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"

/// @brief throws an arango exception with an error code
#define THROW_ARANGO_EXCEPTION(code) \
  throw ::arangodb::basics::Exception(code, __FILE__, __LINE__)

/// @brief throws an arango exception with an error code and arbitrary
/// arguments (to be inserted in printf-style manner)
#define THROW_ARANGO_EXCEPTION_PARAMS(code, ...)                                   \
  throw arangodb::basics::Exception(                                               \
      code, ::arangodb::basics::Exception::FillExceptionString(code, __VA_ARGS__), \
      __FILE__, __LINE__)

/// @brief throws an arango exception with an error code and arbitrary
/// arguments (to be inserted in printf-style manner)
#define THROW_ARANGO_EXCEPTION_FORMAT(code, format, ...)                                          \
  do {                                                                                            \
    auto const errnoStr = TRI_errno_string(code);                                                 \
    throw ::arangodb::basics::Exception(code,                                                     \
                                        ::arangodb::basics::Exception::FillFormatExceptionString( \
                                            "%*s: " format, errnoStr.size(),                      \
                                            errnoStr.data(), __VA_ARGS__),                        \
                                        __FILE__, __LINE__);                                      \
  } while (0)

/// @brief throws an arango exception with an error code and an already-built
/// error message
#define THROW_ARANGO_EXCEPTION_MESSAGE(code, message) \
  throw ::arangodb::basics::Exception(code, message, __FILE__, __LINE__)

/// @brief throws an arango result if the result fails
#define THROW_ARANGO_EXCEPTION_IF_FAIL(expression)                                          \
  do {                                                                                      \
    auto&& expressionResult = (expression);                                                 \
    if (expressionResult.fail()) {                                                          \
      throw ::arangodb::basics::Exception(std::move(expressionResult), __FILE__, __LINE__); \
    }                                                                                       \
  } while (0)

// Fix MSVC's preprocessor...
#define EXPAND_(args) args

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define THROW_AND_CATCH_(THROW_MACRO, ...)                                  \
  do {                                                                      \
    try {                                                                   \
      EXPAND_(THROW_MACRO(__VA_ARGS__));                                    \
    } catch (::arangodb::basics::Exception const& ex) {                     \
      LOG_TOPIC("fa7a1", FATAL, ::arangodb::Logger::CRASH) << ex.message(); \
      TRI_ASSERT(false);                                                    \
    } catch (...) {                                                         \
      TRI_ASSERT(false);                                                    \
    }                                                                       \
  } while (0)

#define ASSERT_OR_THROW_ARANGO_EXCEPTION(...) \
  THROW_AND_CATCH_(THROW_ARANGO_EXCEPTION, __VA_ARGS__)

#define ASSERT_OR_THROW_ARANGO_EXCEPTION_PARAMS(...) \
  THROW_AND_CATCH_(THROW_ARANGO_EXCEPTION_PARAMS, __VA_ARGS__)

#define ASSERT_OR_THROW_ARANGO_EXCEPTION_FORMAT(...) \
  THROW_AND_CATCH_(THROW_ARANGO_EXCEPTION_FORMAT, __VA_ARGS__)

#define ASSERT_OR_THROW_ARANGO_EXCEPTION_MESSAGE(...) \
  THROW_AND_CATCH_(THROW_ARANGO_EXCEPTION_MESSAGE, __VA_ARGS__)

#define ASSERT_OR_THROW_ARANGO_EXCEPTION_IF_FAIL(...) \
  THROW_AND_CATCH_(THROW_ARANGO_EXCEPTION_IF_FAIL, __VA_ARGS__)

#else

#define ASSERT_OR_THROW_ARANGO_EXCEPTION(...) \
  EXPAND_(THROW_ARANGO_EXCEPTION(__VA_ARGS__))
#define ASSERT_OR_THROW_ARANGO_EXCEPTION_PARAMS(...) \
  EXPAND_(THROW_ARANGO_EXCEPTION_PARAMS(__VA_ARGS__))
#define ASSERT_OR_THROW_ARANGO_EXCEPTION_FORMAT(...) \
  EXPAND_(THROW_ARANGO_EXCEPTION_FORMAT(__VA_ARGS__))
#define ASSERT_OR_THROW_ARANGO_EXCEPTION_MESSAGE(...) \
  EXPAND_(THROW_ARANGO_EXCEPTION_MESSAGE(__VA_ARGS__))
#define ASSERT_OR_THROW_ARANGO_EXCEPTION_IF_FAIL(...) \
  EXPAND_(THROW_ARANGO_EXCEPTION_IF_FAIL(__VA_ARGS__))

#endif

namespace arangodb::basics {

/// @brief arango exception type
class Exception final : public virtual std::exception {
 public:
  static std::string FillExceptionString(ErrorCode, ...);
  static std::string FillFormatExceptionString(char const* format, ...);

 public:
  Exception(ErrorCode code, char const* file, int line);
  Exception(Result const&, char const* file, int line);
  Exception(Result&&, char const* file, int line);

  Exception(ErrorCode code, std::string_view errorMessage, char const* file, int line);

  Exception(ErrorCode code, std::string&& errorMessage, char const* file, int line);

  Exception(ErrorCode code, char const* errorMessage, char const* file, int line);

  ~Exception() override = default;

 public:
  [[nodiscard]] char const* what() const noexcept override;
  [[nodiscard]] std::string const& message() const noexcept;
  [[nodiscard]] ErrorCode code() const noexcept;
  void addToMessage(std::string const&);

 private:
  void appendLocation() noexcept;

 protected:
  std::string _errorMessage;
  char const* _file;
  int const _line;
  ErrorCode const _code;
};

template <typename F>
Result catchToResult(F&& fn, ErrorCode defaultError = TRI_ERROR_INTERNAL) {
  // TODO check whether there are other specific exceptions we should catch
  Result result{TRI_ERROR_NO_ERROR};
  try {
    result = std::forward<F>(fn)();
  } catch (arangodb::basics::Exception const& e) {
    result.reset(e.code(), e.message());
  } catch (std::bad_alloc const&) {
    result.reset(TRI_ERROR_OUT_OF_MEMORY);
  } catch (std::exception const& e) {
    result.reset(defaultError, e.what());
  } catch (...) {
    result.reset(defaultError);
  }
  return result;
}

template <typename F>
Result catchVoidToResult(F&& fn, ErrorCode defaultError = TRI_ERROR_INTERNAL) {
  auto wrapped = [&fn]() -> Result {
    std::forward<F>(fn)();
    return Result{TRI_ERROR_NO_ERROR};
  };
  return catchToResult(wrapped, defaultError);
}

}  // namespace arangodb::basics
