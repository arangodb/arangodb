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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ConditionVariable.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/SyncerId.h"
#include "Replication/common-defines.h"
#include "Replication/utilities.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/Identifiers/ServerId.h"
#include "VocBase/ticks.h"

#include <function2.hpp>

#include <memory>
#include <string>
#include <unordered_map>

struct TRI_vocbase_t;

namespace arangodb {
namespace httpclient {
class SimpleHttpResult;
}  // namespace httpclient

namespace transaction {
class Methods;
}

namespace velocypack {
class Slice;
}

class LogicalCollection;

class Syncer : public std::enable_shared_from_this<Syncer> {
 public:
  /// @brief a helper object used for synchronization between the
  /// dump apply thread and some helper job posted into the scheduler
  /// for async fetching of the next dump results
  class JobSynchronizer : public std::enable_shared_from_this<JobSynchronizer> {
   public:
    JobSynchronizer(JobSynchronizer const&) = delete;
    JobSynchronizer& operator=(JobSynchronizer const&) = delete;

    explicit JobSynchronizer(std::shared_ptr<Syncer const> syncer);
    ~JobSynchronizer();

    /// @brief whether or not a response has arrived
    bool gotResponse() const noexcept;

    /// @brief will be called whenever a response for the job comes in
    void gotResponse(
        std::unique_ptr<arangodb::httpclient::SimpleHttpResult> response,
        double time) noexcept;

    /// @brief will be called whenever an error occurred
    /// expects "res" to be an error!
    void gotResponse(arangodb::Result&& res, double time = 0.0) noexcept;

    /// @brief the calling Syncer will call and block inside this function until
    /// there is a response or the syncer/server is shut down
    arangodb::Result waitForResponse(
        std::unique_ptr<arangodb::httpclient::SimpleHttpResult>& response);

    /// @brief post an async request to the scheduler
    /// this will increase the number of inflight jobs, and count it down
    /// when the posted request has finished
    void request(fu2::unique_function<void()> cb);

    /// @brief notifies that a job was posted
    /// returns false if job counter could not be increased (e.g. because
    /// the syncer was stopped/aborted already)
    bool jobPosted();

    /// @brief notifies that a job was done
    void jobDone() noexcept;

    /// @brief checks if there are jobs in flight (can be 0 or 1 job only)
    bool hasJobInFlight() const noexcept;

    /// @brief return the stored elapsed time for the job
    double time() const noexcept { return _time; }

   private:
    /// @brief the shared syncer we use to check if sychronization was
    /// externally aborted
    std::shared_ptr<Syncer const> _syncer;

    /// @brief condition variable used for synchronization
    arangodb::basics::ConditionVariable mutable _condition;

    /// @brief true if a response was received
    bool _gotResponse;

    /// @brief elapsed time for performing the job
    double _time;

    /// @brief the processing response of the job (indicates failure if no
    /// response was received or if something went wrong)
    arangodb::Result _res;

    /// @brief the callback to execute
    fu2::unique_function<void()> _cb;

    /// @brief id of the current callback to execute. this is necessary
    /// because if we post a job to the scheduler and then execute it
    /// ourselves before the scheduler has a chance to do so, we need a
    /// way to abort the scheduler callback, so it does not execute the
    /// old job once more. we do so by posting the job's original id
    /// along with the job to the scheduler, and check in the scheduler
    /// callback if the stored id is still the same as the posted one.
    uint64_t _id;

    /// @brief id of the last job posted (will be increased whenever a new
    /// job is posted)
    uint64_t _nextId;

    /// @brief the response received by the job (nullptr if no response
    /// received)
    std::unique_ptr<arangodb::httpclient::SimpleHttpResult> _response;

    /// @brief number of posted jobs in flight
    uint64_t _jobsInFlight;
  };

  class JobSynchronizerScope {
   public:
    JobSynchronizerScope(JobSynchronizerScope const&) = delete;
    JobSynchronizerScope& operator=(JobSynchronizerScope const&) = delete;

    explicit JobSynchronizerScope(std::shared_ptr<Syncer const> syncer);
    ~JobSynchronizerScope();

    JobSynchronizer* operator->();
    JobSynchronizer const* operator->() const;

    std::shared_ptr<JobSynchronizer> clone() const;

   private:
    std::shared_ptr<JobSynchronizer> _synchronizer;
  };

  struct SyncerState {
    SyncerId syncerId;

    /// @brief configuration
    ReplicationApplierConfiguration applier;

    /// @brief object holding the HTTP client and all connection machinery
    replutils::Connection connection;

    /// @brief database name
    std::string databaseName{};

    /// Is this syncer allowed to handle its own batch
    bool isChildSyncer{false};

    /// @brief leaderId, this is used in the cluster to the unique ID of the
    /// source server (the shard leader in this case). We need this information
    /// to apply the changes locally to a shard, which is configured as a
    /// follower and thus only accepts modifications that are replications
    /// from the leader. Leave empty if there is no concept of a "leader".
    std::string leaderId{};

    /// @brief local server id
    ServerId localServerId{ServerId::none()};

    /// @brief local server id
    std::string localServerIdString{};

    /// @brief information about the leader state
    replutils::LeaderInfo leader;

    /// @brief lazy loaded list of vocbases
    std::unordered_map<std::string, DatabaseGuard> vocbases{};

    SyncerState(Syncer*, ReplicationApplierConfiguration const&);
  };

  Syncer(Syncer const&) = delete;
  Syncer& operator=(Syncer const&) = delete;

  explicit Syncer(ReplicationApplierConfiguration const&);

  virtual ~Syncer();

  /// @brief request location rewriter (injects database name)
  static std::string rewriteLocation(void const*, std::string const&);

  void setLeaderId(std::string const& leaderId) { _state.leaderId = leaderId; }

  // TODO worker-safety
  void setAborted(bool value);

  // TODO worker-safety
  virtual bool isAborted() const;

  SyncerId syncerId() const noexcept;

 protected:
  /// @brief reload all users
  // TODO worker safety
  void reloadUsers();

  /// @brief apply a single marker from the collection dump
  /// If the returned Result is TRI_ERROR_ARANGO_TRY_AGAIN,
  /// conflictingDocumentKey will be set to the key of the conflicting document.
  /// Otherwise, it will be untouched.
  // TODO worker-safety
  Result applyCollectionDumpMarker(transaction::Methods&,
                                   LogicalCollection* coll,
                                   TRI_replication_operation_e,
                                   velocypack::Slice slice,
                                   std::string& conflictingDocumentKey);

  /// @brief creates a collection, based on the VelocyPack provided
  // TODO worker safety - create/drop phase
  Result createCollection(TRI_vocbase_t& vocbase, velocypack::Slice slice,
                          LogicalCollection** dst);

  /// @brief drops a collection, based on the VelocyPack provided
  // TODO worker safety - create/drop phase
  Result dropCollection(velocypack::Slice slice, bool reportError);

  /// @brief creates an index, based on the VelocyPack provided
  Result createIndex(velocypack::Slice slice);

  /// @brief creates an index, or returns the existing matching index if there
  /// is one
  void createIndexInternal(velocypack::Slice idxDef, LogicalCollection&);

  /// @brief drops an index, based on the VelocyPack provided
  Result dropIndex(velocypack::Slice slice);

  /// @brief creates a view, based on the VelocyPack provided
  Result createView(TRI_vocbase_t& vocbase, velocypack::Slice slice);

  /// @brief drops a view, based on the VelocyPack provided
  Result dropView(velocypack::Slice slice, bool reportError);

  // TODO worker safety
  virtual TRI_vocbase_t* resolveVocbase(velocypack::Slice slice);

  // TODO worker safety
  std::shared_ptr<LogicalCollection> resolveCollection(TRI_vocbase_t& vocbase,
                                                       velocypack::Slice slice);

  // TODO worker safety
  std::unordered_map<std::string, DatabaseGuard> const& vocbases() const {
    return _state.vocbases;
  }

 protected:
  /// @brief state information for the syncer
  SyncerState _state;
};

}  // namespace arangodb
