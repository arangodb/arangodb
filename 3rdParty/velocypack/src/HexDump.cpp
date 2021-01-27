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
#include "velocypack/HexDump.h"

using namespace arangodb::velocypack;

std::string HexDump::toHex(uint8_t value, std::string const& header) {
  std::string result(header);
  appendHex(result, value);
  return result;
}

void HexDump::appendHex(std::string& result, uint8_t value) {
  uint8_t x = value / 16;
  result.push_back((x < 10 ? ('0' + x) : ('a' + x - 10)));
  x = value % 16;
  result.push_back((x < 10 ? ('0' + x) : ('a' + x - 10)));
}

std::string HexDump::toString() const {
  ValueLength length = slice.byteSize();
  std::string result;
  result.reserve(4 * (length + separator.size()) + (length / valuesPerLine) + 1); 

  int current = 0;

  for (uint8_t it : slice) {
    if (current != 0) {
      result.append(separator);

      if (valuesPerLine > 0 && current == valuesPerLine) {
        result.push_back('\n');
        current = 0;
      }
    }

    result.append(header);
    HexDump::appendHex(result, it);
    ++current;
  }

  return result;
}

namespace arangodb {
namespace velocypack {

std::ostream& operator<<(std::ostream& stream, HexDump const& hexdump) {
  stream << hexdump.toString();
  return stream;
}

}
}
