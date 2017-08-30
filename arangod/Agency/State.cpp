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

inline static std::string stringify(index_t index) {
  std::ostringstream i_str;
  i_str << std::setw(20) << std::setfill('0') << index;
  return i_str.str();
}

/// Persist one entry
bool State::persist(index_t index, term_t term,
                    arangodb::velocypack::Slice const& entry,
                    std::string const& clientId) const {

  LOG_TOPIC(TRACE, Logger::AGENCY) << "persist index=" << index
    << " term=" << term << " entry: " << entry.toJson();

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

  trx.addHint(transaction::Hints::Hint::SINGLE_OPERATION);
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
    return false;
  }

  res = trx.finish(result.code);

  LOG_TOPIC(TRACE, Logger::AGENCY) << "persist done index=" << index
    << " term=" << term << " entry: " << entry.toJson() << " ok:" << res.ok();

  return res.ok();
}


/// Log transaction (leader)
std::vector<index_t> State::log(
  query_t const& transactions, std::vector<bool> const& applicable, term_t term) {

  std::vector<index_t> idx(applicable.size());
  size_t j = 0;
  auto const& slice = transactions->slice();

  if (!slice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      30000, "Agency syntax requires array of transactions [[<queries>]]");
  }

  TRI_ASSERT(slice.length() == applicable.size());
  MUTEX_LOCKER(mutexLocker, _logLock); 
  
  TRI_ASSERT(!_log.empty()); // log must never be empty
  
  for (auto const& i : VPackArrayIterator(slice)) {

    if (!i.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        30000,
        "Transaction syntax is [{<operations>}, {<preconditions>}, \"clientId\"]"
        );
    }
    
    if (applicable[j]) {
      std::string clientId((i.length()==3) ? i[2].copyString() : "");
      idx[j] = logNonBlocking(_log.back().index+1, i[0], term, clientId, true);
    }
    ++j;
  }

  return idx;
  
}


index_t State::log(velocypack::Slice const& slice, term_t term,
                   std::string const& clientId) {
  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order
  return logNonBlocking(_log.back().index+1, slice, term, clientId, true);
}


/// Log transaction (leader)
index_t State::logNonBlocking(
  index_t idx, velocypack::Slice const& slice, term_t term,
  std::string const& clientId, bool leading) {

  _logLock.assertLockedByCurrentThread();

  TRI_ASSERT(!_log.empty()); // log must not ever be empty

  auto buf = std::make_shared<Buffer<uint8_t>>();
  
  buf->append((char const*)slice.begin(), slice.byteSize());
  
  if (!persist(idx, term, slice, clientId)) {         // log to disk or die
    if (leading) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
        << "RAFT leader fails to persist log entries!"
        << __FILE__ << ":" << __LINE__;
      FATAL_ERROR_EXIT();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "RAFT follower fails to persist log entries!"
        << __FILE__ << ":" << __LINE__;
      return 0;
    }
  }

  try {
    _log.push_back(log_t(idx, term, buf, clientId));  // log to RAM or die
  } catch (std::bad_alloc const&) {
    if (leading) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
        << "RAFT leader fails to allocate volatile log entries!"
        << __FILE__ << ":" << __LINE__;
      FATAL_ERROR_EXIT();
    } else {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "RAFT follower fails to allocate volatile log entries!"
        << __FILE__ << ":" << __LINE__;
      return 0;
    }
  }

  if (leading) {
    try {
      _clientIdLookupTable.emplace(            // keep track of client or die
        std::pair<std::string, index_t>(clientId, idx));
    } catch (...) {
      LOG_TOPIC(FATAL, Logger::AGENCY)
        << "RAFT leader fails to expand client lookup table!"
        << __FILE__ << ":" << __LINE__;
      FATAL_ERROR_EXIT();
    } 
  }
  
  return _log.back().index;
  
}

/// Log transactions (follower)
index_t State::log(query_t const& transactions, size_t ndups) {
  VPackSlice slices = transactions->slice();

  TRI_ASSERT(slices.isArray());

  size_t nqs = slices.length();

  TRI_ASSERT(nqs > ndups);
  std::string clientId;

  MUTEX_LOCKER(mutexLocker, _logLock);  // log entries must stay in order

  for (size_t i = ndups; i < nqs; ++i) {
    VPackSlice const& slice = slices[i];

    // first to disk
    if (logNonBlocking(
          slice.get("index").getUInt(), slice.get("query"),
          slice.get("term").getUInt(), slice.get("clientId").copyString())==0) {
      break;
    }
  }

  return _log.empty() ? 0 : _log.back().index;
}

size_t State::removeConflicts(query_t const& transactions,  bool gotSnapshot) { 
  // Under MUTEX in Agent
  // Note that this will ignore a possible snapshot in the first position!
  // This looks through the transactions and skips over those that are
  // already present (or even already compacted). As soon as we find one
  // for which the new term is higher than the locally stored term, we erase
  // the locally stored log from that position and return, such that we
  // can append from this point on the new stuff. If our log is behind,
  // we might find a position at which we do not yet have log entries,
  // in which case we return and let others update our log.
  VPackSlice slices = transactions->slice();
  TRI_ASSERT(slices.isArray());
  size_t ndups = gotSnapshot ? 1 : 0;

  LOG_TOPIC(TRACE, Logger::AGENCY) << "removeConflicts " << slices.toJson();
  try {
    MUTEX_LOCKER(logLock, _logLock);

    index_t lastIndex = (!_log.empty()) ? _log.back().index : 0; 

    while (ndups < slices.length()) {
      VPackSlice slice = slices[ndups];
      index_t idx = slice.get("index").getUInt();
      if (idx > lastIndex) {
        LOG_TOPIC(TRACE, Logger::AGENCY) << "removeConflicts "
          << idx << " > " << lastIndex << " break.";
        break;
      }
      term_t trm = slice.get("term").getUInt();
      if (idx < _cur) { // already compacted, treat as equal
        ++ndups;
        continue;
      }
      size_t pos = idx - _cur;  // position in _log
      TRI_ASSERT(pos < _log.size());
      if (idx == _log.at(pos).index && trm != _log.at(pos).term) {
        // Found an outdated entry, remove everything from here in our local
        // log:
        LOG_TOPIC(DEBUG, Logger::AGENCY)
            << "Removing " << _log.size() - pos
            << " entries from log starting with " << idx
            << "==" << _log.at(pos).index << " and " << trm << "="
            << _log.at(pos).term;

        // persisted logs
        std::string const aql(std::string("FOR l IN log FILTER l._key >= '") + 
                              stringify(idx) + "' REMOVE l IN log");

        auto bindVars = std::make_shared<VPackBuilder>();
        bindVars->openObject();
        bindVars->close();

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
            
        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "removeConflicts done: ndups=" << ndups << " first log entry: "
          << _log.front().index << " last log entry: " << _log.back().index;
        break;
      } else {
        ++ndups;
      }
    }
  } catch (std::exception const& e) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << e.what() << " " << __FILE__
                                     << __LINE__;
  }

  return ndups;
}

/// Get log entries from indices "start" to "end"
std::vector<log_t> State::get(index_t start, index_t end) const {
  
  std::vector<log_t> entries;
  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  if (_log.empty()) {
    return entries;
  }

  // start must be greater than or equal to the lowest index
  // and smaller than or equal to the largest index
  if (start < _log[0].index) {
    start = _log.front().index;
  } else if (start > _log.back().index) {
    start = _log.back().index;
  }

  // end must be greater than or equal to start
  // and smaller than or equal to the largest index
  if (end <= start) {
    end = start;
  } else if (
    end == (std::numeric_limits<uint64_t>::max)() || end > _log.back().index) {
    end = _log.back().index;
  }

  // subtract offset _cur
  start -= _cur;
  end -= (_cur-1);

  for (size_t i = start; i < end; ++i) {
    entries.push_back(_log[i]);
  }

  return entries;
}

/// Get log entries from indices "start" to "end"
/// Throws std::out_of_range exception
log_t State::at(index_t index) const {

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


/// Check for a log entry, returns 0, if the log does not contain an entry
/// with index `index`, 1, if it does contain one with term `term` and
/// -1, if it does contain one with another term than `term`:
int State::checkLog(index_t index, term_t term) const {

  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  // Catch exceptions and avoid overflow:
  if (index < _cur || index - _cur > _log.size()) {
    return 0;
  }

  try {
    return _log.at(index-_cur).term == term ? 1 : -1;
  } catch (...) {}

  return 0;
}

/// Have log with specified index and term
bool State::has(index_t index, term_t term) const {

  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  // Catch exceptions and avoid overflow:
  if (index < _cur || index - _cur > _log.size()) {
    return false;
  }

  try {
    return _log.at(index-_cur).term == term;
  } catch (...) {}

  return false;
  
}


/// Get vector of past transaction from 'start' to 'end'
VPackBuilder State::slices(index_t start,
                           index_t end) const {
  VPackBuilder slices;
  slices.openArray();

  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)

  if (!_log.empty()) {
    if (start < _log.front().index) {  // no start specified
      start = _log.front().index;
    }

    if (start > _log.back().index) {  // no end specified
      slices.close();
      return slices;
    }

    if (end == (std::numeric_limits<uint64_t>::max)() ||
        end > _log.back().index) {
      end = _log.back().index;
    }

    for (size_t i = start - _cur; i <= end - _cur; ++i) {
      try {
        slices.add(VPackSlice(_log.at(i).entry->data()));
      } catch (std::exception const&) {
        break;
      }
    }
  }

  mutexLocker.unlock();

  slices.close();

  return slices;
}

/// Get log entry by log index, copy entry because we do no longer have the
/// lock after the return
log_t State::operator[](index_t index) const {
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
        log_t(index_t(0), term_t(0), buf, std::string()));
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

/// @brief load a compacted snapshot, returns true if successfull and false
/// otherwise. In case of success store and index are modified. The store
/// is reset to the state after log index `index` has been applied. Sets
/// `index` to 0 if there is no compacted snapshot.
bool State::loadLastCompactedSnapshot(Store& store, index_t& index,
                                      term_t& term) {
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
  
  if (result.isArray()) {
    if (result.length() == 1) {
      VPackSlice i = result[0];
      VPackSlice ii = i.resolveExternals();
      try {
        store = ii.get("readDB");
        index = basics::StringUtils::uint64(ii.get("_key").copyString());
        term = ii.get("term").getNumber<uint64_t>();
        return true;
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__
                                       << __LINE__;
      }
    } else if (result.length() == 0) {
      // No compaction snapshot yet
      index = 0;
      term = 0;
      return true;
    }
  } else {
    // We should never be here! Just die!
    LOG_TOPIC(FATAL, Logger::AGENCY)
      << "Error retrieving last persisted compaction. The result was not an Array";
    FATAL_ERROR_EXIT();
  }

  return false;
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

  MUTEX_LOCKER(logLock, _logLock);

  if (result.isArray() && result.length()) {
    for (auto const& i : VPackArrayIterator(result)) {
      auto ii = i.resolveExternals();
      buffer_t tmp = std::make_shared<arangodb::velocypack::Buffer<uint8_t>>();
      (*_agent) = ii;
      try {
        _cur = basics::StringUtils::uint64(ii.get("_key").copyString());
      } catch (std::exception const& e) {
        LOG_TOPIC(ERR, Logger::AGENCY) << e.what() << " " << __FILE__
                                       << __LINE__;
      }
    }
  }

  // We can be sure that every compacted snapshot only contains index entries
  // that have been written and agreed upon by an absolute majority of agents.
  if (!_log.empty()) {
    index_t lastIndex = _log.back().index;
    
    logLock.unlock();
    _agent->lastCommitted(lastIndex);
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
            log_t(
              basics::StringUtils::uint64(
                ii.get(StaticStrings::KeyString).copyString()),
              ii.get("term").getNumber<uint64_t>(), tmp, clientId));
        } catch (std::exception const& e) {
          LOG_TOPIC(ERR, Logger::AGENCY)
            << "Failed to convert " +
            ii.get(StaticStrings::KeyString).copyString() +
            " to integer."
            << e.what();
        }
      }
    }
    TRI_ASSERT(!_log.empty());
  }

  return true;
}

/// Find entry by index and term
bool State::find(index_t prevIndex, term_t prevTerm) {
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (prevIndex > _log.size()) {
    return false;
  }
  return _log.at(prevIndex).term == prevTerm;
}

/// Log compaction
bool State::compact(index_t cind) {
  // We need to compute the state at index cind and 
  //   cind <= _lastAppliedIndex
  // and usually it is < because compactionKeepSize > 0. We start at the
  // latest compaction state and advance from there:
  Store snapshot(_agent, "snapshot");
  index_t index;
  term_t term;
  if (!loadLastCompactedSnapshot(snapshot, index, term)) {
    return false;
  }
  if (index > cind) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Strange, last compaction snapshot " << index << " is younger than "
      << "currently attempted snapshot " << cind;
    return false;
  } else if (index == cind) {
    return true;  // already have snapshot for this index
  } else {  // now we know index < cind
    // Apply log entries to snapshot up to and including index cind:
    MUTEX_LOCKER(mutexLocker, _agent->_compactionLock);
    
    auto logs = slices(index + 1, cind);
    log_t last = at(cind);
    snapshot.applyLogEntries(logs, cind, last.term,
        false  /* do not perform callbacks */);

    mutexLocker.unlock();

    if (!persistCompactionSnapshot(cind, last.term, snapshot)) {
      LOG_TOPIC(ERR, Logger::AGENCY)
        << "Could not persist compaction snapshot.";
      return false;
    }
  }

  // Now clean up old stuff which is included in the latest compaction snapshot:
  try {
    compactVolatile(cind);
    compactPersisted(cind);
    removeObsolete(cind);
  } catch (std::exception const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to compact persisted store.";
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
  }
  return true;
}

/// Compact volatile state
bool State::compactVolatile(index_t cind) {
  // Note that we intentionally keep the index cind although it is, strictly
  // speaking, no longer necessary. This is to make sure that _log does not
  // become empty! DO NOT CHANGE! This is used elsewhere in the code!
  MUTEX_LOCKER(mutexLocker, _logLock);
  if (!_log.empty() && cind > _cur && cind - _cur < _log.size()) {
    _log.erase(_log.begin(), _log.begin() + (cind - _cur));
    TRI_ASSERT(_log.begin()->index == cind);
    _cur = _log.begin()->index;
  }
  return true;
}

/// Compact persisted state
bool State::compactPersisted(index_t cind) {
  // Note that we intentionally keep the index cind although it is, strictly
  // speaking, no longer necessary. This is to make sure that _log does not
  // become empty! DO NOT CHANGE! This is used elsewhere in the code!
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

  return true;
}

/// Remove outdated compaction snapshots
bool State::removeObsolete(index_t cind) {
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
bool State::persistReadDB(index_t cind) {
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

/// Persist a compaction snapshot
bool State::persistCompactionSnapshot(index_t cind,
                                      arangodb::consensus::term_t term,
                                      arangodb::consensus::Store& snapshot) {
  if (checkCollection("compact")) {
    std::stringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << cind;

    Builder store;
    { VPackObjectBuilder s(&store);
      store.add(VPackValue("readDB"));
      { VPackArrayBuilder a(&store);
        snapshot.dumpToBuilder(store); }
      store.add("term", VPackValue(static_cast<double>(term)));
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

  LOG_TOPIC(ERR, Logger::AGENCY) << "Failed to persist snapshot for compaction!";
  return false;
}

/// @brief restoreLogFromSnapshot, needed in the follower, this erases the
/// complete log and persists the given snapshot. After this operation, the
/// log is empty and something ought to be appended to it rather quickly.
bool State::restoreLogFromSnapshot(Store& snapshot,
                                   index_t index,
                                   term_t term) {
  MUTEX_LOCKER(locker, _logLock);
  if (!persistCompactionSnapshot(index, term, snapshot)) {
    LOG_TOPIC(ERR, Logger::AGENCY)
      << "Could not persist received log snapshot.";
    return false;
  }
  // Now we need to completely erase our log, both persisted and volatile:
  LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Removing complete log because of new snapshot.";

  // persisted logs
  std::string const aql(std::string("FOR l IN log REMOVE l IN log"));

  arangodb::aql::Query query(
    false, _vocbase, aql::QueryString(aql), nullptr, nullptr,
    arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(_queryRegistry);

  // We ignore the result, in the worst case we have some log entries
  // too many.

  // volatile logs
  _log.clear();
  _cur = index;
  // This empty log should soon be rectified!
  return true;
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
  if (!query->slice().isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
        20001, 
        std::string("Inquiry handles a list of string clientIds: [<clientId>] ")
        + ". We got " + query->toJson());
  }
  
  std::vector<std::vector<log_t>> result;
  size_t pos = 0;
  
  MUTEX_LOCKER(mutexLocker, _logLock); // Cannot be read lock (Compaction)
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
index_t State::lastIndex() const {
  MUTEX_LOCKER(mutexLocker, _logLock); 
  return (!_log.empty()) ? _log.back().index : 0; 
}

