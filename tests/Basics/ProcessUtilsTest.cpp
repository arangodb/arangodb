////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/process-utils.h"

#include "gtest/gtest.h"

#include <array>

namespace {

// Gives up after a few seconds, so that a regression fails the test instead of
// hanging it.
struct CountingDeadline {
  int calls = 0;
  bool operator()() { return ++calls > 20; }
};

}  // namespace

TEST(ProcessUtilsTest, read_pipe_without_open_pipe_returns_immediately) {
  ExternalProcess process;
  ASSERT_EQ(-1, process._readPipe);

  std::array<char, 64> buffer;
  EXPECT_EQ(0, TRI_ReadPipe(&process, buffer.data(), buffer.size()));
}

TEST(ProcessUtilsTest,
     read_pipe_with_deadline_without_open_pipe_returns_immediately) {
  ExternalProcess process;
  ASSERT_EQ(-1, process._readPipe);

  CountingDeadline deadline;
  std::array<char, 64> buffer;
  EXPECT_EQ(0, TRI_ReadPipe(&process, buffer.data(), buffer.size(),
                            [&deadline] { return deadline(); }));
  EXPECT_EQ(0, deadline.calls);
}

TEST(ProcessUtilsTest, write_pipe_without_open_pipe_fails) {
  ExternalProcess process;
  ASSERT_EQ(-1, process._writePipe);

  EXPECT_FALSE(TRI_WritePipe(&process, "abc", 3));
}
