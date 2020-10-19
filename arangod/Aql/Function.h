////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Functions.h"

#include <cstdint>
#include <string>
#include <utility>
#include <type_traits>

namespace arangodb {
namespace aql {

struct Function {
  enum class Conversion : uint8_t { None = 0, Optional = 1, Required = 2 };

  /// @brief arbitrary function flags. note that these must be mutually
  /// exclusive when bit-ORed
  enum class Flags : uint8_t {
    None = 0,

    /// @brief whether or not the function is deterministic (i.e. its results
    /// are identical when called repeatedly with the same input values)
    Deterministic = 1,

    /// @brief whether or not the function result is cacheable in the query
    /// cache
    Cacheable = 2,

    /// @brief whether or not the function may be executed on DB servers
    CanRunOnDBServer = 4,

    /// @brief exclude the function from being evaluated during AST
    /// optimizations evaluation of function will only happen at query runtime
    NoEval = 8,
  };

  /// @brief helper for building flags
  template <typename... Args>
  static inline std::underlying_type<Flags>::type makeFlags(Flags flag, Args... args) {
    return static_cast<std::underlying_type<Flags>::type>(flag) + makeFlags(args...);
  }

  static std::underlying_type<Flags>::type makeFlags();

  Function() = delete;

  /// @brief create the function
  Function(std::string const& name, char const* arguments,
           std::underlying_type<Flags>::type flags,
           FunctionImplementation implementation = nullptr);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  Function(std::string const& name,
           FunctionImplementation implementation);
#endif

  /// @brief return whether a specific flag is set for the function
  bool hasFlag(Flags flag) const;

  /// @brief return the number of required arguments
  std::pair<size_t, size_t> numArguments() const;

  /// @brief whether or not a positional argument needs to be converted from a
  /// collection parameter to a collection name parameter
  Conversion getArgumentConversion(size_t position) const;

  /// @brief parse the argument list and set the minimum and maximum number of
  /// arguments
  void initializeArguments();

  /// @brief function name (name visible to the end user, may be an alias)
  std::string name;

  /// @brief function arguments
  char const* arguments;

  /// @brief function flags
  std::underlying_type<Flags>::type const flags;

  /// @brief minimum number of required arguments
  size_t minRequiredArguments;

  /// @brief maximum number of required arguments
  size_t maxRequiredArguments;

  /// @brief C++ implementation of the function
  FunctionImplementation const implementation;

  /// @brief function argument conversion information
  std::vector<Conversion> conversions;

  /// @brief maximum number of function arguments that can be used
  static constexpr size_t maxArguments = 65536;
};
}  // namespace aql
}  // namespace arangodb

#endif
