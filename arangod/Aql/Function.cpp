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
      
Function::Function (std::string const& externalName,
                    std::string const& internalName,
                    std::string const& arguments,
                    bool isDeterministic,
                    bool canThrow,
                    FunctionImplementation implementation)
  : internalName(internalName),
    externalName(externalName),
    arguments(arguments),
    isDeterministic(isDeterministic),
    canThrow(canThrow),
    implementation(implementation),
    conversions() {

  initializeArguments();
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
/// @brief parse the argument list and set the minimum and maximum number of
/// arguments
////////////////////////////////////////////////////////////////////////////////

void Function::initializeArguments () {
  minRequiredArguments = 0;
  maxRequiredArguments = 0;

  size_t position = 0;

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
        ++position;
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
        ++position;
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

      case 'h':
        // we found a collection parameter
        
        // set the conversion info for the position
        if (conversions.size() <= position) {
          // we don't yet have another parameter at this position
          conversions.push_back(CONVERSION_REQUIRED);
        }
        else if (conversions[position] == CONVERSION_NONE) {
          // we already had a parameter at this position
          conversions[position] = CONVERSION_OPTIONAL;
        }
        foundArg = true;
        break;

      default:
        // we found any other parameter

        // set the conversion info for the position
        if (conversions.size() <= position) {
          // we don't yet have another parameter at this position
          conversions.push_back(CONVERSION_NONE);
        }
        else if (conversions[position] == CONVERSION_REQUIRED) {
          // we already had a parameter at this position
          conversions[position] = CONVERSION_OPTIONAL;
        }
        foundArg = true;
        break;
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
