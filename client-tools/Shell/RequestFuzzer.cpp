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
#include <Rest/CommonDefines.h>

#include "Logger/LogMacros.h"

namespace rest = arangodb::rest;

namespace arangodb::fuzzer {

void RequestFuzzer::randomizeCharOperation(std::string& input,
                                           uint32_t numIts) {
  while (numIts--) {
    CharOperation charOp = static_cast<CharOperation>(
        _randContext.mt() % CharOperation::kMaxCharOpValue);
    switch (charOp) {
      case CharOperation::kAddString:
        generateRandReadableAsciiString(input);
        break;
      case CharOperation::kAddInt32: {
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
        _randContext.mt() % LineOperation::kMaxLineOpValue);
    switch (lineOp) {
      case LineOperation::kInjectRandByteInLine: {
        if (_headerSplitInLines.size() > 1) {
          uint32_t initLinePos;
          if (generateRandNumWithinRange<uint32_t>(0, 99) > 0) {
            initLinePos = 1;
          } else {
            initLinePos = 0;
          }
          uint32_t randPos = generateRandNumWithinRange<uint32_t>(
              initLinePos, _headerSplitInLines.size() - 1);
          generateRandAsciiChar(_headerSplitInLines[randPos]);
        }
        break;
      }
      case LineOperation::kCopyLine: {
        if (_headerSplitInLines.size() > 1) {
          uint32_t index = generateRandNumWithinRange<uint32_t>(
              1, _headerSplitInLines.size() - 1);
          _headerSplitInLines.emplace_back(_headerSplitInLines.at(index));
        }
        break;
      }
      case LineOperation::kAddLine: {
        if (_usedKeys.size() <= _wordListForKeys.size() - 5) {
          std::string keyName;
          std::string value;
          do {
            uint32_t keyPos = generateRandNumWithinRange<uint32_t>(
                0, _wordListForKeys.size() - 1);
            if (_wordListForKeys[keyPos] != "random") {
              keyName = _wordListForKeys[keyPos];
            } else {
              keyName.clear();
              randomizeCharOperation(keyName, 1);
            }
          } while (!_usedKeys.emplace(keyName).second);

          randomizeCharOperation(value, 1);
          _headerSplitInLines.emplace_back(std::move(keyName) + ":" +
                                           std::move(value));
        }
        break;
      }
      default:
        TRI_ASSERT(false);
    }
  }
}

void RequestFuzzer::randomizeHeader(std::string& header,
                                    std::optional<size_t> const payloadSize) {
  if (payloadSize.has_value()) {
    _reqBodySize = payloadSize.value();
  } else {
    _reqBodySize = 0;
  }
  std::string firstLine;
  if (generateRandNumWithinRange<uint32_t>(0, 99) > 0) {
    rest::RequestType randReqType;
    using rt = rest::RequestType;
    do {
      randReqType = static_cast<rest::RequestType>(
          _randContext.mt() %
          static_cast<std::underlying_type<rest::RequestType>::type>(
              rest::RequestType::ILLEGAL));
    } while (randReqType == rt::HEAD);
    if (randReqType == rt::GET || randReqType == rt::HEAD ||
        randReqType == rt::OPTIONS) {
      _reqHasBody = false;
      _reqBodySize = 0;
    }
    firstLine.append(rest::requestToString(randReqType));
  } else {
    randomizeCharOperation(firstLine, 1);
  }
  firstLine.append(" ");
  uint32_t numNestedRoutes =
      generateRandNumWithinRange<uint32_t>(1, kMaxNestedRoutes);
  for (uint32_t i = 0; i < numNestedRoutes; ++i) {
    uint32_t routePos =
        generateRandNumWithinRange<uint32_t>(0, _wordListForRoute.size() - 1);
    if (!(_wordListForRoute[routePos] == "random") ||
        generateRandNumWithinRange<uint32_t>(0, 99) > 20) {
      firstLine.append(_wordListForRoute[routePos]);
    } else {
      randomizeCharOperation(firstLine, 1);
    }
  }
  firstLine.append(" ");
  if (generateRandNumWithinRange<uint32_t>(0, 99) > 0) {
    firstLine.append(" HTTP/");
  } else {
    randomizeCharOperation(firstLine, 1);
  }
  if (generateRandNumWithinRange<uint32_t>(0, 99) > 2) {
    firstLine.append("1.1");
  } else {
    if (_randContext.mt() % 2 == 0) {
      firstLine.append(
          std::to_string(generateRandNumWithinRange<uint32_t>(0, 9)) + "." +
          std::to_string(generateRandNumWithinRange<uint32_t>(0, 9)));
    } else {
      firstLine.append(std::to_string(generateRandInt32()));
    }
  }
  _headerSplitInLines.emplace_back(firstLine);
  randomizeLineOperation(_numIterations);
  if (_reqBodySize) {
    _headerSplitInLines.emplace_back("Content-length:" +
                                     std::to_string(_reqBodySize));
  }
  TRI_ASSERT(header.empty());
  for (uint32_t i = 0; i < _headerSplitInLines.size(); ++i) {
    header.append(_headerSplitInLines[i] + "\r\n");
  }
  header.append("\r\n");  // new line for the end of the header
}

std::optional<std::string> RequestFuzzer::randomizeBody() {
  _reqHasBody = false;
  _headerSplitInLines.clear();
  if (generateRandNumWithinRange<uint32_t>(0, 99) > 30) {
    velocypack::Builder builder;
    randomizeBodyInternal(builder);
    _reqHasBody = true;
    return builder.slice().toString();
    // sample body
    //   return ("{\"database\": true, \"url\": true}");
  } else {
    return std::nullopt;
  }
}

void RequestFuzzer::randomizeBodyInternal(velocypack::Builder& builder) {
  while (true) {
    BodyOperation bodyOp = static_cast<BodyOperation>(
        _randContext.mt() % BodyOperation::kMaxBodyOpValue);
    if (_recursionDepth > kMaxDepth && bodyOp <= BodyOperation::kAddObject) {
      continue;
    }
    switch (bodyOp) {
      case kAddArray: {
        builder.openArray(_randContext.mt() % 2 ? true : false);
        uint32_t numMembers = _randContext.mt() % kArrayNumMembers;
        for (uint32_t i = 0; i < numMembers; ++i) {
          ++_recursionDepth;
          randomizeBodyInternal(builder);
          --_recursionDepth;
        }
        builder.close();
        break;
      }
      case kAddObject: {
        builder.openObject(_randContext.mt() % 2 ? true : false);
        uint32_t numMembers = _randContext.mt() % kObjNumMembers;
        if (_tempObjectKeys.size() <= _recursionDepth) {
          _tempObjectKeys.resize(_recursionDepth + 1);
        } else {
          _tempObjectKeys[_recursionDepth].clear();
        }
        ++_recursionDepth;
        for (uint32_t i = 0; i < numMembers; ++i) {
          while (true) {
            _tempStr.clear();
            generateRandReadableAsciiString(_tempStr);
            if (_tempObjectKeys[_recursionDepth - 1].emplace(_tempStr).second) {
              break;
            }
          }
          // key
          builder.add(velocypack::Value(_tempStr));
          // value
          randomizeBodyInternal(builder);
        }
        --_recursionDepth;
        _tempObjectKeys[_recursionDepth].clear();
        builder.close();
        break;
      }
      case kAddCharSeq: {
        _tempStr.clear();
        generateRandReadableAsciiString(_tempStr);
        builder.add(velocypack::Value(_tempStr));
        break;
      }
      default:
        TRI_ASSERT(false);
    }
    break;
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

void RequestFuzzer::generateRandAsciiChar(std::string& input) {
  input[generateRandNumWithinRange<uint32_t>(0, input.size())] =
      generateRandNumWithinRange<uint32_t>(0x0, 0x7F);
}

void RequestFuzzer::generateRandReadableAsciiString(std::string& input) {
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

};  // namespace arangodb::fuzzer