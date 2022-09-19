////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Julia Puget
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace arangodb {

// control chars will not be escaped, but suppressed
struct ControlCharsSuppressor {
  static constexpr size_t maxCharLength() { return 1; }
  static void writeCharIntoOutputBuffer(uint32_t c, std::string& output,
                                        int numBytes);
};

// control chars will be escaped
struct ControlCharsEscaper {
  static constexpr size_t maxCharLength() {
    // worst case: "\x07"
    return 4;
  }
  static void writeCharIntoOutputBuffer(uint32_t c, std::string& output,
                                        int numBytes);
};

// Unicode chars will be retained
struct UnicodeCharsRetainer {
  static constexpr size_t maxCharLength() {
    // worst case 4 digits
    return 4;
  }
  static void writeCharIntoOutputBuffer(uint32_t c, std::string& output,
                                        int numBytes);
};

// Unicode chars will be escaped
struct UnicodeCharsEscaper {
  static constexpr size_t maxCharLength() {
    // "\u" +4 digits
    return 6;
  }

  static void writeCharIntoOutputBuffer(uint32_t c, std::string& output,
                                        int numBytes);

 private:
  static void writeCharHelper(uint16_t c, std::string& output);
};

template<typename ControlCharHandler, typename UnicodeCharHandler>
struct Escaper {
  static void writeIntoOutputBuffer(std::string_view message,
                                    std::string& output);
};

}  // namespace arangodb
