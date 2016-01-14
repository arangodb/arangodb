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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASICS_VELOCY_PACK_HELPER_H
#define LIB_BASICS_VELOCY_PACK_HELPER_H 1

#include "Basics/JsonHelper.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace triagens {
namespace basics {

class VelocyPackHelper {
 private:
  VelocyPackHelper() = delete;
  ~VelocyPackHelper() = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric value
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T getNumericValue(VPackSlice const& slice, T defaultValue) {
    if (slice.isNumber()) {
      return slice.getNumber<T>();
    }
    return defaultValue;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a numeric sub-element, or a default if it does not exist
  //////////////////////////////////////////////////////////////////////////////

  template <typename T>
  static T getNumericValue(VPackSlice const& slice, char const* name,
                           T defaultValue) {
    VPackSlice sub = slice.get(name);
    if (sub.isNumber()) {
      return sub.getNumber<T>();
    }
    return defaultValue;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a boolean sub-element, or a default if it does not exist
  //////////////////////////////////////////////////////////////////////////////

  static bool getBooleanValue(VPackSlice const&, char const*, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or throws if <name> does not exist
  /// or it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string checkAndGetStringValue(VPackSlice const&, char const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a string sub-element, or the default value if it does not
  /// exist
  /// or it is not a string
  //////////////////////////////////////////////////////////////////////////////

  static std::string getStringValue(VPackSlice const&, char const*,
                                    std::string const&);


  //////////////////////////////////////////////////////////////////////////////
  /// @brief convert a Object sub value into a uint64
  //////////////////////////////////////////////////////////////////////////////

  static uint64_t stringUInt64(VPackSlice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Build TRI_json_t from VelocyPack. Just a temporary solution
  //////////////////////////////////////////////////////////////////////////////

  static TRI_json_t* velocyPackToJson(VPackSlice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief parses a json file to VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  static std::shared_ptr<VPackBuilder> velocyPackFromFile(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief writes a VelocyPack to a file
  //////////////////////////////////////////////////////////////////////////////

  static bool velocyPackToFile(char const*, VPackSlice const&, bool);
};
}
}

#endif

