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
/// @author Andreas Streichardt
////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <thread>

#include "gtest/gtest.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "VocBase/Identifiers/ServerId.h"
#include "RestServer/arangod.h"
#include "Rest/Version.h"
#include "Basics/VelocyPackHelper.h"

char const* ARGV0 = "";

void arangodb::rest::Version::initialize() {}
void arangodb::basics::VelocyPackHelper::initialize() {}

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  // our gtest version is old and doesn't have the GTEST_FLAG_SET macro yet,
  // thus this funny workaround for now.
  // GTEST_FLAG_SET(death_test_style, "threadsafe");
  (void)(::testing::GTEST_FLAG(death_test_style) = "threadsafe");

  int subargc = 0;
  char** subargv = (char**)malloc(sizeof(char*) * argc);
  bool logLineNumbers = false;
  arangodb::RandomGenerator::initialize(
      arangodb::RandomGenerator::RandomType::MERSENNE);
  // global setup...
  for (int i = 0; i < argc; i++) {
    if (strcmp(argv[i], "--log.line-number") == 0) {
      if (i < argc) {
        i++;
        if (i < argc) {
          if (strcmp(argv[i], "true") == 0) {
            logLineNumbers = true;
          }
          i++;
        }
      }
    } else {
      subargv[subargc] = argv[i];
      subargc++;
    }
  }

  ARGV0 = subargv[0];

  arangodb::ArangodServer server(nullptr, nullptr);

  arangodb::Logger::setShowLineNumber(logLineNumbers);
  arangodb::Logger::initialize(server, false);
  arangodb::Logger::addAppender(arangodb::Logger::defaultLogGroup(), "-");

  arangodb::ArangoGlobalContext ctx(1, const_cast<char**>(&ARGV0), ".");
  ctx.exit(0);  // set "good" exit code by default

  // Run tests in subthread such that it has a larger stack size in libmusl,
  // the stack size for subthreads has been reconfigured in the
  // ArangoGlobalContext above in the libmusl case:
  int result;
  std::thread{[&](int argc, char* argv[]) { result = RUN_ALL_TESTS(); },
              subargc, subargv}
      .join();

  arangodb::Logger::shutdown();
  // global clean-up...
  free(subargv);
  return (result < 0xff ? result : 0xff);
}
