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
#include "Transaction/StandaloneContext.h"
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

inline static std::string timestamp() {
  std::time_t t = std::time(nullptr);
  char mbstr[100];
  return
    std::strftime(
      mbstr, sizeof(mbstr), "%Y-%m-%d %H:%M:%S %Z", std::localtime(&t)) ?
    std::string(mbstr) : std::string();
}

inline static std::string stringify(arangodb::consensus::index_t index) {
  std::ostringstream i_str;
  i_str << std::setw(20) << std::setfill('0') << index;
  return i_str.str();
}

/// Persist one entry
bool State::persist(arangodb::consensus::index_t index, term_t term,
                    arangodb::velocypack::Slice const& entry,
                    std::string const& clientId) const {

  Builder body;
  {
    VPackObjectBuilder b(&body);
    body.add("_key", Value(stringify(index)));
    body.add("term", Value(term));
    body.add("request", entry);
    body.add("clientId", Value(clientId));
    body.add("timestamp", Value(timestamp()));
  }
  
  TRI_ASSERT(_vocbase != nullptr);
  auto transactionContext =
    std::make_shared<transaction::StandaloneContext>(_vocbase);
  SingleCollectionTransaction trx(
    transactionContext, "log", AccessMode::Type::WRITE);
  
  Result res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  OperationResult result;
  try {
    result = trx.insert("log", body.slice(), _options);
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Failed to persist log entry:" << e.what();
  }
  
  res = trx.finish(result.code);

  return res.ok();
}


/// Log transaction (leader)
std::vector<arangodb::consensus::index_t> State::log(
  query_t const& transactions, std::vector<bool> const& applicable, term_t term) {
  
  std::vector<arangodb::consensus::index_t> idx(applicable.size());
  
  size_t j = 0;
  
  auto const& slice = transactions->slice();

  if (!slice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      30000, "Agency syntax requires array of transactions [[<queries>]]");
  }

  TRI_ASSERT(slice.length() == applicable.size());
  
  MUTEX_LOCKER(mutexLocker, _logLock); 
  
  for (auto const& i : VPackArrayIterator(slice)) {

    if (!i.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        30000,
        "Transaction syntax is [{<operations>}, {<preconditions>}, \"clientId\"]"
        );
    }

    if (applicable[j]) {

      std::shared_ptr<Buffer<uint8_t>> buf =
          std::make_shared<Buffer<uint8_t>>();
      buf->append((char const*)i[0].begin(), i[0].byteSize());

      std::string clientId;
      if (i.length()==3) {
        clientId = i[2].copyString();
      }
      
      TRI_ASSERT(!_log.empty()); // log must not ever be empty
      idx[j] = _log.back().index + 1;
      _log.push_back(log_t(idx[j], term, buf, clientId));  // log to RAM
      _clientIdLookupTable.emplace(
        std::pair<std::string, arangodb::consensus::index_t>(clientId, idx[j]));
      persist(idx[j], term, i[0], clientId);               // log to disk
    }
    
    ++j;
  }

  return idx;
  
}

/// Log transaction (leader)
arangodb::consensus::index_t State::log(
  velocypack::Slice const& slice, term_t term, std::string const& clientId) {

  arangodb::consensus::index_t idx = 0;
  auto buf = std::make_shared<Buffer<uint8_t>>();
  
  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order
  buf->append((char const*)slice.begin(), slice.byteSize());
  TRI_ASSERT(!_log.empty()); // log must not ever be empty
  idx = _log.back().index + 1;
  _log.push_back(log_t(idx, term, buf, clientId));  // log to RAM
  _clientIdLookupTable.emplace(
    std::pair<std::string, arangodb::consensus::index_t>(clientId, idx));
  persist(idx, term, slice, clientId);               // log to disk

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
  std::string clientId;

  for (size_t i = ndups; i < nqs; ++i) {
    VPackSlice slice = slices[i];

    try {

      auto idx = slice.get("index").getUInt();
      auto trm = slice.get("term").getUInt();
      clientId = slice.get("clientId").copyString();

      auto buf = std::make_shared<Buffer<uint8_t>>();
      buf->append((char const*)slice.get("query").begin(),
                  slice.get("query").byteSize());
      // to RAM
      _log.push_back(log_t(idx, trm, buf, clientId));
      _clientIdLookupTable.emplace(
        std::pair<std::string, arangodb::consensus::index_t>(clientId, idx));

      // to disk
      persist(idx, trm, slice.get("query"), clientId);

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
                false, _vocbase, aql::QueryString(aql), bindVars, nullptr,
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

  if (end == (std::numeric_limits<uint64_t>::max)() || end > _log.back().index) {
    end = _log.back().index;
  }

  if (start < _log[0].index) {
    start = _log[0].index;
  }

  for (size_t i = start - _cur; i <= end - _cur; ++i) {
    entries.push_back(_log[i]);
  }

  return entries;
}

/// Get log entries from indices "start" to "end"
/// Throws std::out_of_range exception
log_t State::at(arangodb::consensus::index_t index) const {

  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  
  if (_cur > index) {
    std::string excMessage = 
      std::string(
        "Access before the start of the log deque: (first, requested): (") +
      std::to_string(_cur) + ", " + std::to_string(index);
    LOG_TOPIC(DEBUG, Logger::AGENCY) << excMessage;
    throw std::out_of_range(excMessage);
  }
  
  auto pos = index - _cur;
  if (pos > _log.size()) {
    std::string excMessage = 
      std::string(
        "Access beyond the end of the log deque: (last, requested): (") +
      std::to_string(_cur+_log.size()) + ", " + std::to_string(index);
    LOG_TOPIC(DEBUG, Logger::AGENCY) << excMessage;
    throw std::out_of_range(excMessage);
  }

  return _log[pos];

}


/// Have log with specified index and term
bool State::has(arangodb::consensus::index_t index, term_t term) const {

  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  try {
    return _log.at(index-_cur).term == term;
  } catch (...) {}

  return false;
  
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
  { VPackObjectBuilder b(&body);
    body.add("type", VPackValue(static_cast<int>(TRI_COL_TYPE_DOCUMENT))); 
    body.add("name", VPackValue(name));
    body.add("isSystem", VPackValue(LogicalCollection::IsSystemName(name)));
  }

  arangodb::LogicalCollection const* collection =
      _vocbase->createCollection(body.slice());

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_errno(), "cannot create collection");
  }

  return true;
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
      _log.push_back(
        log_t(arangodb::consensus::index_t(0), term_t(0), buf, std::string()));
      persist(0, 0, value, std::string());
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
  arangodb::aql::Query query(false, _vocbase, aql::QueryString(aql), bindVars,
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

  arangodb::aql::Query query(false, _vocbase, aql::QueryString(aql), bindVars,
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

    MUTEX_LOCKER(guard, _configurationWriteLock);

    LOG_TOPIC(DEBUG, Logger::AGENCY) << "New agency!";

    TRI_ASSERT(_agent != nullptr);
    _agent->id(to_string(boost::uuids::random_generator()()));

    auto transactionContext =
        std::make_shared<transaction::StandaloneContext>(_vocbase);
    SingleCollectionTransaction trx(
      transactionContext, "configuration", AccessMode::Type::WRITE);

    Result res = trx.begin();
    OperationResult result;

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    Builder doc;
    { VPackObjectBuilder d(&doc);
      doc.add("_key", VPackValue("0"));
      doc.add("cfg", _agent->config().toBuilder()->slice()); }

    try {
      result = trx.insert("configuration", doc.slice(), _options);
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Failed to persist configuration entry:" << e.what();
      FATAL_ERROR_EXIT();
    }

    res = trx.finish(result.code);

    return res.ok();
  }

  return true;
}

/// Load beyond last compaction
bool State::loadRemaining() {
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  std::string const aql(std::string("FOR l IN log SORT l._key RETURN l"));
  arangodb::aql::Query query(false, _vocbase, aql::QueryString(aql), bindVars,
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
      std::string clientId;
      for (auto const& i : VPackArrayIterator(result)) {

        buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();

        auto ii = i.resolveExternals();
        auto req = ii.get("request");
        tmp->append(req.startAs<char const>(), req.byteSize());

        clientId = req.hasKey("clientId") ?
          req.get("clientId").copyString() : std::string();

        try {
          _log.push_back(
            log_t(std::stoi(ii.get(StaticStrings::KeyString).copyString()),
                  static_cast<term_t>(ii.get("term").getUInt()), tmp, clientId));
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
  
  _agent->lastCommitted(back);
  _agent->rebuildDBs();

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

  arangodb::aql::Query query(false, _vocbase, aql::QueryString(aql), bindVars,
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

    arangodb::aql::Query query(false, _vocbase, aql::QueryString(aql),
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
    std::stringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << cind;

    Builder store;
    { VPackObjectBuilder s(&store);
      store.add(VPackValue("readDB"));
      { VPackArrayBuilder a(&store); 
        _agent->readDB().dumpToBuilder(store); }
      store.add("_key", VPackValue(i_str.str())); }

    TRI_ASSERT(_vocbase != nullptr);
    auto transactionContext =
        std::make_shared<transaction::StandaloneContext>(_vocbase);
    SingleCollectionTransaction trx(
      transactionContext, "compact", AccessMode::Type::WRITE);

    Result res = trx.begin();

    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }

    auto result = trx.insert("compact", store.slice(), _options);
    res = trx.finish(result.code);

    return res.ok();
  }

  LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist read DB for compaction!";
  return false;
}

void State::persistActiveAgents(query_t const& active, query_t const& pool) {
  TRI_ASSERT(_vocbase != nullptr);

  Builder builder;
  { VPackObjectBuilder guard(&builder);
    builder.add("_key", VPackValue("0"));
    builder.add(VPackValue("cfg"));
    { VPackObjectBuilder guard2(&builder);
      builder.add("active", active->slice());
      builder.add("pool", pool->slice());
    }
  }

  MUTEX_LOCKER(guard, _configurationWriteLock);

  auto transactionContext =
      std::make_shared<transaction::StandaloneContext>(_vocbase);
  SingleCollectionTransaction trx(
    transactionContext, "configuration", AccessMode::Type::WRITE);

  Result res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto result = trx.update("configuration", builder.slice(), _options);
  if (!result.successful()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(result.code, result.errorMessage);
  }
  res = trx.finish(result.code);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
  }
}

query_t State::allLogs() const {
  MUTEX_LOCKER(mutexLocker, _logLock);

  auto bindVars = std::make_shared<VPackBuilder>();
  { VPackObjectBuilder(bindVars.get()); }
  
  std::string const comp("FOR c IN compact SORT c._key RETURN c");
  std::string const logs("FOR l IN log SORT l._key RETURN l");

  arangodb::aql::Query compq(false, _vocbase, aql::QueryString(comp),
                             bindVars, nullptr, arangodb::aql::PART_MAIN);
  arangodb::aql::Query logsq(false, _vocbase, aql::QueryString(logs),
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
  { VPackObjectBuilder(everything.get());
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
  }
  return everything;
  
}

std::vector<std::vector<log_t>> State::inquire(query_t const& query) const {

  std::vector<std::vector<log_t>> result;
  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)
  
  if (!query->slice().isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        210002,
        std::string("Inquiry handles a list of string clientIds: [<clientId>] ")
        + ". We got " + query->toJson());
    return result;
  }

  size_t pos = 0;
  for (auto const& i : VPackArrayIterator(query->slice())) {

    if (!i.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        210002, std::string("ClientIds must be strings. On position ")
        + std::to_string(pos) + " we got " + i.toJson());
    }

    std::vector<log_t> transactions;
    auto ret = _clientIdLookupTable.equal_range(i.copyString());
    for (auto it = ret.first; it != ret.second; ++it) {
      if (it->second < _log[0].index) {
        continue;
      }
      transactions.push_back(_log.at(it->second-_cur));
    }
    result.push_back(transactions);

    pos++;
    
  }

  return result;
  
}

// Index of last log entry
arangodb::consensus::index_t State::lastIndex() const {
  MUTEX_LOCKER(mutexLocker, _logLock); 
  return (!_log.empty()) ? _log.back().index : 0; 
}

