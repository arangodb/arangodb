////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_EXCEPTION_H
#define VELOCYPACK_EXCEPTION_H 1

#include <exception>
#include <iosfwd>

#include "velocypack/velocypack-common.h"

namespace arangodb {
namespace velocypack {

// base exception class
class Exception : public virtual std::exception {
 public:
  enum ExceptionType {
    InternalError = 1,
    NotImplemented = 2,

    NoJsonEquivalent = 10,
    ParseError = 11,
    UnexpectedControlCharacter = 12,
    IndexOutOfBounds = 13,
    NumberOutOfRange = 14,
    InvalidUtf8Sequence = 15,
    InvalidAttributePath = 16,
    InvalidValueType = 17,
    DuplicateAttributeName = 18,
    NeedCustomTypeHandler = 19,
    NeedAttributeTranslator = 20,
    CannotTranslateKey = 21,
    KeyNotFound = 22, // not used anymore

    BuilderNotSealed = 30,
    BuilderNeedOpenObject = 31,
    BuilderNeedOpenArray = 32,
    BuilderNeedOpenCompound = 33,
    BuilderUnexpectedType = 34,
    BuilderUnexpectedValue = 35,
    BuilderNeedSubvalue = 36,
    BuilderExternalsDisallowed = 37,
    BuilderKeyAlreadyWritten = 38,
    BuilderKeyMustBeString = 39,
    BuilderCustomDisallowed = 40,
    BuilderTagsDisallowed = 41,
    BuilderBCDDisallowed = 42,

    ValidatorInvalidLength = 50,
    ValidatorInvalidType = 51,

    UnknownError = 999
  };

 private:
  ExceptionType _type;
  char const* _msg;

 public:
  Exception(ExceptionType type, char const* msg) noexcept;

  explicit Exception(ExceptionType type) noexcept : Exception(type, message(type)) {}
  
  Exception(Exception const& other) = default;
  Exception(Exception&& other) noexcept = default;
  Exception& operator=(Exception const& other) = default;
  Exception& operator=(Exception&& other) noexcept = default;
  
  ~Exception() = default;

  char const* what() const noexcept { return _msg; }

  ExceptionType errorCode() const noexcept { return _type; }

  static char const* message(ExceptionType type) noexcept {
    switch (type) {
      case InternalError:
        return "Internal error";
      case NotImplemented:
        return "Not implemented";
      case NoJsonEquivalent:
        return "Type has no equivalent in JSON";
      case ParseError:
        return "Parse error";
      case UnexpectedControlCharacter:
        return "Unexpected control character";
      case DuplicateAttributeName:
        return "Duplicate attribute name";
      case IndexOutOfBounds:
        return "Index out of bounds";
      case NumberOutOfRange:
        return "Number out of range";
      case InvalidUtf8Sequence:
        return "Invalid UTF-8 sequence";
      case InvalidAttributePath:
        return "Invalid attribute path";
      case InvalidValueType:
        return "Invalid value type for operation";
      case NeedCustomTypeHandler:
        return "Cannot execute operation without custom type handler";
      case NeedAttributeTranslator:
        return "Cannot execute operation without attribute translator";
      case CannotTranslateKey:
        return "Cannot translate key";
      case KeyNotFound:
        return "Key not found";
      case BuilderNotSealed:
        return "Builder value not yet sealed";
      case BuilderNeedOpenObject:
        return "Need open Object";
      case BuilderNeedOpenArray:
        return "Need open Array";
      case BuilderNeedSubvalue:
        return "Need subvalue in current Object or Array";
      case BuilderNeedOpenCompound:
        return "Need open compound value (Array or Object)";
      case BuilderUnexpectedType:
        return "Unexpected type";
      case BuilderUnexpectedValue:
        return "Unexpected value";
      case BuilderExternalsDisallowed:
        return "Externals are not allowed in this configuration";
      case BuilderKeyAlreadyWritten:
        return "The key of the next key/value pair is already written";
      case BuilderKeyMustBeString:
        return "The key of the next key/value pair must be a string";
      case BuilderCustomDisallowed:
        return "Custom types are not allowed in this configuration";
      case BuilderTagsDisallowed:
        return "Tagged types are not allowed in this configuration";
      case BuilderBCDDisallowed:
        return "BCD types are not allowed in this configuration";
    
      case ValidatorInvalidType:
        return "Invalid type found in binary data";
      case ValidatorInvalidLength:
        return "Invalid length found in binary data";

      case UnknownError:
      default:
        return "Unknown error";
    }
  }
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Exception const*);

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Exception const&);

#endif
