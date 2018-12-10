////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_MASKINGS_ATTRIBUTE_XIFY_FRONT_H
#define ARANGODB_MASKINGS_ATTRIBUTE_XIFY_FRONT_H 1

#include "Maskings/MaskingFunction.h"

namespace arangodb {
namespace maskings {
class XifyFront : public MaskingFunction {
 public:
  XifyFront(Maskings* maskings, int64_t length, bool hash, uint64_t seed)
      : MaskingFunction(maskings),
        _length((uint64_t)length),
        _randomSeed(seed),
        _hash(hash) {}

  VPackValue mask(bool) const override;
  VPackValue mask(std::string const&, std::string& buffer) const override;
  VPackValue mask(int64_t) const override;
  VPackValue mask(double) const override;

 private:
  uint64_t _length;
  uint64_t _randomSeed;
  bool _hash;
};
}  // namespace maskings
}  // namespace arangodb

#endif
