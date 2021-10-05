////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <string>
#include <fstream>

#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

static void usage(char* argv[]) {
#ifdef __linux__
  std::cout << "Usage: " << argv[0] << " [OPTIONS] INFILE [OUTFILE]"
            << std::endl;
#else
  std::cout << "Usage: " << argv[0] << " [OPTIONS] INFILE OUTFILE" << std::endl;
#endif
  std::cout << "This program reads the VPack INFILE into a string and saves its"
            << std::endl;
  std::cout << "JSON representation in file OUTFILE. Will work only for input"
            << std::endl;
  std::cout << "files up to 2 GB size." << std::endl;
#ifdef __linux__
  std::cout << "If no OUTFILE is specified, the generated JSON value be"
            << std::endl;
  std::cout << "printed to stdout." << std::endl;
#endif
  std::cout << "Available options are:" << std::endl;
  std::cout << " --pretty                  pretty-print JSON output" << std::endl;
  std::cout << " --no-pretty               don't pretty print JSON output" << std::endl;
  std::cout << " --print-unsupported       convert non-JSON types into something else" << std::endl;
  std::cout << " --no-print-unsupported    fail when encoutering a non-JSON type" << std::endl;
  std::cout << " --hex                     try to turn hex-encoded input into binary vpack" << std::endl;
  std::cout << " --validate                validate input VelocyPack data" << std::endl;
  std::cout << " --no-validate             don't validate input VelocyPack data" << std::endl;
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

static inline bool isOption(char const* arg, char const* expected) {
  return (strcmp(arg, expected) == 0);
}

int main(int argc, char* argv[]) {
  VELOCYPACK_GLOBAL_EXCEPTION_TRY

  char const* infileName = nullptr;
  char const* outfileName = nullptr;
  bool allowFlags = true;
  bool pretty = true;
  bool printUnsupported = true;
  bool hex = false;
  bool validate = true;

  int i = 1;
  while (i < argc) {
    char const* p = argv[i];
    if (allowFlags && isOption(p, "--help")) {
      usage(argv);
      return EXIT_SUCCESS;
    } else if (allowFlags && isOption(p, "--pretty")) {
      pretty = true;
    } else if (allowFlags && isOption(p, "--no-pretty")) {
      pretty = false;
    } else if (allowFlags && isOption(p, "--print-unsupported")) {
      printUnsupported = true;
    } else if (allowFlags && isOption(p, "--no-print-unsupported")) {
      printUnsupported = false;
    } else if (allowFlags && isOption(p, "--hex")) {
      hex = true;
    } else if (allowFlags && isOption(p, "--validate")) {
      validate = true;
    } else if (allowFlags && isOption(p, "--no-validate")) {
      validate = false;
    } else if (allowFlags && isOption(p, "--")) {
      allowFlags = false;
    } else if (infileName == nullptr) {
      infileName = p;
    } else if (outfileName == nullptr) {
      outfileName = p;
    } else {
      usage(argv);
      return EXIT_FAILURE;
    }
    ++i;
  }

#ifdef __linux__
  if (infileName == nullptr) {
    infileName = "-";
  }
#endif

  if (infileName == nullptr) {
    usage(argv);
    return EXIT_FAILURE;
  }

#ifdef __linux__
  // treat missing outfile as stdout
  bool toStdOut = false;
  if (outfileName == nullptr || strcmp(outfileName, "+") == 0) {
    outfileName = "/proc/self/fd/1";
    toStdOut = true;
  }
#else
  bool const toStdOut = false;
  if (outfileName == nullptr) {
    usage(argv);
    return EXIT_FAILURE;
  }
#endif

  // treat "-" as stdin
  std::string infile = infileName;
#ifdef __linux__
  if (infile == "-") {
    infile = "/proc/self/fd/0";
  }
#endif

  std::string s;
  std::ifstream ifs(infile, std::ifstream::in);

  if (!ifs.is_open()) {
    std::cerr << "Cannot read infile '" << infile << "'" << std::endl;
    return EXIT_FAILURE;
  }

  {
    char buffer[32768];
    s.reserve(sizeof(buffer));

    while (ifs.good()) {
      ifs.read(&buffer[0], sizeof(buffer));
      s.append(buffer, checkOverflow(ifs.gcount()));
    }
  }
  ifs.close();

  if (hex) {
    s = convertFromHex(s);
  }
  
  if (validate) {
    Validator validator;
    validator.validate(reinterpret_cast<uint8_t const*>(s.data()), s.size(), false);
  }

  Slice const slice(reinterpret_cast<uint8_t const*>(s.data()));

  Options options;
  options.prettyPrint = pretty;
  options.unsupportedTypeBehavior = 
    (printUnsupported ? Options::ConvertUnsupportedType : Options::FailOnUnsupportedType);

  Buffer<char> buffer(4096);
  CharBufferSink sink(&buffer);
  Dumper dumper(&sink, &options);

  try {
    dumper.dump(slice);
  } catch (Exception const& ex) {
    std::cerr << "An exception occurred while processing infile '" << infile
              << "': " << ex.what() << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "An unknown exception occurred while processing infile '"
              << infile << "'" << std::endl;
    return EXIT_FAILURE;
  }

  std::ofstream ofs(outfileName, std::ofstream::out);

  if (!ofs.is_open()) {
    std::cerr << "Cannot write outfile '" << outfileName << "'" << std::endl;
    return EXIT_FAILURE;
  }

  // reset stream
  if (!toStdOut) {
    ofs.seekp(0);
  }

  // write into stream
  char const* start = buffer.data();
  ofs.write(start, buffer.size());

  ofs.close();

  if (!toStdOut) {
    std::cout << "Successfully converted JSON infile '" << infile << "'"
              << std::endl;
    std::cout << "VPack Infile size: " << s.size() << std::endl;
    std::cout << "JSON Outfile size: " << buffer.size() << std::endl;
  }
  
  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
