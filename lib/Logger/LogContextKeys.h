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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

namespace arangodb {

inline constexpr char logContextKeyTerm[] = "term";
inline constexpr char logContextKeyLeaderId[] = "leader-id";
inline constexpr char logContextKeyLogComponent[] = "log-component";
inline constexpr char logContextKeyStateComponent[] = "state-component";
inline constexpr char logContextKeyStateImpl[] = "state-impl";
inline constexpr char logContextKeyStateRole[] = "state-role";
inline constexpr char logContextKeyMessageId[] = "message-id";
inline constexpr char logContextKeyPrevLogIdx[] = "prev-log-idx";
inline constexpr char logContextKeyPrevLogTerm[] = "prev-log-term";
inline constexpr char logContextKeyLeaderCommit[] = "leader-commit";
inline constexpr char logContextKeyFollowerId[] = "follower-id";
inline constexpr char logContextKeyLogId[] = "log-id";
inline constexpr char logContextKeyDatabaseName[] = "database-name";
inline constexpr char logContextKeyCollectionName[] = "collection-name";
inline constexpr char logContextKeyCollectionId[] = "collection-id";
inline constexpr char logContextKeyMyself[] = "myself";
inline constexpr char logContextKeySnapshotId[] = "snapshot-id";
inline constexpr char logContextKeyTrxId[] = "trx-id";

}  // namespace arangodb
