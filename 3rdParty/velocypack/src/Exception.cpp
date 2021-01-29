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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include <ostream>

#include "velocypack/velocypack-common.h"
#include "velocypack/Exception.h"

using namespace arangodb::velocypack;

Exception::Exception(ExceptionType type, char const* msg) noexcept
    : _type(type), _msg(msg) {}

char const* Exception::message(ExceptionType type) noexcept {
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
    case BadTupleSize:
      return "Array size does not match tuple size";
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

std::ostream& operator<<(std::ostream& stream, Exception const* ex) {
  stream << "[Exception " << ex->what() << "]";
  return stream;
}

std::ostream& operator<<(std::ostream& stream, Exception const& ex) {
  return operator<<(stream, &ex);
}
