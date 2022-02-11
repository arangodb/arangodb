////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <atomic>
#include <charconv>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <vector>

#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

struct VPackFormat {};

struct JSONFormat {};

enum FuzzBytes {
  FIRST_BYTE = 0,
  RANDOM_BYTE,
  LAST_BYTE,
  NUM_OPTIONS_BYTES
};

enum FuzzActions {
  REMOVE_BYTE = 0,
  REPLACE_BYTE,
  INSERT_BYTE,
  NUM_OPTIONS_ACTIONS
};

enum RandomBuilderAdditions {
  ADD_ARRAY = 0,
  ADD_OBJECT,
  ADD_BOOLEAN,
  ADD_STRING,
  ADD_NULL,
  ADD_UINT64,
  ADD_INT64,
  ADD_DOUBLE, // here starts values that are only for vpack
  ADD_UTC_DATE,
  ADD_BINARY,
  ADD_EXTERNAL,
  ADD_ILLEGAL,
  ADD_MIN_KEY,
  ADD_MAX_KEY,
  ADD_MAX_VPACK_VALUE
};

struct KnownLimitValues {
  static constexpr uint8_t utf81ByteFirstLowerBound = 0x00;
  static constexpr uint8_t utf81ByteFirstUpperBound = 0x7F;
  static constexpr uint8_t utf82BytesFirstLowerBound = 0xC2;
  static constexpr uint8_t utf82BytesFirstUpperBound = 0xDF;
  static constexpr uint8_t utf83BytesFirstLowerBound = 0xE0;
  static constexpr uint8_t utf83BytesFirstUpperBound = 0xEF;
  static constexpr uint8_t utf83BytesE0ValidatorLowerBound = 0xA0;
  static constexpr uint8_t utf83BytesEDValidatorUpperBound = 0x9F;
  static constexpr uint8_t utf84BytesFirstLowerBound = 0xF0;
  static constexpr uint8_t utf84BytesFirstUpperBound = 0xF4;
  static constexpr uint8_t utf84BytesF0ValidatorLowerBound = 0x90;
  static constexpr uint8_t utf84BytesF4ValidatorUpperBound = 0x8F;
  static constexpr uint8_t utf8CommonLowerBound = 0x80;
  static constexpr uint8_t utf8CommonUpperBound = 0xBF;
  static constexpr uint32_t minUtf8RandStringLength = 0;
  static constexpr uint32_t maxUtf8RandStringLength = 1000;
  static constexpr uint32_t maxBinaryRandLength = 1000;
  static constexpr uint32_t maxDepth = 10;
  static constexpr uint32_t objNumMembers = 10;
  static constexpr uint32_t arrayNumMembers = 10;
  static constexpr uint32_t randBytePosOffset = 8;
  static constexpr uint32_t numFuzzIterations = 10;
};

struct RandomGenerator {
  RandomGenerator(uint64_t seed) : mt64(seed) {}

  std::mt19937_64 mt64; // this is 64 bits for generating uint64_t in ADD_UINT64 for velocypack
};


struct BuilderContext {
  Builder builder;
  RandomGenerator randomGenerator;
  std::string tempString;
  std::vector<std::unordered_set<std::string>> tempObjectKeys;
  uint32_t recursionDepth = 0;
  bool isEvil;

  BuilderContext() = delete;
  BuilderContext(BuilderContext const&) = delete;
  BuilderContext& operator=(BuilderContext const&) = delete;

  BuilderContext(Options const* options, uint64_t seed, bool isEvil)
      : builder(options), randomGenerator(seed), isEvil(isEvil) {}
};

Slice nullSlice(Slice::nullSlice());

static std::mutex mtx;

static void usage(char const* argv[]) {
  std::cout << "Usage: " << argv[0] << " options"
            << std::endl;
  std::cout << "This program creates random VPack or JSON structures and validates them. (Default: VPack)"
            << std::endl;
  std::cout << "Available format options are:" << std::endl;
  std::cout << " --vpack                create VPack"
            << std::endl;
  std::cout << " --json                 create JSON"
            << std::endl;
  std::cout << " --evil                 purposefully fuzz generated VPack, to test for crashes"
            << std::endl;
  std::cout << " --iterations <number>  number of iterations (number > 0). Default: 1"
            << std::endl;
  std::cout << " --threads <number>     number of threads (number > 0). Default: 1"
            << std::endl;
  std::cout << " --seed <number>        number that will be used as seed for random generation (number >= 0). Default: random value"
            << std::endl;
  std::cout << std::endl;
}

static inline bool isOption(char const* arg, char const* expected) {
  return (strcmp(arg, expected) == 0);
}

template <typename T>
static T generateRandWithinRange(T min, T max, RandomGenerator& randomGenerator) {
  return min + (randomGenerator.mt64() % (max + 1 - min));
}

static size_t getBytePos(BuilderContext& ctx, FuzzBytes const& fuzzByte, std::string const& input) {
  size_t inputSize = input.size();
  if (!inputSize) {
    return 0;
  }
  using limits = KnownLimitValues;
  size_t offset = inputSize > (3 * limits::randBytePosOffset) ? limits::randBytePosOffset : 0;
  size_t bytePos = 0;
  if (fuzzByte == FuzzBytes::FIRST_BYTE) {
    bytePos = generateRandWithinRange<size_t>(0, offset, ctx.randomGenerator);
  } else if (fuzzByte == FuzzBytes::RANDOM_BYTE) {
    bytePos = generateRandWithinRange<size_t>(0, inputSize - 1, ctx.randomGenerator);
  } else if (fuzzByte == FuzzBytes::LAST_BYTE) {
    bytePos = generateRandWithinRange<size_t>(inputSize - 1 - offset, inputSize - 1, ctx.randomGenerator);
  }
  return bytePos;
}

static void fuzzBytes(BuilderContext& ctx) {
  using limits = KnownLimitValues;
  for (uint32_t i = 0; i < limits::numFuzzIterations; ++i) {
    FuzzActions fuzzAction = static_cast<FuzzActions>(ctx.randomGenerator.mt64() % FuzzActions::NUM_OPTIONS_ACTIONS);
    FuzzBytes fuzzByte = static_cast<FuzzBytes>(ctx.randomGenerator.mt64() % FuzzBytes::NUM_OPTIONS_BYTES);
    auto& builderBuffer = ctx.builder.bufferRef();
    std::string vpackBytes = builderBuffer.toString();
    size_t bytePos = getBytePos(ctx, fuzzByte, vpackBytes);
    switch (fuzzAction) {
      case REMOVE_BYTE:
        if (vpackBytes.empty()) {
          continue;
        }
        vpackBytes.erase(bytePos, 1);
        break;
      case REPLACE_BYTE:
        if (vpackBytes.empty()) {
          continue;
        }
        vpackBytes[bytePos] = generateRandWithinRange<uint8_t>(0, 255, ctx.randomGenerator);
        break;
      case INSERT_BYTE:
        vpackBytes.insert(bytePos, 1, static_cast<std::string::value_type>(generateRandWithinRange<uint8_t>(0, 255,
                                                                                                            ctx.randomGenerator)));
        break;
      default:
        VELOCYPACK_ASSERT(false);
    }
    builderBuffer.clear();
    builderBuffer.append(reinterpret_cast<uint8_t const*>(vpackBytes.data()), vpackBytes.size());
  }
}

static void generateRandBinary(RandomGenerator& randomGenerator, std::string& output) {
  using limits = KnownLimitValues;
  uint32_t length = randomGenerator.mt64() % limits::maxBinaryRandLength;
  output.reserve(length);
  for (uint32_t i = 0; i < length; ++i) {
    output.push_back(static_cast<std::string::value_type>(randomGenerator.mt64() % 256));
  }
}

static void appendRandUtf8Char(RandomGenerator& randomGenerator, std::string& utf8Str) {
  using limits = KnownLimitValues;
  uint32_t numBytes = generateRandWithinRange(1, 4, randomGenerator);
  switch (numBytes) {
    case 1: {
      utf8Str.push_back(
          generateRandWithinRange<uint8_t>(limits::utf81ByteFirstLowerBound, limits::utf81ByteFirstUpperBound, randomGenerator));
      break;
    }
    case 2: {
      utf8Str.push_back(generateRandWithinRange<uint8_t>(limits::utf82BytesFirstLowerBound, limits::utf82BytesFirstUpperBound,
                                                randomGenerator));
      utf8Str.push_back(
          generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf8CommonUpperBound, randomGenerator));
      break;
    }
    case 3: {
      uint32_t randFirstByte = generateRandWithinRange<uint8_t>(limits::utf83BytesFirstLowerBound,
                                                       limits::utf83BytesFirstUpperBound,
                                                       randomGenerator);
      utf8Str.push_back(randFirstByte);
      if (randFirstByte == 0xE0) {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf83BytesE0ValidatorLowerBound, limits::utf8CommonUpperBound,
                                    randomGenerator));
      } else if (randFirstByte == 0xED) {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf83BytesEDValidatorUpperBound,
                                    randomGenerator));
      } else {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf8CommonUpperBound, randomGenerator));
      }
      utf8Str.push_back(
          generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf8CommonUpperBound, randomGenerator));
      break;
    }
    case 4: {
      uint32_t randFirstByte = generateRandWithinRange<uint8_t>(limits::utf84BytesFirstLowerBound,
                                                       limits::utf84BytesFirstUpperBound,
                                                       randomGenerator);
      utf8Str.push_back(randFirstByte);
      if (randFirstByte == 0xF0) {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf84BytesF0ValidatorLowerBound, limits::utf8CommonUpperBound,
                                    randomGenerator));
      } else if (randFirstByte == 0xF4) {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf84BytesF4ValidatorUpperBound,
                                    randomGenerator));
      } else {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf8CommonUpperBound, randomGenerator));
      }
      for (uint32_t i = 0; i < 2; ++i) {
        utf8Str.push_back(
            generateRandWithinRange<uint8_t>(limits::utf8CommonLowerBound, limits::utf8CommonUpperBound, randomGenerator));
      }
      break;
    }
    default:
      VELOCYPACK_ASSERT(false);
  }
}

static void generateUtf8String(RandomGenerator& randomGenerator, std::string& utf8Str) {
  using limits = KnownLimitValues;
  static_assert(limits::maxUtf8RandStringLength > limits::minUtf8RandStringLength);

  uint32_t length = limits::minUtf8RandStringLength +
                    (randomGenerator.mt64() % (limits::maxUtf8RandStringLength - limits::minUtf8RandStringLength));
  for (uint32_t i = 0; i < length; ++i) {
    appendRandUtf8Char(randomGenerator, utf8Str);
  }
}

template <typename Format>
constexpr RandomBuilderAdditions maxRandomType() {
  if constexpr (std::is_same_v<Format, VPackFormat>) {
    return RandomBuilderAdditions::ADD_MAX_VPACK_VALUE;
  } else {
    return RandomBuilderAdditions::ADD_DOUBLE;
  }
}

template <typename Format>
static void generateVelocypack(BuilderContext& ctx) {
  constexpr RandomBuilderAdditions maxValue = maxRandomType<Format>();

  using limits = KnownLimitValues;

  Builder& builder = ctx.builder;
  RandomGenerator& randomGenerator = ctx.randomGenerator;

  while (true) {
    RandomBuilderAdditions randomBuilderAdds = static_cast<RandomBuilderAdditions>(randomGenerator.mt64() %
                                                                                   maxValue);
    if (ctx.recursionDepth > limits::maxDepth && randomBuilderAdds <= ADD_OBJECT) {
      continue;
    }
    switch (randomBuilderAdds) {
      case ADD_ARRAY: {
        builder.openArray(randomGenerator.mt64() % 2 ? true : false);
        uint32_t numMembers = randomGenerator.mt64() % limits::arrayNumMembers;
        for (uint32_t i = 0; i < numMembers; ++i) {
          ++ctx.recursionDepth;
          generateVelocypack<Format>(ctx);
          --ctx.recursionDepth;
        }
        builder.close();
        break;
      }
      case ADD_OBJECT: {
        builder.openObject(randomGenerator.mt64() % 2 ? true : false);
        uint32_t numMembers = randomGenerator.mt64() % limits::objNumMembers;
        if (ctx.tempObjectKeys.size() <= ctx.recursionDepth) {
          ctx.tempObjectKeys.resize(ctx.recursionDepth + 1);
        } else {
          ctx.tempObjectKeys[ctx.recursionDepth].clear();
        }
        VELOCYPACK_ASSERT(ctx.tempObjectKeys.size() > ctx.recursionDepth);
        ++ctx.recursionDepth;
        for (uint32_t i = 0; i < numMembers; ++i) {
          while (true) {
            ctx.tempString.clear();
            generateUtf8String(randomGenerator, ctx.tempString);
            if (ctx.tempObjectKeys[ctx.recursionDepth - 1].emplace(ctx.tempString).second) {
              break;
            }
          }
          // key
          builder.add(Value(ctx.tempString));
          // value
          generateVelocypack<Format>(ctx);
        }
        --ctx.recursionDepth;
        ctx.tempObjectKeys[ctx.recursionDepth].clear();
        builder.close();
        break;
      }
      case ADD_BOOLEAN:
        builder.add(Value(randomGenerator.mt64() % 2 ? true : false));
        break;
      case ADD_STRING: {
        ctx.tempString.clear();
        generateUtf8String(randomGenerator, ctx.tempString);
        builder.add(Value(ctx.tempString));
        break;
      }
      case ADD_NULL:
        builder.add(Value(ValueType::Null));
        break;
      case ADD_UINT64: {
        uint64_t value = randomGenerator.mt64();
        builder.add(Value(value));
        break;
      }
      case ADD_INT64: {
        uint64_t uintValue = randomGenerator.mt64();
        int64_t intValue;
        memcpy(&intValue, &uintValue, sizeof(uintValue));
        builder.add(Value(intValue));
        break;
      }
      case ADD_DOUBLE: {
        double doubleValue;
        do {
          uint64_t uintValue = randomGenerator.mt64();
          memcpy(&doubleValue, &uintValue, sizeof(uintValue));
        } while (!std::isfinite(doubleValue));
        builder.add(Value(doubleValue));
        break;
      }
      case ADD_UTC_DATE: {
        uint64_t utcDateValue = randomGenerator.mt64();
        builder.add(Value(utcDateValue, ValueType::UTCDate));
        break;
      }
      case ADD_BINARY: {
        ctx.tempString.clear();
        generateRandBinary(randomGenerator, ctx.tempString);
        builder.add(ValuePair(ctx.tempString.data(), ctx.tempString.size(), ValueType::Binary));
        break;
      }
      case ADD_EXTERNAL: {
        builder.add(Value(static_cast<void const*>(&nullSlice), ValueType::External));
        break;
      }
      case ADD_ILLEGAL:
        builder.add(Value(ValueType::Illegal));
        break;
      case ADD_MIN_KEY:
        builder.add(Value(ValueType::MinKey));
        break;
      case ADD_MAX_KEY:
        builder.add(Value(ValueType::MaxKey));
        break;
      default:
        VELOCYPACK_ASSERT(false);
    }
    break;
  }
}

static bool isParamValid(char const* p, uint64_t& value) {
  auto result = std::from_chars(p, p + strlen(p), value);
  if (result.ec != std::errc()) {
    std::cerr << "Error: wrong parameter type: " << p << std::endl;
    return false;
  }
  return true;
}

int main(int argc, char const* argv[]) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  bool isTypeAssigned = false;
  uint32_t numIterations = 1;
  uint32_t numThreads = 1;
  bool isJSON = false;
  std::random_device rd;
  uint64_t seed = rd();
  bool isEvil = false;

  int i = 1;
  while (i < argc) {
    bool isFailure = false;
    char const* p = argv[i];
    if (isOption(p, "--help")) {
      usage(argv);
      return EXIT_SUCCESS;
    } else if (isOption(p, "--vpack") && !isTypeAssigned) {
      isTypeAssigned = true;
      isJSON = false;
    } else if (isOption(p, "--json") && !isTypeAssigned) {
      isTypeAssigned = true;
      isJSON = true;
    } else if (isOption(p, "--iterations")) {
      if (++i >= argc) {
        isFailure = true;
      } else {
        char const* p = argv[i];
        uint64_t value = 0; //these values are uint64_t only for using the same isParamValid function as the seed
        if (!isParamValid(p, value) || !value || value > std::numeric_limits<uint32_t>::max()) {
          isFailure = true;
        } else {
          numIterations = static_cast<uint32_t>(value);
        }
      }
    } else if (isOption(p, "--threads")) {
      if (++i >= argc) {
        isFailure = true;
      } else {
        char const* p = argv[i];
        uint64_t value = 0;
        if (!isParamValid(p, value) || !value) {
          isFailure = true;
        } else {
          numThreads = static_cast<uint32_t>(value);
        }
      }
    } else if (isOption(p, "--seed")) {
      if (++i >= argc) {
        isFailure = true;
      } else {
        char const* p = argv[i];
        uint64_t value = 0;
        if (!isParamValid(p, value)) {
          isFailure = true;
        } else {
          seed = value;
        }
      }
    } else if (isOption(p, "--evil")) {
      isEvil = true;
    } else {
      isFailure = true;
    }
    if (isFailure) {
      usage(argv);
      return EXIT_FAILURE;
    }
    ++i;
  }

  if (isEvil && isJSON) {
    std::cerr << "combining --evil and --json is currently not supported" << std::endl;
    return EXIT_FAILURE;
  }

  std::string_view const progName(isEvil ? "Darkness fuzzer" : "Sunshine fuzzer");

  std::cout
      << progName << " is fuzzing " << (isJSON ? "JSON Parser" : "VPack validator")
      << " with " << numThreads << " thread(s)..."
      << " Iterations: " << numIterations
      << ". Initial seed is " << seed << std::endl;

  uint32_t itsPerThread = numIterations / numThreads;
  uint32_t leftoverIts = numIterations % numThreads;
  std::atomic<bool> stopThreads{false};
  std::atomic<uint64_t> totalValidationErrors{0};

  auto threadCallback = [&stopThreads, &totalValidationErrors]<typename Format>(uint32_t iterations, Format, uint64_t seed, bool isEvil) {
    Options options;
    options.validateUtf8Strings = true;
    options.checkAttributeUniqueness = true;
    options.binaryAsHex = true;
    options.datesAsIntegers = true;

    // extra options for parsing
    Options parseOptions = options;
    parseOptions.clearBuilderBeforeParse = true;
    parseOptions.paddingBehavior = Options::PaddingBehavior::UsePadding;
    parseOptions.dumpAttributesInIndexOrder = false;

    Options validationOptions = options;
    if (isEvil) {
      validationOptions.disallowExternals = true;
      validationOptions.disallowBCD = true;
      validationOptions.disallowCustom = true;
    }
      
    // parser used for JSON-parsing
    Parser parser(&parseOptions);
    // validator used for VPack validation
    Validator validator(&validationOptions);

    uint64_t validationErrors = 0;
    BuilderContext ctx(&options, seed, isEvil);

    try {
      while (iterations-- > 0 && !stopThreads.load(std::memory_order_relaxed)) {
        // clean up for new iteration
        ctx.builder.clear();
        ctx.tempObjectKeys.clear();
        VELOCYPACK_ASSERT(ctx.recursionDepth == 0);

        // generate vpack
        generateVelocypack<Format>(ctx);

        if (isEvil) {
          // intentionally bork the velocypack
          fuzzBytes(ctx);
        }

        try {
          auto& builderBuffer = ctx.builder.bufferRef();
          if constexpr (std::is_same_v<Format, JSONFormat>) {
            Slice s(builderBuffer.data());
            VELOCYPACK_ASSERT(s.byteSize() == builderBuffer.size());
            ctx.tempString.clear();
            parser.parse(s.toJson(ctx.tempString));
          } else {
            validator.validate(builderBuffer.data(), builderBuffer.size());
          }
        } catch (std::exception const &e) {
          ++validationErrors;
          if (!isEvil) {
            // we don't expect any exceptions in non-evil mode, so rethrow them.
            throw;
          }
          // in evil mode, we can't tell if the borked velocypack is valid or not.
          // so have to catch the exception and silence it.
        }
      }
    } catch (std::exception const &e) {
      // need a mutex to prevent multiple threads from writing to std::cerr at the
      // same time
      std::lock_guard<std::mutex> lock(mtx);
      std::cerr 
          << "Caught unexpected exception during fuzzing: " << e.what() << std::endl
          << "Slice data:" << std::endl;
      auto& builderBuffer = ctx.builder.bufferRef();
      if constexpr (std::is_same_v<Format, JSONFormat>) {
        std::cerr << Slice(builderBuffer.data()).toJson() << std::endl;
      } else {
        std::cerr << HexDump(builderBuffer.data(), builderBuffer.size()) << std::endl;
      }
      stopThreads.store(true);
    }
    // add to global counter
    totalValidationErrors.fetch_add(validationErrors);
  };

  std::vector<std::thread> threads;
  threads.reserve(numThreads);
  auto joinThreads = [&threads]() {
    for (auto &t: threads) {
      t.join();
    }
  };

  try {
    for (uint32_t i = 0; i < numThreads; ++i) {
      uint32_t iterations = itsPerThread;
      if (i == numThreads - 1) {
        iterations += leftoverIts;
      }
      if (isJSON) {
        threads.emplace_back(threadCallback, iterations, JSONFormat{}, seed + i, isEvil);
      } else {
        threads.emplace_back(threadCallback, iterations, VPackFormat{}, seed + i, isEvil);
      }
    }
    joinThreads();
  } catch (std::exception const&) {
    stopThreads.store(true);
    joinThreads();
  }

  std::cout << "Caught " << totalValidationErrors.load() << " validation error(s).";
  if (isEvil) {
    std::cout << " These are potentially legitimate.";
  }
  std::cout << std::endl;

  // return proper exit code
  return stopThreads.load() ? EXIT_FAILURE : EXIT_SUCCESS;

  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
