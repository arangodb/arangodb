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

#include <ostream>

#include "velocypack/velocypack-common.h"
#include "velocypack/HexDump.h"

using namespace arangodb::velocypack;

std::string HexDump::toHex(uint8_t value) {
  std::string result("0x");

  uint8_t x = value / 16;
  result.push_back((x < 10 ? ('0' + x) : ('a' + x - 10)));
  x = value % 16;
  result.push_back((x < 10 ? ('0' + x) : ('a' + x - 10)));

  return result;
}

std::ostream& operator<<(std::ostream& stream, HexDump const* hexdump) {
  int current = 0;

  for (uint8_t it : hexdump->slice) {
    if (current != 0) {
      stream << hexdump->separator;

      if (hexdump->valuesPerLine > 0 && current == hexdump->valuesPerLine) {
        stream << std::endl;
        current = 0;
      }
    }

    stream << HexDump::toHex(it);
    ++current;
  }

  return stream;
}

std::ostream& operator<<(std::ostream& stream, HexDump const& hexdump) {
  return operator<<(stream, &hexdump);
}
