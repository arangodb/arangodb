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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_CLUSTER_BLOCKS_H
#define ARANGOD_AQL_CLUSTER_BLOCKS_H 1

#include "Basics/Common.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionStats.h"
#include "Rest/GeneralRequest.h"

namespace arangodb {
class Transaction;
struct ClusterCommResult;

namespace aql {
class AqlItemBlock;
struct Collection;
class ExecutionEngine;

class GatherBlock : public ExecutionBlock {
 public:
  GatherBlock(ExecutionEngine*, GatherNode const*);

  ~GatherBlock();

  /// @brief initialize
  int initialize() override;

  /// @brief shutdown: need our own method since our _buffer is different
  int shutdown(int) override final;

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  /// @brief count: the sum of the count() of the dependencies or -1 (if any
  /// dependency has count -1
  int64_t count() const override final;

  /// @brief remaining: the sum of the remaining() of the dependencies or -1 (if
  /// any dependency has remaining -1
  int64_t remaining() override final;

  /// @brief hasMore: true if any position of _buffer hasMore and false
  /// otherwise.
  bool hasMore() override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t, size_t) override final;

  /// @brief skipSome
  size_t skipSome(size_t, size_t) override final;

 protected:
  /// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
  /// non-simple case only
  bool getBlock(size_t i, size_t atLeast, size_t atMost);

  /// @brief _gatherBlockBuffer: buffer the incoming block from each dependency
  /// separately
  std::vector<std::deque<AqlItemBlock*>> _gatherBlockBuffer;

 private:
  /// @brief _gatherBlockPos: pairs (i, _pos in _buffer.at(i)), i.e. the same as
  /// the usual _pos but one pair per dependency
  std::vector<std::pair<size_t, size_t>> _gatherBlockPos;

  /// @brief _atDep: currently pulling blocks from _dependencies.at(_atDep),
  /// simple case only
  size_t _atDep = 0;

  /// @brief pairs, consisting of variable and sort direction
  /// (true = ascending | false = descending)
  std::vector<std::pair<RegisterId, bool>> _sortRegisters;

  /// @brief isSimple: the block is simple if we do not do merge sort . . .
  bool const _isSimple;

  /// @brief OurLessThan: comparison method for elements of _gatherBlockPos
  class OurLessThan {
   public:
    OurLessThan(arangodb::Transaction* trx,
                std::vector<std::deque<AqlItemBlock*>>& gatherBlockBuffer,
                std::vector<std::pair<RegisterId, bool>>& sortRegisters) 
        : _trx(trx),
          _gatherBlockBuffer(gatherBlockBuffer),
          _sortRegisters(sortRegisters) {}

    bool operator()(std::pair<size_t, size_t> const& a,
                    std::pair<size_t, size_t> const& b);

   private:
    arangodb::Transaction* _trx;
    std::vector<std::deque<AqlItemBlock*>>& _gatherBlockBuffer;
    std::vector<std::pair<RegisterId, bool>>& _sortRegisters;
  };
};

class BlockWithClients : public ExecutionBlock {
 public:
  BlockWithClients(ExecutionEngine* engine, ExecutionNode const* ep,
                   std::vector<std::string> const& shardIds);

  virtual ~BlockWithClients() {}

 public:
  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown
  int shutdown(int) override;

  /// @brief getSome: shouldn't be used, use skipSomeForShard
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief skipSome: shouldn't be used, use skipSomeForShard
  size_t skipSome(size_t atLeast, size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief remaining
  int64_t remaining() override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief hasMore
  bool hasMore() override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief getSomeForShard
  AqlItemBlock* getSomeForShard(size_t atLeast, size_t atMost,
                                std::string const& shardId);

  /// @brief skipSomeForShard
  size_t skipSomeForShard(size_t atLeast, size_t atMost,
                          std::string const& shardId);

  /// @brief skipForShard
  bool skipForShard(size_t number, std::string const& shardId);

  /// @brief hasMoreForShard: any more for shard <shardId>?
  virtual bool hasMoreForShard(std::string const& shardId) = 0;

  /// @brief remainingForShard: remaining for shard <shardId>?
  virtual int64_t remainingForShard(std::string const& shardId) = 0;

 protected:
  /// @brief getOrSkipSomeForShard
  virtual int getOrSkipSomeForShard(size_t atLeast, size_t atMost,
                                    bool skipping, AqlItemBlock*& result,
                                    size_t& skipped,
                                    std::string const& shardId) = 0;

  /// @brief getClientId: get the number <clientId> (used internally)
  /// corresponding to <shardId>
  size_t getClientId(std::string const& shardId);

  /// @brief _shardIdMap: map from shardIds to clientNrs
  std::unordered_map<std::string, size_t> _shardIdMap;

  /// @brief _nrClients: total number of clients
  size_t _nrClients;

  /// @brief _doneForClient: the analogue of _done: _doneForClient.at(i) = true
  /// if we are done for the shard with clientId = i
  std::vector<bool> _doneForClient;
};

class ScatterBlock : public BlockWithClients {
 public:
  ScatterBlock(ExecutionEngine* engine, ScatterNode const* ep,
               std::vector<std::string> const& shardIds)
      : BlockWithClients(engine, ep, shardIds) {}

  ~ScatterBlock() {}

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown
  int shutdown(int) override;

  /// @brief hasMoreForShard: any more for shard <shardId>?
  bool hasMoreForShard(std::string const& shardId) override;

  /// @brief remainingForShard: remaining for shard <shardId>?
  int64_t remainingForShard(std::string const& shardId) override;

 private:
  /// @brief getOrSkipSomeForShard
  int getOrSkipSomeForShard(size_t atLeast, size_t atMost, bool skipping,
                            AqlItemBlock*& result, size_t& skipped,
                            std::string const& shardId) override;

  /// @brief _posForClient:
  std::vector<std::pair<size_t, size_t>> _posForClient;
};

class DistributeBlock : public BlockWithClients {
 public:
  DistributeBlock(ExecutionEngine* engine, DistributeNode const* ep,
                  std::vector<std::string> const& shardIds,
                  Collection const* collection);

  ~DistributeBlock() {}

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown
  int shutdown(int) override;

  /// @brief remainingForShard: remaining for shard <shardId>?
  int64_t remainingForShard(std::string const& shardId) override { return -1; }

  /// @brief hasMoreForShard: any more for shard <shardId>?
  bool hasMoreForShard(std::string const& shardId) override;

 private:
  /// @brief getOrSkipSomeForShard
  int getOrSkipSomeForShard(size_t atLeast, size_t atMost, bool skipping,
                            AqlItemBlock*& result, size_t& skipped,
                            std::string const& shardId) override;

  /// @brief getBlockForClient: try to get at atLeast/atMost pairs into
  /// _distBuffer.at(clientId).
  bool getBlockForClient(size_t atLeast, size_t atMost, size_t clientId);

  /// @brief sendToClient: for each row of the incoming AqlItemBlock use the
  /// attributes <shardKeys> of the register <id> to determine to which shard
  /// the
  /// row should be sent.
  size_t sendToClient(AqlItemBlock*);

  /// @brief create a new document key
  std::string createKey() const;

  /// @brief _distBuffer.at(i) is a deque containing pairs (j,k) such that
  //  _buffer.at(j) row k should be sent to the client with id = i.
  std::vector<std::deque<std::pair<size_t, size_t>>> _distBuffer;

  /// @brief _colectionName: the name of the sharded collection
  Collection const* _collection;

  /// @brief _index: the block in _buffer we are currently considering
  size_t _index;

  /// @brief _regId: the register to inspect
  RegisterId _regId;

  /// @brief a second register to inspect (used only for UPSERT nodes at the
  /// moment to distinguish between search and insert)
  RegisterId _alternativeRegId;

  /// @brief whether or not the collection uses the default sharding
  bool _usesDefaultSharding;
};

class RemoteBlock : public ExecutionBlock {
  /// @brief constructors/destructors
 public:
  RemoteBlock(ExecutionEngine* engine, RemoteNode const* en,
              std::string const& server, std::string const& ownName,
              std::string const& queryId);

  ~RemoteBlock();

  /// @brief timeout
  static double const defaultTimeOut;

  /// @brief initialize
  int initialize() override final;

  /// @brief initializeCursor, could be called multiple times
  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  /// @brief shutdown, will be called exactly once for the whole query
  int shutdown(int) override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  /// @brief skipSome
  size_t skipSome(size_t atLeast, size_t atMost) override final;

  /// @brief hasMore
  bool hasMore() override final;

  /// @brief count
  int64_t count() const override final;

  /// @brief remaining
  int64_t remaining() override final;

  /// @brief internal method to send a request
 private:
  std::unique_ptr<arangodb::ClusterCommResult> sendRequest(
      rest::RequestType type, std::string const& urlPart,
      std::string const& body) const;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string _server;

  /// @brief our own identity, in case of the coordinator this is empty,
  /// in case of the DBservers, this is the shard ID as a string
  std::string _ownName;

  /// @brief the ID of the query on the server as a string
  std::string _queryId;

  /// @brief the ID of the query on the server as a string
  ExecutionStats _deltaStats;

  /// @brief whether or not this block will forward initialize, 
  /// initializeCursor or shutDown requests
  bool const _isResponsibleForInitializeCursor;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
