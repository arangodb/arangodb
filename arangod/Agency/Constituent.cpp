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
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Constituent.h"

#include <chrono>
#include <chrono>
#include <iomanip>
#include <iomanip>
#include <thread>
#include <thread>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "Agency/Agent.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Basics/ConditionLocker.h"
#include "Cluster/ClusterComm.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "Utils/OperationOptions.h"
#include "Utils/OperationResult.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/collection.h"
#include "VocBase/vocbase.h"
#include "Agency/NotifierThread.h"

using namespace arangodb::consensus;
using namespace arangodb::rest;
using namespace arangodb::velocypack;
using namespace arangodb;

// Configure with agent's configuration
void Constituent::configure(Agent* agent) {
  _agent = agent;
  TRI_ASSERT(_agent != nullptr);
  if (size() == 1) {
    _role = LEADER;
  } else {
    _id = _agent->config().id;
    if (_agent->config().notify) {  // (notify everyone)
      notifyAll();
    }
  }
}

// Default ctor
Constituent::Constituent()
    : Thread("Constituent"),
      _vocbase(nullptr),
      _queryRegistry(nullptr),
      _term(0),
      _leaderID((std::numeric_limits<uint32_t>::max)()),
      _id(0),
      // XXX #warning KAVEH use RandomGenerator
      _gen(std::random_device()()),
      _role(FOLLOWER),
      _agent(nullptr),
      _votedFor((std::numeric_limits<uint32_t>::max)()),
      _notifier(nullptr) {
  _gen.seed(RandomGenerator::interval(UINT32_MAX));
}

// Shutdown if not already
Constituent::~Constituent() { shutdown(); }

// Configuration
config_t const& Constituent::config() const { return _agent->config(); }

// Wait for sync
bool Constituent::waitForSync() const { return _agent->config().waitForSync; }

// Random sleep times in election process
duration_t Constituent::sleepFor(double min_t, double max_t) {
  dist_t dis(min_t, max_t);
  return duration_t((long)std::round(dis(_gen) * 1000.0));
}

// Get my term
term_t Constituent::term() const {
  MUTEX_LOCKER(guard, _castLock);
  return _term;
}

// Update my term
void Constituent::term(term_t t) {
  term_t tmp;

  {
    MUTEX_LOCKER(guard, _castLock);
    tmp = _term;
    _term = t;
  }

  if (tmp != t) {
    LOG_TOPIC(INFO, Logger::AGENCY) << "Updating term to " << t;

    Builder body;
    body.add(VPackValue(VPackValueType::Object));
    std::ostringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << t;
    body.add("_key", Value(i_str.str()));
    body.add("term", Value(t));
    body.add("voted_for", Value((uint32_t)_votedFor));
    body.close();

    TRI_ASSERT(_vocbase != nullptr);
    auto transactionContext =
        std::make_shared<StandaloneTransactionContext>(_vocbase);
    SingleCollectionTransaction trx(transactionContext, "election",
                                    TRI_TRANSACTION_WRITE);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    OperationOptions options;
    options.waitForSync = waitForSync();
    options.silent = true;

    OperationResult result = trx.insert("election", body.slice(), options);
    /*res = */ trx.finish(result.code);  // OMG
  }
}

/// @brief My role
role_t Constituent::role() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role;
}

/// @brief Become follower in term
void Constituent::follow(term_t t) {
  MUTEX_LOCKER(guard, _castLock);
  if (_role != FOLLOWER) {
    LOG_TOPIC(INFO, Logger::AGENCY)
        << "Role change: Converting to follower in term " << t;
  }
  _term = t;
  _role = FOLLOWER;
}

/// @brief Become leader
void Constituent::lead() {
  MUTEX_LOCKER(guard, _castLock);
  if (_role != LEADER) {
    LOG_TOPIC(INFO, Logger::AGENCY)
        << "Role change: Converted to leader in term " << _term;
    _agent->lead();  // We need to rebuild spear_head and read_db;
  }
  _role = LEADER;
  _leaderID = _id;
}

/// @brief Become follower
void Constituent::candidate() {
  MUTEX_LOCKER(guard, _castLock);
  if (_role != CANDIDATE)
    LOG_TOPIC(INFO, Logger::AGENCY)
        << "Role change: Converted to candidate in term " << _term;
  _role = CANDIDATE;
}

/// @brief Leading?
bool Constituent::leading() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role == LEADER;
}

/// @brief Following?
bool Constituent::following() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role == FOLLOWER;
}

/// @brief Runnig as candidate?
bool Constituent::running() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role == CANDIDATE;
}

/// @brief Get current leader's id
arangodb::consensus::id_t Constituent::leaderID() const { return _leaderID; }

/// @brief Agency size
size_t Constituent::size() const { return config().size(); }

/// @brief Get endpoint to an id
std::string const& Constituent::endpoint(arangodb::consensus::id_t id) const {
  return config().endpoints[id];
}

/// @brief Get all endpoints
std::vector<std::string> const& Constituent::endpoints() const {
  return config().endpoints;
}

/// @brief Notify peers of updated endpoints
void Constituent::notifyAll() {
  std::vector<std::string> toNotify;
  // Send request to all but myself
  for (arangodb::consensus::id_t i = 0; i < size(); ++i) {
    if (i != _id) {
      toNotify.push_back(endpoint(i));
    }
  }

  // Body contains endpoints list
  auto body = std::make_shared<VPackBuilder>();
  body->openObject();
  body->add("endpoints", VPackValue(VPackValueType::Array));
  for (auto const& i : endpoints()) {
    body->add(Value(i));
  }
  body->close();
  body->close();

  // Last process notifies everyone
  std::stringstream path;

  path << "/_api/agency_priv/notifyAll?term=" << _term << "&agencyId=" << _id;

  _notifier = std::make_unique<NotifierThread>(path.str(), body, toNotify);
  _notifier->start();
}

/// @brief Vote
bool Constituent::vote(term_t term, arangodb::consensus::id_t id,
                       index_t prevLogIndex, term_t prevLogTerm) {
  term_t t = 0;
  arangodb::consensus::id_t lid = 0;
  {
    MUTEX_LOCKER(guard, _castLock);
    t = _term;
    lid = _leaderID;
  }
  if (term > t || (t == term && lid == id)) {
    this->term(term);
    {
      MUTEX_LOCKER(guard, _castLock);
      _cast = true;    // Note that I voted this time around.
      _votedFor = id;  // The guy I voted for I assume leader.
      _leaderID = id;
    }
    if (_role > FOLLOWER) {
      follow(_term);
    }
    _agent->persist(term, id);
    _cv.signal();
    return true;
  } else {  // Myself running or leading
    return false;
  }
}

/// @brief Call to election
void Constituent::callElection() {
  std::vector<bool> votes(size(), false);

  votes.at(_id) = true;  // vote for myself
  _cast = true;
  if (_role == CANDIDATE) {
    this->term(_term + 1);  // raise my term
  }

  std::string body;
  std::vector<OperationID> operationIDs(config().endpoints.size());
  std::stringstream path;

  path << "/_api/agency_priv/requestVote?term=" << _term
       << "&candidateId=" << _id << "&prevLogIndex=" << _agent->lastLog().index
       << "&prevLogTerm=" << _agent->lastLog().term;

  // Ask everyone for their vote
  for (arangodb::consensus::id_t i = 0; i < config().endpoints.size(); ++i) {
    if (i != _id && endpoint(i) != "") {
      auto headerFields =
          std::make_unique<std::unordered_map<std::string, std::string>>();
      operationIDs[i] = arangodb::ClusterComm::instance()->asyncRequest(
          "1", 1, config().endpoints[i], GeneralRequest::RequestType::GET,
          path.str(), std::make_shared<std::string>(body), headerFields,
          nullptr, config().minPing, true);
    }
  }

  // Wait randomized timeout
  std::this_thread::sleep_for(
      sleepFor(.5 * config().minPing, .8 * config().minPing));

  // Collect votes
  // FIXME: This code can be improved: One can wait for an arbitrary
  // result by creating a coordinatorID and waiting for a pattern.
  for (arangodb::consensus::id_t i = 0; i < config().endpoints.size(); ++i) {
    if (i != _id && endpoint(i) != "") {
      ClusterCommResult res =
          arangodb::ClusterComm::instance()->enquire(operationIDs[i]);

      if (res.status == CL_COMM_SENT) {  // Request successfully sent
        res = arangodb::ClusterComm::instance()->wait(
            "1", 1, operationIDs[i], "1");
        std::shared_ptr<Builder> body = res.result->getBodyVelocyPack();
        if (body->isEmpty()) {  // body empty
          continue;
        } else {
          if (body->slice().isObject()) {  // body
            VPackSlice slc = body->slice();
            if (slc.hasKey("term") && slc.hasKey("voteGranted")) {  // OK
              term_t t = slc.get("term").getUInt();
              if (t > _term) {  // follow?
                follow(t);
                break;
              }
              votes[i] = slc.get("voteGranted").getBool();  // Get vote
            }
          }
        }
      } else {  // Request failed
        votes[i] = false;
      }
    }
  }

  // Count votes
  size_t yea = 0;
  for (size_t i = 0; i < size(); ++i) {
    if (votes[i]) {
      yea++;
    }
  }

  // Evaluate election results
  if (yea > size() / 2) {
    lead();
  } else {
    follow(_term);
  }
}

void Constituent::beginShutdown() {
  _notifier.reset();
  Thread::beginShutdown();
}

bool Constituent::start(TRI_vocbase_t* vocbase,
                        aql::QueryRegistry* queryRegistry) {
  _vocbase = vocbase;
  _queryRegistry = queryRegistry;
  return Thread::start();
}

void Constituent::run() {
  TRI_ASSERT(_vocbase != nullptr);
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  // Query
  std::string const aql("FOR l IN election SORT l._key DESC LIMIT 1 RETURN l");
  arangodb::aql::Query query(false, _vocbase, aql.c_str(), aql.size(), bindVars,
                             nullptr, arangodb::aql::PART_MAIN);

  auto queryResult = query.execute(_queryRegistry);
  if (queryResult.code != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
  }

  VPackSlice result = queryResult.result->slice();

  if (result.isArray()) {
    for (auto const& i : VPackArrayIterator(result)) {
      try {
        _term = i.get("term").getUInt();
        _votedFor =
            static_cast<decltype(_votedFor)>(i.get("voted_for").getUInt());
      } catch (std::exception const&) {
        LOG_TOPIC(ERR, Logger::AGENCY)
            << "Persisted election entries corrupt! Defaulting term,vote (0,0)";
      }
    }
  }

  // Always start off as follower
  while (!this->isStopping() && size() > 1) {
    if (_role == FOLLOWER) {
      bool cast = false;

      {
        MUTEX_LOCKER(guard, _castLock);
        _cast = false;  // New round set not cast vote
      }

      dist_t dis(config().minPing, config().maxPing);
      long rand_wait = static_cast<long>(dis(_gen) * 1000000.0);
      /*bool timedout =*/_cv.wait(rand_wait);

      {
        MUTEX_LOCKER(guard, _castLock);
        cast = _cast;
      }

      if (!cast) {
        candidate();  // Next round, we are running
      }
    } else {
      callElection();  // Run for office
    }
  }
}
