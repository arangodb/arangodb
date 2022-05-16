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

namespace {

static constexpr char alphaNumericChars[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";

static constexpr std::array<std::string_view, 13> wordListForRoute = {
    {"/_db", "/_admin", "/_api", "/_system", "/_cursor", "/version", "/status",
     "/license", "/collection", "/database", "/current", "/log", "/"}};

static constexpr std::array<std::string_view, 48> wordListForKeys = {
    {"Accept",
     "",
     "Accept-Charset",
     "Accept-Encoding",
     "Accept-Language",
     "Accept-Ranges",
     "Allow",
     "Authorization",
     "Cache-control",
     "Connection",
     "Content-encoding",
     "Content-language",
     "Content-location",
     "Content-MD5",
     "Content-range",
     "Content-type",
     "Date",
     "ETag",
     "Expect",
     "Expires",
     "From",
     "Host",
     "If-Match",
     "If-modified-since",
     "If-none-match",
     "If-range",
     "If-unmodified-since",
     "Last-modified",
     "Location",
     "Max-forwards",
     "Pragma",
     "Proxy-authenticate",
     "Proxy-authorization",
     "Range",
     "Referer",
     "Retry-after",
     "Server",
     "TE",
     "Trailer",
     "Transfer-encoding",
     "Upgrade",
     "User-agent",
     "Vary",
     "Via",
     "Warning",
     "Www-authenticate",
     "random"}};

}  // namespace

namespace arangodb::fuzzer {

void RequestFuzzer::randomizeCharOperation(std::string& input,
                                           uint32_t numIts) {
  while (numIts--) {
    CharOperation charOp = static_cast<CharOperation>(
        _randContext.mt() % CharOperation::kMaxCharOpValue);
    switch (charOp) {
      case CharOperation::kAddString:
        generateRandAlphaNumericString(input);
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
          uint32_t randPos = generateRandNumWithinRange<uint32_t>(
              1, static_cast<uint32_t>(_headerSplitInLines.size()) - 1);
          if (generateRandNumWithinRange<uint32_t>(0, 99) < 10) {
            generateRandAsciiChar(_headerSplitInLines[randPos]);
          } else {
            generateRandAlphaNumericChar(_headerSplitInLines[randPos]);
          }
        }
        break;
      }
      case LineOperation::kCopyLine: {
        if (_headerSplitInLines.size() > 5) {
          uint32_t index = generateRandNumWithinRange<uint32_t>(
              1, static_cast<uint32_t>(_headerSplitInLines.size()) - 1);
          _headerSplitInLines.emplace_back(_headerSplitInLines.at(index));
        }
        break;
      }
      case LineOperation::kAddLine: {
        if (_usedKeys.size() <= wordListForKeys.size() - 5) {
          std::string keyName;
          do {
            uint32_t keyPos = generateRandNumWithinRange<uint32_t>(
                0, static_cast<uint32_t>(wordListForKeys.size()) - 1);
            if (wordListForKeys[keyPos] != "random") {
              keyName = wordListForKeys[keyPos];
            } else {
              keyName.clear();
              randomizeCharOperation(keyName, 1);
            }
          } while (!_usedKeys.emplace(keyName).second);

          std::string value;
          if (keyName == "Authorization") {
            // add random HTTP header authorization data in some cases
            uint32_t v = generateRandNumWithinRange<uint32_t>(0, 99);
            if (v >= 75) {
              value = "Basic ";
            } else if (v >= 50) {
              value = "Bearer ";
            }
          }
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

std::unique_ptr<fuerte::Request> RequestFuzzer::createRequest() {
  // reset internal state
  _headerSplitInLines.clear();
  _recursionDepth = 0;
  _tempObjectKeys.clear();
  _usedKeys.clear();

  std::string header;
  auto req = std::make_unique<fuerte::Request>();
  fuerte::RestVerb requestType = generateHeader(header);
  req->header.restVerb = requestType;

  if (requestType != fuerte::RestVerb::Get &&
      requestType != fuerte::RestVerb::Head &&
      requestType != fuerte::RestVerb::Options) {
    velocypack::Builder builder;
    generateBody(builder);
    if (_randContext.mt() % 2 == 0) {
      std::string bodyAsStr = builder.slice().toString();
      header.append("Content-length:" + std::to_string(bodyAsStr.size()) +
                    "\r\n");
      req->addBinary(reinterpret_cast<uint8_t const*>(&bodyAsStr[0]),
                     bodyAsStr.length());
    } else {
      req->addBinary(builder.slice().start(), builder.slice().byteSize());
      header.append("Content-length:" +
                    std::to_string(builder.slice().byteSize()) + "\r\n");
      req->header.contentType(fuerte::ContentType::VPack);
    }
  }
  header.append("\r\n");
  req->setFuzzReqHeader(std::move(header));

  return req;  // for not preventing copy elision
}

fuerte::RestVerb RequestFuzzer::generateHeader(std::string& header) {
  fuerte::RestVerb requestType = fuerte::RestVerb::Illegal;

  std::string firstLine;
  if (generateRandNumWithinRange<uint32_t>(0, 99) > 0) {
    requestType = static_cast<fuerte::RestVerb>(
        _randContext.mt() %
        (static_cast<uint32_t>(fuerte::RestVerb::Options) + 1));
    firstLine.append(fuerte::to_string(requestType));
  } else {
    randomizeCharOperation(firstLine, 1);
  }
  firstLine.append(" ");
  uint32_t numNestedRoutes =
      generateRandNumWithinRange<uint32_t>(1, kMaxNestedRoutes);
  for (uint32_t i = 0; i < numNestedRoutes; ++i) {
    uint32_t routePos = generateRandNumWithinRange<uint32_t>(
        0, static_cast<uint32_t>(wordListForRoute.size()) - 1);
    if (generateRandNumWithinRange<uint32_t>(0, 99) > 10) {
      firstLine.append(wordListForRoute[routePos]);
    } else {
      firstLine.append("/");
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
  TRI_ASSERT(header.empty());
  for (size_t i = 0; i < _headerSplitInLines.size(); ++i) {
    header.append(_headerSplitInLines[i] + "\r\n");
  }

  return requestType;
}

void RequestFuzzer::generateBody(velocypack::Builder& builder) {
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
          generateBody(builder);
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
            generateRandAlphaNumericString(_tempStr);
            if (_tempObjectKeys[_recursionDepth - 1].emplace(_tempStr).second) {
              break;
            }
          }
          // key
          builder.add(velocypack::Value(_tempStr));
          // value
          generateBody(builder);
        }
        --_recursionDepth;
        _tempObjectKeys[_recursionDepth].clear();
        builder.close();
        break;
      }
      case kAddCharSeq: {
        _tempStr.clear();
        generateRandAlphaNumericString(_tempStr);
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
  TRI_ASSERT(!input.empty());
  // abort loop after x many tries, in case we only have ':' chars
  size_t triesLeft = 10;
  uint32_t randPos;
  do {
    randPos = generateRandNumWithinRange<uint32_t>(
        0, static_cast<uint32_t>(input.size()) - 1);
  } while (input[randPos] == ':' && triesLeft-- > 0);
  input[randPos] = generateRandNumWithinRange<uint32_t>(0x0, 0x7F);
}

void RequestFuzzer::generateRandAlphaNumericChar(std::string& input) {
  TRI_ASSERT(!input.empty());
  // abort loop after x many tries, in case we only have ':' chars
  size_t triesLeft = 10;
  uint32_t randPos;
  do {
    randPos = generateRandNumWithinRange<uint32_t>(
        0, static_cast<uint32_t>(input.size()) - 1);
  } while (input[randPos] == ':' && triesLeft-- > 0);
  input[randPos] =
      alphaNumericChars[_randContext.mt() % (sizeof(alphaNumericChars) - 1)];
}

void RequestFuzzer::generateRandAlphaNumericString(std::string& input) {
  uint32_t randStrLength = generateRandNumWithinRange<uint32_t>(
      1, _randContext.maxRandAsciiStringLength);

  input.reserve(input.size() + randStrLength);
  for (uint32_t i = 0; i < randStrLength; ++i) {
    input +=
        alphaNumericChars[_randContext.mt() % (sizeof(alphaNumericChars) - 1)];
  }
}

}  // namespace arangodb::fuzzer
