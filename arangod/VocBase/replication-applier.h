////////////////////////////////////////////////////////////////////////////////
/// @brief replication applier
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_VOC_BASE_REPLICATION__APPLIER_H
#define ARANGODB_VOC_BASE_REPLICATION__APPLIER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/threads.h"
#include "Utils/ReplicationTransaction.h"
#include "VocBase/replication-common.h"
#include "VocBase/voc-types.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_json_t;
struct TRI_server_t;
struct TRI_vocbase_t;

// -----------------------------------------------------------------------------
// --SECTION--                                               REPLICATION APPLIER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief struct containing a replication apply configuration
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_applier_configuration_s {
  char*         _endpoint;
  char*         _database;
  char*         _username;
  char*         _password;
  double        _requestTimeout;
  double        _connectTimeout;
  uint64_t      _ignoreErrors;
  uint64_t      _maxConnectRetries;
  uint64_t      _chunkSize;
  uint32_t      _sslProtocol;
  bool          _autoStart;
  bool          _adaptivePolling;
  bool          _includeSystem;
  bool          _requireFromPresent;
  std::string   _restrictType;
  std::unordered_map<std::string, bool> _restrictCollections;
}
TRI_replication_applier_configuration_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief struct containing a replication apply error
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_applier_error_s {
  int           _code;
  char*         _msg;
  char          _time[24];
}
TRI_replication_applier_error_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication application
////////////////////////////////////////////////////////////////////////////////

struct TRI_replication_applier_state_t {
  TRI_voc_tick_t                           _lastProcessedContinuousTick;
  TRI_voc_tick_t                           _lastAppliedContinuousTick;
  TRI_voc_tick_t                           _lastAvailableContinuousTick;
  bool                                     _active;
  char*                                    _progressMsg;
  char                                     _progressTime[24];
  TRI_server_id_t                          _serverId;
  TRI_replication_applier_error_t          _lastError;
  uint64_t                                 _failedConnects;
  uint64_t                                 _totalRequests;
  uint64_t                                 _totalFailedConnects;
  uint64_t                                 _totalEvents;
  uint64_t                                 _skippedOperations;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replication applier
////////////////////////////////////////////////////////////////////////////////

class TRI_replication_applier_t {
  public:

    TRI_replication_applier_t (TRI_server_t*,
                               TRI_vocbase_t*);

    ~TRI_replication_applier_t ();

  public:

////////////////////////////////////////////////////////////////////////////////
/// @brief pauses and checks whether the apply thread should terminate
////////////////////////////////////////////////////////////////////////////////

    bool wait (uint64_t);

    bool isTerminated () {
      return _terminateThread.load();
    }
  
    void setTermination (bool value) {
      _terminateThread.store(value);
    }

    void addRemoteTransaction (triagens::arango::ReplicationTransaction* trx) {
      _runningRemoteTransactions.insert(std::make_pair(trx->externalId(), trx));
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database name
////////////////////////////////////////////////////////////////////////////////

    char const* databaseName () const {
      return _databaseName.c_str();
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier
////////////////////////////////////////////////////////////////////////////////

    int start (TRI_voc_tick_t, 
               bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
////////////////////////////////////////////////////////////////////////////////

    int stop (bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the applier and "forget" everything
////////////////////////////////////////////////////////////////////////////////

    int forget ();

////////////////////////////////////////////////////////////////////////////////
/// @brief shuts down the replication applier
////////////////////////////////////////////////////////////////////////////////

    int shutdown ();

    void abortRunningRemoteTransactions () {
      size_t const n = _runningRemoteTransactions.size();
      triagens::arango::TransactionBase::increaseNumbers((int) n, (int) n);

      for (auto it = _runningRemoteTransactions.begin(); it != _runningRemoteTransactions.end(); ++it) {
        auto trx = (*it).second;

        // do NOT write abort markers so we can resume running transactions later
        trx->removeHint(TRI_TRANSACTION_HINT_NO_ABORT_MARKER, true);
        delete trx;
      }

      _runningRemoteTransactions.clear();
    }

  private:
    
    std::string                              _databaseName;

  public:

    TRI_server_t*                            _server;
    TRI_vocbase_t*                           _vocbase;
    triagens::basics::ReadWriteLock          _statusLock;
    std::atomic<bool>                        _terminateThread;
    TRI_replication_applier_state_t          _state;
    TRI_replication_applier_configuration_t  _configuration;
    TRI_thread_t                             _thread;
    std::unordered_map<TRI_voc_tid_t, triagens::arango::ReplicationTransaction*> _runningRemoteTransactions;
};

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t* TRI_CreateReplicationApplier (TRI_server_t*,
                                                         TRI_vocbase_t*);

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of the replication apply configuration
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_t* TRI_JsonConfigurationReplicationApplier (TRI_replication_applier_configuration_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_ConfigureReplicationApplier (TRI_replication_applier_t*,
                                     TRI_replication_applier_configuration_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication apply state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationApplier (TRI_replication_applier_t*,
                                 TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of an applier
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_t* TRI_JsonReplicationApplier (TRI_replication_applier_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief register an applier error
////////////////////////////////////////////////////////////////////////////////

int TRI_SetErrorReplicationApplier (TRI_replication_applier_t*,
                                    int,
                                    char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the progress with or without a lock
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProgressReplicationApplier (TRI_replication_applier_t*,
                                        char const*,
                                        bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStateReplicationApplier (TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyStateReplicationApplier (TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application state to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveStateReplicationApplier (TRI_vocbase_t*,
                                     TRI_replication_applier_state_t const*,
                                     bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application state file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveStateReplicationApplier (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief load the replication application state from a file
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadStateReplicationApplier (TRI_vocbase_t*,
                                     TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_InitConfigurationReplicationApplier (TRI_replication_applier_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyConfigurationReplicationApplier (TRI_replication_applier_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief copy an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyConfigurationReplicationApplier (TRI_replication_applier_configuration_t const*,
                                              TRI_replication_applier_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application configuration file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveConfigurationReplicationApplier (TRI_vocbase_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application configuration to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveConfigurationReplicationApplier (TRI_vocbase_t*,
                                             TRI_replication_applier_configuration_t const*,
                                             bool);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
