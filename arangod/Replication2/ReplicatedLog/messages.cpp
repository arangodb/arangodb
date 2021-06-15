////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright $YEAR-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "messages.h"

#include <Logger/LogMacros.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

AppendEntriesRequest::AppendEntriesRequest(AppendEntriesRequest&& other) noexcept try
    : leaderTerm(other.leaderTerm),
      leaderId(std::move(other.leaderId)),
      prevLogTerm(other.prevLogTerm),
      prevLogIndex(other.prevLogIndex),
      leaderCommit(other.leaderCommit),
      messageId(other.messageId),
      waitForSync(other.waitForSync),
      entries(std::move(other.entries)) {
  // Note that immer::flex_vector is currently not nothrow move-assignable,
  // though it probably does not throw any exceptions. However, we *need* this
  // to be noexcept, otherwise we cannot keep the persistent and in-memory state
  // in sync.
  //
  // So even if flex_vector could actually throw here, we deliberately add the
  // noexcept!
  //
  // The try/catch is *only* for logging, but *must* terminate (e.g. by
  // rethrowing) the process if an exception is caught.
} catch (std::exception const& ex) {
  LOG_TOPIC("33563", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an AppendEntriesRequest. This is "
         "fatal, as consistency of persistent and in-memory state can no "
         "longer be guaranteed. The process will terminate now. The exception "
         "was: "
      << ex.what();
  throw;
} catch (...) {
  LOG_TOPIC("9771c", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an AppendEntriesRequest. This is "
         "fatal, as consistency of persistent and in-memory state can no "
         "longer be guaranteed. The process will terminate now.";
  throw;
}

auto AppendEntriesRequest::operator=(replicated_log::AppendEntriesRequest&& other) noexcept
    -> AppendEntriesRequest& try {
  // Note that immer::flex_vector is currently not nothrow move-assignable,
  // though it probably does not throw any exceptions. However, we *need* this
  // to be noexcept, otherwise we cannot keep the persistent and in-memory state
  // in sync.
  //
  // So even if flex_vector could actually throw here, we deliberately add the
  // noexcept!
  //
  // The try/catch is *only* for logging, but *must* terminate (e.g. by
  // rethrowing) the process if an exception is caught.
  leaderTerm = other.leaderTerm;
  leaderId = std::move(other.leaderId);
  prevLogTerm = other.prevLogTerm;
  prevLogIndex = other.prevLogIndex;
  leaderCommit = other.leaderCommit;
  messageId = other.messageId;
  waitForSync = other.waitForSync;
  entries = std::move(other.entries);

  return *this;
} catch (std::exception const& ex) {
  LOG_TOPIC("33563", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an AppendEntriesRequest. This is "
         "fatal, as consistency of persistent and in-memory state can no "
         "longer be guaranteed. The process will terminate now. The exception "
         "was: "
      << ex.what();
  throw;
} catch (...) {
  LOG_TOPIC("9771c", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an AppendEntriesRequest. This is "
         "fatal, as consistency of persistent and in-memory state can no "
         "longer be guaranteed. The process will terminate now.";
  throw;
}
