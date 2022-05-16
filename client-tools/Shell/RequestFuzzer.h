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

#include <fuerte/message.h>

namespace arangodb::fuzzer {

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
    explicit RandContext(uint32_t seed) : mt(seed) {}

    std::mt19937 mt;
    static constexpr uint32_t maxRandAsciiStringLength = 50;
  };

  struct Result {
    std::string restVerb;
    std::string location;
  };

 public:
  RequestFuzzer(std::optional<uint32_t> numIt, std::optional<uint32_t> seed)
      : _numIterations(numIt.value_or(defaultNumIterations)),
        _seed(seed.value_or(std::random_device()())),
        _randContext{_seed},
        _headerSplitInLines{} {}

  [[nodiscard]] uint32_t getSeed() const noexcept { return _seed; }

  std::unique_ptr<fuerte::Request> createRequest();

 private:
  fuerte::RestVerb generateHeader(std::string& header);

  void generateBody(velocypack::Builder& builder);

  void randomizeCharOperation(std::string& input, uint32_t numIts);

  void randomizeLineOperation(uint32_t numIts);

  template<typename T>
  T generateRandNumWithinRange(T min, T max);

  void generateRandAlphaNumericString(std::string& input);
  void generateRandAsciiChar(std::string& input);
  void generateRandAlphaNumericChar(std::string& input);

  int32_t generateRandInt32();

  static constexpr size_t defaultNumIterations = 10;
  uint32_t _numIterations;
  uint32_t _seed;
  RandContext _randContext;
  std::vector<std::string> _headerSplitInLines;
  std::string _tempStr;

  static constexpr uint32_t kMaxNestedRoutes = 4;
  static constexpr uint32_t kMaxDepth = 4;
  static constexpr uint32_t kObjNumMembers = 4;
  static constexpr uint32_t kArrayNumMembers = 4;

  uint32_t _recursionDepth = 0;
  std::vector<std::unordered_set<std::string>> _tempObjectKeys;
  std::unordered_set<std::string> _usedKeys;
};

}  // namespace arangodb::fuzzer
