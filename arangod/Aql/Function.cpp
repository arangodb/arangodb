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
                   bool isDeterministic, bool canThrow, bool canRunOnDBServer,
                   bool canPassArgumentsByReference,
                   FunctionImplementation implementation,
                   ExecutionCondition condition)
    : name(name),
      nonAliasedName(name),
      arguments(arguments),
      isDeterministic(isDeterministic),
      canThrow(canThrow),
      canRunOnDBServer(canRunOnDBServer),
      canPassArgumentsByReference(canPassArgumentsByReference),
      implementation(implementation),
      condition(condition),
      conversions() {
  initializeArguments();

  LOG_TOPIC(TRACE, Logger::FIXME) << "setting up AQL function '" << name << 
                                     "'. cacheable: " << isCacheable() << 
                                     ", deterministic: " << isDeterministic << 
                                     ", canThrow: " << canThrow << 
                                     ", canRunOnDBServer: " << canRunOnDBServer << 
                                     ", canPassArgumentsByReference: " << canPassArgumentsByReference << 
                                     ", hasCxxImplementation: " << (implementation != nullptr) << 
                                     ", hasConversions: " << !conversions.empty();
                                     
  // condition must only be set if we also have an implementation
  TRI_ASSERT(implementation != nullptr || condition == nullptr);
  
  LOG_TOPIC(TRACE, Logger::FIXME) << "setting up AQL function '" << name << ", hasImpl:" << hasImplementation(); 
}

/// @brief destroy the function
Function::~Function() {}

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
        maxRequiredArguments = MaxArguments;
        return;

      case 'h':
        // we found a collection parameter

        // set the conversion info for the position
        if (conversions.size() <= position) {
          // we don't yet have another parameter at this position
          conversions.emplace_back(CONVERSION_REQUIRED);
        } else if (conversions[position] == CONVERSION_NONE) {
          // we already had a parameter at this position
          conversions[position] = CONVERSION_OPTIONAL;
        }
        foundArg = true;
        break;

      case '.':
        // we found any other parameter

        // set the conversion info for the position
        if (conversions.size() <= position) {
          // we don't yet have another parameter at this position
          conversions.emplace_back(CONVERSION_NONE);
        } else if (conversions[position] == CONVERSION_REQUIRED) {
          // we already had a parameter at this position
          conversions[position] = CONVERSION_OPTIONAL;
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
