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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/SliceContainer.h>

#include "Logger/LogMacros.h"

#include "Runner.h"

namespace {

struct InvalidArgumentException : std::exception {
  explicit InvalidArgumentException(std::string_view arg) : arg(arg) {}
  [[nodiscard]] const char* what() const noexcept override {
    return arg.data();
  }

 private:
  std::string_view arg;
};

struct Options {
  std::string_view configFile;
  std::string_view report;
  std::vector<std::string_view> params;
};

std::pair<std::string_view, std::string_view> splitKeyValue(
    std::string_view s) {
  auto pos = s.find('=');
  if (pos == std::string::npos) {
    throw InvalidArgumentException(s);
  }
  return {s.substr(0, pos), s.substr(pos + 1)};
}

void addConfig(arangodb::velocypack::Builder& config, std::string_view param) {
  auto [path, v] = splitKeyValue(param);
  auto value = arangodb::velocypack::Parser::fromJson(v.data(), v.size());

  arangodb::velocypack::Builder builder;
  builder.openObject();
  auto fieldPos = path.find('.');
  for (;;) {
    builder.add(VPackValue(path.substr(0, fieldPos)));
    if (fieldPos != std::string::npos) {
      builder.openObject();
      path = path.substr(fieldPos + 1);
      fieldPos = path.find('.');
    } else {
      break;
    }
  }
  builder.add(value->slice());
  while (builder.isOpenObject()) {
    builder.close();
  }

  arangodb::velocypack::SliceContainer configSlice(config.slice());
  config.clear();
  arangodb::velocypack::Collection::merge(config, configSlice.slice(),
                                          builder.slice(), true);
}

auto parseConfig(Options const& opts) {
  std::ifstream t(std::string(opts.configFile));
  std::stringstream buffer;
  buffer << t.rdbuf();

  auto config = arangodb::velocypack::Parser::fromJson(buffer.str());

  for (auto& v : opts.params) {
    try {
      addConfig(*config, v);
    } catch (...) {
      std::cerr << "Failed to process config parameter " << v << std::endl;
      throw;
    }
  }
  return config;
}

void parseOptions(int argc, char const* argv[], Options& opts) {
  opts.configFile = argv[1];
  for (int i = 2; i < argc; ++i) {
    if (argv[i] == std::string("--")) {
      for (int j = i + 1; j < argc; ++j) {
        opts.params.emplace_back(argv[j]);
      }
      return;
    }

    auto [key, value] = splitKeyValue(argv[i]);
    if (key == "--report") {
      opts.report = value;
    } else {
      throw InvalidArgumentException(argv[i]);
    }
  }
}

void printUsage() {
  std::cout << "Usage: sepp"
            << " <config-file>"
            << " [--report=<report-file>]"
            << " [-- <some.attribute.path>=<value> ...]" << std::endl;
}

}  // namespace

int main(int argc, char const* argv[]) {
  std::cout << "Storage Engine Performance Predictor\n" << std::endl;

  if (argc < 2) {
    printUsage();
    return 1;
  }

  try {
    Options options;
    parseOptions(argc, argv, options);

    auto config = parseConfig(options);
    arangodb::sepp::Runner runner(argv[0], options.report, config->slice());
    runner.run();

  } catch (InvalidArgumentException const& e) {
    std::cerr << "Invalid argument: " << e.what() << std::endl;
    printUsage();
    return 1;
  } catch (std::exception const& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }

  return 0;
}
