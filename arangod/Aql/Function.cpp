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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Function.h"
#include "Basics/Exceptions.h"

using namespace arangodb::aql;

/// @brief create the function
Function::Function(std::string const& name,
                   char const* arguments,
                   std::underlying_type<Flags>::type flags,
                   FunctionImplementation implementation)
    : name(name),
      arguments(arguments),
      flags(flags),
      implementation(implementation),
      conversions() {
  initializeArguments();

  // almost all AQL functions have a cxx implementation
  // only function V8() plus the ArangoSearch functions do not
  LOG_TOPIC(TRACE, Logger::FIXME) << "registered AQL function '" << name <<
                                     "'. cacheable: " << hasFlag(Flags::Cacheable) <<
                                     ", deterministic: " << hasFlag(Flags::Deterministic) <<
                                     ", canRunOnDBServer: " << hasFlag(Flags::CanRunOnDBServer) <<
                                     ", hasCxxImplementation: " << (implementation != nullptr) <<
                                     ", hasConversions: " << !conversions.empty();
}

/// @brief parse the argument list and set the minimum and maximum number of
/// arguments
void Function::initializeArguments() {
  minRequiredArguments = 0;
  maxRequiredArguments = 0;

  size_t position = 0;

  // setup some parsing state
  bool inOptional = false;
  bool foundArg = false;

  char const* p = arguments;
  while (true) {
    char const c = *p++;

    switch (c) {
      case '\0':
        // end of argument list
        if (foundArg) {
          if (!inOptional) {
            ++minRequiredArguments;
          }
          ++maxRequiredArguments;
        }
        return;

      case '|':
        // beginning of optional arguments
        ++position;
        TRI_ASSERT(!inOptional);
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

        if (!inOptional) {
          ++minRequiredArguments;
        }
        ++maxRequiredArguments;
        foundArg = false;
        break;

      case '+':
        // repeated optional argument
        TRI_ASSERT(inOptional);
        maxRequiredArguments = maxArguments;
        return;

      case 'h':
        // we found a collection parameter

        // set the conversion info for the position
        if (conversions.size() <= position) {
          // we don't yet have another parameter at this position
          conversions.emplace_back(Conversion::Required);
        } else if (conversions[position] == Conversion::None) {
          // we already had a parameter at this position
          conversions[position] = Conversion::Optional;
        }
        foundArg = true;
        break;

      case '.':
        // we found any other parameter

        // set the conversion info for the position
        if (conversions.size() <= position) {
          // we don't yet have another parameter at this position
          conversions.emplace_back(Conversion::None);
        } else if (conversions[position] == Conversion::Required) {
          // we already had a parameter at this position
          conversions[position] = Conversion::Optional;
        }
        foundArg = true;
        break;

      default: {
        // unknown parameter type
        std::string message("unknown function signature parameter type for AQL function '");
        message += name + "': " + c;
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, message);
      }
    }
  }
}
