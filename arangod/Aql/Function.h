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

#ifndef ARANGOD_AQL_FUNCTION_H
#define ARANGOD_AQL_FUNCTION_H 1

#include "Basics/Common.h"
#include "Aql/Functions.h"

namespace arangodb {
namespace aql {

struct Function {
  enum Conversion { CONVERSION_NONE, CONVERSION_OPTIONAL, CONVERSION_REQUIRED };

  Function() = delete;

  /// @brief create the function
  Function(std::string const& name, 
           char const* arguments, bool isDeterministic,
           bool canThrow, bool canRunOnDBServer,
           bool canPassArgumentsByReference,
           FunctionImplementation implementation = nullptr,
           ExecutionCondition = nullptr);

  /// @brief destroy the function
  ~Function();

  /// @brief checks if the function has been deprecated
  inline bool isDeprecated() const {
    // currently there are no deprecated functions
    return false;
  }

  inline bool hasImplementation() const {
    return implementation != nullptr;
  }

  /// @brief checks if the function produces a result that can
  /// be cached by the AQL query result cache
  /// the return value is currently the same flag as isDeterministic
  bool isCacheable() const { return isDeterministic; }

  /// @brief return the number of required arguments
  inline std::pair<size_t, size_t> numArguments() const {
    return std::make_pair(minRequiredArguments, maxRequiredArguments);
  }

  /// @brief whether or not a positional argument needs to be converted from a
  /// collection parameter to a collection name parameter
  inline Conversion getArgumentConversion(size_t position) const {
    if (position >= conversions.size()) {
      return CONVERSION_NONE;
    }
    return conversions[position];
  }

  /// @brief return the name of the function's v8 implementation
  std::string v8FunctionName() const {
    return std::string("AQL_") + nonAliasedName;
  }

  /// @brief parse the argument list and set the minimum and maximum number of
  /// arguments
  void initializeArguments();

  /// @brief function name (name visible to the end user, may be an alias)
  std::string name;
  
  /// @brief function name (internal, must not be an alias)
  std::string const nonAliasedName;
  
  /// @brief function arguments
  char const* arguments;

  /// @brief whether or not the function is deterministic (i.e. its results are
  /// identical when called repeatedly with the same input values)
  bool const isDeterministic;

  /// @brief whether or not the function may throw a runtime exception
  bool const canThrow;

  /// @brief whether or not the function may be executed on DB servers
  bool const canRunOnDBServer;

  /// @brief whether or not the function can get its arguments passed by
  /// reference
  bool const canPassArgumentsByReference;

  /// @brief minimum number of required arguments
  size_t minRequiredArguments;

  /// @brief maximum number of required arguments
  size_t maxRequiredArguments;

  /// @brief C++ implementation of the function (maybe nullptr)
  FunctionImplementation implementation;

  /// @brief condition under which the C++ implementation of the function is
  /// executed (if returns false, the function will be executed as its
  /// JavaScript
  /// variant)
  ExecutionCondition condition;

  /// @brief function argument conversion information
  std::vector<Conversion> conversions;

  /// @brief maximum number of function arguments that can be used
  static size_t const MaxArguments = 65536;
};
}
}

#endif
