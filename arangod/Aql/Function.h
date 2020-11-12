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

#include "Aql/Functions.h"

#include <cstdint>
#include <string>
#include <utility>
#include <type_traits>

namespace arangodb {
namespace velocypack {
class Builder;
}

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

    // the following two flags control separately if a function can be pushed
    // down to DB servers for execution. In almost cases we want to have the
    // "Cluster" flag set for functions that have the "OneShard" flag set,
    // but this is currently only enforced via assertions - and we may want
    // to change this in the future so we can have functions that are _not_
    // pushed down to DB servers in OneShard mode but are in normal Cluster
    // mode.

    /// @brief whether or not the function may be executed on DB servers, 
    /// general cluster case (non-OneShard)
    /// note: in almost all circumstances it is also useful to set the flag
    /// CanRunOnDBServerOneShard in addition!
    CanRunOnDBServerCluster = 4,
    
    /// @brief whether or not the function may be executed on DB servers,
    /// OneShard databases. 
    /// note: this flag must be set in addition to CanRunOnDBServerCluster
    /// to make a function run on DB servers in OneShard mode! 
    CanRunOnDBServerOneShard = 8,
    
    /// @brief whether or not the function may read documents from the database
    CanReadDocuments = 16,

    /// @brief exclude the function from being evaluated during AST
    /// optimizations evaluation of function will only happen at query runtime
    NoEval = 32,
  };

  /// @brief helper for building flags
  template <typename... Args>
  static inline std::underlying_type<Flags>::type makeFlags(Flags flag, Args... args) noexcept {
    return static_cast<std::underlying_type<Flags>::type>(flag) | makeFlags(args...);
  }

  static std::underlying_type<Flags>::type makeFlags() noexcept;

  Function() = delete;

  /// @brief create the function
  Function(std::string const& name, char const* arguments,
           std::underlying_type<Flags>::type flags,
           FunctionImplementation implementation);

  /// @brief whether or not the function is based on V8
  bool hasV8Implementation() const noexcept;
  
  /// @brief whether or not the function is based on cxx
  bool hasCxxImplementation() const noexcept;

  /// @brief return whether a specific flag is set for the function
  bool hasFlag(Flags flag) const noexcept;

  /// @brief return the number of required arguments
  std::pair<size_t, size_t> numArguments() const;

  /// @brief whether or not a positional argument needs to be converted from a
  /// collection parameter to a collection name parameter
  Conversion getArgumentConversion(size_t position) const;

  /// @brief parse the argument list and set the minimum and maximum number of
  /// arguments
  void initializeArguments();

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

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
