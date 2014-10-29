////////////////////////////////////////////////////////////////////////////////
/// @brief arango exceptions
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_UTILS_EXCEPTION_H
#define ARANGODB_UTILS_EXCEPTION_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"

#include <errno.h>

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION(code)                                           \
  throw triagens::arango::Exception(code, __FILE__, __LINE__)

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code and arbitrary 
/// arguments (to be inserted in printf-style manner)
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION_PARAMS(code, ...)                               \
  throw triagens::arango::Exception(code, triagens::arango::Exception::FillExceptionString(code, __VA_ARGS__), __FILE__, __LINE__, true)        

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an arango exception with an error code and an already-built
/// error message
////////////////////////////////////////////////////////////////////////////////

#define THROW_ARANGO_EXCEPTION_MESSAGE(code, message)                          \
  throw triagens::arango::Exception(code, message, __FILE__, __LINE__, false)

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

////////////////////////////////////////////////////////////////////////////////
/// @brief arango exception type
////////////////////////////////////////////////////////////////////////////////

    class Exception : public virtual std::exception {

      public:

        Exception (int code,
                   char const* file,
                   int line);
        
        Exception (int code,
                   std::string const& errorMessage,
                   char const* file,
                   int line,
                   bool errnoStringResolved);

        ~Exception () throw ();

      public:

        char const * what () const throw (); 

        std::string message () const throw () {
          return _errorMessage;
        }
        
        int code () const throw () {
          return _code;
        }
        
        void addToMessage(std::string More) {
          _errorMessage += More;
        }

        static std::string FillExceptionString (int, ...);

      protected:
        std::string       _errorMessage;
        char const*       _file;
        int const         _line;
        int const         _code;
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
