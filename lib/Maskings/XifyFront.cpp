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

#include "XifyFront.h"

#include "Basics/StringUtils.h"
#include "Basics/fasthash.h"
#include "Maskings/Maskings.h"

static std::string const xxxx("xxxx");

using namespace arangodb;
using namespace arangodb::maskings;

VPackValue XifyFront::mask(bool) const { return VPackValue(xxxx); }

VPackValue XifyFront::mask(std::string const& data, std::string& buffer) const {
  char const* p = data.c_str();
  char const* q = p;
  char const* e = p + data.size();

  buffer.clear();
  buffer.reserve(data.size());

  while (p < e) {
    while (p < e && isNameChar(*p)) {
      ++p;
    }

    if (p != q) {
      char const* w = p - _length;

      while (q < w) {
        buffer.push_back('x');
        ++q;
      }

      while (q < p) {
        buffer.push_back(*q);
        ++q;
      }
    }

    while (p < e && !isNameChar(*p)) {
      buffer.push_back(' ');
      ++p;
    }

    q = p;
  }

  if (_hash) {
    uint64_t hash;

    if (_randomSeed == 0) {
      hash = fasthash64(data.c_str(), data.size(), _maskings->randomSeed());
    } else {
      hash = fasthash64(data.c_str(), data.size(), _randomSeed);
    }

    std::string hash64 =
        basics::StringUtils::encodeBase64(std::string((char const*)&hash, 8));

    buffer.push_back(' ');
    buffer.append(hash64);
  }

  return VPackValue(buffer);
}

VPackValue XifyFront::mask(int64_t) const { return VPackValue(xxxx); }

VPackValue XifyFront::mask(double) const { return VPackValue(xxxx); }
