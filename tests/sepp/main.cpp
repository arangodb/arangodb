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

#include <velocypack/Builder.h>
#include <velocypack/Collection.h>
#include <velocypack/Parser.h>
#include <velocypack/SliceContainer.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Logger/LogMacros.h"

#include "Benchmark.h"

void addConfig(arangodb::velocypack::Builder& config, std::string_view param) {
  std::cout << "processing arg " << param << std::endl;
  auto pos = param.find('=');
  if (pos == std::string::npos) {
    throw std::runtime_error("Invalid config parameter \"" +
                             std::string(param) +
                             "\". Config parameters must be of the form "
                             "\"<some.attribute.path>=<value>\"");
  }
  auto path = param.substr(0, pos);
  auto value = arangodb::velocypack::Parser::fromJson(
      std::string(param.substr(pos + 1)));

  arangodb::velocypack::Builder builder;
  builder.openObject();
  auto fieldPos = path.find(".");
  for (;;) {
    builder.add(VPackValue(path.substr(0, fieldPos)));
    if (fieldPos != std::string::npos) {
      builder.openObject();
      path = path.substr(fieldPos + 1);
      fieldPos = path.find(".");
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

int main(int argc, char const* argv[]) {
  std::cout << "Storage Engine Performance Predictor\n" << std::endl;

  try {
    std::string name = "sepp";
    auto options = std::make_shared<arangodb::options::ProgramOptions>(
        argv[0], "Usage: " + name + " [<options>]",
        "For more information use:", "");

    if (argc > 1) {
      std::ifstream t(argv[1]);
      std::stringstream buffer;
      buffer << t.rdbuf();

      auto config = arangodb::velocypack::Parser::fromJson(buffer.str());

      for (int i = 2; i < argc; ++i) {
        addConfig(*config, argv[i]);
      }
      std::cout << config->slice().toJson();
    }
    arangodb::sepp::Benchmark benchmark;

    benchmark.run(argv[0]);

  } catch (std::exception& e) {
    std::cerr << e.what();
    return 1;
  }

  return 0;
}
