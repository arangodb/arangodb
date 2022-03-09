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

#include <array>
#include <charconv>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "Basics/VelocyPackHelper.h"

namespace arangodb {
namespace fuzzer {

enum CharOperation { kAddString = 0, kAddInt32, kMaxCharOpValue };

enum LineOperation {
  kCopyLine = 0,
  kInjectRandByteInLine,
  kAddLine,
  kMaxLineOpValue
};

enum BodyOperation { kAddArray = 0, kAddObject, kAddCharSeq, kMaxBodyOpValue };

class RequestFuzzer {
  struct RandContext {
    RandContext(uint32_t seed) : mt(seed) {}

    std::mt19937 mt;
    static constexpr uint32_t maxRandAsciiStringLength = 50;
  };

  struct Result {
    std::string restVerb;
    std::string location;
  };

 public:
  RequestFuzzer(std::optional<uint32_t> numIt, std::optional<uint32_t> seed)
      : _numIterations(numIt.value_or(limitNumIterations)),
        _seed(seed.value_or(std::random_device()())),
        _randContext{_seed},
        _stringLines{},
        _keysAndValues{},
        _reqHasBody(false) {}

  void randomizeHeader(std::string& header, std::optional<size_t> payloadSize);
  std::optional<std::string> randomizeBody();
  void randomizeBodyInternal(velocypack::Builder& builder);
  uint32_t getSeed() { return _seed; }

 private:
  void randomizeCharOperation(std::string& input, uint32_t numIts);

  void randomizeLineOperation(uint32_t numIts);

  template<typename T>
  T generateRandNumWithinRange(T min, T max);

  void generateRandReadableAsciiString(std::string& input);
  void generateRandAsciiChar(std::string& input);

  int32_t generateRandInt32();

  static constexpr size_t limitNumIterations = 10;
  uint32_t _numIterations;
  uint32_t _seed;
  RandContext _randContext;
  std::vector<std::string> _stringLines;
  std::unordered_map<std::string, std::string> _keysAndValues;
  std::string _tempStr;
  bool _reqHasBody;
  std::string _reqBody;
  size_t _reqBodySize;

  static constexpr uint32_t _maxNestedRoutes = 4;
  static constexpr uint32_t _maxDepth = 4;
  static constexpr uint32_t _objNumMembers = 4;
  static constexpr uint32_t _arrayNumMembers = 4;
  uint32_t _recursionDepth = 0;
  std::vector<std::unordered_set<std::string>> _tempObjectKeys;

  static constexpr std::array<std::string_view, 13> _wordListForRoute = {
      {"/_db", "/_admin", "/_api", "/_system", "/_cursor", "/version",
       "/status", "/license", "/collection", "/database", "/current", "/log",
       "random"}};

  static constexpr std::array<std::string_view, 48> _wordListForKeys = {
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
       "Content-length",
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
};
}  // namespace fuzzer
}  // namespace arangodb
