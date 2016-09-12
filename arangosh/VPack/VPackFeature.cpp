////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "ProgramOptions/ProgramOptions.h"

#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/Validator.h>
#include <velocypack/velocypack-aliases.h>

#include <iostream>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

////////////////////////////////////////////////////////////////////////////////
/// @brief portably and safely reads a number
////////////////////////////////////////////////////////////////////////////////
  
template <typename T>
static inline T ReadNumber(uint8_t const* source, uint32_t length) {
  T value = 0;
  uint64_t x = 0;
  uint8_t const* end = source + length;
  do {
    value += static_cast<T>(*source++) << x;
    x += 8;
  } while (source < end);
  return value;
}

static std::string ConvertFromHex(std::string const& value) {
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

  void dump(VPackSlice const& value, VPackDumper* dumper,
            VPackSlice const& base) override final {
    dumper->appendString(toString(value, nullptr, base));
  }
  
  std::string toString(VPackSlice const& value, VPackOptions const* options,
                       VPackSlice const& base) override final {
    uint64_t cid = ReadNumber<uint64_t>(value.begin() + 1, sizeof(uint64_t));
    return "collection id " + std::to_string(cid);
  }
};


VPackFeature::VPackFeature(application_features::ApplicationServer* server,
                           int* result)
    : ApplicationFeature(server, "VPack"),
      _result(result),
      _prettyPrint(true),
      _hexInput(false),
      _printNonJson(true) {
  requiresElevatedPrivileges(false);
  setOptional(false);
}

void VPackFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption(
      "--input-file", "input filename",
      new StringParameter(&_inputFile));
  options->addOption(
      "--output-file", "output filename",
      new StringParameter(&_outputFile));
  options->addOption(
      "--pretty", "pretty print result",
      new BooleanParameter(&_prettyPrint));
  options->addOption(
      "--hex", "read hex-encoded input",
      new BooleanParameter(&_hexInput));
  options->addOption(
      "--print-non-json", "print non-JSON types",
      new BooleanParameter(&_printNonJson));
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

  std::string s = basics::FileUtils::slurp(_inputFile);

  if (_hexInput) {
    s = ConvertFromHex(s);
  }

  auto customTypeHandler = std::make_unique<CustomTypeHandler>();
  
  VPackOptions options;
  options.prettyPrint = _prettyPrint;
  options.unsupportedTypeBehavior = 
    (_printNonJson ? VPackOptions::ConvertUnsupportedType : VPackOptions::FailOnUnsupportedType);
  options.customTypeHandler = customTypeHandler.get();

  try {
    VPackValidator validator(&options);
    validator.validate(s.c_str(), s.size(), false);
  } catch (std::exception const& ex) {
    std::cerr << "Invalid VPack input while processing infile '" << _inputFile
              << "': " << ex.what() << std::endl;
    *_result = TRI_ERROR_INTERNAL;
    return;
  }

  VPackSlice const slice(s.c_str());

  VPackBuffer<char> buffer(4096);
  VPackCharBufferSink sink(&buffer);
  VPackDumper dumper(&sink, &options);

  try {
    dumper.dump(slice);
  } catch (std::exception const& ex) {
    std::cerr << "An exception occurred while processing infile '" << _inputFile
              << "': " << ex.what() << std::endl;
    *_result = TRI_ERROR_INTERNAL;
    return;
  } catch (...) {
    std::cerr << "An unknown exception occurred while processing infile '"
              << _inputFile << "'" << std::endl;
    *_result = TRI_ERROR_INTERNAL;
    return;
  }

  std::ofstream ofs(_outputFile, std::ofstream::out);

  if (!ofs.is_open()) {
    std::cerr << "Cannot write outfile '" << _outputFile << "'" << std::endl;
    *_result = TRI_ERROR_INTERNAL;
    return;
  }
  
  // reset stream
  // cppcheck-suppress *
  if (!toStdOut) {
    ofs.seekp(0);
  }
  
  // write into stream
  char const* start = buffer.data();
  ofs.write(start, buffer.size());
  ofs.close();

  // cppcheck-suppress *
  if (!toStdOut) {
    std::cout << "Successfully converted JSON infile '" << _inputFile << "'"
              << std::endl;
    std::cout << "VPack Infile size: " << s.size() << std::endl;
    std::cout << "JSON Outfile size: " << buffer.size() << std::endl;
  }

  *_result = TRI_ERROR_NO_ERROR;
}
