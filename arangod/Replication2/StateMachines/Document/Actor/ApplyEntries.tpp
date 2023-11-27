////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "ApplyEntries.h"

namespace arangodb::replication2::replicated_state::document::actor {

using namespace arangodb::actor;

void ApplyEntriesState::applyEntries() {
  auto applyResult = state._guardedData.doUnderLock(
      [this](auto& data) -> ResultT<std::optional<LogIndex>> {
        if (data.didResign()) {
          return {TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED};
        }

        return basics::catchToResultT([&]() -> std::optional<LogIndex> {
          std::optional<LogIndex> releaseIndex;

          while (auto entry = entries->next()) {
            if (state._resigning) {
              // We have not officially resigned yet, but we are about to.
              // So, we can just stop here.
              break;
            }
            auto [index, doc] = *entry;

            auto currentReleaseIndex = std::visit(
                [this, &data, index = index](
                    auto const& op) -> ResultT<std::optional<LogIndex>> {
                  return data.applyEntry(state._handlers, op, index);
                },
                doc.getInnerOperation());

            if (currentReleaseIndex.fail()) {
              TRI_ASSERT(state._handlers.errorHandler
                             ->handleOpResult(doc.getInnerOperation(),
                                              currentReleaseIndex.result())
                             .fail())
                  << currentReleaseIndex.result()
                  << " should have been already handled for operation "
                  << doc.getInnerOperation()
                  << " during applyEntries of follower " << state.gid;
              LOG_CTX("0aa2e", FATAL, state.loggerContext)
                  << "failed to apply entry " << doc
                  << " on follower: " << currentReleaseIndex.result();
              TRI_ASSERT(false) << currentReleaseIndex.result();
              FATAL_ERROR_EXIT();
            }
            if (currentReleaseIndex->has_value()) {
              releaseIndex = std::move(currentReleaseIndex).get();
            }
          }

          return releaseIndex;
        });
      });

  if (state._resigning) {
    promise.setValue(
        Result{TRI_ERROR_REPLICATION_REPLICATED_LOG_FOLLOWER_RESIGNED});
    return;
  }

  if (applyResult.fail()) {
    promise.setValue(applyResult.result());
    return;
  }
  if (applyResult->has_value()) {
    // The follower might have resigned, so we need to be careful when
    // accessing the stream.
    auto releaseRes = basics::catchVoidToResult([&] {
      auto const& stream = state.getStream();
      stream->release(applyResult->value());
    });
    if (releaseRes.fail()) {
      LOG_CTX("10f07", ERR, state.loggerContext)
          << "Failed to get stream! " << releaseRes;
    }
  }

  promise.setValue(Result{TRI_ERROR_NO_ERROR});
}

}  // namespace arangodb::replication2::replicated_state::document::actor
