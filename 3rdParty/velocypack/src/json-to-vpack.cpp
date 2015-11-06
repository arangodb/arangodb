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

#include "velocypack/velocypack-common.h"
#include "velocypack/Builder.h"
#include "velocypack/Exception.h"
#include "velocypack/Parser.h"
#include "velocypack/Slice.h"
#include "velocypack/Value.h"

using namespace arangodb::velocypack;

static void usage (char* argv[]) {
  std::cout << "Usage: " << argv[0] << " INFILE OUTFILE" << std::endl;
  std::cout << "This program reads the JSON INFILE into a string and saves its" << std::endl;
  std::cout << "VPack representation in file OUTFILE. Will work only for input" << std::endl;
  std::cout << "files up to 2 GB size." << std::endl;
}

int main (int argc, char* argv[]) {
  if (argc < 3) {
    usage(argv);
    return EXIT_FAILURE;
  }

  // treat "-" as stdin
  std::string infile = argv[1];
#ifdef __linux__
  if (infile == "-") {
    infile = "/proc/self/fd/0";
  }
#endif

  std::string s;
  std::ifstream ifs(infile, std::ifstream::in);

  if (! ifs.is_open()) {
    std::cerr << "Cannot read infile '" << argv[1] << "'" << std::endl;
    return EXIT_FAILURE;
  }

  char buffer[4096];
  while (ifs.good()) {
    ifs.read(&buffer[0], sizeof(buffer));
    s.append(buffer, ifs.gcount());
  }
  ifs.close();

  Parser parser;
  try {
    parser.parse(s);
  }
  catch (Exception const& ex) {
    std::cerr << "An exception occurred while parsing infile '" << argv[1] << "': " << ex.what() << std::endl;
    std::cerr << "Error position: " << parser.errorPos() << std::endl;
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cerr << "An unknown exception occurred while parsing infile '" << argv[1] << "'" << std::endl;
    return EXIT_FAILURE;
  }
  
  std::ofstream ofs(argv[2], std::ofstream::out);
 
  if (! ofs.is_open()) {
    std::cerr << "Cannot write outfile '" << argv[2] << "'" << std::endl;
    return EXIT_FAILURE;
  }

  // reset stream
  ofs.seekp(0);

  // write into stream
  Builder builder = parser.steal();
  uint8_t const* start = builder.start();
  ofs.write(reinterpret_cast<char const*>(start), builder.size());

  if (! ofs) {
    std::cerr << "Cannot write outfile '" << argv[2] << "'" << std::endl;
    ofs.close();
    return EXIT_FAILURE;
  }

  ofs.close();

  std::cout << "Successfully converted JSON infile '" << argv[1] << "'" << std::endl;
  std::cout << "JSON Infile size:   " << s.size() << std::endl;
  std::cout << "VPack Outfile size: " << builder.size() << std::endl;
  
  return EXIT_SUCCESS;
}
