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

#ifndef VELOCYPACK_VALIDATOR_H
#define VELOCYPACK_VALIDATOR_H 1

#include "velocypack/velocypack-common.h"
#include "velocypack/Options.h"

namespace arangodb {
namespace velocypack {
class Slice;

class Validator {
  // This class can validate a binary VelocyPack value.

 public:
  explicit Validator(Options const* options = &Options::Defaults)
      : options(options) {
    if (options == nullptr) {
      throw Exception(Exception::InternalError, "Options cannot be a nullptr");
    }
  }

  ~Validator() = default;

 public:
  // validates a VelocyPack Slice value starting at ptr, with length bytes length
  // throws if the data is invalid
  bool validate(char const* ptr, size_t length, bool isSubPart = false) const {
    return validate(reinterpret_cast<uint8_t const*>(ptr), length, isSubPart);
  }

  // validates a VelocyPack Slice value starting at ptr, with length bytes length
  // throws if the data is invalid
  bool validate(uint8_t const* ptr, size_t length, bool isSubPart = false) const;

 private:
  void validateArray(uint8_t const* ptr, size_t length) const;
  void validateCompactArray(uint8_t const* ptr, size_t length) const;
  void validateUnindexedArray(uint8_t const* ptr, size_t length) const;
  void validateIndexedArray(uint8_t const* ptr, size_t length) const;
  void validateObject(uint8_t const* ptr, size_t length) const;
  void validateCompactObject(uint8_t const* ptr, size_t length) const;
  void validateIndexedObject(uint8_t const* ptr, size_t length) const;
  void validateBufferLength(size_t expected, size_t actual, bool isSubPart) const;
  void validateSliceLength(uint8_t const* ptr, size_t length, bool isSubPart) const;
  ValueLength readByteSize(uint8_t const*& ptr, uint8_t const* end) const;

 public:
  Options const* options;
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
