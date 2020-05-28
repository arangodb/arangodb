////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

#include "LoggerStream.h"

#include "Logger/Logger.h"

using namespace arangodb;

LoggerStream::~LoggerStream() {
  try {
    Logger::log(_function, _file, _line, _level, _topicId, _out.str());
  } catch (...) {
    try {
      // logging the error may fail as well, and we should never throw in the
      // dtor
      std::cerr << "failed to log: " << _out.str() << std::endl;
    } catch (...) {
    }
  }
}

// print a hex representation of the binary data
LoggerStream& LoggerStream::operator<<(Logger::BINARY const& binary) {
  try {
    uint8_t const* ptr = static_cast<uint8_t const*>(binary.baseAddress);
    uint8_t const* end = ptr + binary.size;

    while (ptr < end) {
      uint8_t n = *ptr;

      uint8_t n1 = n >> 4;
      uint8_t n2 = n & 0x0F;

      _out << "\\x" << static_cast<char>((n1 < 10) ? ('0' + n1) : ('A' + n1 - 10))
           << static_cast<char>((n2 < 10) ? ('0' + n2) : ('A' + n2 - 10));
      ++ptr;
    }
  } catch (...) {
    // ignore any errors here. logging should not have side effects
  }

  return *this;
}

// print a character array
LoggerStream& LoggerStream::operator<<(Logger::CHARS const& data) {
  try {
    _out.write(data.data, data.size);
  } catch (...) {
    // ignore any errors here. logging should not have side effects
  }

  return *this;
}

LoggerStream& LoggerStream::operator<<(Logger::RANGE const& range) {
  try {
    _out << range.baseAddress << " - "
         << static_cast<void const*>(static_cast<char const*>(range.baseAddress) +
                                     range.size)
         << " (" << range.size << " bytes)";
  } catch (...) {
    // ignore any errors here. logging should not have side effects
  }

  return *this;
}

LoggerStream& LoggerStream::operator<<(Logger::FIXED const& value) {
  try {
    std::ostringstream tmp;
    tmp << std::setprecision(value._precision) << std::fixed << value._value;
    _out << tmp.str();
  } catch (...) {
    // ignore any errors here. logging should not have side effects
  }

  return *this;
}
