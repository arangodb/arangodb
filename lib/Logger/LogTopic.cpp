////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <cstdint>
#include <map>
#include <type_traits>

#include "LogTopic.h"

#include "Basics/MutexLocker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#include "Enterprise/Ldap/LdapFeature.h"
#endif

using namespace arangodb;

namespace {

std::atomic<uint16_t> NEXT_TOPIC_ID(0);

class Topics {
 public:
  static Topics& instance() noexcept {
    // local to avoid init-order-fiasco problem
    static Topics INSTANCE;
    return INSTANCE;
  }

  template <typename Visitor>
  bool visit(Visitor const& visitor) const {
    MUTEX_LOCKER(guard, _namesLock);

    for (auto const& topic : _names) {
      if (!visitor(topic.first, topic.second)) {
        return false;
      }
    }

    return true;
  }

  bool setLogLevel(std::string const& name, LogLevel level) {
    MUTEX_LOCKER(guard, _namesLock);

    auto const it = _names.find(name);

    if (it == _names.end()) {
      return false;
    }

    auto* topic = it->second;

    if (!topic) {
      return false;
    }

    topic->setLogLevel(level);
    return true;
  }

  LogTopic* find(std::string const& name) const noexcept {
    MUTEX_LOCKER(guard, _namesLock);
    auto const it = _names.find(name);
    return it == _names.end() ? nullptr : it->second;
  }

  void emplace(std::string const& name, LogTopic* topic) noexcept {
    try {
      MUTEX_LOCKER(guard, _namesLock);
      _names[name] = topic;
    } catch (...) {
    }
  }

 private:
  mutable Mutex _namesLock;
  std::map<std::string, LogTopic*> _names;

  Topics() = default;
  Topics(const Topics&) = delete;
  Topics& operator=(const Topics&) = delete;
};  // Topics

}  // namespace

LogTopic Logger::AGENCY("agency", LogLevel::INFO);
LogTopic Logger::AGENCYCOMM("agencycomm", LogLevel::INFO);
LogTopic Logger::AGENCYSTORE("agencystore", LogLevel::WARN);
LogTopic Logger::AQL("aql", LogLevel::INFO);
LogTopic Logger::AUTHENTICATION("authentication");
LogTopic Logger::AUTHORIZATION("authorization");
LogTopic Logger::BACKUP("backup");
LogTopic Logger::CACHE("cache", LogLevel::INFO);
LogTopic Logger::CLUSTER("cluster", LogLevel::INFO);
LogTopic Logger::CLUSTERCOMM("clustercomm", LogLevel::INFO);
LogTopic Logger::COLLECTOR("collector");
LogTopic Logger::COMMUNICATION("communication", LogLevel::INFO);
LogTopic Logger::COMPACTOR("compactor");
LogTopic Logger::CONFIG("config");
LogTopic Logger::CRASH("crash");
LogTopic Logger::DATAFILES("datafiles", LogLevel::INFO);
LogTopic Logger::DEVEL("development", LogLevel::FATAL);
LogTopic Logger::DUMP("dump", LogLevel::INFO);
LogTopic Logger::ENGINES("engines", LogLevel::INFO);
LogTopic Logger::FIXME("general", LogLevel::INFO);
LogTopic Logger::FLUSH("flush", LogLevel::INFO);
LogTopic Logger::GRAPHS("graphs", LogLevel::INFO);
LogTopic Logger::HEARTBEAT("heartbeat", LogLevel::INFO);
LogTopic Logger::HTTPCLIENT("httpclient", LogLevel::WARN);
LogTopic Logger::MAINTENANCE("maintenance", LogLevel::WARN);
LogTopic Logger::MEMORY("memory", LogLevel::INFO);
LogTopic Logger::MMAP("mmap");
LogTopic Logger::PERFORMANCE("performance", LogLevel::WARN);
LogTopic Logger::PREGEL("pregel", LogLevel::INFO);
LogTopic Logger::QUERIES("queries", LogLevel::INFO);
LogTopic Logger::REPLICATION("replication", LogLevel::INFO);
LogTopic Logger::REQUESTS("requests", LogLevel::FATAL);  // suppress
LogTopic Logger::RESTORE("restore", LogLevel::INFO);
LogTopic Logger::ROCKSDB("rocksdb", LogLevel::WARN);
LogTopic Logger::SECURITY("security", LogLevel::INFO);
LogTopic Logger::SSL("ssl", LogLevel::WARN);
LogTopic Logger::STARTUP("startup", LogLevel::INFO);
LogTopic Logger::STATISTICS("statistics", LogLevel::INFO);
LogTopic Logger::SUPERVISION("supervision", LogLevel::INFO);
LogTopic Logger::SYSCALL("syscall", LogLevel::INFO);
LogTopic Logger::THREADS("threads", LogLevel::WARN);
LogTopic Logger::TRANSACTIONS("trx", LogLevel::WARN);
LogTopic Logger::TTL("ttl", LogLevel::WARN);
LogTopic Logger::VALIDATION("validation", LogLevel::INFO);
LogTopic Logger::V8("v8", LogLevel::WARN);
LogTopic Logger::VIEWS("views", LogLevel::FATAL);

#ifdef USE_ENTERPRISE
LogTopic LdapFeature::LDAP_TOPIC("ldap", LogLevel::INFO);

LogTopic AuditFeature::AUDIT_AUTHENTICATION("audit-authentication", LogLevel::DEBUG);
LogTopic AuditFeature::AUDIT_AUTHORIZATION("audit-authorization", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_DATABASE("audit-database", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_COLLECTION("audit-collection", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_VIEW("audit-view", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_DOCUMENT("audit-document", LogLevel::DEBUG);
LogTopic AuditFeature::AUDIT_SERVICE("audit-service", LogLevel::INFO);
LogTopic AuditFeature::AUDIT_HOTBACKUP("audit-hotbackup", LogLevel::INFO);
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
    LOG_TOPIC("5363d", WARN, arangodb::Logger::FIXME) << "strange topic '" << name << "'";
  }
}

LogTopic* LogTopic::lookup(std::string const& name) {
  return Topics::instance().find(name);
}

std::string LogTopic::lookup(size_t topicId) {
  std::string name("UNKNOWN");

  auto visitor = [&name, topicId](std::string const&, LogTopic const* topic) {
    if (topic->_id == topicId) {
      name = topic->_name;
      return false;  // break the loop
    }
    return true;
  };

  Topics::instance().visit(visitor);

  return name;
}

LogTopic::LogTopic(std::string const& name)
    : LogTopic(name, LogLevel::DEFAULT) {}

LogTopic::LogTopic(std::string const& name, LogLevel level)
    : _id(NEXT_TOPIC_ID.fetch_add(1, std::memory_order_seq_cst)),
      _name(name),
      _level(level) {
  if (name != "fixme" && name != "general") {
    // "fixme" is a remainder from ArangoDB < 3.2, when it was
    // allowed to log messages without a topic. From 3.2 onwards,
    // logging is always topic-based, and all previously topicless
    // log invocations now use the log topic "fixme".
    _displayName = std::string("{") + name + "} ";
  }

  Topics::instance().emplace(name, this);
}
