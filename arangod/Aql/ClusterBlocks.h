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
#include "Aql/SortRegister.h"
#include "Rest/GeneralRequest.h"

#include <velocypack/Builder.h>

namespace arangodb {
namespace transaction {
class Methods;
}

struct ClusterCommResult;

namespace aql {
class AqlItemBlock;
struct Collection;
class ExecutionEngine;

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
  AqlItemBlock* getSome(size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief skipSome: shouldn't be used, use skipSomeForShard
  size_t skipSome(size_t atMost) override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief hasMore
  bool hasMore() override final {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /// @brief getSomeForShard
  AqlItemBlock* getSomeForShard(size_t atMost, std::string const& shardId);

  /// @brief skipSomeForShard
  size_t skipSomeForShard(size_t atMost, std::string const& shardId);

  /// @brief skipForShard
  bool skipForShard(size_t number, std::string const& shardId);

  /// @brief hasMoreForShard: any more for shard <shardId>?
  virtual bool hasMoreForShard(std::string const& shardId) = 0;

 protected:
  /// @brief getOrSkipSomeForShard
  virtual int getOrSkipSomeForShard(size_t atMost,
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

 private:
  bool _wasShutdown;
};

class ScatterBlock : public BlockWithClients {
 public:
  ScatterBlock(ExecutionEngine* engine, ScatterNode const* ep,
               std::vector<std::string> const& shardIds)
      : BlockWithClients(engine, ep, shardIds) {}

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown
  int shutdown(int) override;

  /// @brief hasMoreForShard: any more for shard <shardId>?
  bool hasMoreForShard(std::string const& shardId) override;

 private:
  /// @brief getOrSkipSomeForShard
  int getOrSkipSomeForShard(size_t atMost, bool skipping,
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

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown
  int shutdown(int) override;

  /// @brief hasMoreForShard: any more for shard <shardId>?
  bool hasMoreForShard(std::string const& shardId) override;

 private:
  /// @brief getOrSkipSomeForShard
  int getOrSkipSomeForShard(size_t atMost, bool skipping,
                            AqlItemBlock*& result, size_t& skipped,
                            std::string const& shardId) override;

  /// @brief getBlockForClient: try to get at atMost pairs into
  /// _distBuffer.at(clientId).
  bool getBlockForClient(size_t atMost, size_t clientId);

  /// @brief sendToClient: for each row of the incoming AqlItemBlock use the
  /// attributes <shardKeys> of the register <id> to determine to which shard
  /// the
  /// row should be sent.
  size_t sendToClient(AqlItemBlock*);

  /// @brief create a new document key
  std::string createKey(arangodb::velocypack::Slice) const;

  // a reusable Builder object for building _key values
  arangodb::velocypack::Builder _keyBuilder;

  // a reusable Builder object for building document objects
  arangodb::velocypack::Builder _objectBuilder;

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

  /// @brief allow specified keys even in non-default sharding case
  bool _allowSpecifiedKeys;
};

class RemoteBlock final : public ExecutionBlock {
  /// @brief constructors/destructors
 public:
  RemoteBlock(ExecutionEngine* engine, RemoteNode const* en,
              std::string const& server, std::string const& ownName,
              std::string const& queryId);

  /// @brief timeout
  static double const defaultTimeOut;

  /// @brief initializeCursor, could be called multiple times
  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  /// @brief shutdown, will be called exactly once for the whole query
  int shutdown(int) override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) override final;

  /// @brief skipSome
  size_t skipSome(size_t atMost) override final;

  /// @brief hasMore
  bool hasMore() override final;

 private:
  /// @brief internal method to send a request
  std::unique_ptr<arangodb::ClusterCommResult> sendRequest(
      rest::RequestType type, std::string const& urlPart,
      std::string const& body) const;

  /// @brief our server, can be like "shard:S1000" or like "server:Claus"
  std::string const _server;

  /// @brief our own identity, in case of the coordinator this is empty,
  /// in case of the DBservers, this is the shard ID as a string
  std::string const _ownName;

  /// @brief the ID of the query on the server as a string
  std::string const _queryId;

  /// @brief whether or not this block will forward initialize,
  /// initializeCursor or shutDown requests
  bool const _isResponsibleForInitializeCursor;
};

////////////////////////////////////////////////////////////////////////////////
/// @class UnsortingGatherBlock
/// @brief Execution block for gathers without order
////////////////////////////////////////////////////////////////////////////////
class UnsortingGatherBlock final : public ExecutionBlock {
 public:
  UnsortingGatherBlock(ExecutionEngine& engine, GatherNode const& en)
    : ExecutionBlock(&engine, &en) {
    TRI_ASSERT(en.elements().empty());
  }

  /// @brief shutdown: need our own method since our _buffer is different
  int shutdown(int errorCode) override final;

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  /// @brief hasMore: true if any position of _buffer hasMore and false
  /// otherwise.
  bool hasMore() override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) override final;

  /// @brief skipSome
  size_t skipSome(size_t atMost) override final;

 private:
  /// @brief _atDep: currently pulling blocks from _dependencies.at(_atDep),
  size_t _atDep{};
}; // UnsortingGatherBlock

////////////////////////////////////////////////////////////////////////////////
/// @struct SortingStrategy
////////////////////////////////////////////////////////////////////////////////
struct SortingStrategy {
  typedef std::pair<
    size_t, // dependency index
    size_t // position within a dependecy
  > ValueType;

  virtual ~SortingStrategy() = default;

  /// @brief returns next value
  virtual ValueType nextValue() = 0;

  /// @brief prepare strategy fetching values
  virtual void prepare(std::vector<ValueType>& /*blockPos*/) { }

  /// @brief resets strategy state
  virtual void reset() = 0;
}; // SortingStrategy

////////////////////////////////////////////////////////////////////////////////
/// @class SortingGatherBlock
/// @brief Execution block for gathers with order
////////////////////////////////////////////////////////////////////////////////
class SortingGatherBlock final : public ExecutionBlock {
 public:
  SortingGatherBlock(
    ExecutionEngine& engine,
    GatherNode const& en
  );

  /// @brief shutdown: need our own method since our _buffer is different
  int shutdown(int errorCode) override final;

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override final;

  /// @brief hasMore: true if any position of _buffer hasMore and false
  /// otherwise.
  bool hasMore() override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) override final;

  /// @brief skipSome
  size_t skipSome(size_t atMost) override final;

 private:
  /// @brief getBlock: from dependency i into _gatherBlockBuffer.at(i),
  /// non-simple case only
  bool getBlock(size_t i, size_t atMost);

  /// @brief _gatherBlockBuffer: buffer the incoming block from each dependency
  /// separately
  std::vector<std::deque<AqlItemBlock*>> _gatherBlockBuffer;

  /// @brief _gatherBlockPos: pairs (i, _pos in _buffer.at(i)), i.e. the same as
  /// the usual _pos but one pair per dependency
  std::vector<std::pair<size_t, size_t>> _gatherBlockPos;

  /// @brief sort elements for this block
  std::vector<SortRegister> _sortRegisters;

  /// @brief sorting strategy
  std::unique_ptr<SortingStrategy> _strategy;
}; // SortingGatherBlock



class SingleRemoteOperationBlock final : public ExecutionBlock {
  /// @brief constructors/destructors
 private:
  bool getOne(arangodb::aql::AqlItemBlock* aqlres,
              size_t outputCounter);
 public:
  SingleRemoteOperationBlock(ExecutionEngine* engine,
                             SingleRemoteOperationNode const* en
                             );

  /// @brief timeout
  static double const defaultTimeOut;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atMost) override final;

  /// @brief skipSome
  size_t skipSome(size_t atMost) override final;

  /// @brief hasMore
  bool hasMore() override final;

 private:

  /// @brief _colectionName: the name of the sharded collection
  Collection const* _collection;

  /// @brief the key of the document to fetch
  std::string const _key;
};

}  // namespace arangodb::aql
}  // namespace arangodb

#endif
