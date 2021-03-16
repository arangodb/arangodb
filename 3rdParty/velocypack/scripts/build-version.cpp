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
#include <regex>

// a simple tool to generate a version number
// reads in an existing version file and increments the patch
// level in it. creates a new file if it does not yet exist
int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " FILE" << std::endl;
    return EXIT_FAILURE;
  }

  unsigned long major = 0;
  unsigned long minor = 0;
  unsigned long patch = 0;
  std::regex version(
      "\\s*#define VELOCYPACK_VERSION\\s*\"([0-9]+)\\.([0-9]+)\\.([0-9]+)\"",
      std::regex::ECMAScript);

  std::string infile = argv[1];
  std::string s;
  std::ifstream ifs(infile, std::ifstream::in);

  char buffer[4096];
  if (ifs.is_open()) {
    ifs.read(&buffer[0], sizeof(buffer));
    s.append(buffer, ifs.gcount());
    ifs.close();
  }

  std::smatch match;

  if (std::regex_search(s, match, version)) {
    major = std::stoul(match[1].str());
    minor = std::stoul(match[2].str());
    patch = std::stoul(match[3].str());
  }

  ++patch;

  std::string result;
  result.append("// this is an auto-generated file. do not edit!\n\n");
  result.append("#ifndef VELOCYPACK_VERSION_NUMBER_H\n");
  result.append("#define VELOCYPACK_VERSION_NUMBER_H 1\n\n");
  result.append("#define VELOCYPACK_VERSION \"" + std::to_string(major) + "." +
                std::to_string(minor) + "." + std::to_string(patch) + "\"\n\n");
  result.append("#define VELOCYPACK_VERSION_MAJOR " + std::to_string(major) +
                "\n");
  result.append("#define VELOCYPACK_VERSION_MINOR " + std::to_string(minor) +
                "\n");
  result.append("#define VELOCYPACK_VERSION_PATCH " + std::to_string(patch) +
                "\n");
  result.append("\n#endif");

  std::string outfile = argv[1];
  std::ofstream ofs(outfile, std::ifstream::out);
  ofs.seekp(0);
  ofs.write(result.c_str(), result.size());
  ofs.close();

  return EXIT_SUCCESS;
}
