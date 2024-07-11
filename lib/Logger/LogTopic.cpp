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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "LogTopic.h"

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/Topics.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#endif

using namespace arangodb;

namespace {

class Topics {
 public:
  static Topics& instance() noexcept {
    // local to avoid init-order-fiasco problem
    static Topics INSTANCE;
    return INSTANCE;
  }

  template<typename Visitor>
  bool visit(Visitor const& visitor) const {
    for (auto const& [name, idx] : _nameToIndex) {
      if (!visitor(name, _topics[idx])) {
        return false;
      }
    }

    return true;
  }

  bool setLogLevel(std::string const& name, LogLevel level) {
    auto* topic = find(name);

    if (topic == nullptr) {
      TRI_ASSERT(false) << "topic '" << name << "' not initialized";
      return false;
    }

    topic->setLogLevel(level);
    return true;
  }

  LogTopic* get(size_t idx) const noexcept { return _topics[idx]; }

  LogTopic* find(std::string const& name) const noexcept {
    auto const it = _nameToIndex.find(name);
    return it == _nameToIndex.end() ? nullptr : _topics[it->second];
  }

  void emplace(std::string const& name, LogTopic* topic) noexcept {
    auto const it = _nameToIndex.find(name);
    ADB_PROD_ASSERT(it != _nameToIndex.end() && _topics[it->second] == nullptr);
    _topics[it->second] = topic;
  }

 private:
  Topics() {
    logger::TopicList::foreach ([this]<typename Topic>() {
      _nameToIndex[std::string(Topic::name)] =
          logger::TopicList::index<Topic>();
    });
  }
  std::array<LogTopic*, logger::kNumTopics> _topics{};
  std::map<std::string, size_t> _nameToIndex;

  Topics(const Topics&) = delete;
  Topics& operator=(const Topics&) = delete;
};  // Topics

}  // namespace

// pseudo-topic to address all log topics
std::string const LogTopic::ALL("all");

LogTopic Logger::AGENCY(logger::topic::Agency{});
LogTopic Logger::AGENCYCOMM(logger::topic::Agencycomm{});
LogTopic Logger::AGENCYSTORE(logger::topic::Agencystore{});
LogTopic Logger::AQL(logger::topic::Aql{});
LogTopic Logger::AUTHENTICATION(logger::topic::Authentication{});
LogTopic Logger::AUTHORIZATION(logger::topic::Authorization{});
LogTopic Logger::BACKUP(logger::topic::Backup{});
LogTopic Logger::BENCH(logger::topic::Bench{});
LogTopic Logger::CACHE(logger::topic::Cache{});
LogTopic Logger::CLUSTER(logger::topic::Cluster{});
LogTopic Logger::COMMUNICATION(logger::topic::Communication{});
LogTopic Logger::CONFIG(logger::topic::Config{});
LogTopic Logger::CRASH(logger::topic::Crash{});
LogTopic Logger::DEVEL(logger::topic::Development{});
LogTopic Logger::DUMP(logger::topic::Dump{});
LogTopic Logger::ENGINES(logger::topic::Engines{});
LogTopic Logger::FIXME(logger::topic::Fixme{});
LogTopic Logger::FLUSH(logger::topic::Flush{});
LogTopic Logger::GRAPHS(logger::topic::Graphs{});
LogTopic Logger::HEARTBEAT(logger::topic::Heartbeat{});
LogTopic Logger::HTTPCLIENT(logger::topic::Httpclient{});
LogTopic Logger::LICENSE(logger::topic::License{});
LogTopic Logger::MAINTENANCE(logger::topic::Maintenance{});
LogTopic Logger::MEMORY(logger::topic::Memory{});
LogTopic Logger::QUERIES(logger::topic::Queries{});
LogTopic Logger::REPLICATION(logger::topic::Replication{});
LogTopic Logger::REPLICATION2(logger::topic::Replication2{});
LogTopic Logger::REPLICATED_STATE(logger::topic::ReplicatedState{});
LogTopic Logger::REPLICATED_WAL(logger::topic::ReplicatedWal{});
LogTopic Logger::REQUESTS(logger::topic::Requests{});
LogTopic Logger::RESTORE(logger::topic::Restore{});
LogTopic Logger::ROCKSDB(logger::topic::Rocksdb{});
LogTopic Logger::SECURITY(logger::topic::Security{});
LogTopic Logger::SSL(logger::topic::Ssl{});
LogTopic Logger::STARTUP(logger::topic::Startup{});
LogTopic Logger::STATISTICS(logger::topic::Statistics{});
LogTopic Logger::SUPERVISION(logger::topic::Supervision{});
LogTopic Logger::SYSCALL(logger::topic::Syscall{});
LogTopic Logger::THREADS(logger::topic::Threads{});
LogTopic Logger::TRANSACTIONS(logger::topic::Trx{});
LogTopic Logger::TTL(logger::topic::Ttl{});
LogTopic Logger::VALIDATION(logger::topic::Validation{});
LogTopic Logger::V8(logger::topic::V8{});
LogTopic Logger::VIEWS(logger::topic::Views{});
LogTopic Logger::DEPRECATION(logger::topic::Deprecation{});

#ifdef USE_ENTERPRISE
LogTopic AuditFeature::AUDIT_AUTHENTICATION(logger::audit::Authentication{});
LogTopic AuditFeature::AUDIT_AUTHORIZATION(logger::audit::Authorization{});
LogTopic AuditFeature::AUDIT_DATABASE(logger::audit::Database{});
LogTopic AuditFeature::AUDIT_COLLECTION(logger::audit::Collection{});
LogTopic AuditFeature::AUDIT_VIEW(logger::audit::View{});
LogTopic AuditFeature::AUDIT_DOCUMENT(logger::audit::Document{});
LogTopic AuditFeature::AUDIT_SERVICE(logger::audit::Service{});
LogTopic AuditFeature::AUDIT_HOTBACKUP(logger::audit::HotBackup{});
#endif

std::vector<std::pair<std::string, LogLevel>> LogTopic::logLevelTopics() {
  std::vector<std::pair<std::string, LogLevel>> levels;

  auto visitor = [&levels](std::string const& name, LogTopic const* topic) {
    levels.emplace_back(name, topic->level());
    return true;
  };

  Topics::instance().visit(visitor);

  return levels;
}

void LogTopic::setLogLevel(std::string const& name, LogLevel level) {
  if (!Topics::instance().setLogLevel(name, level)) {
    LOG_TOPIC("5363d", WARN, arangodb::Logger::FIXME)
        << "strange topic '" << name << "'";
  }
}

LogTopic* LogTopic::lookup(std::string const& name) {
  return Topics::instance().find(name);
}

std::string_view LogTopic::lookup(size_t topicId) {
  auto* topic = Topics::instance().get(topicId);
  return topic->name();
}

template<typename Topic>
LogTopic::LogTopic(Topic)
    : LogTopic(std::string(Topic::name), Topic::defaultLevel,
               logger::TopicList::index<Topic>()) {
  static_assert(logger::TopicList::contains<Topic>(),
                "Topic not found in TopicList");
}

LogTopic::LogTopic(std::string const& name, LogLevel level, size_t id)
    : _id(id), _name(name), _level(level) {
  // "all" is only a pseudo-topic.
  TRI_ASSERT(name != "all");

  if (name != "fixme" && name != "general") {
    // "fixme" is a remainder from ArangoDB < 3.2, when it was
    // allowed to log messages without a topic. From 3.2 onwards,
    // logging is always topic-based, and all previously topicless
    // log invocations now use the log topic "fixme".
    _displayName = std::string("{") + name + "} ";
  }

  Topics::instance().emplace(name, this);
  TRI_ASSERT(_id < MAX_LOG_TOPICS);
}

// those two log topics are created in other files, so we have to explicitly
// instantiate them here
template LogTopic::LogTopic(logger::topic::ArangoSearch);
template LogTopic::LogTopic(logger::topic::LibIResearch);
