////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_VOC_BASE_ACCESS_MODE_H
#define ARANGOD_VOC_BASE_ACCESS_MODE_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"

namespace arangodb {
 
struct AccessMode {
  enum class Type {
    NONE = 0,
    READ = 1,
    WRITE = 2,
    EXCLUSIVE = 4
  };
  // no need to create an object of it
  AccessMode() = delete; 

  static_assert(AccessMode::Type::NONE < AccessMode::Type::READ &&
                AccessMode::Type::READ < AccessMode::Type::WRITE &&
                AccessMode::Type::WRITE < AccessMode::Type::EXCLUSIVE,
                "AccessMode::Type total order fail");
  
  static inline bool isNone(Type type) {
    return (type == Type::NONE);
  }
  
  static inline bool isRead(Type type) {
    return (type == Type::READ);
  }
  
  static inline bool isWrite(Type type) {
    return (type == Type::WRITE);
  }
  
  static inline bool isExclusive(Type type) {
    return (type == Type::EXCLUSIVE);
  }

  static inline bool isWriteOrExclusive(Type type) {
    return (isWrite(type) || isExclusive(type));
  }

  /// @brief get the transaction type from a string
  static Type fromString(char const* value) {
    if (strcmp(value, "read") == 0) {
      return Type::READ;
    } 
    if (strcmp(value, "write") == 0) {
      return Type::WRITE;
    } 
    if (strcmp(value, "exclusive") == 0) {
      return Type::EXCLUSIVE;
    } 
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                  "invalid access type");
  }

  /// @brief return the type of the transaction as a string
  static inline char const* typeString(Type value) {
    switch (value) {
      case Type::NONE: 
        return "none";
      case Type::READ:
        return "read";
      case Type::WRITE:
        return "write";
      case Type::EXCLUSIVE:
        return "exclusive";
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                  "invalid access type");
  }
};

}

#endif
