////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License{};
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

#pragma once

#include <string_view>

#include "Basics/Meta/TypeList.h"
#include "Logger/LogLevel.h"

namespace arangodb::logger {

namespace topic {
struct Agency {
  static constexpr std::string_view name = "agency";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Agencycomm {
  static constexpr std::string_view name = "agencycomm";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Agencystore {
  static constexpr std::string_view name = "agencystore";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Aql {
  static constexpr std::string_view name = "aql";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Authentication {
  static constexpr std::string_view name = "authentication";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Authorization {
  static constexpr std::string_view name = "authorization";
  static constexpr LogLevel defaultLevel = LogLevel::DEFAULT;
};
struct Backup {
  static constexpr std::string_view name = "backup";
  static constexpr LogLevel defaultLevel = LogLevel::DEFAULT;
};
struct Bench {
  static constexpr std::string_view name = "bench";
  static constexpr LogLevel defaultLevel = LogLevel::DEFAULT;
};
struct Cache {
  static constexpr std::string_view name = "cache";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Cluster {
  static constexpr std::string_view name = "cluster";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Communication {
  static constexpr std::string_view name = "communication";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Config {
  static constexpr std::string_view name = "config";
  static constexpr LogLevel defaultLevel = LogLevel::DEFAULT;
};
struct Crash {
  static constexpr std::string_view name = "crash";
  static constexpr LogLevel defaultLevel = LogLevel::DEFAULT;
};
struct Development {
  static constexpr std::string_view name = "development";
  static constexpr LogLevel defaultLevel = LogLevel::FATAL;
};
struct Dump {
  static constexpr std::string_view name = "dump";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Engines {
  static constexpr std::string_view name = "engines";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Fixme {
  static constexpr std::string_view name = "general";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Flush {
  static constexpr std::string_view name = "flush";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Graphs {
  static constexpr std::string_view name = "graphs";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Heartbeat {
  static constexpr std::string_view name = "heartbeat";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Httpclient {
  static constexpr std::string_view name = "httpclient";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct License {
  static constexpr std::string_view name = "license";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Maintenance {
  static constexpr std::string_view name = "maintenance";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Memory {
  static constexpr std::string_view name = "memory";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Queries {
  static constexpr std::string_view name = "queries";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Replication {
  static constexpr std::string_view name = "replication";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Replication2 {
  static constexpr std::string_view name = "replication2";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct ReplicatedState {
  static constexpr std::string_view name = "rep-state";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct ReplicatedWal {
  static constexpr std::string_view name = "rep-wal";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Requests {
  static constexpr std::string_view name = "requests";
  static constexpr LogLevel defaultLevel = LogLevel::FATAL;  // suppress
};
struct Restore {
  static constexpr std::string_view name = "restore";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Rocksdb {
  static constexpr std::string_view name = "rocksdb";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Security {
  static constexpr std::string_view name = "security";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Ssl {
  static constexpr std::string_view name = "ssl";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Startup {
  static constexpr std::string_view name = "startup";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Statistics {
  static constexpr std::string_view name = "statistics";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Supervision {
  static constexpr std::string_view name = "supervision";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Syscall {
  static constexpr std::string_view name = "syscall";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Threads {
  static constexpr std::string_view name = "threads";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Trx {
  static constexpr std::string_view name = "trx";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Ttl {
  static constexpr std::string_view name = "ttl";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Validation {
  static constexpr std::string_view name = "validation";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct V8 {
  static constexpr std::string_view name = "v8";
  static constexpr LogLevel defaultLevel = LogLevel::WARN;
};
struct Views {
  static constexpr std::string_view name = "views";
  static constexpr LogLevel defaultLevel = LogLevel::FATAL;
};
struct Deprecation {
  static constexpr std::string_view name = "deprecation";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct ArangoSearch {
  static constexpr std::string_view name = "arangosearch";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct LibIResearch {
  static constexpr std::string_view name = "libiresearch";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
}  // namespace topic

#ifdef USE_ENTERPRISE
namespace audit {
struct Authentication {
  static constexpr std::string_view name = "audit-authentication";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Authorization {
  static constexpr std::string_view name = "audit-authorization";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Database {
  static constexpr std::string_view name = "audit-database";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Collection {
  static constexpr std::string_view name = "audit-collection";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct View {
  static constexpr std::string_view name = "audit-view";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Document {
  static constexpr std::string_view name = "audit-document";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct Service {
  static constexpr std::string_view name = "audit-service";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
struct HotBackup {
  static constexpr std::string_view name = "audit-hotbackup";
  static constexpr LogLevel defaultLevel = LogLevel::INFO;
};
}  // namespace audit
#endif

using TopicList = meta::TypeList<
    topic::Agency, topic::Agencycomm, topic::Agencystore, topic::Aql,
    topic::Authentication, topic::Authorization, topic::Backup, topic::Bench,
    topic::Cache, topic::Cluster, topic::Communication, topic::Config,
    topic::Crash, topic::Development, topic::Dump, topic::Engines, topic::Fixme,
    topic::Flush, topic::Graphs, topic::Heartbeat, topic::Httpclient,
    topic::License, topic::Maintenance, topic::Memory, topic::Queries,
    topic::Replication, topic::Replication2, topic::ReplicatedState,
    topic::ReplicatedWal, topic::Requests, topic::Restore, topic::Rocksdb,
    topic::Security, topic::Ssl, topic::Startup, topic::Statistics,
    topic::Supervision, topic::Syscall, topic::Threads, topic::Trx, topic::Ttl,
    topic::Validation, topic::V8, topic::Views, topic::Deprecation,
    topic::ArangoSearch, topic::LibIResearch
#ifdef USE_ENTERPRISE
    ,
    audit::Authentication, audit::Authorization, audit::Database,
    audit::Collection, audit::View, audit::Document, audit::Service,
    audit::HotBackup
#endif
    >;

static constexpr size_t kNumTopics = TopicList::size();

template<typename T>
concept IsLogTopic = TopicList::contains<T>();

}  // namespace arangodb::logger
