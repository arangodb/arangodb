////////////////////////////////////////////////////////////////////////////////
/// @brief basic exceptions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_EXCEPTIONS_H
#define ARANGODB_BASICS_EXCEPTIONS_H 1

#include "Basics/Common.h"

#include <errno.h>

#include "Basics/StringUtils.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief diagnostic output
////////////////////////////////////////////////////////////////////////////////

#define DIAGNOSTIC_INFORMATION(e) \
  e.what()

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an unqualified exception
////////////////////////////////////////////////////////////////////////////////

#define THROW_TRIAGENS_ERROR(type, details)                                    \
  throw triagens::basics::TriagensError(type, details, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for internal errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_INTERNAL_ERROR(details)                                          \
  throw triagens::basics::InternalError(details, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for out-of-memory errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_OUT_OF_MEMORY_ERROR()                                            \
  throw triagens::basics::OutOfMemoryError(__FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file open errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_FILE_OPEN_ERROR(func, file, mode, error)                         \
  throw triagens::basics::FileError(func, "file open error", file, mode, error, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_FILE_FUNC_ERROR(func, details, error)                            \
  throw triagens::basics::FileError(func, details, "", "", error, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_FILE_ERROR(details, error)                                       \
  throw triagens::basics::FileError("<unknown>", details, "", "", error, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief rethrows an exception with filename
////////////////////////////////////////////////////////////////////////////////

#define RETHROW_FILE_NAME(ex, file)                                            \
  do {                                                                         \
    ex.setFilename(file);                                                      \
    throw ex;                                                                  \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parse errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_PARSE_ERROR(details)                                             \
  throw triagens::basics::ParseError(details, -1, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parse errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_PARSE_ERROR_L(details, line)                                     \
  throw triagens::basics::ParseError(details, line, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief rethrows an exception with line
////////////////////////////////////////////////////////////////////////////////

#define RETHROW_LINE(ex, line)                                                 \
  do {                                                                         \
    ex.setLineNumber(line);                                                    \
    throw ex;                                                                  \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parameter errors
////////////////////////////////////////////////////////////////////////////////

#define THROW_PARAMETER_ERROR(parameter, details, func)                        \
  throw triagens::basics::ParameterError(parameter, details, func, __FILE__, __LINE__)

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

namespace triagens {
  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for all errors
////////////////////////////////////////////////////////////////////////////////

    class TriagensError : public virtual std::exception {
      public:
        TriagensError (std::string const& type,
                       std::string const& details,
                       char const* file,
                       int line);

        ~TriagensError () throw ();

      public:
        char const * what () const throw();

      protected:
        std::string _message;
        std::string _type;
        std::string _details;
        std::string _file;
        int _line;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for internal errors
////////////////////////////////////////////////////////////////////////////////

    class InternalError : public TriagensError {
      public:
        InternalError (std::string const& details, char const* file, int line);
        InternalError (std::exception const& ex, char const* file, int line);
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for out-of-memory errors
////////////////////////////////////////////////////////////////////////////////

    class OutOfMemoryError : public TriagensError {
      public:
        OutOfMemoryError (char const* file, int line);
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for file errors
////////////////////////////////////////////////////////////////////////////////

    class FileError : public TriagensError {
      public:
        FileError (std::string const& func,
                   std::string const& details,
                   std::string const& filename,
                   std::string const& mode,
                   int error,
                   char const* file,
                   int line);

        ~FileError () throw ();

      public:
        void setFilename (std::string const&);

      protected:
        std::string _func;
        std::string _filename;
        std::string _mode;
        int _error;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for parse errors
////////////////////////////////////////////////////////////////////////////////

    struct ParseError : public TriagensError {
      public:
        ParseError (std::string const& details,
                    int lineNumber,
                    char const* file,
                    int line);

      public:
        void setLineNumber (int);

      protected:
        int _lineNumber;
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for parameter errors
////////////////////////////////////////////////////////////////////////////////

    struct ParameterError : public TriagensError {
      public:
        ParameterError (std::string const& parameter,
                        std::string const& details,
                        std::string const& func,
                        char const* file,
                        int line);

        ~ParameterError () throw ();

      protected:
        std::string _parameter;
        std::string _func;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
