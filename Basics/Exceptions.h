////////////////////////////////////////////////////////////////////////////////
/// @brief basic exceptions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2011 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_JUTLAND_BASICS_EXCEPTIONS_H
#define TRIAGENS_JUTLAND_BASICS_EXCEPTIONS_H 1

#include "Basics/Common.h"

#include <errno.h>

#if BOOST_VERSION < 103600
#undef USE_BOOST_EXCEPTIONS
#else
#define USE_BOOST_EXCEPTIONS 1
#endif

#ifdef USE_BOOST_EXCEPTIONS
#if BOOST_VERSION < 104000
#include <boost/exception.hpp>
#include <boost/exception/info.hpp>
#else
#include <boost/exception/all.hpp>
#endif
#else
#include "Basics/StringUtils.h"
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                             boost
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

#if defined(USE_BOOST_EXCEPTIONS) && (BOOST_VERSION < 104000)

namespace boost {
  typedef error_info<struct errinfo_api_function_,char const *> errinfo_api_function;
  typedef error_info<struct errinfo_at_line_,int> errinfo_at_line;
  typedef error_info<struct errinfo_errno_,int> errinfo_errno;
  typedef error_info<struct errinfo_file_name_,std::string> errinfo_file_name;
  typedef error_info<struct errinfo_file_open_mode_,std::string> errinfo_file_open_mode;
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief diagnostic output
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define DIAGNOSTIC_INFORMATION(e) \
    boost::diagnostic_information(e)

#else

#define DIAGNOSTIC_INFORMATION(e) \
    e.diagnostic_information()

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an unqualified exception
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_TRIAGENS_ERROR(mesg, details)                           \
    do {                                                              \
      BOOST_THROW_EXCEPTION(triagens::basics::TriagensError()         \
                            << triagens::ErrorMessage(mesg)           \
                            << triagens::ErrorDetails(details));      \
      throw "this cannot happen";                                     \
    }                                                                 \
    while (0)

#else

#define THROW_TRIAGENS_ERROR(mesg, details)                           \
    throw triagens::basics::TriagensError(mesg, details)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for internal errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_INTERNAL_ERROR(a)                                            \
    do {                                                                   \
      BOOST_THROW_EXCEPTION(triagens::basics::InternalError()              \
                            << triagens::ErrorMessage(a)                   \
                            << boost::errinfo_at_line(__LINE__)            \
                            << boost::errinfo_file_name(__FILE__));        \
      throw "this cannot happen";                                          \
    }                                                                      \
    while (0)

#else

#define THROW_INTERNAL_ERROR(a)                                            \
    throw triagens::basics::InternalError(a)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for internal errors with line info
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_INTERNAL_ERROR_L(a, file, line)                              \
    do {                                                                   \
      BOOST_THROW_EXCEPTION(triagens::basics::InternalError()              \
                            << triagens::ErrorMessage(a)                   \
                            << boost::errinfo_at_line(line)                \
                            << boost::errinfo_file_name(file));            \
      throw "this cannot happen";                                          \
    }                                                                      \
    while (0)

#else

#define THROW_INTERNAL_ERROR_L(a, file, line)                              \
    throw triagens::basics::InternalError(a)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for out-of-memory errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_OUT_OF_MEMORY_ERROR()                                     \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::OutOfMemoryError());      \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_OUT_OF_MEMORY_ERROR()                     \
    throw triagens::basics::OutOfMemoryError()

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file open errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_FILE_OPEN_ERROR(func, file, mode)                          \
    do {                                                                 \
      BOOST_THROW_EXCEPTION(triagens::basics::FileError()                \
                            << triagens::ErrorMessage("file open error") \
                            << boost::errinfo_api_function(func)         \
                            << boost::errinfo_errno(errno)               \
                            << boost::errinfo_file_name(file)            \
                            << boost::errinfo_file_open_mode(mode));     \
      throw "this cannot happen";                                        \
    }                                                                    \
    while (0)

#else

#define THROW_FILE_OPEN_ERROR(func, file, mode)                          \
    throw triagens::basics::FileError("file open error", file)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_FILE_FUNC_ERROR(func, mesg)                               \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::FileError()               \
                            << triagens::ErrorMessage(mesg)             \
                            << boost::errinfo_api_function(func)        \
                            << boost::errinfo_errno(errno));            \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_FILE_FUNC_ERROR(func, mesg)                               \
    throw triagens::basics::FileError(func, mesg)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_FILE_FUNC_ERROR_E(func, mesg, error)                      \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::FileError()               \
                            << triagens::ErrorMessage(mesg)             \
                            << boost::errinfo_api_function(func)        \
                            << boost::errinfo_errno(error));            \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_FILE_FUNC_ERROR_E(func, mesg, error)                      \
    throw triagens::basics::FileError(func, mesg + (" " + triagens::basics::StringUtils::itoa(error)))

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for file errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_FILE_ERROR(mesg)                                          \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::FileError()               \
                            << triagens::ErrorMessage(mesg));           \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_FILE_ERROR(mesg)                                          \
    throw triagens::basics::FileError(mesg)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief rethrows an exception with filename
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define RETHROW_FILE_NAME(ex, file)                                       \
    do {                                                                  \
      ex << boost::errinfo_file_name(file);                               \
      throw ex;                                                           \
    } while (0)

#else

#define RETHROW_FILE_NAME(ex, file)                                       \
    throw ex

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parse errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_PARSE_ERROR(mesg)                                         \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::ParseError()              \
                            << triagens::ErrorMessage(mesg));           \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_PARSE_ERROR(mesg)                                         \
    throw triagens::basics::ParseError(mesg)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parse errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_PARSE_ERROR_L(line, mesg)                                 \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::ParseError()              \
                            << triagens::ErrorMessage(mesg)             \
                            << boost::errinfo_at_line(line));           \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_PARSE_ERROR_L(line, mesg)                                 \
    throw triagens::basics::ParseError(mesg, "line " + triagens::basics::StringUtils::itoa(line))

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parse errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_PARSE_ERROR_D(mesg, details)                              \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::ParseError()              \
                            << triagens::ErrorMessage(mesg)             \
                            << triagens::ErrorDetails(details));        \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_PARSE_ERROR_D(mesg, details)                              \
    throw triagens::basics::ParseError(mesg, details)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief rethrows an exception with line
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define RETHROW_LINE(ex, line)                                            \
    do {                                                                  \
      ex << boost::errinfo_at_line(line);                                 \
      throw ex;                                                           \
    } while (0)

#else

#define RETHROW_LINE(ex, line)                                            \
    throw ex

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an exception for parameter errors
///
/// Some compilers do not know that BOOST_THROW_EXCEPTION never returns.
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

#define THROW_PARAMETER_ERROR(param, func, mesg)                        \
    do {                                                                \
      BOOST_THROW_EXCEPTION(triagens::basics::ParameterError()          \
                            << triagens::ErrorMessage(mesg)             \
                            << ErrorParameterName(param)                \
                            << boost::errinfo_api_function(func));      \
      throw "this cannot happen";                                       \
    }                                                                   \
    while (0)

#else

#define THROW_PARAMETER_ERROR(param, func, mesg)                        \
    throw triagens::basics::ParameterError(mesg, param + string(" in ") + func)

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

namespace triagens {

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief message info
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS
  typedef boost::error_info<struct TagMessage, string> ErrorMessage;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief message details
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS
  typedef boost::error_info<struct TagDetails, string> ErrorDetails;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief parameter name
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS
  typedef boost::error_info<struct TagParameterName, string> ErrorParameterName;
#endif

  namespace basics {

////////////////////////////////////////////////////////////////////////////////
/// @brief base class for all errors
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

    class TriagensError : public virtual std::exception, public virtual boost::exception {
      public:
        ~TriagensError () throw() {
        }

      public:
        char const * what () const throw() {
          return "triagens error";
        }

        string description () const {
          return boost::diagnostic_information(*this);
        }
    };

#else

    class TriagensError : public virtual std::exception {
      public:
        TriagensError ()
          : message("unknown") {
        }

        TriagensError (string mesg)
          : message(mesg) {
        }

        TriagensError (string mesg, string details)
          : message(mesg + " (" + details + ")") {
        }

        ~TriagensError () throw() {
        }

      public:
        string diagnostic_information () const {
          return string(what()) + ": " + message;
        }

        string description () const {
          return diagnostic_information();
        }

        char const * what () const throw() {
          return "triagens error";
        }

        string const& details () const throw() {
          return message;
        }

      private:
        string const message;
    };

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for internal errors
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

    struct InternalError : public TriagensError {
      public:
        InternalError () {
        }

        InternalError (string const& what) {
          *this << ErrorMessage(what);
        }

        InternalError (std::exception const& ex) {
          *this << ErrorMessage(ex.what());
        }

      public:
        char const * what () const throw() {
          return "internal error";
        }
    };

#else

    struct InternalError : public TriagensError {
      public:
        InternalError () : TriagensError("internal error") {
        }

        InternalError (string mesg) : TriagensError(mesg) {
        }

        InternalError (std::exception const& ex) : TriagensError(ex.what()) {
        }

      public:
        char const * what () const throw() {
          return "internal error";
        }
    };

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for out-of-memory errors
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

    struct OutOfMemoryError : public TriagensError {
      public:
        char const * what () const throw() {
          return "out-of-memory error";
        }
    };

#else

    struct OutOfMemoryError : public TriagensError {
      public:
        OutOfMemoryError () : TriagensError("out-of-memory") {
        }

      public:
        char const * what () const throw() {
          return "out-of-memory error";
        }
    };

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for file errors
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

    struct FileError : public TriagensError {
      public:
        char const * what () const throw() {
          return "file error";
        }
    };

#else

    struct FileError : public TriagensError {
      public:
        FileError () : TriagensError("file error") {
        }

        FileError (string mesg) : TriagensError(mesg) {
        }

        FileError (string mesg, string details) : TriagensError(mesg, details) {
        }

      public:
        char const * what () const throw() {
          return "file error";
        }
    };

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for parse errors
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

    struct ParseError : public TriagensError {
      public:
        char const * what () const throw() {
          return "parse error";
        }
    };

#else

    struct ParseError : public TriagensError {
      public:
        ParseError () : TriagensError("parse error") {
        }

        ParseError (string mesg) : TriagensError(mesg) {
        }

        ParseError (string mesg, string details) : TriagensError(mesg, details) {
        }

      public:
        char const * what () const throw() {
          return "parse error";
        }
    };

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief exception for parameter errors
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

    struct ParameterError : public TriagensError {
      public:
        char const * what () const throw() {
          return "parameter error";
        }
    };

#else

    struct ParameterError : public TriagensError {
      public:
        ParameterError () : TriagensError("parameter error") {
        }

        ParameterError (string mesg) : TriagensError(mesg) {
        }

        ParameterError (string mesg, string details) : TriagensError(mesg, details) {
        }

      public:
        char const * what () const throw() {
          return "parameter error";
        }
    };

#endif

  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                             boost
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Utilities
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef USE_BOOST_EXCEPTIONS

/// @cond IGNORE

namespace boost {

#if BOOST_VERSION < 104300
#define TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE char const *
#else
#define TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE std::string
#endif

  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE triagens::ErrorMessage::tag_typeid_name() const {
    return "message";
  }



  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE triagens::ErrorDetails::tag_typeid_name() const {
    return "details";
  }



  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE boost::errinfo_errno::tag_typeid_name() const {
    return "errno";
  }



  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE boost::errinfo_file_name::tag_typeid_name() const {
    return "file name";
  }



  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE boost::errinfo_file_open_mode::tag_typeid_name() const {
    return "file open mode";
  }



  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE boost::errinfo_api_function::tag_typeid_name() const {
    return "api function";
  }



  template<>
  inline TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE boost::errinfo_at_line::tag_typeid_name() const {
    return "at line";
  }

#undef TRIAGENS_TAG_TYPEID_NAME_RETURN_TYPE

}

/// @endcond

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
