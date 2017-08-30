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

#include <thread>
#include <chrono>
#include <iomanip>

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
#include "Transaction/StandaloneContext.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb::consensus;
using namespace arangodb::rest;
using namespace arangodb::velocypack;
using namespace arangodb;

//  (std::numeric_limits<std::string>::max)();

/// Raft role names for display purposes
const std::vector<std::string> roleStr({"Follower", "Candidate", "Leader"});

/// Configure with agent's configuration
void Constituent::configure(Agent* agent) {
  MUTEX_LOCKER(guard, _castLock);

  _agent = agent;
  TRI_ASSERT(_agent != nullptr);

  if (size() == 1) {
    _role = LEADER;
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Set _role to LEADER in term " << _term;
  }
  
}

// Default ctor
Constituent::Constituent()
    : Thread("Constituent"),
      _vocbase(nullptr),
      _queryRegistry(nullptr),
      _term(0),
      _cast(false),
      _leaderID(NO_LEADER),
      _lastHeartbeatSeen(0.0),
      _role(FOLLOWER),
      _agent(nullptr),
      _votedFor(NO_LEADER) {}

/// Shutdown if not already
Constituent::~Constituent() {
  if (!isStopping()) {
    shutdown();
  }
}

/// Wait for sync
bool Constituent::waitForSync() const { return _agent->config().waitForSync(); }

/// Get my term
term_t Constituent::term() const {
  MUTEX_LOCKER(guard, _castLock);
  return _term;
}

/// Update my term
void Constituent::term(term_t t) {
  MUTEX_LOCKER(guard, _castLock);
  termNoLock(t);
}

void Constituent::termNoLock(term_t t) {
  // Only call this when you have the _castLock
  _castLock.assertLockedByCurrentThread();

  term_t tmp = _term;
  _term = t;

  if (tmp != t) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << _id << ": changing term, current role:"
      << roleStr[_role] << " new term " << t;

    _cast = false;

    if (!_votedFor.empty()) {
      Builder body;
      { VPackObjectBuilder b(&body);
        std::ostringstream i_str;
        i_str << std::setw(20) << std::setfill('0') << t;
        body.add("_key", Value(i_str.str()));
        body.add("term", Value(t));
        body.add("voted_for", Value(_votedFor)); }
      
      TRI_ASSERT(_vocbase != nullptr);
      auto transactionContext =
        std::make_shared<transaction::StandaloneContext>(_vocbase);
      SingleCollectionTransaction trx(transactionContext, "election",
                                      AccessMode::Type::WRITE);
      
      auto res = trx.begin();
      
      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
      
      OperationOptions options;
      options.waitForSync = _agent->config().waitForSync();
      options.silent = true;
      
      OperationResult result;
      try {
        result = trx.insert("election", body.slice(), options);
      } catch (std::exception const& e) {
        LOG_TOPIC(FATAL, Logger::AGENCY)
          << "Failed to persist RAFT election ballot: " << e.what() << ". Bailing out."
          << __FILE__ << ":" << __LINE__;
        FATAL_ERROR_EXIT();
      }

      res = trx.finish(result.code);

    }
  }
}

bool Constituent::logUpToDate(
  arangodb::consensus::index_t prevLogIndex, term_t prevLogTerm) const {
  log_t myLastLogEntry = _agent->state().lastLog();
  return (prevLogTerm > myLastLogEntry.term ||
          (prevLogTerm == myLastLogEntry.term &&
           prevLogIndex >= myLastLogEntry.index));
}


bool Constituent::logMatches(
  arangodb::consensus::index_t prevLogIndex, term_t prevLogTerm) const {

  int res = _agent->state().checkLog(prevLogIndex, prevLogTerm);
  if (res == 1) {
    return true;
  } else if (res == -1) {
    return false;
  } else {
    return true;  // This is important: If we have compacted away this log
                  // entry, then we know that this or a later entry was
                  // already committed by a majority and is therefore
                  // set in stone. Therefore the check must return true
                  // here and this is correct behaviour.
                  // The other case in which we do not have the log entry
                  // is if it is so new that we have never heard about it
                  // in this case we can safely return true here as well,
                  // since we will replace our own log anyway in the very
                  // near future.
  }
}


/// My role
role_t Constituent::role() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role;
}

/// Become follower in term
void Constituent::follow(term_t t) {
  MUTEX_LOCKER(guard, _castLock);
  followNoLock(t);
}

void Constituent::followNoLock(term_t t) {
  _castLock.assertLockedByCurrentThread();

  _term = t;
  _role = FOLLOWER;

  LOG_TOPIC(DEBUG, Logger::AGENCY) << "Set _role to FOLLOWER in term " << _term;

  if (_leaderID == _id) {
    _leaderID = NO_LEADER;
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Setting _leaderID to NO_LEADER.";
  } else {
    LOG_TOPIC(INFO, Logger::AGENCY)
      << _id << ": following " << _leaderID << " in term " << t ;
  }
  
  CONDITION_LOCKER(guard, _cv);
  _cv.signal();
}

/// Become leader
void Constituent::lead(term_t term) {

  // we need to rebuild spear_head and read_db

  _agent->beginPrepareLeadership();
  TRI_DEFER(_agent->endPrepareLeadership());
  
  if (!_agent->prepareLead()) {
    {
      MUTEX_LOCKER(guard, _castLock);
      followNoLock(term);
    }
    return;
  }

  {
    MUTEX_LOCKER(guard, _castLock);

    // if we already have a higher term, ignore this request
    if (term < _term) {
      followNoLock(_term);
      return;
    }

    // if we already lead, ignore this request
    if (_role == LEADER) {
      TRI_ASSERT(_leaderID == _id);
      return;
    }

    // I'm the leader
    _role = LEADER;

    LOG_TOPIC(INFO, Logger::AGENCY) << _id << ": leading in term " << _term;
    _leaderID = _id;

    // Keep track of this election time:
    MUTEX_LOCKER(locker, _recentElectionsMutex);
    _recentElections.push_back(readSystemClock());
  }

  // we need to start work as leader
  _agent->lead();
  
}

/// Become candidate
void Constituent::candidate() {
  MUTEX_LOCKER(guard, _castLock);

  if (_leaderID != NO_LEADER) {
    _leaderID = NO_LEADER;
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Set _leaderID to NO_LEADER";
  }

  if (_role != CANDIDATE) {
    _role = CANDIDATE;
    LOG_TOPIC(INFO, Logger::AGENCY) << _id << ": candidating in term " << _term;

    // Keep track of this election time:
    MUTEX_LOCKER(locker, _recentElectionsMutex);
    _recentElections.push_back(readSystemClock());
  }
}

/// Leading?
bool Constituent::leading() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role == LEADER;
}

/// Following?
bool Constituent::following() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role == FOLLOWER;
}

/// Running as candidate?
bool Constituent::running() const {
  MUTEX_LOCKER(guard, _castLock);
  return _role == CANDIDATE;
}

/// Get current leader's id
std::string Constituent::leaderID() const {
  MUTEX_LOCKER(guard, _castLock);
  return _leaderID;
}

/// Agency size
size_t Constituent::size() const { return _agent->config().size(); }

/// Get endpoint to an id
std::string Constituent::endpoint(std::string id) const {
  return _agent->config().poolAt(id);
}

/// @brief Check leader
bool Constituent::checkLeader(
  term_t term, std::string id, index_t prevLogIndex, term_t prevLogTerm) {

  TRI_ASSERT(_vocbase != nullptr);

  MUTEX_LOCKER(guard, _castLock);

  LOG_TOPIC(TRACE, Logger::AGENCY)
    << "checkLeader(term: " << term << ", leaderId: "<< id
    << ", prev-log-index: " << prevLogIndex << ", prev-log-term: "
    << prevLogTerm << ") in term " << _term;

  if (term < _term) {
    return false;
  }

  _lastHeartbeatSeen = TRI_microtime();
  LOG_TOPIC(TRACE, Logger::AGENCY)
    << "setting last heartbeat: " << _lastHeartbeatSeen;
  
  if (term > _term) {
    termNoLock(term);
  }

  if (!logMatches(prevLogIndex, prevLogTerm)) {
    return false;
  }
  
  if (_leaderID != id) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Set _leaderID to " << id << " in term " << _term;
    _leaderID = id;

    // Recall time of this leadership change:
    {
      MUTEX_LOCKER(locker, _recentElectionsMutex);
      _recentElections.push_back(readSystemClock());
    }

    TRI_ASSERT(_leaderID != _id);
    if (_role != FOLLOWER) {
      followNoLock(term);
    }
  }
  
  return true;
}

/// @brief Vote
bool Constituent::vote(term_t termOfPeer, std::string id, index_t prevLogIndex,
                       term_t prevLogTerm) {

  if (!_agent->ready()) {
    return false;
  }
  
  TRI_ASSERT(_vocbase != nullptr);
 
  MUTEX_LOCKER(guard, _castLock);
  
  LOG_TOPIC(TRACE, Logger::AGENCY)
    << "vote(termOfPeer: " << termOfPeer << ", leaderId: " << id
    << ", prev-log-index: " << prevLogIndex << ", prev-log-term: " << prevLogTerm
    << ") in (my) term " << _term;

  if (termOfPeer > _term) {
    termNoLock(termOfPeer);

    if (_role != FOLLOWER) {
      followNoLock(_term);
    }

    _cast = false;
    _votedFor = "";
  } else if (termOfPeer < _term) {
    // termOfPeer < _term, simply ignore and do not vote:
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "ignoring RequestVoteRPC with old term " << termOfPeer
      << ", we are already at term " << _term;
    return false;
  }

  if (_cast) {   // already voted in this term
    if (_votedFor == id) {
      LOG_TOPIC(DEBUG, Logger::AGENCY) << "repeating vote for " << id;
      return true;
    }
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "not voting for " << id
      << " since we have already voted for " << _votedFor
      << " in this term";
    return false;
  }

  // Now decide whether or not we vote for this server, we have to
  // take into account paragraph 5.4.1 in the Raft paper, so we need
  // to check that his log is at least as up to date as ours:
  log_t myLastLogEntry = _agent->state().lastLog();
  if (prevLogTerm > myLastLogEntry.term ||
      (prevLogTerm == myLastLogEntry.term &&
       prevLogIndex >= myLastLogEntry.index)) {
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "voting for " << id << " in term "
      << _term;
    _cast = true;
    _votedFor = id;
    return true;
  }
  LOG_TOPIC(DEBUG, Logger::AGENCY) << "not voting for " << id
    << " since his log is not up to date: "
    << "my last log entry: (" << myLastLogEntry.term << ", "
    << myLastLogEntry.index << "), his last log entry: ("
    << prevLogTerm << ", " << prevLogIndex << ")";
  return false;   // do not vote for this uninformed guy!
}

/// @brief Call to election
void Constituent::callElection() {

  using namespace std::chrono;
  auto timeout = steady_clock::now() +
    duration<double>(_agent->config().minPing() *
                     _agent->config().timeoutMult());
  
  std::vector<std::string> active = _agent->config().active();
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  
  term_t savedTerm;
  {
    MUTEX_LOCKER(locker, _castLock);
    this->termNoLock(_term + 1);  // raise my term
    _cast     = true;
    _votedFor = _id;
    savedTerm = _term;
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Set _leaderID to NO_LEADER"
      << " in term " << _term;
    _leaderID = NO_LEADER;
  }

  std::string body;
  std::stringstream path;

  int64_t electionEventCount = countRecentElectionEvents(3600);
  if (electionEventCount <= 0) {
    electionEventCount = 1;
  } else if (electionEventCount > 10) {
    electionEventCount = 10;
  }
  if (_agent->getTimeoutMult() != electionEventCount) {
    _agent->adjustTimeoutMult(electionEventCount);
    LOG_TOPIC(WARN, Logger::AGENCY) << "Candidate: setting timeout multiplier "
      "to " << electionEventCount << " for next term...";
  }

  path << "/_api/agency_priv/requestVote?term=" << savedTerm
       << "&candidateId=" << _id << "&prevLogIndex=" << _agent->lastLog().index
       << "&prevLogTerm=" << _agent->lastLog().term
       << "&timeoutMult=" << electionEventCount;

  auto cc = ClusterComm::instance();

  // Ask everyone for their vote
  for (auto const& i : active) {
    if (i != _id) {
      auto headerFields =
        std::make_unique<std::unordered_map<std::string, std::string>>();
      if (!isStopping() && cc != nullptr) {
         cc->asyncRequest(
          "", coordinatorTransactionID, _agent->config().poolAt(i),
          rest::RequestType::GET, path.str(),
          std::make_shared<std::string>(body), headerFields,
          nullptr, 0.9 * _agent->config().minPing() *
                         _agent->config().timeoutMult(), true);
      }
    }
  }

  // Collect ballots. I vote for myself.
  size_t yea = 1;
  size_t nay = 0;
  size_t majority = size() / 2 + 1;
  
  // We collect votes, we leave the following loop when one of the following
  // conditions is met:
  //   (1) A majority of nay votes have been received
  //   (2) A majority of yea votes (including ourselves) have been received
  //   (3) At least yyy time has passed, in this case we give up without
  //       a conclusive vote.
  while (true) {

    if (steady_clock::now() >= timeout) {       // Timeout. 
      MUTEX_LOCKER(locker, _castLock);
      follow(_term);        
      break;
    }

    if (!isStopping() && cc != nullptr) {
      auto res = ClusterComm::instance()->wait(
        "", coordinatorTransactionID, 0, "",
        duration<double>(timeout - steady_clock::now()).count());
      
      if (res.status == CL_COMM_SENT) {
        auto body = res.result->getBodyVelocyPack();
        VPackSlice slc = body->slice();
        
        // Got ballot
        if (slc.isObject() && slc.hasKey("term") && slc.hasKey("voteGranted")) {
          
          // Follow right away?
          term_t t = slc.get("term").getUInt(), term;
          {
            MUTEX_LOCKER(locker, _castLock);
            term = _term;
          }
          if (t > term) {
            follow(t);
            break;
          }
          
          // Check result and counts
          if(slc.get("voteGranted").getBool()) { // majority in favour?
            if (++yea >= majority) {
              lead(savedTerm);
              break;
            }
            // Vote is counted as yea, continue loop
            continue;
          }
        }
      }
    }
    // Count the vote as a nay
    if (++nay >= majority) {                  // Network: majority against?
      term_t term;
      {
        MUTEX_LOCKER(locker, _castLock);
        term = _term;
      }
      follow(term);
      break;
    }
    
  }
  
  LOG_TOPIC(DEBUG, Logger::AGENCY)
    << "Election: Have received " << yea << " yeas and " << nay << " nays, the "
    << (yea >= majority ? "yeas" : "nays") << " have it.";
  
  // Clean up
  if (!isStopping() && cc != nullptr) {
    ClusterComm::instance()->drop("", coordinatorTransactionID, 0, "");
  }
  
}

void Constituent::update(std::string const& leaderID, term_t t) {
  MUTEX_LOCKER(guard, _castLock);
  _term = t;

  if (_leaderID != leaderID) {
    LOG_TOPIC(DEBUG, Logger::AGENCY)
      << "Constituent::update: setting _leaderID to " << leaderID
      << " in term " << _term;
    _leaderID = leaderID;
    _role = FOLLOWER;
  }
}

/// Start clean shutdown
void Constituent::beginShutdown() {
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _cv);
  guard.broadcast();
}

/// Start operation
bool Constituent::start(TRI_vocbase_t* vocbase,
                        aql::QueryRegistry* queryRegistry) {
  TRI_ASSERT(vocbase != nullptr);
  _vocbase = vocbase;
  _queryRegistry = queryRegistry;

  return Thread::start();
}

/// Get persisted information and run election process
void Constituent::run() {

  // single instance
  _id = _agent->config().id();

  TRI_ASSERT(_vocbase != nullptr);
  auto bindVars = std::make_shared<VPackBuilder>();
  bindVars->openObject();
  bindVars->close();

  // Most recent vote
  {
    std::string const aql("FOR l IN election SORT l._key DESC LIMIT 1 RETURN l");
    arangodb::aql::Query query(false, _vocbase, arangodb::aql::QueryString(aql),
                               bindVars, nullptr, arangodb::aql::PART_MAIN);

    auto queryResult = query.execute(_queryRegistry);
    if (queryResult.code != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION_MESSAGE(queryResult.code, queryResult.details);
    }

    VPackSlice result = queryResult.result->slice();

    if (result.isArray()) {
      for (auto const& i : VPackArrayIterator(result)) {
        auto ii = i.resolveExternals();
        try {
          MUTEX_LOCKER(locker, _castLock);
          _term = ii.get("term").getUInt();
          _votedFor = ii.get("voted_for").copyString();
        } catch (std::exception const&) {
          LOG_TOPIC(ERR, Logger::AGENCY)
              << "Persisted election entries corrupt! Defaulting term,vote (0,0)";
        }
      }
    }
  }

  std::vector<std::string> act = _agent->config().active();

  while (
    !this->isStopping() // Obvious
    && (!_agent->ready() ||
        find(act.begin(), act.end(), _id) == act.end())) { // Active agent 
    CONDITION_LOCKER(guardv, _cv);
    _cv.wait(50000);
    act = _agent->config().active();
  }

  if (size() == 1) {
    MUTEX_LOCKER(guard, _castLock);
    _leaderID = _agent->config().id();
    LOG_TOPIC(DEBUG, Logger::AGENCY) << "Set _leaderID to " << _leaderID
      << " in term " << _term;
  } else {

    {
      MUTEX_LOCKER(guard, _castLock);
      _role = FOLLOWER;
    }
    while (!this->isStopping()) {
      
      role_t role;
      {
        MUTEX_LOCKER(guard, _castLock);
        role = _role;
      }

      if (role == FOLLOWER) {
        static double const M = 1.0e6;
        int64_t a = static_cast<int64_t>(M * _agent->config().minPing() *
                                         _agent->config().timeoutMult());
        int64_t b = static_cast<int64_t>(M * _agent->config().maxPing() *
                                         _agent->config().timeoutMult());
        int64_t randTimeout = RandomGenerator::interval(a, b);
        int64_t randWait = randTimeout;

        {
          MUTEX_LOCKER(guard, _castLock);

          // in the beginning, pure random, after that, we might have to
          // wait for less than planned, since the last heartbeat we have
          // seen is already some time ago, note that this waiting time
          // can become negative:
          if (_lastHeartbeatSeen > 0.0) {
            double now = TRI_microtime();
            randWait -= static_cast<int64_t>(M * (now-_lastHeartbeatSeen));
            if (randWait < a) {
              randWait = a;
            } else if (randWait > b) {
              randWait = b;
            }
          }
        }
       
        LOG_TOPIC(TRACE, Logger::AGENCY)
          << "Random timeout: " << randTimeout << ", wait: " << randWait;

        if (randWait > 0.0) {
          CONDITION_LOCKER(guardv, _cv);
          _cv.wait(randWait);
        }

        bool isTimeout = false;

        {
          MUTEX_LOCKER(guard, _castLock);

          if (_lastHeartbeatSeen <= 0.0) {
            LOG_TOPIC(TRACE, Logger::AGENCY) << "no heartbeat seen";
            isTimeout = true;
          } else { 
            double diff = TRI_microtime() - _lastHeartbeatSeen;
            LOG_TOPIC(TRACE, Logger::AGENCY) << "last heartbeat: " << diff << "sec ago";
        
            isTimeout = (static_cast<int32_t>(M * diff) > randTimeout);
          }
        }

        if (isTimeout) {
          LOG_TOPIC(TRACE, Logger::AGENCY) << "timeout, calling an election";
          candidate();
          _agent->unprepareLead();
        }

      } else if (role == CANDIDATE) {
        callElection();  // Run for office
      } else {
        int32_t left =
          static_cast<int32_t>(100000.0 * _agent->config().minPing() *
                               _agent->config().timeoutMult());
        long randTimeout = static_cast<long>(left);
        {
          CONDITION_LOCKER(guardv, _cv);
          _cv.wait(randTimeout);
        }
      }
    }
  }
}

int64_t Constituent::countRecentElectionEvents(double threshold) {
  // This discards all election events that are older than `threshold`
  // seconds and returns the number of more recent ones.

  auto now = readSystemClock();
  MUTEX_LOCKER(locker, _recentElectionsMutex);
  int64_t count = 0;
  for (auto iter = _recentElections.begin(); iter != _recentElections.end(); ) {
    if (now - *iter > threshold) {
      // If event is more then 15 minutes ago, discard
      iter = _recentElections.erase(iter);  // moves to the next one in list
    } else {
      ++count;
      ++iter;
    }
  }
  return count;
}
