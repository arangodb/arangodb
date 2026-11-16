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

#include "gtest/gtest.h"

#include "Utils/ConnectionHandles.h"

#include <cstddef>
#include <memory>
#include <string>

using namespace arangodb;

namespace {

/**
 * Connection that only has a state and counts how often it was cancelled
 */
class FakeConnection final : public fuerte::Connection {
 public:
  explicit FakeConnection(State state = State::Connected)
      : fuerte::Connection(fuerte::detail::ConnectionConfiguration{}),
        _state(state) {}

  void sendRequest(std::unique_ptr<fuerte::Request>,
                   fuerte::RequestCallback) override {}
  auto requestsLeft() const -> std::size_t override { return 0; }
  auto state() const -> State override { return _state; }
  void cancel() override {
    _state = State::Closed;
    ++cancelled;
  }
  auto localEndpoint() -> std::string override { return ""; }

  int cancelled = 0;

 private:
  State _state;
};

using Handles = ConnectionHandles<std::string>;

auto connected() -> std::shared_ptr<FakeConnection> {
  return std::make_shared<FakeConnection>();
}

auto closed() -> std::shared_ptr<FakeConnection> {
  return std::make_shared<FakeConnection>(FakeConnection::State::Closed);
}

}  // namespace

// keep

TEST(ConnectionHandlesTest, keepIgnoresNullConnection) {
  Handles handles;

  handles.keep("a", {nullptr, "config a"});

  EXPECT_TRUE(handles.inactive().empty());
}

TEST(ConnectionHandlesTest, keepCancelsInsteadOfKeepingClosedConnection) {
  Handles handles;
  auto const connection = closed();

  handles.keep("a", {connection, "config a"});

  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_EQ(1, connection->cancelled);
}

TEST(ConnectionHandlesTest, keepCancelsInsteadOfKeepingConnectionWithoutHandle) {
  Handles handles;
  auto const connection = connected();

  handles.keep("", {connection, "config"});

  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_EQ(1, connection->cancelled);
}

TEST(ConnectionHandlesTest, keepReplacesAndCancelsEarlierConnectionOfHandle) {
  Handles handles;
  auto const earlier = connected();
  auto const later = connected();

  handles.keep("a", {earlier, "earlier"});
  handles.keep("a", {later, "later"});

  ASSERT_EQ(1u, handles.inactive().size());
  EXPECT_EQ(later, handles.inactive().at("a").connection);
  EXPECT_EQ("later", handles.inactive().at("a").config);
  EXPECT_EQ(1, earlier->cancelled);
  EXPECT_EQ(0, later->cancelled);
}

TEST(ConnectionHandlesTest, keepingTheSameConnectionAgainDoesNotCancelIt) {
  Handles handles;
  auto const connection = connected();

  handles.keep("a", {connection, "config a"});
  handles.keep("a", {connection, "config a"});

  EXPECT_EQ(1u, handles.inactive().size());
  EXPECT_EQ(0, connection->cancelled);
}

// take

TEST(ConnectionHandlesTest, takeReturnsAndRemovesTheKeptEntry) {
  Handles handles;
  auto const connection = connected();
  handles.keep("a", {connection, "config a"});

  auto const entry = handles.take("a");

  ASSERT_TRUE(entry.has_value());
  EXPECT_EQ(connection, entry->connection);
  EXPECT_EQ("config a", entry->config);
  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_FALSE(handles.take("a").has_value());
}

TEST(ConnectionHandlesTest, takeOfUnknownHandleIsEmpty) {
  Handles handles;

  EXPECT_FALSE(handles.take("unknown").has_value());
}

// activate

TEST(ConnectionHandlesTest, activateMakesTheEntryActive) {
  Handles handles;
  auto const connection = connected();

  handles.activate("a", {connection, "config a"});

  EXPECT_EQ(connection, handles.connection());
  EXPECT_EQ("config a", handles.config());
  EXPECT_EQ("a", handles.activeHandle());
  EXPECT_TRUE(handles.inactive().empty());
}

TEST(ConnectionHandlesTest, activateKeepsThePreviouslyActiveConnection) {
  Handles handles;
  auto const a = connected();
  auto const b = connected();
  handles.activate("a", {a, "config a"});

  handles.activate("b", {b, "config b"});

  EXPECT_EQ("b", handles.activeHandle());
  ASSERT_EQ(1u, handles.inactive().size());
  EXPECT_EQ(a, handles.inactive().at("a").connection);
  EXPECT_EQ("config a", handles.inactive().at("a").config);
  EXPECT_EQ(0, a->cancelled);
}

TEST(ConnectionHandlesTest, activateSupersedesTheActiveConnectionOfTheHandle) {
  Handles handles;
  auto const earlier = connected();
  auto const later = connected();
  handles.activate("a", {earlier, "earlier"});

  handles.activate("a", {later, "later"});

  EXPECT_EQ(later, handles.connection());
  EXPECT_EQ("later", handles.config());
  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_EQ(1, earlier->cancelled);
}

TEST(ConnectionHandlesTest, activatingTheActiveConnectionAgainKeepsIt) {
  Handles handles;
  auto const connection = connected();
  handles.activate("a", {connection, "config a"});

  handles.activate("a", {connection, "config a"});

  EXPECT_EQ(connection, handles.connection());
  EXPECT_EQ(0, connection->cancelled);
}

TEST(ConnectionHandlesTest, activateDoesNotKeepAClosedPreviousConnection) {
  Handles handles;
  auto const previous = connected();
  handles.activate("a", {previous, "config a"});
  previous->cancel();

  handles.activate("b", {connected(), "config b"});

  EXPECT_TRUE(handles.inactive().empty());
}

TEST(ConnectionHandlesTest, switchingBackRestoresConnectionAndConfig) {
  Handles handles;
  auto const a = connected();
  auto const b = connected();
  handles.activate("a", {a, "config a"});
  handles.activate("b", {b, "config b"});

  handles.activate("a", *handles.take("a"));

  EXPECT_EQ(a, handles.connection());
  EXPECT_EQ("config a", handles.config());
  EXPECT_EQ("a", handles.activeHandle());
  ASSERT_EQ(1u, handles.inactive().size());
  EXPECT_EQ(b, handles.inactive().at("b").connection);
}

// deactivate, dropActive, clear

TEST(ConnectionHandlesTest, deactivateKeepsTheConnectionAndTheConfig) {
  Handles handles;
  auto const connection = connected();
  handles.activate("a", {connection, "config a"});

  handles.deactivate();

  EXPECT_EQ(nullptr, handles.connection());
  EXPECT_EQ("", handles.activeHandle());
  EXPECT_EQ("config a", handles.config());
  EXPECT_EQ(connection, handles.inactive().at("a").connection);
}

TEST(ConnectionHandlesTest, deactivateWithoutActiveConnectionChangesNothing) {
  Handles handles;
  handles.config() = "next";

  handles.deactivate();

  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_EQ("next", handles.config());
}

TEST(ConnectionHandlesTest, dropActiveForgetsTheConnectionWithoutKeepingIt) {
  Handles handles;
  auto const connection = connected();
  handles.activate("a", {connection, "config a"});

  handles.dropActive();

  EXPECT_EQ(nullptr, handles.connection());
  EXPECT_EQ("", handles.activeHandle());
  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_EQ("config a", handles.config());
}

TEST(ConnectionHandlesTest, clearForgetsAllConnectionsButKeepsTheConfig) {
  Handles handles;
  handles.activate("a", {connected(), "config a"});
  handles.activate("b", {connected(), "config b"});

  handles.clear();

  EXPECT_EQ(nullptr, handles.connection());
  EXPECT_EQ("", handles.activeHandle());
  EXPECT_TRUE(handles.inactive().empty());
  EXPECT_EQ("config b", handles.config());
}
