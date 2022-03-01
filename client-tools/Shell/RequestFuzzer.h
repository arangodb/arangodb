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

#include <charconv>
#include <deque>
#include <map>
#include <random>
#include <string>
#include <unordered_map>
#include <Logger/LogMacros.h>

namespace arangodb {
namespace fuzzer {

enum CharOperation { ADD_STRING, ADD_INT_32, MAX_CHAR_OP_VALUE };

enum LineOperation {

  COPY_LINE,
  REMOVE_LINE,
  ADD_LINE,
  MAX_LINE_OP_VALUE
};

enum RequestType {
  GET,
  PUT,
  POST,
  PATCH,
  DELETE,
  HEAD,
  RAND_REQ_TYPE,
  MAX_REQ_VALUE
};

enum CommonRoute {
  WORD1,
  WORD2,
  WORD3,
  WORD4,
  WORD5,
  WORD6,
  RAND_COMMON_ROUTE,
  MAX_ROUTE_VALUE
};

enum class HttpWord { HTTP, RAND_HTTP_WORD, MAX_VALUE };

enum class HttpVersion { ACTUAL, RAND_HTTP_VERSION, MAX_VALUE };

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
  RequestFuzzer(uint32_t numIt = limitNumIterations,
                uint32_t seed = std::random_device()())
      : _numIterations(numIt),
        _seed(seed),
        _randContext{seed},
        _stringLines{} {}

  void randomizeHeader(std::string &header);

 private:
  void randomizeCharOperation(std::string &input, uint32_t numIts = 1);

  void randomizeLineOperation(uint32_t numIts = 1);

  template<typename T>
  T generateRandNumWithinRange(T min, T max);

  void generateRandAsciiString(std::string &input);

  static constexpr size_t limitNumIterations = 10;
  uint32_t _numIterations;
  uint32_t _seed;
  RandContext _randContext;
  std::vector<std::string> _stringLines;
  RequestType _reqType;
  HttpWord _httpWord;
  HttpVersion _httpVersion;

  const std::unordered_map<RequestType, std::string> _requestTypes = {
      {RequestType::GET, "GET"},       {RequestType::PUT, "PUT"},
      {RequestType::POST, "POST"},     {RequestType::PATCH, "PATCH"},
      {RequestType::DELETE, "DELETE"}, {RequestType::HEAD, "HEAD"}};

  const std::unordered_map<CommonRoute, std::string> _routeWordList = {
      {CommonRoute::WORD1, "_db"},     {CommonRoute::WORD2, "_admin"},
      {CommonRoute::WORD3, "_api"},    {CommonRoute::WORD4, "_system"},
      {CommonRoute::WORD5, "_cursor"}, {CommonRoute::WORD6, "aardvak"}};
};
}  // namespace fuzzer
}  // namespace arangodb
