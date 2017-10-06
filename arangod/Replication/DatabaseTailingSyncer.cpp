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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "DatabaseTailingSyncer.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Replication/DatabaseInitialSyncer.h"
#include "Replication/DatabaseReplicationApplier.h"
#include "Rest/HttpRequest.h"
#include "RestServer/DatabaseFeature.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Hints.h"
#include "Utils/CollectionGuard.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::httpclient;
using namespace arangodb::rest;

DatabaseTailingSyncer::DatabaseTailingSyncer(
    TRI_vocbase_t* vocbase,
    ReplicationApplierConfiguration const& configuration,
    TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId)
    : TailingSyncer(configuration, initialTick, barrierId),
      _applier(vocbase->replicationApplier()),
      _vocbase(vocbase),
      _useTick(useTick),
      _hasWrittenState(false) {
  _vocbases.emplace(vocbase->name(), DatabaseGuard(vocbase));
}

/// @brief run method, performs continuous synchronization
int DatabaseTailingSyncer::run() {
  if (_client == nullptr || _connection == nullptr || _endpoint == nullptr) {
    return TRI_ERROR_INTERNAL;
  }

  TRI_DEFER(sendRemoveBarrier());
  uint64_t shortTermFailsInRow = 0;

retry:
  double const start = TRI_microtime();
  std::string errorMsg;

  int res = TRI_ERROR_NO_ERROR;
  uint64_t connectRetries = 0;

  // reset failed connects
  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    _applier->_state._failedConnects = 0;
  }

  while (vocbase()->state() == TRI_vocbase_t::State::NORMAL) {
    setProgress("fetching master state information");
    Result r = getMasterState();
    res = r.errorNumber();
    errorMsg = r.errorMessage();


    if (res == TRI_ERROR_REPLICATION_NO_RESPONSE) {
      // master error. try again after a sleep period
      connectRetries++;

      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
        _applier->_state._totalFailedConnects++;
      }

      if (connectRetries <= _configuration._maxConnectRetries) {
        // check if we are aborted externally
        if (_applier->sleepIfStillActive(_configuration._connectionRetryWaitTime)) {
          setProgress(
              "fetching master state information failed. will retry now. "
              "retries left: " +
              std::to_string(_configuration._maxConnectRetries -
                             connectRetries));
          continue;
        }

        // somebody stopped the applier
        res = TRI_ERROR_REPLICATION_APPLIER_STOPPED;
      }
    }

    // we either got a connection or an error
    break;
  }

  if (res == TRI_ERROR_NO_ERROR) {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    try {
      getLocalState();
      _applier->_state._failedConnects = 0;
      _applier->_state._totalRequests++;
    } catch (basics::Exception const& ex) {
      res = ex.code();
      errorMsg = ex.what();
    } catch (std::exception const& ex) {
      res = TRI_ERROR_INTERNAL;
      errorMsg = ex.what();
    } catch (...) {
      res = TRI_ERROR_INTERNAL;
    }
  }

  if (res != TRI_ERROR_NO_ERROR) {
    // stop ourselves
    _applier->stop(false);
    return _applier->setError(res, errorMsg);
  }

  if (res == TRI_ERROR_NO_ERROR) {
    res = runContinuousSync(errorMsg);
  }

  if (res != TRI_ERROR_NO_ERROR) {
    _applier->setError(res, errorMsg);

    // stop ourselves
    _applier->stop(false);

    if (res == TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT ||
        res == TRI_ERROR_REPLICATION_NO_START_TICK) {
      if (res == TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT) {
        LOG_TOPIC(WARN, Logger::REPLICATION)
            << "replication applier stopped for database '" << _databaseName
            << "' because required tick is not present on master";
      }

      // remove previous applier state
      abortOngoingTransactions();

      _applier->removeState();

      // TODO: merge with removeState
      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

        LOG_TOPIC(DEBUG, Logger::REPLICATION)
            << "stopped replication applier for database '" << _databaseName
            << "' with lastProcessedContinuousTick: "
            << _applier->_state._lastProcessedContinuousTick
            << ", lastAppliedContinuousTick: "
            << _applier->_state._lastAppliedContinuousTick
            << ", safeResumeTick: " << _applier->_state._safeResumeTick;

        _applier->_state._lastProcessedContinuousTick = 0;
        _applier->_state._lastAppliedContinuousTick = 0;
        _applier->_state._safeResumeTick = 0;
        _applier->_state._failedConnects = 0;
        _applier->_state._totalRequests = 0;
        _applier->_state._totalFailedConnects = 0;

        saveApplierState();
      }

      if (!_configuration._autoResync) {
        return res;
      }

      if (TRI_microtime() - start < 120.0) {
        // the applier only ran for less than 2 minutes. probably
        // auto-restarting it won't help much
        shortTermFailsInRow++;
      } else {
        shortTermFailsInRow = 0;
      }

      // check if we've made too many retries
      if (shortTermFailsInRow > _configuration._autoResyncRetries) {
        if (_configuration._autoResyncRetries > 0) {
          // message only makes sense if there's at least one retry
          LOG_TOPIC(WARN, Logger::REPLICATION)
              << "aborting automatic resynchronization for database '"
              << _databaseName << "' after "
              << _configuration._autoResyncRetries << " retries";
        } else {
          LOG_TOPIC(WARN, Logger::REPLICATION)
              << "aborting automatic resynchronization for database '"
              << _databaseName << "' because autoResyncRetries is 0";
        }

        // always abort if we get here
        return res;
      }

      // do an automatic full resync
      LOG_TOPIC(WARN, Logger::REPLICATION)
          << "restarting initial synchronization for database '"
          << _databaseName << "' because autoResync option is set. retry #"
          << shortTermFailsInRow;

      // start initial synchronization
      errorMsg = "";

      try {
        TRI_ASSERT(!_configuration._skipCreateDrop);
        DatabaseInitialSyncer syncer(vocbase(), _configuration);
        Result r = syncer.run(_configuration._incremental);
        
        if (r.ok()) {
          TRI_voc_tick_t lastLogTick = syncer.getLastLogTick();
          LOG_TOPIC(INFO, Logger::REPLICATION)
              << "automatic resynchronization for database '" << _databaseName
              << "' finished. restarting continuous replication applier from "
                 "tick "
              << lastLogTick;
          _initialTick = lastLogTick;
          _useTick = true;
          goto retry;
        }
        res = r.errorNumber();
        errorMsg = r.errorMessage();
        // fall through otherwise
      } catch (...) {
        errorMsg = "caught an exception";
        res = TRI_ERROR_INTERNAL;
      }
    }

    return res;
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief set the applier progress
void DatabaseTailingSyncer::setProgress(std::string const& msg) {
  TailingSyncer::setProgress(msg);
  _applier->setProgress(msg);
}

/// @brief save the current applier state
void DatabaseTailingSyncer::saveApplierState() {
  LOG_TOPIC(TRACE, Logger::REPLICATION)
      << "saving replication applier state. last applied continuous tick: "
      << _applier->_state._lastAppliedContinuousTick
      << ", safe resume tick: " << _applier->_state._safeResumeTick;

  try {
    _applier->persistState(false);
  } catch (basics::Exception const& ex) {
    std::string errorMsg = std::string("unable to save replication applier state: ") + ex.what();
    LOG_TOPIC(WARN, Logger::REPLICATION) << errorMsg;
    THROW_ARANGO_EXCEPTION_MESSAGE(ex.code(), errorMsg);
  } catch (std::exception const& ex) {
    std::string errorMsg = std::string("unable to save replication applier state: ") + ex.what();
    LOG_TOPIC(WARN, Logger::REPLICATION) << errorMsg;
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  } catch (...) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "caught unknown exception while saving applier state");
  }
}

/// @brief get local replication apply state
void DatabaseTailingSyncer::getLocalState() {
  uint64_t oldTotalRequests = _applier->_state._totalRequests;
  uint64_t oldTotalFailedConnects = _applier->_state._totalFailedConnects;

  bool const foundState = _applier->loadState();
  _applier->_state._state = ReplicationApplierState::ActivityState::RUNNING;
  _applier->_state._totalRequests = oldTotalRequests;
  _applier->_state._totalFailedConnects = oldTotalFailedConnects;

  if (!foundState) {
    // no state file found, so this is the initialization
    _applier->_state._serverId = _masterInfo._serverId;
    _applier->persistState(true);
    return;
  }

  if (_masterInfo._serverId != _applier->_state._serverId &&
      _applier->_state._serverId != 0) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_REPLICATION_MASTER_CHANGE, std::string("encountered wrong master id in replication state file. found: ") + StringUtils::itoa(_masterInfo._serverId) + ", expected: " + StringUtils::itoa(_applier->_state._serverId));
  }
}

/// @brief perform a continuous sync with the master
int DatabaseTailingSyncer::runContinuousSync(std::string& errorMsg) {
  static uint64_t const MinWaitTime = 300 * 1000;        // 0.30 seconds
  static uint64_t const MaxWaitTime = 60 * 1000 * 1000;  // 60 seconds
  uint64_t connectRetries = 0;
  uint64_t inactiveCycles = 0;

  // get start tick
  // ---------------------------------------
  TRI_voc_tick_t fromTick = 0;
  TRI_voc_tick_t safeResumeTick = 0;

  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

    if (_useTick) {
      // use user-defined tick
      fromTick = _initialTick;
      _applier->_state._lastAppliedContinuousTick = 0;
      _applier->_state._lastProcessedContinuousTick = 0;
      // saveApplierState();
    } else {
      // if we already transferred some data, we'll use the last applied tick
      if (_applier->_state._lastAppliedContinuousTick >= fromTick) {
        fromTick = _applier->_state._lastAppliedContinuousTick;
      }
      safeResumeTick = _applier->_state._safeResumeTick;
    }
  }

  LOG_TOPIC(DEBUG, Logger::REPLICATION)
      << "requesting continuous synchronization, fromTick: " << fromTick
      << ", safeResumeTick " << safeResumeTick << ", useTick: " << _useTick
      << ", initialTick: " << _initialTick;

  if (fromTick == 0) {
    return TRI_ERROR_REPLICATION_NO_START_TICK;
  }

  // get the applier into a sensible start state by fetching the list of
  // open transactions from the master
  TRI_voc_tick_t fetchTick = safeResumeTick;

  if (safeResumeTick > 0 && safeResumeTick == fromTick) {
    // special case in which from and to are equal
    fetchTick = safeResumeTick;
  } else {
    // adjust fetchTick so we can tail starting from the tick containing
    // the open transactions we did not commit locally
    Result r = fetchOpenTransactions(safeResumeTick, fromTick, fetchTick);

    if (r.fail()) {
      errorMsg = r.errorMessage();
      return r.errorNumber();
    }
  }

  if (fetchTick > fromTick) {
    // must not happen
    return TRI_ERROR_INTERNAL;
  }

  std::string const progress =
      "starting with from tick " + StringUtils::itoa(fromTick) +
      ", fetch tick " + StringUtils::itoa(fetchTick) + ", open transactions: " +
      StringUtils::itoa(_ongoingTransactions.size());
  setProgress(progress);

  // run in a loop. the loop is terminated when the applier is stopped or an
  // error occurs
  while (true) {
    bool worked;
    bool masterActive = false;
    errorMsg = "";

    // fetchTick is passed by reference!
    Result r = followMasterLog(fetchTick, fromTick, _configuration._ignoreErrors, worked, masterActive);
    int res = r.errorNumber();
    errorMsg = r.errorMessage();

    uint64_t sleepTime;

    if (res == TRI_ERROR_REPLICATION_NO_RESPONSE ||
        res == TRI_ERROR_REPLICATION_MASTER_ERROR) {
      // master error. try again after a sleep period
      if (_configuration._connectionRetryWaitTime > 0) {
        sleepTime = _configuration._connectionRetryWaitTime;
        if (sleepTime < MinWaitTime) {
          sleepTime = MinWaitTime;
        }
      } else {
        // default to prevent spinning too busy here
        sleepTime = 30 * 1000 * 1000;
      }

      connectRetries++;

      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
        _applier->_state._totalFailedConnects++;
      }

      if (connectRetries > _configuration._maxConnectRetries) {
        // halt
        return res;
      }
    } else {
      connectRetries = 0;

      {
        WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

        _applier->_state._failedConnects = connectRetries;
        _applier->_state._totalRequests++;
      }

      if (res != TRI_ERROR_NO_ERROR) {
        // some other error we will not ignore
        return res;
      }

      // no error
      if (worked) {
        // we have done something, so we won't sleep (but check for cancelation)
        inactiveCycles = 0;
        sleepTime = 0;
      } else {
        sleepTime = _configuration._idleMinWaitTime;
        if (sleepTime < MinWaitTime) {
          sleepTime = MinWaitTime;  // hard-coded minimum wait time
        }

        if (_configuration._adaptivePolling) {
          inactiveCycles++;
          if (inactiveCycles > 60) {
            sleepTime *= 5;
          } else if (inactiveCycles > 30) {
            sleepTime *= 3;
          }
          if (inactiveCycles > 15) {
            sleepTime *= 2;
          }

          if (sleepTime > _configuration._idleMaxWaitTime) {
            sleepTime = _configuration._idleMaxWaitTime;
          }
        }

        if (sleepTime > MaxWaitTime) {
          sleepTime = MaxWaitTime;  // hard-coded maximum wait time
        }
      }
    }

    LOG_TOPIC(TRACE, Logger::REPLICATION) << "master active: " << masterActive
                                          << ", worked: " << worked
                                          << ", sleepTime: " << sleepTime;

    // this will make the applier thread sleep if there is nothing to do,
    // but will also check for cancelation
    if (!_applier->sleepIfStillActive(sleepTime)) {
      return TRI_ERROR_REPLICATION_APPLIER_STOPPED;
    }
  }

  // won't be reached
  return TRI_ERROR_INTERNAL;
}

/// @brief fetch the open transactions we still need to complete
Result DatabaseTailingSyncer::fetchOpenTransactions(TRI_voc_tick_t fromTick,
                                                   TRI_voc_tick_t toTick,
                                                   TRI_voc_tick_t& startTick) {
  std::string const baseUrl = BaseUrl + "/determine-open-transactions";
  std::string const url = baseUrl + "?serverId=" + _localServerIdString +
                          "&from=" + StringUtils::itoa(fromTick) + "&to=" +
                          StringUtils::itoa(toTick);

  std::string const progress = "fetching initial master state with from tick " +
                               StringUtils::itoa(fromTick) + ", to tick " +
                               StringUtils::itoa(toTick);

  setProgress(progress);

  // send request
  std::unique_ptr<SimpleHttpResult> response(
      _client->request(rest::RequestType::GET, url, nullptr, 0));

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": " + _client->getErrorMessage());
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + ": " + response->getHttpReturnMessage());
  }

  bool fromIncluded = false;

  bool found;
  std::string header =
      response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);

  if (found) {
    fromIncluded = StringUtils::boolean(header);
  }

  // fetch the tick from where we need to start scanning later
  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);

  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_LASTTICK + " is missing in determine-open-transactions response");
  }

  TRI_voc_tick_t readTick = StringUtils::uint64(header);

  if (!fromIncluded && _requireFromPresent && fromTick > 0) {
    return Result(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT, std::string("required init tick value '") + StringUtils::itoa(fromTick) + "' is not present (anymore?) on master at " + _masterInfo._endpoint + ". Last tick available on master is '" + StringUtils::itoa(readTick) + "'. It may be required to do a full resync and increase the number of historic logfiles on the master.");
  }

  startTick = readTick;
  if (startTick == 0) {
    startTick = toTick;
  }

  VPackBuilder builder;
  Result r = parseResponse(builder, response.get());

  if (r.fail()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": invalid response type for initial data. expecting array");
  }

  VPackSlice const slice = builder.slice();
  if (!slice.isArray()) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": invalid response type for initial data. expecting array");
  }

  for (auto const& it : VPackArrayIterator(slice)) {
    if (!it.isString()) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": invalid response type for initial data. expecting array of ids");
    }

    _ongoingTransactions.emplace(StringUtils::uint64(it.copyString()), nullptr);
  }

  {
    std::string const progress =
        "fetched initial master state for from tick " +
        StringUtils::itoa(fromTick) + ", to tick " + StringUtils::itoa(toTick) +
        ", got start tick: " + StringUtils::itoa(readTick) +
        ", open transactions: " + std::to_string(_ongoingTransactions.size());

    setProgress(progress);
  }

  return Result();
}

/// @brief run the continuous synchronization
Result DatabaseTailingSyncer::followMasterLog(TRI_voc_tick_t& fetchTick,
                                              TRI_voc_tick_t firstRegularTick,
                                              uint64_t& ignoreCount, bool& worked,
                                              bool& masterActive) {
  std::string const baseUrl = BaseUrl + "/logger-follow?chunkSize=" +
                              StringUtils::itoa(_configuration._chunkSize) + "&barrier=" +
                              StringUtils::itoa(_barrierId);

  worked = false;

  std::string const url = baseUrl + "&from=" + StringUtils::itoa(fetchTick) +
                          "&firstRegular=" +
                          StringUtils::itoa(firstRegularTick) + "&serverId=" +
                          _localServerIdString + "&includeSystem=" +
                          (_configuration._includeSystem ? "true" : "false");

  // send request
  std::string const progress =
      "fetching master log from tick " + StringUtils::itoa(fetchTick) +
      ", first regular tick " + StringUtils::itoa(firstRegularTick) +
      ", barrier: " + StringUtils::itoa(_barrierId) + ", open transactions: " +
      std::to_string(_ongoingTransactions.size());
  setProgress(progress);

  std::string body;
  if (!_ongoingTransactions.empty()) {
    // stringify list of open transactions
    body.append("[\"");
    bool first = true;

    for (auto& it : _ongoingTransactions) {
      if (first) {
        first = false;
      } else {
        body.append("\",\"");
      }

      body.append(StringUtils::itoa(it.first));
    }
    body.append("\"]");
  } else {
    body.append("[]");
  }

  std::unique_ptr<SimpleHttpResult> response(
      _client->request(rest::RequestType::PUT, url, body.c_str(), body.size()));

  if (response == nullptr || !response->isComplete()) {
    return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": " + _client->getErrorMessage());
  }

  TRI_ASSERT(response != nullptr);

  if (response->wasHttpError()) {
    return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + ": " + response->getHttpReturnMessage());
  }

  bool found;
  std::string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);

  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_CHECKMORE + " is missing in logger-follow response");
  }

  bool checkMore = StringUtils::boolean(header);

  // was the specified from value included the result?
  bool fromIncluded = false;
  header = response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);
  if (found) {
    fromIncluded = StringUtils::boolean(header);
  }

  bool active = false;
  header = response->getHeaderField(TRI_REPLICATION_HEADER_ACTIVE, found);
  if (found) {
    active = StringUtils::boolean(header);
  }

  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_LASTINCLUDED + " is missing in logger-follow response");
  }

  TRI_voc_tick_t lastIncludedTick = StringUtils::uint64(header);

  if (lastIncludedTick > fetchTick) {
    fetchTick = lastIncludedTick;
    worked = true;
  } else {
    // we got the same tick again, this indicates we're at the end
    checkMore = false;
  }

  header = response->getHeaderField(TRI_REPLICATION_HEADER_LASTTICK, found);
  if (!found) {
    return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": required header "+ TRI_REPLICATION_HEADER_LASTTICK + " is missing in logger-follow response");
  }

  bool bumpTick = false;
  TRI_voc_tick_t tick = StringUtils::uint64(header);
  if (!checkMore && tick > lastIncludedTick) {
    // the master has a tick value which is not contained in this result
    // but it claims it does not have any more data
    // so it's probably a tick from an invisible operation (such as
    // closing
    // a WAL file)
    bumpTick = true;
  }

  {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    _applier->_state._lastAvailableContinuousTick = tick;
  }
  
  if (!fromIncluded && _requireFromPresent && fetchTick > 0) {
    return Result(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT, std::string("required follow tick value '") + StringUtils::itoa(fetchTick) + "' is not present (anymore?) on master at " + _masterInfo._endpoint + ". Last tick available on master is '" + StringUtils::itoa(tick) + "'. It may be required to do a full resync and increase the number of historic logfiles on the master.");
  }

  TRI_voc_tick_t lastAppliedTick;

  {
    READ_LOCKER(locker, _applier->_statusLock);
    lastAppliedTick = _applier->_state._lastAppliedContinuousTick;
  }

  uint64_t processedMarkers = 0;
  Result r = applyLog(response.get(), firstRegularTick, processedMarkers, ignoreCount);

  // cppcheck-suppress *
  if (processedMarkers > 0) {
    worked = true;

    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    _applier->_state._totalEvents += processedMarkers;

    if (_applier->_state._lastAppliedContinuousTick != lastAppliedTick) {
      _hasWrittenState = true;
      saveApplierState();
    }
  } else if (bumpTick) {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

    if (_applier->_state._lastProcessedContinuousTick < tick) {
      _applier->_state._lastProcessedContinuousTick = tick;
    }

    if (_ongoingTransactions.empty() &&
        _applier->_state._safeResumeTick == 0) {
      _applier->_state._safeResumeTick = tick;
    }

    if (!_hasWrittenState) {
      _hasWrittenState = true;
      saveApplierState();
    }
  }

  if (!_hasWrittenState && _useTick) {
    // write state at least once so the start tick gets saved
    _hasWrittenState = true;

    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

    _applier->_state._lastAppliedContinuousTick = firstRegularTick;
    _applier->_state._lastProcessedContinuousTick = firstRegularTick;

    if (_ongoingTransactions.empty() &&
        _applier->_state._safeResumeTick == 0) {
      _applier->_state._safeResumeTick = firstRegularTick;
    }

    saveApplierState();
  }

  if (r.fail()) {
    return r;
  }

  masterActive = active;

  if (!worked) {
    if (checkMore) {
      worked = true;
    }
  }

  return Result();
}

void DatabaseTailingSyncer::preApplyMarker(TRI_voc_tick_t firstRegularTick,
                                      TRI_voc_tick_t newTick) {
  if (newTick >= firstRegularTick) {
    WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);
    if (newTick > _applier->_state._lastProcessedContinuousTick) {
      _applier->_state._lastProcessedContinuousTick = newTick;
    }
  }
}

void DatabaseTailingSyncer::postApplyMarker(uint64_t processedMarkers,
                                       bool skipped) {
  WRITE_LOCKER_EVENTUAL(writeLocker, _applier->_statusLock);

  if (_applier->_state._lastProcessedContinuousTick >
      _applier->_state._lastAppliedContinuousTick) {
    _applier->_state._lastAppliedContinuousTick =
        _applier->_state._lastProcessedContinuousTick;
  }

  if (skipped) {
    ++_applier->_state._skippedOperations;
  } else if (_ongoingTransactions.empty()) {
    _applier->_state._safeResumeTick =
        _applier->_state._lastProcessedContinuousTick;
  }
}


/// @brief finalize the synchronization of a collection by tailing the WAL
/// and filtering on the collection name until no more data is available
Result DatabaseTailingSyncer::syncCollectionFinalize(std::string const& collectionName) {
  // fetch master state just once
  Result r = getMasterState();
  if (r.fail()) {
    return r;
  }
  
  // print extra info for debugging
  _configuration._verbose = true;
  // we do not want to apply rename, create and drop collection operations
  _ignoreRenameCreateDrop = true;
  
  TRI_voc_tick_t fromTick = _initialTick;
  
  while (true) {
    if (application_features::ApplicationServer::isStopping()) {
      return Result(TRI_ERROR_SHUTTING_DOWN);
    }
    
    std::string const baseUrl =
      BaseUrl + "/logger-follow?chunkSize=" +
      StringUtils::itoa(_configuration._chunkSize) + "&from=" +
      StringUtils::itoa(fromTick) + "&serverId=" + _localServerIdString +
      "&collection=" + StringUtils::urlEncode(collectionName);
    
    // send request
    std::unique_ptr<SimpleHttpResult> response(_client->request(rest::RequestType::GET, baseUrl, nullptr, 0));
    
    if (response == nullptr || !response->isComplete()) {
      return Result(TRI_ERROR_REPLICATION_NO_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": " + _client->getErrorMessage());
    }
    
    TRI_ASSERT(response != nullptr);
    
    if (response->wasHttpError()) {
      return Result(TRI_ERROR_REPLICATION_MASTER_ERROR, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": HTTP " + StringUtils::itoa(response->getHttpReturnCode()) + ": " + response->getHttpReturnMessage());
    }
    
    if (response->getHttpReturnCode() == 204) {
      // HTTP 204 No content: this means we are done
      return Result();
    }
    
    bool found;
    std::string header = response->getHeaderField(TRI_REPLICATION_HEADER_CHECKMORE, found);
    bool checkMore = false;
    if (found) {
      checkMore = StringUtils::boolean(header);
    }
    
    TRI_voc_tick_t lastIncludedTick;
    header =
    response->getHeaderField(TRI_REPLICATION_HEADER_LASTINCLUDED, found);
    if (!found) {
      return Result(TRI_ERROR_REPLICATION_INVALID_RESPONSE, std::string("got invalid response from master at ") + _masterInfo._endpoint + ": required header " + TRI_REPLICATION_HEADER_LASTINCLUDED + " is missing in logger-follow response");
    }
    lastIncludedTick = StringUtils::uint64(header);
    
    // was the specified from value included the result?
    bool fromIncluded = false;
    header = response->getHeaderField(TRI_REPLICATION_HEADER_FROMPRESENT, found);
    if (found) {
      fromIncluded = StringUtils::boolean(header);
    }
    if (!fromIncluded && fromTick > 0) { // && _requireFromPresent
      return Result(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT, std::string("required follow tick value '") + StringUtils::itoa(lastIncludedTick) + "' is not present (anymore?) on master at " +  _masterInfo._endpoint + ". Last tick available on master is '" + StringUtils::itoa(lastIncludedTick) + "'. It may be required to do a full resync and increase the number of historic logfiles on the master.");
    }
    
    uint64_t processedMarkers = 0;
    uint64_t ignoreCount = 0;
    Result r = applyLog(response.get(), fromTick, processedMarkers, ignoreCount);
    if (r.fail()) {
      return r;
    }
    
    if (!checkMore) {  // || processedMarkers == 0) { // TODO: check if we need this
      // done!
      return Result();
    }
  }
}
