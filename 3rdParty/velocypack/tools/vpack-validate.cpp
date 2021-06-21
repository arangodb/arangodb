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
#include "velocypack/vpack.h"
#include "velocypack/velocypack-exception-macros.h"

using namespace arangodb::velocypack;

static void usage(char* argv[]) {
  std::cout << "Usage: " << argv[0] << " [OPTIONS] INFILE" << std::endl;
  std::cout << "This program reads the VPack INFILE into a string and "
            << std::endl;
  std::cout << "validates it. Will work only for input files up to 2 GB size." 
            << std::endl;
  std::cout << "Available options are:" << std::endl;
  std::cout << " --hex                     try to turn hex-encoded input into binary vpack" << std::endl;
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
  bool allowFlags = true;
  bool hex = false;

  int i = 1;
  while (i < argc) {
    char const* p = argv[i];
    if (allowFlags && isOption(p, "--help")) {
      usage(argv);
      return EXIT_SUCCESS;
    } else if (allowFlags && isOption(p, "--hex")) {
      hex = true;
    } else if (allowFlags && isOption(p, "--")) {
      allowFlags = false;
    } else if (infileName == nullptr) {
      infileName = p;
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

  try {
    Validator validator;
    validator.validate(reinterpret_cast<uint8_t const*>(s.data()), s.size(), false);
    std::cout << "The velocypack in infile '" << infile << "' is valid" << std::endl;
  } catch (Exception const& ex) {
    std::cerr << "An exception occurred while processing infile '" << infile
              << "': " << ex.what() << std::endl;
    return EXIT_FAILURE;
  } catch (...) {
    std::cerr << "An unknown exception occurred while processing infile '"
              << infile << "'" << std::endl;
    return EXIT_FAILURE;
  }

  VELOCYPACK_GLOBAL_EXCEPTION_CATCH
}
