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

#include "SCC.h"
#include "Pregel/VertexComputation.h"
#include "Vocbase/vocbase.h"
#include "Cluster/ClusterInfo.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static int64_t _shardCount(TRI_vocbase_t* vocbase, ShardID const& shard) {
    SingleCollectionTransaction
trx(StandaloneTransactionContext::Create(vocbase),
                                    shard,
                                    TRI_TRANSACTION_READ);

    int res = trx.begin();
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
    }
    OperationResult opResult = trx.count(shard);
    res = trx.finish(opResult.code);
    if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION (res);
    }
    VPackSlice s = opResult.slice();
    TRI_ASSERT(s.isNumber());
    return s.getInt();
}

size_t SCCAlgorithm::estimatedVertexSize() const { return sizeof(int64_t); }

struct SCCGraphFormat : public GraphFormat<int64_t, int64_t> {
  uint64_t currentId = 0;
  public:
     void willUseCollection(TRI_vocbase_t *vocbase, std::string const& shard, bool isEdgeCollection) override {
       int64_t count = _shardCount(vocbase, shard);
       currentId = ClusterInfo::instance()->uniqid(count);
     }
  IntegerGraphFormat(std::string const& field, int64_t vertexNull,
                     int64_t edgeNull)
      : _field(field), _vDefault(vertexNull), _eDefault(edgeNull) {}

  size_t copyVertexData(VPackSlice document, void* targetPtr,
                        size_t maxSize) const override {
    VPackSlice val = document.get(_field);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _vDefault;
    return sizeof(int64_t);
  }

  size_t copyEdgeData(VPackSlice document, void* targetPtr,
                      size_t maxSize) const override {
    VPackSlice val = document.get(_field);
    *((int64_t*)targetPtr) = val.isInteger() ? val.getInt() : _eDefault;
    return sizeof(int64_t);
  }

  int64_t readVertexData(void* ptr) const override { return *((int64_t*)ptr); }
  int64_t readEdgeData(void* ptr) const override { return *((int64_t*)ptr); }
};

std::shared_ptr<GraphFormat<int64_t, int64_t>> SCCAlgorithm::inputFormat()
    const {
  return std::make_shared<IntegerGraphFormat>("value", -1, 1);
}

std::shared_ptr<MessageFormat<int64_t>> SCCAlgorithm::messageFormat() const {
  return std::shared_ptr<IntegerMessageFormat>(new IntegerMessageFormat());
}

std::shared_ptr<MessageCombiner<int64_t>> SCCAlgorithm::messageCombiner()
    const {
  return std::shared_ptr<MinIntegerCombiner>(new MinIntegerCombiner());
}

struct SCCComputation : public VertexComputation<int64_t, int64_t, int64_t> {
  SCCComputation() {}
  void compute(std::string const& vertexID,
               MessageIterator<int64_t> const& messages) override {
    /*int64_t tmp = vertexData();
    for (const int64_t* msg : messages) {
      if (tmp < 0 || *msg < tmp) {
        tmp = *msg;
      };
    }
    int64_t* state = mutableVertexData();
    if (tmp >= 0 && (getGlobalSuperstep() == 0 || tmp != *state)) {
      LOG(INFO) << "Recomputing value for vertex " << vertexID;
      *state = tmp;  // update state

      EdgeIterator<int64_t> edges = getEdges();
      for (EdgeEntry<int64_t>* edge : edges) {
        int64_t val = *(edge->getData()) + tmp;
        uint64_t toID = edge->toPregelID();
        sendMessage(toID, val);
      }
    }*/
    voteHalt();
  }
};

std::shared_ptr<VertexComputation<int64_t, int64_t, int64_t>>
SCCAlgorithm::createComputation() const {
  return std::shared_ptr<SCCComputation>(new SCCComputation());
}
