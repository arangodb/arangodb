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
  
  ::testing::InitGoogleTest(&argc, argv);  // removes google test parameters
  auto arguments = parse_args(argc, argv);      // init √global Schmutz

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
