////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Graetzer
////////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>

#include <fuerte/VpackInit.h>

#include "gtest/gtest.h"
#include "common.h"

// as it is not const this var has external linkage
// it is assigned a value in main
std::string myEndpoint = "tcp://localhost:8529";
std::string myAuthentication = "basic:root:";

std::vector<std::string> parse_args(int const& argc, char const* const* argv) {
  std::vector<std::string> rv;
  for (int i = 0; i < argc; i++) {
    rv.emplace_back(argv[i]);
  }
  return rv;
}

int main(int argc, char** argv) {
  arangodb::fuerte::helper::VpackInit vpack;

  ::testing::InitGoogleTest(&argc, argv);   // removes google test parameters
  auto arguments = parse_args(argc, argv);  // init √global Schmutz

  const std::string endpointArg = "--endpoint=";
  const std::string authArg = "--authentication=";
  for (auto const& arg : arguments) {
    if (arg.substr(0, endpointArg.size()) == endpointArg) {
      myEndpoint = arg.substr(endpointArg.size());
    } else if (arg.substr(0, authArg.size()) == authArg) {
      myAuthentication = arg.substr(authArg.size());
    }
  }

  return RUN_ALL_TESTS();
}
