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
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>

#include "velocypack/vpack.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

using namespace arangodb::velocypack;

static void usage(char* argv[]) {
  std::cout << "Usage: " << argv[0]
            << " FILENAME.json RUNTIME_IN_SECONDS COPIES TYPE" << std::endl;
  std::cout << "This program reads the file into a string, makes COPIES copies"
            << std::endl;
  std::cout << "and then parses the copies in a round-robin fashion to VPack."
            << std::endl;
  std::cout << "1 copy means its running in cache, more copies make it run"
            << std::endl;
  std::cout << "out of cache. The target areas are also in a different memory"
            << std::endl;
  std::cout << "area for each copy." << std::endl;
  std::cout << "TYPE must be either 'vpack' or 'rapidjson'." << std::endl;
}

static std::string tryReadFile(std::string const& filename) {
  std::string s;
  std::ifstream ifs(filename.c_str(), std::ifstream::in);

  if (!ifs.is_open()) {
    std::cerr << "Cannot open input file '" << filename << "'" << std::endl;
    ::exit(EXIT_FAILURE);
  }

  char buffer[4096];
  while (ifs.good()) {
    ifs.read(&buffer[0], sizeof(buffer));
    s.append(buffer, ifs.gcount());
  }
  ifs.close();

  return s;
}

static std::string readFile(std::string filename) {
#ifdef _WIN32
  std::string const separator("\\");
#else
  std::string const separator("/");
#endif
  filename = "tests" + separator + "jsonSample" + separator + filename;

  for (size_t i = 0; i < 3; ++i) {
    try {
      return tryReadFile(filename);
    } catch (...) {
      filename = ".." + separator + filename;
    }
  }
  throw "cannot open input file";
}

static void run(std::string& data, int runTime, size_t copies, bool useVPack,
                bool fullOutput) {
  Options options;

  std::vector<std::string> inputs;
  std::vector<Parser*> outputs;
  inputs.push_back(data);
  outputs.push_back(new Parser(&options));

  for (size_t i = 1; i < copies; i++) {
    // Make an explicit copy:
    data.clear();
    data.insert(data.begin(), inputs[0].begin(), inputs[0].end());
    inputs.push_back(data);
    outputs.push_back(new Parser(&options));
  }

  size_t count = 0;
  size_t total = 0;
  auto start = std::chrono::high_resolution_clock::now();
  decltype(start) now;

  try {
    do {
      for (int i = 0; i < 2; i++) {
        if (useVPack) {
          outputs[count]->clear();
          outputs[count]->parse(inputs[count]);
        } else {
          rapidjson::Document d;
          d.Parse(inputs[count].c_str());
        }
        count++;
        if (count >= copies) {
          count = 0;
        }
        total++;
      }
      now = std::chrono::high_resolution_clock::now();
    } while (std::chrono::duration_cast<std::chrono::duration<int>>(now - start)
                 .count() < runTime);

    std::chrono::duration<double> totalTime =
        std::chrono::duration_cast<std::chrono::duration<double>>(now - start);

    if (fullOutput) {
      std::cout << "Total runtime: " << totalTime.count() << " s" << std::endl;
      std::cout << "Have parsed " << total << " times with "
                << (useVPack ? "vpack" : "rapidjson") << " using " << copies
                << " copies of JSON data, each of size " << inputs[0].size()
                << "." << std::endl;
      std::cout << "Parsed " << inputs[0].size() * total << " bytes in total."
                << std::endl;
    }
    std::cout << "This is "
              << static_cast<double>(inputs[0].size() * total) /
                     totalTime.count() << " bytes/s"
              << " or " << total / totalTime.count() << " JSON docs per second."
              << std::endl;
  } catch (Exception const& ex) {
    std::cerr << "An exception occurred while running bench: " << ex.what()
              << std::endl;
    ::exit(EXIT_FAILURE);
  } catch (std::exception const& ex) {
    std::cerr << "An exception occurred while running bench: " << ex.what()
              << std::endl;
    ::exit(EXIT_FAILURE);
  } catch (...) {
    std::cerr << "An unknown exception occurred while running bench"
              << std::endl;
    ::exit(EXIT_FAILURE);
  }

  for (auto& it : outputs) {
    delete it;
  }
}

static void runDefaultBench() {
  auto runComparison = [](std::string const& filename) {
    std::string data = std::move(readFile(filename));

    std::cout << std::endl;
    std::cout << "# " << filename << " ";
    for (size_t i = 0; i < 30 - filename.size(); ++i) {
      std::cout << "#";
    }
    std::cout << std::endl;

    std::cout << "vpack:        ";
    run(data, 10, 1, true, false);

    std::cout << "rapidjson:    ";
    run(data, 10, 1, false, false);
  };

  runComparison("small.json");
  runComparison("sample.json");
  runComparison("sampleNoWhite.json");
  runComparison("commits.json");
}

int main(int argc, char* argv[]) {
  if (argc == 1) {
    runDefaultBench();
    return EXIT_FAILURE;
  }

  if (argc != 5) {
    usage(argv);
    return EXIT_FAILURE;
  }

  bool useVPack;
  if (::strcmp(argv[4], "vpack") == 0) {
    useVPack = true;
  } else if (::strcmp(argv[4], "rapidjson") == 0) {
    useVPack = false;
  } else {
    usage(argv);
    return EXIT_FAILURE;
  }

  size_t copies = std::stoul(argv[3]);
  int runTime = std::stoi(argv[2]);

  // read input file
  std::string s = std::move(readFile(argv[1]));

  run(s, runTime, copies, useVPack, true);

  return EXIT_SUCCESS;
}
