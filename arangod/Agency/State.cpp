////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// WITHOUT ARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "State.h"

#include <velocypack/Buffer.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "Agency/Agent.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "RestServer/QueryRegistryFeature.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::aql;
using namespace arangodb::consensus;
using namespace arangodb::velocypack;
using namespace arangodb::rest;

/// Construct with endpoint
State::State(std::string const& endpoint)
    : _agent(nullptr),
      _vocbase(nullptr),
      _endpoint(endpoint),
      _collectionsChecked(false),
      _collectionsLoaded(false),
      _queryRegistry(nullptr),
      _cur(0) {}

/// Default dtor
State::~State() {}

inline static std::string stringify(arangodb::consensus::index_t index) {
  std::ostringstream i_str;
  i_str << std::setw(20) << std::setfill('0') << index;
  return i_str.str();
}

/// Persist one entry
bool State::persist(arangodb::consensus::index_t index, term_t term,
                    arangodb::velocypack::Slice const& entry) const {
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  body.add("_key", Value(stringify(index)));
  body.add("term", Value(term));
  body.add("request", entry);
  body.close();
  
  TRI_ASSERT(_vocbase != nullptr);
  auto transactionContext =
    std::make_shared<StandaloneTransactionContext>(_vocbase);
  SingleCollectionTransaction trx(transactionContext, "log",
                                  TRI_TRANSACTION_WRITE);
  
  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  OperationResult result;
  try {
    result = trx.insert("log", body.slice(), _options);
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist log entry:"
                                   << e.what();
  }
  res = trx.finish(result.code);

  return (res == TRI_ERROR_NO_ERROR);
}

/// Log transaction (leader)
std::vector<arangodb::consensus::index_t> State::log(
    query_t const& transaction, std::vector<bool> const& appl, term_t term) {
  std::vector<arangodb::consensus::index_t> idx(appl.size());
  std::vector<bool> good = appl;
  size_t j = 0;
  auto const& slice = transaction->slice();

  if (!slice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(30000,
                                   "Agency request syntax is [[<queries>]]");
  }

  if (slice.length() != good.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(30000,
                                   "Agency request syntax is [[<queries>]]");
  }

  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order
  for (auto const& i : VPackArrayIterator(slice)) {
    TRI_ASSERT(i.isArray());
    if (good[j]) {
      std::shared_ptr<Buffer<uint8_t>> buf =
          std::make_shared<Buffer<uint8_t>>();
      buf->append((char const*)i[0].begin(), i[0].byteSize());
      TRI_ASSERT(!_log.empty()); // log must not ever be empty
      idx[j] = _log.back().index + 1;
      _log.push_back(log_t(idx[j], term, buf));  // log to RAM
      persist(idx[j], term, i[0]);               // log to disk
      ++j;
    }
  }

  return idx;
}

/// Log transaction (leader)
arangodb::consensus::index_t State::log(
  velocypack::Slice const& slice, term_t term) {

  arangodb::consensus::index_t idx = 0;
  auto buf = std::make_shared<Buffer<uint8_t>>();
  
  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order
  buf->append((char const*)slice.begin(), slice.byteSize());
  TRI_ASSERT(!_log.empty()); // log must not ever be empty
  idx = _log.back().index + 1;
  _log.push_back(log_t(idx, term, buf));  // log to RAM
  persist(idx, term, slice);               // log to disk

  return _log.back().index;
  
}

/// Log transactions (follower)
arangodb::consensus::index_t State::log(query_t const& transactions,
                                        size_t ndups) {
  VPackSlice slices = transactions->slice();

  TRI_ASSERT(slices.isArray());

  size_t nqs = slices.length();

  TRI_ASSERT(nqs > ndups);

  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order

  for (size_t i = ndups; i < nqs; ++i) {
    VPackSlice slice = slices[i];

    try {
      auto idx = slice.get("index").getUInt();
      auto trm = slice.get("term").getUInt();
      auto buf = std::make_shared<Buffer<uint8_t>>();

      buf->append((char const*)slice.get("query").begin(),
                  slice.get("query").byteSize());
      // to RAM
      _log.push_back(log_t(idx, trm, buf));
      // to disk
      persist(idx, trm, slice.get("query"));
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__ << __LINE__;
    }
  }

  TRI_ASSERT(!_log.empty());
  return _log.back().index;
}

size_t State::removeConflicts(query_t const& transactions) { // Under MUTEX in Agent
  VPackSlice slices = transactions->slice();
  TRI_ASSERT(slices.isArray());
  size_t ndups = 0;

  if (slices.length() > 0) {
    auto bindVars = std::make_shared<VPackBuilder>();
    bindVars->openObject();
    bindVars->close();

    try {
      MUTEX_LOCKER(logLock, _logLock);
      auto idx = slices[0].get("index").getUInt();
      auto pos = idx - _cur;

      if (pos < _log.size()) {
        for (auto const& slice : VPackArrayIterator(slices)) {
          auto trm = slice.get("term").getUInt();
          idx = slice.get("index").getUInt();
          pos = idx - _cur;

          if (pos < _log.size()) {
            if (idx == _log.at(pos).index && trm != _log.at(pos).term) {
              LOG_TOPIC(DEBUG, Logger::AGENCY)
                  << "Removing " << _log.size() - pos
                  << " entries from log starting with " << idx
                  << "==" << _log.at(pos).index << " and " << trm << "="
                  << _log.at(pos).term;

              // persisted logs
              std::string const aql(std::string("FOR l IN log FILTER l._key >= '") + 
                                    stringify(idx) + "' REMOVE l IN log");

              arangodb::aql::Query query(
                false, _vocbase, aql.c_str(), aql.size(), bindVars, nullptr,
                arangodb::aql::PART_MAIN);

              auto queryResult = query.execute(_queryRegistry);

              if (queryResult.code != TRI_ERROR_NO_ERROR) {
                THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code,
                                               queryResult.details);
              }

              // volatile logs
              _log.erase(_log.begin() + pos, _log.end());
              
              break;

            } else {
              ++ndups;
            }
          }
        }
      }
    } catch (std::exception const& e) {
      LOG_TOPIC(DEBUG, Logger::AGENCY) << e.what() << " " << __FILE__
                                       << __LINE__;
    }
  }

  return ndups;
}

/// Get log entries from indices "start" to "end"
std::vector<log_t> State::get(arangodb::consensus::index_t start,
                              arangodb::consensus::index_t end) const {
  std::vector<log_t> entries;
  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  if (_log.empty()) {
    return entries;
  }

  if (end == (std::numeric_limits<uint64_t>::max)() || end > _log.size() - 1) {
    end = _log.size() - 1;
  }

  if (start < _log[0].index) {
    start = _log[0].index;
  }

  for (size_t i = start - _cur; i <= end; ++i) {
    entries.push_back(_log[i]);
  }

  return entries;
}

/// Get vector of past transaction from 'start' to 'end'
std::vector<VPackSlice> State::slices(arangodb::consensus::index_t start,
                                      arangodb::consensus::index_t end) const {
  std::vector<VPackSlice> slices;
  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  if (_log.empty()) {
    return slices;
  }

  if (start < _log.front().index) {  // no start specified
    start = _log.front().index;
  }

  if (start > _log.back().index) {  // no end specified
    return slices;
  }

  if (end == (std::numeric_limits<uint64_t>::max)() ||
      end > _log.back().index) {
    end = _log.back().index;
  }

  for (size_t i = start - _cur; i <= end - _cur; ++i) {
    try {
      slices.push_back(VPackSlice(_log.at(i).entry->data()));
    } catch (std::exception const&) {
      break;
    }
  }

  return slices;
}

/// Get log entry by log index, copy entry because we do no longer have the
/// lock after the return
log_t State::operator[](arangodb::consensus::index_t index) const {
  MUTEX_LOCKER(mutexLocker, _logLock);  // Cannot be read lock (Compaction)
  TRI_ASSERT(index - _cur < _log.size());
  return _log.at(index - _cur);
}

/// Get last log entry, copy entry because we do no longer have the lock
/// after the return
log_t State::lastLog() const {
  MUTEX_LOCKER(mutexLocker, _logLock);  // Cannot be read lock (Compaction)
  TRI_ASSERT(!_log.empty());
  return _log.back();
}

/// Configure with agent
bool State::configure(Agent* agent) {
  _agent = agent;
  _endpoint = agent->endpoint();
  _collectionsChecked = false;
  return true;
}

/// Check if collections exist otherwise create them
bool State::checkCollections() {
  if (!_collectionsChecked) {
    _collectionsChecked = checkCollection("log") && checkCollection("election");
  }
  return _collectionsChecked;
}

/// Create agency collections
bool State::createCollections() {
  if (!_collectionsChecked) {
    return (createCollection("log") && createCollection("election") &&
            createCollection("compact"));
  }
  return _collectionsChecked;
}

/// Check collection by name
bool State::checkCollection(std::string const& name) {
  if (!_collectionsChecked) {
    return (_vocbase->lookupCollection(name) != nullptr);
  }
  return true;
}

/// Create collection by name
bool State::createCollection(std::string const& name) {
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  body.add("type", VPackValue(static_cast<int>(TRI_COL_TYPE_DOCUMENT))); 
  body.add("name", VPackValue(name));
  body.add("isSystem", VPackValue(LogicalCollection::IsSystemName(name)));
  body.close();

  TRI_voc_cid_t cid = 0;
  arangodb::LogicalCollection const* collection =
      _vocbase->createCollection(body.slice(), cid, true);

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_errno(), "cannot create collection");
  }

  return true;
}

template <class T>
std::ostream& operator<<(std::ostream& o, std::deque<T> const& d) {
  for (auto const& i : d) {
    o << i;
  }
  return o;
}

/// Load collections
bool State::loadCollections(TRI_vocbase_t* vocbase,
                            QueryRegistry* queryRegistry, bool waitForSync) {

  _vocbase = vocbase;
  _queryRegistry = queryRegistry;

  TRI_ASSERT(_vocbase != nullptr);

  _options.waitForSync = waitForSync;
  _options.silent = true;

  
  if (loadPersisted()) {
    MUTEX_LOCKER(logLock, _logLock);
    if (_log.empty()) {
      std::shared_ptr<Buffer<uint8_t>> buf =
          std::make_shared<Buffer<uint8_t>>();
      VPackSlice value = arangodb::basics::VelocyPackHelper::EmptyObjectValue();
      buf->append(value.startAs<char const>(), value.byteSize());
      _log.push_back(log_t(arangodb::consensus::index_t(0), term_t(0), buf));
      persist(0, 0, value);
    }
    return true;
  }

  return false;
}

/// Load actually persisted collections
bool State::loadPersisted() {
  TRI_ASSERT(_vocbase != nullptr);

  if (!checkCollection("configuration")) {
    createCollection("configuration");
  }

  loadOrPersistConfiguration();

  if (checkCollection("log") && checkCollection("compact")) {
      bool lc = loadCompacted();
      bool lr = loadRemaining();
        return (lc && lr);
  }

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Couldn't find persisted log";
  createCollections();

  return true;
}

/// Load compaction collection
bool State::loadCompacted() {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  
  std::string const aql(
      std::string("FOR c IN compact SORT c._key DESC LIMIT 1 RETURN c"));
  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  VPackSlice result = queryResult.result->slice();

  if (result.isArray() && result.length()) {
    MUTEX_LOCKER(logLock, _logLock);
    for (auto const& i : VPackArrayIterator(result)) {
      auto ii = i.resolveExternals();
      buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();
      (*_agent) = ii;
      try {
        _cur = std::stoul(ii.get("_key").copyString());
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__
                                       << __LINE__;
      }
    }
  }

  return true;
}


/// Load persisted configuration
bool State::loadOrPersistConfiguration() {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::string const aql(
      std::string("FOR c in configuration FILTER c._key==\"0\" RETURN c.cfg"));

  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  VPackSlice result = queryResult.result->slice();

  if (result.isArray() &&
      result.length()) {  // We already have a persisted conf

    try {
      LOG_TOPIC(DEBUG, Logger::AGENCY)
        << "Merging configuration " << result[0].resolveExternals().toJson();
      _agent->mergeConfiguration(result[0].resolveExternals());
      
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY)
          << "Failed to merge persisted configuration into runtime "
             "configuration: "
          << e.what();
      FATAL_ERROR_EXIT();
    }

  } else {  // Fresh start

    LOG_TOPIC(DEBUG, Logger::AGENCY) << "New agency!";

    TRI_ASSERT(_agent != nullptr);
    _agent->id(to_string(boost::uuids::random_generator()()));

    auto transactionContext =
        std::make_shared<StandaloneTransactionContext>(_vocbase);
    SingleCollectionTransaction trx(transactionContext, "configuration",
                                    TRI_TRANSACTION_WRITE);

    int res = trx.begin();
    OperationResult result;

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    Builder doc;
    doc.openObject();
    doc.add("_key", VPackValue("0"));
    doc.add("cfg", _agent->config().toBuilder()->slice());
    doc.close();
    try {
      result = trx.insert("configuration", doc.slice(), _options);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist configuration entry:"
                                     << e.what();
      FATAL_ERROR_EXIT();
    }

    res = trx.finish(result.code);

    return (res == TRI_ERROR_NO_ERROR);
  }

  return true;
}

/// Load beyond last compaction
bool State::loadRemaining() {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::string const aql(std::string("FOR l IN log SORT l._key RETURN l"));
  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);
      
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }
      
  auto result = queryResult.result->slice();
  index_t back = 0;

  {
    MUTEX_LOCKER(logLock, _logLock);
    if (result.isArray()) {
      
      _log.clear();
      
      for (auto const& i : VPackArrayIterator(result)) {
        buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();
        auto ii = i.resolveExternals();
        auto req = ii.get("request");
        tmp->append(req.startAs<char const>(), req.byteSize());
        try {
          _log.push_back(
            log_t(std::stoi(ii.get(StaticStrings::KeyString).copyString()),
                  static_cast<term_t>(ii.get("term").getUInt()), tmp));
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failed to convert " +
            ii.get(StaticStrings::KeyString).copyString() +
            " to integer via std::stoi."
            << e.what();
        }
      }
    }
    TRI_ASSERT(!_log.empty());
    back = _log.back().index;
  }
  
  _agent->rebuildDBs();
  _agent->lastCommitted(back);

  return true;
}

/// Find entry by index and term
bool State::find(arangodb::consensus::index_t prevIndex, term_t prevTerm) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (prevIndex > _log.size()) {
    return false;
  }
  return _log.at(prevIndex).term == prevTerm;
}

/// Log compaction
bool State::compact(arangodb::consensus::index_t cind) {
  bool saved = persistReadDB(cind);

  if (saved) {
    compactVolatile(cind);

    try {
      compactPersisted(cind);
      removeObsolete(cind);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to compact persisted store.";
      LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
    }
    return true;
  } else {
    return false;
  }
}

/// Compact volatile state
bool State::compactVolatile(arangodb::consensus::index_t cind) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (!_log.empty() && cind > _cur && cind - _cur < _log.size()) {
    _log.erase(_log.begin(), _log.begin() + (cind - _cur));
    _cur = _log.begin()->index;
  }
  return true;
}

/// Compact persisted state
bool State::compactPersisted(arangodb::consensus::index_t cind) {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::stringstream i_str;
  i_str << std::setw(20) << std::setfill('0') << cind;

  std::string const aql(std::string("FOR l IN log FILTER l._key < \"") +
                        i_str.str() + "\" REMOVE l IN log");

  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  return true;
}

/// Remove outdate compactions
bool State::removeObsolete(arangodb::consensus::index_t cind) {
  if (cind > 3 * _agent->config().compactionStepSize()) {
    auto bindVars = std::make_shared<VPackBuilder>();
    bindVars->openObject();
    bindVars->close();

    std::stringstream i_str;
    i_str << std::setw(20) << std::setfill('0')
          << -3 * _agent->config().compactionStepSize() + cind;

    std::string const aql(std::string("FOR c IN compact FILTER c._key < \"") +
                          i_str.str() + "\" REMOVE c IN compact");

    arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(),
                               bindVars, nullptr, arangodb::aql::PART_MAIN);

    auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);
    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }
  }
  return true;
}


/// Persist the globally commited truth
bool State::persistReadDB(arangodb::consensus::index_t cind) {
  if (checkCollection("compact")) {
    Builder store;
    store.openObject();
    store.add("readDB", VPackValue(VPackValueType::Array));
    _agent->readDB().dumpToBuilder(store);
    store.close();
    std::stringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << cind;
    store.add("_key", VPackValue(i_str.str()));
    store.close();

    TRI_ASSERT(_vocbase != nullptr);
    auto transactionContext =
        std::make_shared<StandaloneTransactionContext>(_vocbase);
    SingleCollectionTransaction trx(transactionContext, "compact",
                                    TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    auto result = trx.insert("compact", store.slice(), _options);
    res = trx.finish(result.code);

    return (res == TRI_ERROR_NO_ERROR);
  }

  LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist read DB for compaction!";
  return false;
}

bool State::persistActiveAgents(query_t const& active, query_t const& pool) {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::stringstream aql;
  aql << "FOR c IN configuration UPDATE {_key:c._key} WITH {cfg:{active:";
  aql << active->slice().toJson();
  aql << ", pool:";
  aql << pool->slice().toJson();
  aql << "}} IN configuration";
  std::string aqlStr = aql.str();

  arangodb::aql::Query query(false, _vocbase, aqlStr.c_str(), aqlStr.size(),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(QueryRegistryFeature::QUERY_REGISTRY);
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  return true;
}

query_t State::allLogs() const {
  MUTEX_LOCKER(mutexLocker, _logLock);

  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();
  
  std::string const comp("FOR c IN compact SORT c._key RETURN c");
  std::string const logs("FOR l IN log SORT l._key RETURN l");

  arangodb::aql::Query compq(false, _vocbase, comp.c_str(), comp.size(),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);
  arangodb::aql::Query logsq(false, _vocbase, logs.c_str(), logs.size(),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);

  auto compqResult = compq.execute(QueryRegistryFeature::QUERY_REGISTRY);
  if (compqResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(compqResult.code, compqResult.details);
  }
  auto logsqResult = logsq.execute(QueryRegistryFeature::QUERY_REGISTRY);
  if (logsqResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(logsqResult.code, logsqResult.details);
  }

  auto everything = std::make_shared<VPackBuilder>();

  everything->openObject();

  try {
    everything->add("compact", compqResult.result->slice());
  } catch (std::exception const&) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Failed to assemble compaction part of everything package";
  }

  try{
    everything->add("logs", logsqResult.result->slice());
  } catch (std::exception const&) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Failed to assemble remaining part of everything package";
  }

  everything->close();

  return everything;
  
}
