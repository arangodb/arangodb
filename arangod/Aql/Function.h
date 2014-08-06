////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, built-in function
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_AQL_FUNCTION_H
#define ARANGODB_AQL_FUNCTION_H 1

#include "Basics/Common.h"

namespace triagens {
  namespace aql {

    struct Function {

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

      Function () = delete;

      Function (std::string const& name,
                std::string const& arguments,
                bool isDeterministic)
        : name(name),
          arguments(arguments),
          isDeterministic(isDeterministic) {

        initArguments();
      }

      ~Function () {
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the function has been deprecated
////////////////////////////////////////////////////////////////////////////////

      inline bool isDeprecated () const {
        // currently there are no deprecated functions
        return false;
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the number of required arguments
////////////////////////////////////////////////////////////////////////////////

      inline std::pair<size_t, size_t> numArguments () const {
        return std::make_pair(minRequiredArguments, maxRequiredArguments);
      }

////////////////////////////////////////////////////////////////////////////////
/// @brief parse the argument list and set the minimum and maximum number of
/// arguments
////////////////////////////////////////////////////////////////////////////////

      void initArguments () {
        minRequiredArguments = maxRequiredArguments = 0;

        // setup some parsing state
        bool inOptional = false;
        bool foundArg   = false;

        char const* p = arguments.c_str();
        while (true) {
          char const c = *p++;

          switch (c) {
            case '\0':
              // end of argument list
              if (foundArg) {
                if (! inOptional) {
                  ++minRequiredArguments;
                }
                ++maxRequiredArguments;
              }
              return;

            case '|':
              // beginning of optional arguments
              TRI_ASSERT(! inOptional);
              if (foundArg) {
                ++minRequiredArguments;
                ++maxRequiredArguments;
              }
              inOptional = true;
              foundArg = false;
              break;

            case ',':
              // next argument
              TRI_ASSERT(foundArg);

              if (! inOptional) {
                ++minRequiredArguments;
              }
              ++maxRequiredArguments;
              foundArg = false;
              break;

            case '+':
              // repeated optional argument
              TRI_ASSERT(inOptional);
              maxRequiredArguments = MaxArguments;
              return;

            default:
              foundArg = true;
          }
        }
      }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief function name
////////////////////////////////////////////////////////////////////////////////

      std::string const    name;

////////////////////////////////////////////////////////////////////////////////
/// @brief function arguments
////////////////////////////////////////////////////////////////////////////////

      std::string const    arguments;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the function is deterministic (i.e. its results are
/// identical when called repeatedly with the same input values)
////////////////////////////////////////////////////////////////////////////////

      bool const           isDeterministic;

////////////////////////////////////////////////////////////////////////////////
/// @brief minimum number of required arguments
////////////////////////////////////////////////////////////////////////////////

      size_t               minRequiredArguments;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of required arguments
////////////////////////////////////////////////////////////////////////////////

      size_t               maxRequiredArguments;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of function arguments that can be used
////////////////////////////////////////////////////////////////////////////////

      static size_t const  MaxArguments = 1024;

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
