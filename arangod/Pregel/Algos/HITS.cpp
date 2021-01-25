////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "HITS.h"
#include <cmath>
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/IncomingCache.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb;
using namespace arangodb::pregel;
using namespace arangodb::pregel::algos;

static std::string const kAuthNorm = "auth";
static std::string const kHubNorm = "hub";

struct HITSWorkerContext : public WorkerContext {
  HITSWorkerContext() {}

  double authNormRoot = 0;
  double hubNormRoot = 0;

  void preGlobalSuperstep(uint64_t gss) override {
    double const* authNorm = getAggregatedValue<double>(kAuthNorm);
    double const* hubNorm = getAggregatedValue<double>(kHubNorm);
    authNormRoot = std::sqrt(*authNorm);
    hubNormRoot = std::sqrt(*hubNorm);
  }
};

struct HITSComputation
    : public VertexComputation<HITSValue, int8_t, SenderMessage<double>> {
  HITSComputation() {}

  void compute(MessageIterator<SenderMessage<double>> const& messages) override {
    double auth = 0.0;
    double hub = 0.0;
    // we don't know our incoming neighbours in step 0, therfore we need step 0
    // as 'initialization' before actually starting to converge
    if (globalSuperstep() <= 1) {
      auth = 1.0;
      hub = 1.0;
    } else {
      HITSWorkerContext const* ctx = static_cast<HITSWorkerContext const*>(context());
      for (SenderMessage<double> const* message : messages) {
        // we don't put a valid shard id into the messages FROM
        // our outgoing messages
        if (message->senderId.isValid()) {
          auth += message->value;  // hub from incoming Neighbors
        } else {
          hub += message->value;  // auth from our outgoing Neighbors
        }
      }

      auth /= ctx->authNormRoot;
      hub /= ctx->hubNormRoot;
      mutableVertexData()->authorityScore = auth;
      mutableVertexData()->hubScore = hub;
    }
    aggregate<double>(kAuthNorm, hub * hub);
    aggregate<double>(kHubNorm, auth * auth);

    // no sender required, the senders have an outgoing edge to us
    SenderMessage<double> authData(PregelID(), auth);
    for (SenderMessage<double> const* message : messages) {
      if (message->senderId.isValid()) {  // send to incoming Neighbors
        sendMessage(message->senderId, authData);
      }
    }
    SenderMessage<double> hubData(this->pregelId(), hub);
    sendMessageToAllNeighbours(hubData);
  }
};

VertexComputation<HITSValue, int8_t, SenderMessage<double>>* HITS::createComputation(
    WorkerConfig const* config) const {
  return new HITSComputation();
}

struct HITSGraphFormat : public GraphFormat<HITSValue, int8_t> {
  const std::string _resultField;

  explicit HITSGraphFormat(application_features::ApplicationServer& server,
                           std::string const& result)
      : GraphFormat<HITSValue, int8_t>(server), _resultField(result) {}

  size_t estimatedEdgeSize() const override { return 0; }

  void copyVertexData(arangodb::velocypack::Options const&, std::string const& /*documentId*/,
                      arangodb::velocypack::Slice /*document*/, HITSValue& /*targetPtr*/,
                      uint64_t& /*vertexIdRange*/) override {}

  bool buildVertexDocument(arangodb::velocypack::Builder& b, HITSValue const* value) const override {
    b.add(_resultField + "_auth", VPackValue(value->authorityScore));
    b.add(_resultField + "_hub", VPackValue(value->hubScore));
    return true;
  }
};

GraphFormat<HITSValue, int8_t>* HITS::inputFormat() const {
  return new HITSGraphFormat(_server, _resultField);
}

WorkerContext* HITS::workerContext(VPackSlice userParams) const {
  return new HITSWorkerContext();
}

struct HITSMasterContext : public MasterContext {
  HITSMasterContext() : authNorm(0), hubNorm(0) {}

  double authNorm;
  double hubNorm;

  bool postGlobalSuperstep() override {
    double const* an = getAggregatedValue<double>(kAuthNorm);
    double const* hn = getAggregatedValue<double>(kHubNorm);
    double diff = std::max(std::abs(authNorm - *an), std::abs(hubNorm - *hn));
    bool converged = globalSuperstep() > 2 && (diff < 0.00001);
    authNorm = *an;
    hubNorm = *hn;
    // might fail on small very sparse / disconnected graphs
    return authNorm != 0 && hubNorm != 0 && !converged;
  }
};

MasterContext* HITS::masterContext(VPackSlice userParams) const {
  return new HITSMasterContext();
}

IAggregator* HITS::aggregator(std::string const& name) const {
  if (name == kHubNorm || name == kAuthNorm) {
    return new SumAggregator<double>(false);  // non perm
  }
  return nullptr;
}
