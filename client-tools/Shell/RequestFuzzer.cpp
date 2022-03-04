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

#include "RequestFuzzer.h"
#include <cstring>

#include <Basics/debugging.h>

#include <Logger/LogMacros.h>

namespace arangodb {
namespace fuzzer {

void RequestFuzzer::randomizeCharOperation(std::string& input,
                                           uint32_t numIts) {
  while (numIts--) {
    CharOperation charOp = static_cast<CharOperation>(
        _randContext.mt() % CharOperation::MAX_CHAR_OP_VALUE);
    switch (charOp) {
      case CharOperation::ADD_STRING:
        generateRandAsciiString(input);
        break;
      case CharOperation::ADD_INT_32: {
        input.append(std::to_string(generateRandInt32()));
        break;
      }
      default:
        TRI_ASSERT(false);
    }
  }
}

void RequestFuzzer::randomizeLineOperation(uint32_t numIts) {
  uint32_t its = numIts;
  while (its--) {
    LineOperation lineOp = static_cast<LineOperation>(
        _randContext.mt() % LineOperation::MAX_LINE_OP_VALUE);
    switch (lineOp) {
      case LineOperation::INJECT_RAND_BYTE_IN_LINE: {
        if (_stringLines.size() > 1) {
          uint32_t initLinePos;
          if (generateRandNumWithinRange<uint32_t>(0, 99) > 0) {
            initLinePos = 1;
          } else {
            initLinePos = 0;
          }
          uint32_t randPos = generateRandNumWithinRange<uint32_t>(
              initLinePos, _stringLines.size() - 1);
          generateRandAsciiString(_stringLines[randPos], true);
        }
        break;
      }
      case LineOperation::COPY_LINE: {
        if (_stringLines.size() > 1) {
          _stringLines.emplace_back(
              _stringLines.at(generateRandNumWithinRange<uint32_t>(
                  1, _stringLines.size() - 1)));
        }
        break;
      }
      case LineOperation::ADD_LINE: {
        std::string charsToAppend;
        uint32_t numHeaderFields =
            generateRandNumWithinRange<uint32_t>(1, _wordListForKeys.size());
        std::unordered_map<std::string, std::string> keysAndValues;
        for (uint32_t i = 0; i < numHeaderFields; ++i) {
          uint32_t keyPos = generateRandNumWithinRange<uint32_t>(
              0, _wordListForKeys.size() - 1);
          std::string keyName;
          std::string value;
          if (!(_wordListForKeys[keyPos] == "random")) {
            keyName = _wordListForKeys[keyPos];
          } else {
            randomizeCharOperation(keyName, 1);
          }
          randomizeCharOperation(value,
                                 generateRandNumWithinRange<uint32_t>(1, 3));
          keysAndValues[keyName] = value;
        }
        for (auto const& [key, value] : keysAndValues) {
          _stringLines.emplace_back(std::move(key) + ":" + std::move(value));
        }
        break;
      }
      default:
        TRI_ASSERT(false);
    }
  }
}

void RequestFuzzer::writeSampleHeader(std::string& header) {
  header.append("GET /_api/version HTTP/1.1\r\n\r\nHost: 127.0.0.1:8529\r\n");
}

void RequestFuzzer::randomizeHeader(std::string& header) {
  RequestType randReqType =
      static_cast<RequestType>(_randContext.mt() % RequestType::MAX_REQ_VALUE);
  std::string firstLine;
  if (randReqType != RequestType::RAND_REQ_TYPE) {
    firstLine.append(_requestTypes.at(randReqType));
  } else {
    randomizeCharOperation(firstLine);
  }
  firstLine.append(" ");
  uint32_t numNestedRoutes =
      generateRandNumWithinRange<uint32_t>(1, _maxNestedRoutes);
  for (uint32_t i = 0; i < numNestedRoutes; ++i) {
    uint32_t routePos =
        generateRandNumWithinRange<uint32_t>(0, _wordListForRoute.size() - 1);
    if (!(_wordListForRoute[routePos] == "random")) {
      firstLine.append(_wordListForRoute[routePos]);
    } else {
      randomizeCharOperation(firstLine);
    }
  }
  firstLine.append(" ");
  if (_randContext.mt() % 2 == 0) {
    firstLine.append(" HTTP/");
  } else {
    randomizeCharOperation(firstLine);
  }
  if (generateRandNumWithinRange<uint32_t>(0, 99) > 0) {
    if (_randContext.mt() % 2 == 0) {
      firstLine.append("1.1");
    } else {
      firstLine.append(
          std::to_string(generateRandNumWithinRange<uint32_t>(0, 9)) + "." +
          std::to_string(generateRandNumWithinRange<uint32_t>(0, 9)));
    }
  } else {
    firstLine.append(std::to_string(generateRandInt32()));
  }
  _stringLines.emplace_back(firstLine);
  randomizeLineOperation(_numIterations);
  for (uint32_t i = 0; i < _stringLines.size(); ++i) {
    header.append(_stringLines[i]);
    if (i == _stringLines.size() - 1) {
      firstLine.append("\r\n\r\n");

    } else {
      firstLine.append("\r\n");
    }
  }
}

template<typename T>
T RequestFuzzer::generateRandNumWithinRange(T min, T max) {
  return min + (_randContext.mt() % (max + 1 - min));
}

int32_t RequestFuzzer::generateRandInt32() {
  uint32_t uintValue = _randContext.mt();
  int32_t intValue;
  memcpy(&intValue, &uintValue, sizeof(uintValue));
  return intValue;
}

void RequestFuzzer::generateRandAsciiString(std::string& input,
                                            bool isRandChar) {
  static constexpr char chars[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  uint32_t randStrLength = isRandChar
                               ? 1
                               : generateRandNumWithinRange<uint32_t>(
                                     0, _randContext.maxRandAsciiStringLength);

  for (uint32_t i = 0; i < randStrLength; ++i) {
    if (isRandChar) {
      input[generateRandNumWithinRange<uint32_t>(0, input.size())] =
          chars[_randContext.mt() % (sizeof(chars) - 1)];
    } else {
      input += chars[_randContext.mt() % (sizeof(chars) - 1)];
    }
  }
}
}  // namespace fuzzer
};  // namespace arangodb