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

#include <thread>

#include "gtest/gtest.h"

#include "Logger/LogAppender.h"
#include "Logger/Logger.h"

char const* ARGV0 = "";

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  // our gtest version is old and doesn't have the GTEST_FLAG_SET macro yet,
  // thus this funny workaround for now.
  // GTEST_FLAG_SET(death_test_style, "threadsafe");
  (void)(::testing::GTEST_FLAG(death_test_style) = "threadsafe");

  arangodb::Logger::initialize(false, 10000);
  arangodb::LogAppender::addAppender(arangodb::Logger::defaultLogGroup(), "-");
  // Run tests in subthread such that it has a larger stack size in libmusl,
  // the stack size for subthreads has been reconfigured in the
  // ArangoGlobalContext above in the libmusl case:
  int result;
  std::thread{[&](int argc, char* argv[]) { result = RUN_ALL_TESTS(); }, argc,
              argv}
      .join();

  arangodb::Logger::shutdown();
  return (result < 0xff ? result : 0xff);
}
