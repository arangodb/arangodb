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
#include <cassert>
#include <cstring>

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
        uint32_t uintValue = _randContext.mt();
        int32_t intValue;
        memcpy(&intValue, &uintValue, sizeof(uintValue));
        input.append(std::to_string(intValue));
        break;
      }
      default:
        assert(false);
    }
  }
}

void RequestFuzzer::randomizeLineOperation(uint32_t numIts) {
  uint32_t its = numIts;
  while (its--) {
    LineOperation lineOp = static_cast<LineOperation>(
        _randContext.mt() % LineOperation::MAX_LINE_OP_VALUE);
    switch (lineOp) {
      case LineOperation::REMOVE_LINE: {
        if (_stringLines.size() <= 1) {
          break;
        }
        _stringLines.erase(
            _stringLines.begin() +
            generateRandNumWithinRange<uint32_t>(1, _stringLines.size() - 1));
        break;
      }
      case LineOperation::COPY_LINE: {
        _stringLines.emplace_back(_stringLines.at(
            generateRandNumWithinRange<uint32_t>(0, _stringLines.size() - 1)));
        break;
      }
      case LineOperation::ADD_LINE: {
        std::string charsToAppend;
        randomizeCharOperation(charsToAppend, its);
        _stringLines.emplace_back(std::move(charsToAppend));
        break;
      }
      default:
        assert(false);
    }
  }
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
  uint32_t numNestedAddrs = generateRandNumWithinRange<uint32_t>(1, 10);
  for (uint32_t j = 0; j < numNestedAddrs; ++j) {
    CommonRoute randCommonRoute = static_cast<CommonRoute>(
        _randContext.mt() % CommonRoute::MAX_ROUTE_VALUE);
    firstLine.append("/");
    if (randCommonRoute != CommonRoute::RAND_COMMON_ROUTE) {
      firstLine.append(_routeWordList.at(randCommonRoute));
    } else {
      randomizeCharOperation(firstLine);
    }
  }
  if (_randContext.mt() % 2 == 0) {
    firstLine.append(" HTTP");
  } else {
    randomizeCharOperation(firstLine);
  }
  if (_randContext.mt() % 2 == 0) {
    firstLine.append("/");
  }
  if (_randContext.mt() % 2 == 0) {
    firstLine.append("1.1");
  } else {
    randomizeCharOperation(firstLine);
  }
  _stringLines.emplace_back(firstLine);
  randomizeLineOperation(_numIterations);
  for (auto const& line : _stringLines) {
    header.append(line);
    header.push_back('\n');
  }
}

template<typename T>
T RequestFuzzer::generateRandNumWithinRange(T min, T max) {
  return min + (_randContext.mt() % (max + 1 - min));
}

void RequestFuzzer::generateRandAsciiString(std::string& input) {
  static constexpr char chars[] =
      "0123456789"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz";

  uint32_t randStrLength = generateRandNumWithinRange<uint32_t>(
      0, _randContext.maxRandAsciiStringLength);

  for (uint32_t i = 0; i < randStrLength; ++i) {
    input += chars[_randContext.mt() % (sizeof(chars) - 1)];
  }
}
}  // namespace fuzzer
};  // namespace arangodb