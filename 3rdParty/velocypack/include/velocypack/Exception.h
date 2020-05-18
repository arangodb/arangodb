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
#include <vector>
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

    ValidatorInvalidLength = 50,
    ValidatorInvalidType = 51,

    UnknownError = 999
  };

 private:
  ExceptionType const _type;
  char const* _msg;
  std::vector<void*> _backtrace;

 public:
  Exception(ExceptionType type, char const* msg) : _type(type), _msg(msg), _backtrace(generateBacktrace()) {}
  explicit Exception(ExceptionType type) : Exception(type, message(type)) {}
  //Exception(Exception const& other) = default;
  Exception(Exception &&) noexcept = default;
  ~Exception() = default;

  char const* what() const noexcept override { return _msg; }
  ExceptionType errorCode() const noexcept { return _type; }
  std::vector<void*> const& backtrace() const noexcept { return _backtrace; }

  std::string formatBacktrace() const;

 private:
  static char const* message(ExceptionType type) noexcept;
  static std::vector<void*> generateBacktrace();

};

}  // namespace arangodb::velocypack
}  // namespace arangodb

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Exception const*);

std::ostream& operator<<(std::ostream&, arangodb::velocypack::Exception const&);

#endif
