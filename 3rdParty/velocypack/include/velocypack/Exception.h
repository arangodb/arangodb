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
    BadTupleSize = 23,

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

  char const* what() const noexcept override { return _msg; }

  ExceptionType errorCode() const noexcept { return _type; }

  static char const* message(ExceptionType type) noexcept;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Exception const*);

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Exception const&);

#endif
