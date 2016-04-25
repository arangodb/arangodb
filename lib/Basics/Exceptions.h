////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_BASICS_EXCEPTIONS_H
#define ARANGODB_BASICS_EXCEPTIONS_H 1

#include "Basics/Common.h"

#include <errno.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief diagnostic output
////////////////////////////////////////////////////////////////////////////////

#define DIAGNOSTIC_INFORMATION(e) e.what()

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION(code) \
  throw arangodb::basics::Exception(code, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code and arbitrary
/// arguments (to be inserted in printf-style manner)
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION_PARAMS(code, ...)                           \
  throw arangodb::basics::Exception(                                       \
      code,                                                                \
      arangodb::basics::Exception::FillExceptionString(code, __VA_ARGS__), \
      __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code and arbitrary
/// arguments (to be inserted in printf-style manner)
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION_FORMAT(code, format, ...)             \
  throw arangodb::basics::Exception(                                 \
      code, arangodb::basics::Exception::FillFormatExceptionString(  \
                "%s: " format, TRI_errno_string(code), __VA_ARGS__), \
      __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code and an already-built
/// error message
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION_MESSAGE(code, message) \
  throw arangodb::basics::Exception(code, message, __FILE__, __LINE__)

namespace arangodb {
namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief arango exception type
////////////////////////////////////////////////////////////////////////////////

class Exception : public virtual std::exception {
 public:
  static std::string FillExceptionString(int, ...);
  static std::string FillFormatExceptionString(char const* format, ...);
  static void SetVerbose(bool);

 public:
  Exception(int code, char const* file, int line);

  Exception(int code, std::string const& errorMessage, char const* file,
            int line);

  Exception(int code, char const* errorMessage, char const* file, int line);

  ~Exception() throw();

 public:
  char const* what() const throw() override;
  std::string message() const throw();
  int code() const throw();
  void addToMessage(std::string const&);
 private:
  void appendLocation ();

 protected:
  std::string _errorMessage;
  char const* _file;
  int const _line;
  int const _code;
};
}
}

#endif
