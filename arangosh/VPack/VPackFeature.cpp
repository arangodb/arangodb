////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "VPackFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"

#include <velocypack/Dumper.h>
#include <velocypack/HexDump.h>
#include <velocypack/Options.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>
#include <unordered_set>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

////////////////////////////////////////////////////////////////////////////////
/// @brief portably and safely reads a number
////////////////////////////////////////////////////////////////////////////////

namespace {

template <typename T>
static inline T readNumber(uint8_t const* source, uint32_t length) {
  T value = 0;
  uint64_t x = 0;
  uint8_t const* end = source + length;
  do {
    value += static_cast<T>(*source++) << x;
    x += 8;
  } while (source < end);
  return value;
}

static std::string convertFromHex(std::string const& value) {
  std::string result;
  result.reserve(value.size());

  size_t const n = value.size();
  int prev = -1;

  for (size_t i = 0; i < n; ++i) {
    int current;

    if (value[i] >= '0' && value[i] <= '9') {
      current = value[i] - '0';
    } else if (value[i] >= 'a' && value[i] <= 'f') {
      current = 10 + (value[i] - 'a');
    } else if (value[i] >= 'A' && value[i] <= 'F') {
      current = 10 + (value[i] - 'A');
    } else {
      prev = -1;
      continue;
    }

    if (prev == -1) {
      // first part of two-byte sequence
      prev = current;
    } else {
      // second part of two-byte sequence
      result.push_back(static_cast<unsigned char>((prev << 4) + current));
      prev = -1;
    }
  }

  return result;
}

// custom type value handler, used for deciphering the _id attribute
struct CustomTypeHandler : public VPackCustomTypeHandler {
  CustomTypeHandler() = default;
  ~CustomTypeHandler() = default;

  void dump(VPackSlice const& value, VPackDumper* dumper, VPackSlice const& base) override final {
    dumper->appendString(toString(value, nullptr, base));
  }

  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    uint64_t cid = ::readNumber<uint64_t>(value.begin() + 1, sizeof(uint64_t));
    return "collection id " + std::to_string(cid);
  }
};

}  // namespace

VPackFeature::VPackFeature(application_features::ApplicationServer& server, int* result)
    : ApplicationFeature(server, "VPack"),
      _result(result),
      _inputType("vpack"),
      _outputType("json-pretty"),
      _failOnNonJson(true) {
  requiresElevatedPrivileges(false);
  setOptional(false);
}

void VPackFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  std::unordered_set<std::string> const inputTypes{{"json", "json-hex", "vpack", "vpack-hex" }};
  std::unordered_set<std::string> const outputTypes{{"json", "json-pretty", "vpack", "vpack-hex" }};

  options->addOption("--input-file", 
#ifdef __linux__
                     "input filename (leave empty or use \"-\" for stdin)",
#else
                     "input filename",
#endif
                     new StringParameter(&_inputFile));

  options->addOption("--output-file", 
#ifdef __linux__
                     "output filename (leave empty or use \"+\" for stdout)",
#else
                     "output filename", 
#endif
                     new StringParameter(&_outputFile));

  options->addOption("--input-type", "type of input", new DiscreteValuesParameter<StringParameter>(&_inputType, inputTypes));
  options->addOption("--output-type", "type of output", new DiscreteValuesParameter<StringParameter>(&_outputType, outputTypes));
  options->addOption("--fail-on-non-json", "fail when trying to emit non-JSON types to JSON output",
                     new BooleanParameter(&_failOnNonJson));
}

void VPackFeature::start() {
  *_result = EXIT_SUCCESS;

  bool toStdOut = false;
#ifdef __linux__
  // treat "-" as stdin. quick hack for linux
  if (_inputFile.empty() || _inputFile == "-") {
    _inputFile = "/proc/self/fd/0";
  }

  // treat missing outfile as stdout
  if (_outputFile.empty() || _outputFile == "+") {
    _outputFile = "/proc/self/fd/1";
    toStdOut = true;
  }
#endif

  // read ipnut
  std::string input = basics::FileUtils::slurp(_inputFile);

  bool const inputIsJson = (_inputType == "json" || _inputType == "json-hex" || _inputType == "json-pretty");
  bool const inputIsHex = (_inputType == "json-hex" || _inputType == "vpack-hex");
  if (inputIsHex) {
    input = ::convertFromHex(input);
  }

  ::CustomTypeHandler customTypeHandler;

  VPackSlice slice;
  std::shared_ptr<VPackBuilder> builder;

  if (inputIsJson) {
    // JSON input
    try {
      builder = VPackParser::fromJson(input);
      slice = builder->slice();
    } catch (std::exception const& ex) {
      LOG_TOPIC("d654d", ERR, Logger::FIXME)
          << "invalid JSON input while processing infile '" << _inputFile
          << "': " << ex.what();
      *_result = EXIT_FAILURE;
      return;
    }
  } else {
    // vpack input
    try {
      VPackValidator validator;
      validator.validate(input.data(), input.size(), false);
    } catch (std::exception const& ex) {
      LOG_TOPIC("4c05d", ERR, Logger::FIXME)
          << "invalid VPack input while processing infile '" << _inputFile
          << "': " << ex.what();
      *_result = EXIT_FAILURE;
      return;
    }

    slice = VPackSlice(reinterpret_cast<uint8_t const*>(input.data()));
  }

  
  // produce output
  std::ofstream ofs(_outputFile, std::ofstream::out);

  if (!ofs.is_open()) {
    LOG_TOPIC("bb8a7", ERR, Logger::FIXME) << "cannot write outfile '" << _outputFile << "'";
    *_result = EXIT_FAILURE;
    return;
  }

  // reset stream
  // cppcheck-suppress *
  if (!toStdOut) {
    ofs.seekp(0);
  }

  bool const outputIsJson = (_outputType == "json" || _outputType == "json-pretty");
  bool const outputIsHex = (_outputType == "vpack-hex");
  if (outputIsJson) {
    // JSON output
    VPackOptions options;
    options.prettyPrint = (_outputType == "json-pretty");
    options.unsupportedTypeBehavior =
        (_failOnNonJson ? VPackOptions::ConvertUnsupportedType : VPackOptions::FailOnUnsupportedType);
    options.customTypeHandler = &customTypeHandler;
  
    arangodb::velocypack::OutputFileStreamSink sink(&ofs);
    VPackDumper dumper(&sink, &options);

    try {
      dumper.dump(slice);
    } catch (std::exception const& ex) {
      LOG_TOPIC("ed2fb", ERR, Logger::FIXME)
          << "caught exception while processing infile '" << _inputFile
          << "': " << ex.what();
      *_result = EXIT_FAILURE;
      return;
    } catch (...) {
      LOG_TOPIC("29ad4", ERR, Logger::FIXME)
          << "caught unknown exception occurred while processing infile '"
          << _inputFile << "'";
      *_result = EXIT_FAILURE;
      return;
    }
  } else if (outputIsHex) {
    // vpack hex output
    ofs << VPackHexDump(slice);
  } else {
    // vpack output
    char const* start = slice.startAs<char const>();
    ofs.write(start, slice.byteSize());
  }

  ofs.close();

  // cppcheck-suppress *
  if (!toStdOut) {
    LOG_TOPIC("0a90f", INFO, Logger::FIXME)
        << "successfully processed infile '" << _inputFile << "'";
  }

  *_result = EXIT_SUCCESS;
}
