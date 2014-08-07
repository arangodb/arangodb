////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, built-in AQL function
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

#include "Aql/Function.h"
#include "Utils/Exception.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the function
////////////////////////////////////////////////////////////////////////////////
      
Function::Function (std::string const& name,
                    std::string const& arguments,
                    bool isDeterministic)
  : name(name),
    arguments(arguments),
    isDeterministic(isDeterministic),
    containsCollectionParameter(false) {

  initArguments();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the function
////////////////////////////////////////////////////////////////////////////////

Function::~Function () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not a positional argument needs to be converted from a
/// collection parameter to a collection name parameter
////////////////////////////////////////////////////////////////////////////////
      
bool Function::mustConvertArgument (size_t position) const {
  bool foundArg = false;
  size_t i = 0;
  char const* p = arguments.c_str();

  while (true) {
    char const c = *p++;

    switch (c) {
      case '\0':
        return false;

      case '|':
      case ',':
        if (foundArg) {
          if (++i > position) {
            return false;
          }
        }
        foundArg = false;
        break;

      case 'h':
        if (i == position) {
          // found an argument to convert
          return true;
        }
        foundArg = true;
        break;

      default:
        foundArg = true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse the argument list and set the minimum and maximum number of
/// arguments
////////////////////////////////////////////////////////////////////////////////

void Function::initArguments () {
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
        if (c == 'h') {
          // note that we found a collection parameter
          containsCollectionParameter = true;
        }
        foundArg = true;
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
