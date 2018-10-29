////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "GraphStore.h"

#include "Basics/Common.h"
#include "Basics/MutexLocker.h"
#include "Graph/EdgeCollectionInfo.h"
#include "Pregel/CommonFormats.h"
#include "Pregel/PregelFeature.h"
#include "Pregel/TypedBuffer.h"
#include "Pregel/Utils.h"
#include "Pregel/WorkerConfig.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>

using namespace arangodb;
using namespace arangodb::pregel;

static uint64_t TRI_totalSystemMemory() {
#ifdef _WIN32
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  return status.ullTotalPhys;
#else
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  long mem = pages * page_size;
  return mem > 0 ? (uint64_t)mem : 0;
#endif
}

template <typename V, typename E>
GraphStore<V, E>::GraphStore(TRI_vocbase_t& vb, GraphFormat<V, E>* graphFormat)
    : _vocbaseGuard(vb),
      _graphFormat(graphFormat),
      _localVerticeCount(0),
      _localEdgeCount(0),
      _runningThreads(0) {}

template <typename V, typename E>
GraphStore<V, E>::~GraphStore() {
  _destroyed = true;
  std::this_thread::sleep_for(std::chrono::microseconds(25 * 1000));
  delete _vertexData;
  delete _edges;
}

static const char* shardError = "Collections need to have the same number of shards"
" use distributeShardsLike";

template <typename V, typename E>
std::map<CollectionID, std::vector<VertexShardInfo>>
  GraphStore<V, E>::_allocateSpace() {
  if (_vertexData || _edges) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "Only allocate messages once");
    //_edges->resize(count);
  }

  std::map<CollectionID, std::vector<VertexShardInfo>> result;

  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Allocating memory";
  uint64_t totalMemory = TRI_totalSystemMemory();

  // Contains the shards located on this db server in the right order
  // assuming edges are sharded after _from, vertices after _key
  // then every ith vertex shard has the corresponding edges in
  // the ith edge shard
  std::map<CollectionID, std::vector<ShardID>> const& vertexCollMap =
  _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeCollMap =
  _config->edgeCollectionShards();
  size_t numShards = SIZE_MAX;

  // Allocating some memory
  uint64_t vCount = 0;
  uint64_t eCount = 0;
  for (auto const& pair : vertexCollMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    if (numShards == SIZE_MAX) {
      numShards = vertexShards.size();
    } else if (numShards != vertexShards.size()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
    }

    for (size_t i = 0; i < vertexShards.size(); i++) {

      VertexShardInfo info;
      info.vertexShard = vertexShards[i];
      info.trx = _createTransaction();

      TRI_voc_cid_t cid = info.trx->addCollectionAtRuntime(info.vertexShard);
      info.trx->pinData(cid);  // will throw when it fails

      OperationResult opResult = info.trx->count(info.vertexShard,
                                                 transaction::CountType::Normal);
      if (opResult.fail() || _destroyed) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
      }
      info.numVertices = opResult.slice().getUInt();
      vCount += info.numVertices;
      if (info.numVertices == 0) {
        continue;
      }

      // distributeshardslike should cause the edges for a vertex to be
      // in the same shard index. x in vertexShard2 => E(x) in edgeShard2
      for (auto const& pair2 : edgeCollMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        if (vertexShards.size() != edgeShards.size()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, shardError);
        }

        ShardID const& eShard = edgeShards[i];
        info.edgeShards.push_back(eShard);

        cid = info.trx->addCollectionAtRuntime(eShard);
        info.trx->pinData(cid);  // will throw when it fails

        OperationResult opResult = info.trx->count(eShard, transaction::CountType::Normal);
        if (opResult.fail() || _destroyed) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
        }
        info.numEdges += opResult.slice().getUInt();
      }
      eCount += info.numEdges;

      result[pair.first].push_back(std::move(info));
    }
  }


  _index.resize(vCount);
  size_t requiredMem = vCount * _graphFormat->estimatedVertexSize() +
                       eCount * _graphFormat->estimatedEdgeSize();
  if (!_config->lazyLoading() && requiredMem > totalMemory / 2) {
    if (_graphFormat->estimatedVertexSize() > 0) {
      _vertexData = new MappedFileBuffer<V>(vCount);
    }
    _edges = new MappedFileBuffer<Edge<E>>(eCount);
  } else {
    if (_graphFormat->estimatedVertexSize() > 0) {
      _vertexData = new VectorTypedBuffer<V>(vCount);
    }
    _edges = new VectorTypedBuffer<Edge<E>>(eCount);
  }

  return result;
}

template <typename V, typename E>
void GraphStore<V, E>::loadShards(WorkerConfig* config,
                                  std::function<void()> const& callback) {
  _config = config;
  TRI_ASSERT(_runningThreads == 0);
  LOG_TOPIC(DEBUG, Logger::PREGEL) << "Using "
  << config->localVertexShardIDs().size()
  << " threads to load data";

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  rest::Scheduler* scheduler = SchedulerFeature::SCHEDULER;
  scheduler->queue(RequestPriority::LOW, [this, scheduler, callback] {

    // hold the current position where the ith vertex shard can
    // start to write its data. At the end the offset should equal the
    // sum of the counts of all ith edge shards
    auto collectionShards = _allocateSpace();


    uint64_t vertexOff = 0;
    std::vector<size_t> edgeDataOffsets; // will contain # off edges in ith shard
    for (auto& collection : collectionShards) {
      if (edgeDataOffsets.size() == 0) {
        edgeDataOffsets.resize(collection.second.size() + 1);
        std::fill(edgeDataOffsets.begin(), edgeDataOffsets.end(), 0);
      }
      TRI_ASSERT(collection.second.size() < edgeDataOffsets.size());
      size_t shardIdx = 0;
      for (VertexShardInfo& info : collection.second) {
        edgeDataOffsets[++shardIdx] += info.numEdges;
      }
    }

    for (auto& collection : collectionShards) {

      size_t shardIdx = 0;
      for (VertexShardInfo& info : collection.second) {

        try {
          // we might have already loaded these shards
          if (_loadedShards.find(info.vertexShard) != _loadedShards.end()) {
            continue;
          }
          _loadedShards.insert(info.vertexShard);
          _runningThreads++;
          TRI_ASSERT(info.numVertices > 0);
          TRI_ASSERT(vertexOff < _index.size());
          TRI_ASSERT(info.numEdges == 0 || edgeDataOffsets[shardIdx] < _edges->size());

          scheduler->queue(RequestPriority::LOW, [this, &info, &edgeDataOffsets, vertexOff, shardIdx] {
            TRI_DEFER(_runningThreads--);// exception safe
            _loadVertices(*info.trx, info.vertexShard, info.edgeShards,
                          vertexOff, edgeDataOffsets[shardIdx]);
            });
          // update to next offset
          vertexOff += info.numVertices;
        } catch(...) {
          LOG_TOPIC(WARN, Logger::PREGEL) << "unhandled exception while "
            <<"loading pregel graph";
        }

        shardIdx++;
      }

      // we can only load one vertex collection at a time
      while (_runningThreads > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(5000));
      }
    }

    scheduler->queue(RequestPriority::LOW, callback);
    });
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    std::string const& documentID) {
  if (!_vertexData) {
    _vertexData = new VectorTypedBuffer<V>(100);
  }
  if (!_edges) {
    _edges = new VectorTypedBuffer<Edge<E>>(100);
  }

  // figure out if we got this vertex locally
  PregelID _id = config->documentIdToPregel(documentID);
  if (config->isLocalVertexShard(_id.shard)) {
    loadDocument(config, _id.shard, _id.key);
  }
}

template <typename V, typename E>
void GraphStore<V, E>::loadDocument(WorkerConfig* config,
                                    PregelShard sourceShard,
                                    PregelKey const& _key) {
  _config = config;
  std::unique_ptr<transaction::Methods> trx(_createTransaction());
  TRI_ASSERT(_config->lazyLoading());

  ManagedDocumentResult mmdr;
  ShardID const& vertexShard = _config->globalShardIDs()[sourceShard];
  Result res = trx->documentFastPathLocal(vertexShard, StringRef(_key),
                                          mmdr, true);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackSlice doc(mmdr.vpack());
  std::string documentId = trx->extractIdString(doc);
  _index.emplace_back(sourceShard, _key);

  VertexEntry& entry = _index.back();
  if (_graphFormat->estimatedVertexSize() > 0) {
    entry._vertexDataOffset = _localVerticeCount;
    entry._edgeDataOffset = _localEdgeCount;

    // allocate space if needed
    if (_vertexData->size() <= _localVerticeCount) {
      // lazy loading always uses vector backed storage
      ((VectorTypedBuffer<V>*)_vertexData)->appendEmptyElement();
    }
    V* data = _vertexData->data() + _localVerticeCount;
    _graphFormat->copyVertexData(documentId, doc, data, sizeof(V));
    _localVerticeCount++;
  }

  // load edges
  std::map<CollectionID, std::vector<ShardID>> const& vertexMap =
      _config->vertexCollectionShards();
  std::map<CollectionID, std::vector<ShardID>> const& edgeMap =
      _config->edgeCollectionShards();
  for (auto const& pair : vertexMap) {
    std::vector<ShardID> const& vertexShards = pair.second;
    auto it = std::find(vertexShards.begin(), vertexShards.end(), vertexShard);
    if (it != vertexShards.end()) {
      size_t pos = (size_t)(it - vertexShards.begin());
      for (auto const& pair2 : edgeMap) {
        std::vector<ShardID> const& edgeShards = pair2.second;
        _loadEdges(*trx, edgeShards[pos], entry, documentId);
      }
      break;
    }
  }
  if (!trx->commit().ok()) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Pregel worker: Failed to commit on a read transaction";
  }
}

template <typename V, typename E>
RangeIterator<VertexEntry> GraphStore<V, E>::vertexIterator() {
  return vertexIterator(0, _index.size());
}

template <typename V, typename E>
RangeIterator<VertexEntry> GraphStore<V, E>::vertexIterator(size_t start,
                                                            size_t end) {
  return RangeIterator<VertexEntry>(_index.data() + start, end - start);
}

template <typename V, typename E>
V* GraphStore<V, E>::mutableVertexData(VertexEntry const* entry) {
  return _vertexData->data() + entry->_vertexDataOffset;
}

template <typename V, typename E>
void GraphStore<V, E>::replaceVertexData(VertexEntry const* entry, void* data,
                                         size_t size) {
  // if (size <= entry->_vertexDataOffset)
  void* ptr = _vertexData->data() + entry->_vertexDataOffset;
  memcpy(ptr, data, size);
  LOG_TOPIC(WARN, Logger::PREGEL)
      << "Don't use this function with varying sizes";
}

template <typename V, typename E>
RangeIterator<Edge<E>> GraphStore<V, E>::edgeIterator(
    VertexEntry const* entry) {
  return RangeIterator<Edge<E>>(_edges->data() + entry->_edgeDataOffset,
                                entry->_edgeCount);
}

template <typename V, typename E>
std::unique_ptr<transaction::Methods> GraphStore<V, E>::_createTransaction() {
  transaction::Options trxOpts;
  trxOpts.waitForSync = false;
  trxOpts.allowImplicitCollections = true;
  auto ctx =
    transaction::StandaloneContext::Create(_vocbaseGuard.database());
  auto trx = std::unique_ptr<transaction::Methods>(
                new transaction::Methods(ctx, {}, {}, {}, trxOpts));
  Result res = trx->begin();

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return trx;
}

template <typename V, typename E>
void GraphStore<V, E>::_loadVertices(transaction::Methods& trx,
                                     ShardID const& vertexShard,
                                     std::vector<ShardID> const& edgeShards,
                                     size_t vertexOffset,
                                     size_t& edgeOffset) {
  TRI_ASSERT(vertexOffset < _index.size());
  uint64_t originalVertexOffset = vertexOffset;

  PregelShard sourceShard = (PregelShard)_config->shardId(vertexShard);
  std::unique_ptr<OperationCursor> cursor =
    trx.indexScan(vertexShard, transaction::Methods::CursorType::ALL);

  if (cursor->fail()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code, "while looking up shard '%s'",
                                  vertexShard.c_str());
  }

  // tell the formatter the number of docs we are about to load
  LogicalCollection* collection = cursor->collection();
  uint64_t number = collection->numberDocuments(&trx, transaction::CountType::Normal);
  _graphFormat->willLoadVertices(number);

  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    if (slice.isExternal()) {
      slice = slice.resolveExternal();
    }
    VertexEntry& ventry = _index[vertexOffset];
    ventry._shard = sourceShard;
    ventry._key = transaction::helpers::extractKeyFromDocument(slice).copyString();
    ventry._edgeDataOffset = edgeOffset;

    // load vertex data
    std::string documentId = trx.extractIdString(slice);
    if (_graphFormat->estimatedVertexSize() > 0) {
      TRI_ASSERT(vertexOffset < _vertexData->size());
      ventry._vertexDataOffset = vertexOffset;
      V* ptr = _vertexData->data() + vertexOffset;
      _graphFormat->copyVertexData(documentId, slice, ptr, sizeof(V));
    }
    // load edges
    for (ShardID const& edgeShard : edgeShards) {
      _loadEdges(trx, edgeShard, ventry, documentId);
    }
    vertexOffset++;
    edgeOffset += ventry._edgeCount;
  };
  while (cursor->nextDocument(cb, 1000)) {
    if (_destroyed) {
      LOG_TOPIC(WARN, Logger::PREGEL) << "Aborted loading graph";
      break;
    }
  }

  // Add all new vertices
  _localVerticeCount += (vertexOffset - originalVertexOffset);

  if (!trx.commit().ok()) {
    LOG_TOPIC(WARN, Logger::PREGEL)
        << "Pregel worker: Failed to commit on a read transaction";
  }
}

template <typename V, typename E>
void GraphStore<V, E>::_loadEdges(transaction::Methods& trx,
                                  ShardID const& edgeShard,
                                  VertexEntry& vertexEntry,
                                  std::string const& documentID) {
  size_t added = 0;
  size_t offset = vertexEntry._edgeDataOffset + vertexEntry._edgeCount;
  // moving pointer to edge

  traverser::EdgeCollectionInfo info(&trx, edgeShard, TRI_EDGE_OUT,
                                     StaticStrings::FromString, 0);
  ManagedDocumentResult mmdr;
  std::unique_ptr<OperationCursor> cursor = info.getEdges(documentID, &mmdr);
  if (cursor->fail()) {
    THROW_ARANGO_EXCEPTION_FORMAT(cursor->code,
                                  "while looking up edges '%s' from %s",
                                  documentID.c_str(), edgeShard.c_str());
  }

  auto cb = [&](LocalDocumentId const& token, VPackSlice slice) {
    if (slice.isExternal()) {
      slice = slice.resolveExternal();
    }

    // If this is called from loadDocument we didn't preallocate the vector
    if (_edges->size() <= offset) {
      if (!_config->lazyLoading()) {
        LOG_TOPIC(ERR, Logger::PREGEL) << "Pregel did not preallocate enough "
                                       << "space for all edges. This hints "
                                       << "at a bug with collection count()";
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
      }
      // lazy loading always uses vector backed storage
      ((VectorTypedBuffer<Edge<E>>*)_edges)->appendEmptyElement();
    }

    std::string toValue = slice.get(StaticStrings::ToString).copyString();
    std::size_t pos = toValue.find('/');
    std::string collectionName = toValue.substr(0, pos);
    Edge<E>* edge = _edges->data() + offset;
    edge->_toKey = toValue.substr(pos + 1, toValue.length() - pos - 1);

    // resolve the shard of the target vertex.
    ShardID responsibleShard;
    int res =
        Utils::resolveShard(_config, collectionName, StaticStrings::KeyString,
                            edge->_toKey, responsibleShard);

    if (res == TRI_ERROR_NO_ERROR) {
      // PregelShard sourceShard = (PregelShard)_config->shardId(edgeShard);
      edge->_targetShard = (PregelShard)_config->shardId(responsibleShard);
      _graphFormat->copyEdgeData(slice, edge->data(), sizeof(E));
      if (edge->_targetShard != (PregelShard)-1) {
        added++;
        offset++;
      } else {
        LOG_TOPIC(ERR, Logger::PREGEL)
            << "Could not resolve target shard of edge";
      }
    } else {
      LOG_TOPIC(ERR, Logger::PREGEL)
          << "Could not resolve target shard of edge";
    }
  };
  while (cursor->nextDocument(cb, 1000)) {
    if (_destroyed) {
      LOG_TOPIC(WARN, Logger::PREGEL) << "Aborted loading graph";
      break;
    }
  }

  // Add up all added elements
  vertexEntry._edgeCount += added;
  _localEdgeCount += added;
}

/// Loops over the array starting a new transaction for different shards
/// Should not dead-lock unless we have to wait really long for other threads
template <typename V, typename E>
void GraphStore<V, E>::_storeVertices(std::vector<ShardID> const& globalShards,
                                      RangeIterator<VertexEntry>& it) {
  // transaction on one shard
  std::unique_ptr<transaction::Methods> trx;
  PregelShard currentShard = (PregelShard)-1;
  Result res = TRI_ERROR_NO_ERROR;

  V* vData = _vertexData->data();

  // loop over vertices
  while (it != it.end()) {
    if (it->shard() != currentShard) {
      if (trx) {
        res = trx->finish(res);

        if (!res.ok()) {
          THROW_ARANGO_EXCEPTION(res);
        }
      }

      currentShard = it->shard();

      ShardID const& shard = globalShards[currentShard];
      transaction::Options transactionOptions;

      transactionOptions.waitForSync = false;
      transactionOptions.allowImplicitCollections = false;
      trx.reset(new transaction::Methods(
        transaction::StandaloneContext::Create(_vocbaseGuard.database()),
        {},
        {shard},
        {},
        transactionOptions
      ));
      res = trx->begin();

      if (!res.ok()) {
        THROW_ARANGO_EXCEPTION(res);
      }
    }

    transaction::BuilderLeaser b(trx.get());

    b->openArray();

    size_t buffer = 0;

    while (it != it.end() && it->shard() == currentShard && buffer < 1000) {
      // This loop will fill a buffer of vertices until we run into a new
      // collection
      // or there are no more vertices for to store (or the buffer is full)
      V* data = vData + it->_vertexDataOffset;
      b->openObject();
      b->add(StaticStrings::KeyString, VPackValue(it->key()));
      /// bool store =
      _graphFormat->buildVertexDocument(*(b.get()), data, sizeof(V));
      b->close();

      ++it;
      ++buffer;
    }
    b->close();
    if (_destroyed) {
      LOG_TOPIC(WARN, Logger::PREGEL)
          << "Storing data was canceled prematurely";
      trx->abort();
      trx.reset();
      break;
    }

    ShardID const& shard = globalShards[currentShard];
    OperationOptions options;
    OperationResult result = trx->update(shard, b->slice(), options);
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result.result);
    }
  }

  if (trx) {
    res = trx->finish(res);
    if (!res.ok()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
}

template <typename V, typename E>
void GraphStore<V, E>::storeResults(WorkerConfig* config,
                                    std::function<void()> const& cb) {
  _config = config;

  double now = TRI_microtime();
  size_t total = _index.size();
  size_t delta = _index.size() / _config->localVertexShardIDs().size();
  if (delta < 1000) {
    delta = _index.size();
  }
  size_t start = 0, end = delta;

  TRI_ASSERT(SchedulerFeature::SCHEDULER != nullptr);
  do {
    _runningThreads++;
    SchedulerFeature::SCHEDULER->queue(RequestPriority::LOW, [this, start, end, now, cb] {
      try {
        RangeIterator<VertexEntry> it = vertexIterator(start, end);
        _storeVertices(_config->globalShardIDs(), it);
        // TODO can't just write edges with smart graphs
      } catch (...) {
        LOG_TOPIC(ERR, Logger::PREGEL) << "Storing vertex data failed";
      }
      _runningThreads--;
      if (_runningThreads == 0) {
        LOG_TOPIC(DEBUG, Logger::PREGEL) << "Storing data took "
                                        << (TRI_microtime() - now) << "s";
        cb();
      }
      });
    start = end;
    end = end + delta;
    if (total < end + delta) {  // swallow the rest
      end = total;
    }
  } while (start != end);
}

template class arangodb::pregel::GraphStore<int64_t, int64_t>;
template class arangodb::pregel::GraphStore<float, float>;
template class arangodb::pregel::GraphStore<double, float>;
template class arangodb::pregel::GraphStore<double, double>;

// specific algo combos
template class arangodb::pregel::GraphStore<SCCValue, int8_t>;
template class arangodb::pregel::GraphStore<ECValue, int8_t>;
template class arangodb::pregel::GraphStore<HITSValue, int8_t>;
template class arangodb::pregel::GraphStore<DMIDValue, float>;
template class arangodb::pregel::GraphStore<LPValue, int8_t>;
template class arangodb::pregel::GraphStore<SLPAValue, int8_t>;
