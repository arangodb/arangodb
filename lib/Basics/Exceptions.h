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
#include "Basics/ResultT.h"
#include "Basics/SourceLocation.h"
#include "Basics/StringUtils.h"
#include "Basics/error.h"
#include "Basics/voc-errors.h"

/// @brief throws an arango exception with an error code
#define THROW_ARANGO_EXCEPTION(code) \
  throw ::arangodb::basics::Exception(code, ADB_HERE)

/// @brief throws an arango exception with an error code and arbitrary
/// arguments (to be inserted in printf-style manner)
#define THROW_ARANGO_EXCEPTION_PARAMS(code, ...) \
  throw ::arangodb::basics::Exception::createWithParams(ADB_HERE, code, __VA_ARGS__)

/// @brief throws an arango exception with an error code and arbitrary
/// arguments (to be inserted in printf-style manner)
#define THROW_ARANGO_EXCEPTION_FORMAT(code, format, ...) \
  throw ::arangodb::basics::Exception::createWithFormat(ADB_HERE, code, format, __VA_ARGS__)

/// @brief throws an arango exception with an error code and an already-built
/// error message
#define THROW_ARANGO_EXCEPTION_MESSAGE(code, message) \
  throw ::arangodb::basics::Exception(code, message, ADB_HERE)

namespace arangodb::basics {

/// @brief arango exception type
class Exception final : public virtual std::exception {
 public:
  static std::string FillExceptionString(ErrorCode, ...);
  static std::string FillFormatExceptionString(char const* format, ...);

 public:
  // primary constructor
  Exception(ErrorCode code, std::string&& errorMessage, SourceLocation location) noexcept;

  // convenience constructors
  Exception(ErrorCode code, SourceLocation location);
  Exception(Result const&, SourceLocation location);
  Exception(Result&&, SourceLocation location) noexcept;
  Exception(ErrorCode code, std::string_view errorMessage, SourceLocation location);
  Exception(ErrorCode code, char const* errorMessage, SourceLocation location);

  // I think we should get rid of the following (char*,int) versions as opposed
  // to the SourceLocation ones, to keep it a little clearer.
  Exception(ErrorCode code, char const* file, int line);
  Exception(Result const&, char const* file, int line);
  Exception(Result&&, char const* file, int line) noexcept;
  Exception(ErrorCode code, std::string_view errorMessage, char const* file, int line);
  Exception(ErrorCode code, std::string&& errorMessage, char const* file, int line) noexcept;
  Exception(ErrorCode code, char const* errorMessage, char const* file, int line);

  ~Exception() override = default;
  Exception(Exception const&) = default;
  Exception(Exception&&) = default;

  template <typename... Args>
  static Exception createWithParams(SourceLocation location, ErrorCode code, Args... args) {
    return Exception(code, ::arangodb::basics::Exception::FillExceptionString(code, args...),
                     location);
  }
  template <typename... Args>
  static Exception createWithFormat(SourceLocation location, ErrorCode code,
                                    const char* fmt, Args... args) {
    auto const errnoStr = TRI_errno_string(code);
    // TODO Move this into the .cpp file to avoid including StringUtils. To do
    //      that, make this into a C-style variadic function, rather than a
    //      variadic template.
    auto message = StringUtils::concatT(
        errnoStr, ": ",
        ::arangodb::basics::Exception::FillFormatExceptionString(fmt, args...));
    return Exception(code, std::move(message), location);
  }

 public:
  [[nodiscard]] char const* what() const noexcept override;
  [[nodiscard]] std::string const& message() const noexcept;
  [[nodiscard]] ErrorCode code() const noexcept;

 private:
  void appendLocation() noexcept;

 protected:
  // TODO Because _errorMessage is a string, Exception is not
  //      nothrow-copy-constructible. Maybe it'd be better to use a
  //      shared_ptr<string> instead?
  std::string _errorMessage;
  SourceLocation _location;
  ErrorCode const _code;
};

namespace helper {
// just so we don't have to include logger and application-exit into this central header.
[[noreturn]] void dieWithLogMessage(char const*);
}  // namespace helper

template <typename F>
[[nodiscard]] auto catchToResult(F&& fn) noexcept -> Result {
  Result result{TRI_ERROR_NO_ERROR};
  // The outer try/catch catches possible exceptions thrown by result.reset(),
  // due to allocation failure. If we don't have enough memory to allocate an
  // error, let's just give up.
  try {
    try {
      result = std::forward<F>(fn)();
      // TODO check whether there are other specific exceptions we should catch
    } catch (arangodb::basics::Exception const& e) {
      result.reset(e.code(), e.message());
    } catch (std::bad_alloc const&) {
      result.reset(TRI_ERROR_OUT_OF_MEMORY);
    } catch (std::exception const& e) {
      result.reset(TRI_ERROR_INTERNAL, e.what());
    } catch (...) {
      result.reset(TRI_ERROR_INTERNAL);
    }
  } catch (std::exception const& e) {
    helper::dieWithLogMessage(e.what());
  } catch (...) {
    helper::dieWithLogMessage(nullptr);
  }
  return result;
}

template <typename F, typename T = std::invoke_result_t<F>>
[[nodiscard]] auto catchToResultT(F&& fn) noexcept -> ResultT<T> {
  Result result{TRI_ERROR_NO_ERROR};
  // The outer try/catch catches possible exceptions thrown by result.reset(),
  // due to allocation failure. If we don't have enough memory to allocate an
  // error, let's just give up.
  try {
    try {
      result = std::forward<F>(fn)();
      // TODO check whether there are other specific exceptions we should catch
    } catch (arangodb::basics::Exception const& e) {
      result.reset(e.code(), e.message());
    } catch (std::bad_alloc const&) {
      result.reset(TRI_ERROR_OUT_OF_MEMORY);
    } catch (std::exception const& e) {
      result.reset(TRI_ERROR_INTERNAL, e.what());
    } catch (...) {
      result.reset(TRI_ERROR_INTERNAL);
    }
  } catch (std::exception const& e) {
    helper::dieWithLogMessage(e.what());
  } catch (...) {
    helper::dieWithLogMessage(nullptr);
  }
  return result;
}

template <typename F>
[[nodiscard]] auto catchVoidToResult(F&& fn) noexcept -> Result {
  auto wrapped = [&fn]() -> Result {
    std::forward<F>(fn)();
    return Result{TRI_ERROR_NO_ERROR};
  };
  return catchToResult(wrapped);
}

namespace helper {
// just so we don't have to include logger into this header
[[noreturn]] void logAndAbort(char const* what);
}

// @brief Throws the passed exception, but in maintainer mode, logs the error
// and aborts instead.
template <typename E>
[[noreturn]] void abortOrThrowException(E&& e) {
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  throw std::forward<E>(e);
#else
  helper::logAndAbort(e.what());
#endif
}

// @brief Forwards arguments to an Exception constructor and calls
// abortOrThrowException
template <typename... Args>
[[noreturn]] void abortOrThrow(Args... args) {
  abortOrThrowException(Exception(std::forward<Args>(args)...));
}

}  // namespace arangodb::basics
