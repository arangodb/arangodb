////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "HITSKleinberg.h"
#include <cmath>
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/GraphStore.h"
#include "Pregel/MasterContext.h"
#include "Pregel/VertexComputation.h"

using namespace arangodb::pregel::algos;
using namespace arangodb;
using namespace arangodb::pregel;

static std::string const authAggregator = "auth";
static std::string const hubAggregator = "hub";
static std::string const maxDiffAuthAggregator = "diffA";
static std::string const maxDiffHubAggregator = "diffH";
static std::string const isLastIterationAggregator = "stop";

using VertexType = HITSKleinbergValue;

static double const epsilon = 0.00001;

enum class State {
  sendInitialHubs,
  updateAuth,
  updateAuthNormalizeHub,
  updateHubNormalizeAuth,
  finallyNormalizeHubs,
  finallyNormalizeAuths
};

struct HITSKleinbergWorkerContext : public WorkerContext {
  HITSKleinbergWorkerContext(size_t maxGSS, size_t numIterations)
      : numIterations(numIterations){};

  double authDivisor = 0;
  double hubDivisor = 0;
  State state = State::sendInitialHubs;
  size_t numIterations;
  size_t currentIteration = 0;

  void preGlobalSuperstep(uint64_t gss) override {
    // todo: we only need this if the global super-step is odd
    //  difficulty: the parent class WorkerContext doesn't know the current gss
    auto const* authNorm = getAggregatedValue<double>(authAggregator);
    auto const* hubNorm = getAggregatedValue<double>(hubAggregator);
    authDivisor = std::sqrt(*authNorm);
    hubDivisor = std::sqrt(*hubNorm);

    double const authMaxDiff =
        *getAggregatedValue<double>(maxDiffAuthAggregator);
    double const hubMaxDiff = *getAggregatedValue<double>(maxDiffHubAggregator);
    const double diff = std::max(authMaxDiff, hubMaxDiff);

    if (diff < epsilon) {
      if (state == State::updateAuthNormalizeHub) {
        state = State::finallyNormalizeHubs;
      } else if (state == State::updateHubNormalizeAuth) {
        state = State::finallyNormalizeAuths;
      }
    }
  }

  void postGlobalSuperstep(uint64_t gss) override {
    switch (state) {
      case State::sendInitialHubs:
        state = State::updateAuth;
        break;
      case State::updateAuth:
      case State::updateAuthNormalizeHub:
        state = State::updateHubNormalizeAuth;
        break;
      case State::updateHubNormalizeAuth:
        ++currentIteration;
        if (currentIteration == numIterations) {
          state = State::finallyNormalizeHubs;
        } else {
          state = State::updateAuthNormalizeHub;
        }
        break;
      case State::finallyNormalizeHubs:
      case State::finallyNormalizeAuths:
        aggregate(isLastIterationAggregator, true);
        break;
    }
  }
};

struct HITSKleinbergComputation
    : public VertexComputation<VertexType, int8_t, SenderMessage<double>> {
  HITSKleinbergComputation() = default;

  void sendAuthToInNeighbors(
      MessageIterator<SenderMessage<double>> const& receivedMessages,
      double auth) {
    SenderMessage<double> authData(this->pregelId(), auth);
    for (SenderMessage<double> const* message : receivedMessages) {
      sendMessage(message->senderId, authData);
    }
  }

  void sendHubToOutNeighbors(double hub) {
    SenderMessage<double> hubData(this->pregelId(), hub);
    sendMessageToAllNeighbours(hubData);
  }

  void compute(
      MessageIterator<SenderMessage<double>> const& messages) override {
    auto const* ctx =
        dynamic_cast<HITSKleinbergWorkerContext const*>(context());
    switch (ctx->state) {
      case State::sendInitialHubs: {
        const double auth = 1.0;
        const double hub = 1.0;

        // these are not normalized, but according to the description of the
        // algorithm in the paper, the 1.0's are used in place of normalized
        // values
        mutableVertexData()->normalizedAuth = auth;  // needed for the next diff
        mutableVertexData()->normalizedHub = hub;
        reportFakeDifference();

        sendHubToOutNeighbors(hub);
        break;
      }

      case State::updateAuth: {
        // We enter this state when all authorities and all hubs are 1.0
        updateStoreAndSendAuth(messages, 1.0);
        reportFakeDifference();
        // authorities are one iteration before hubs
        // authorities are not normalized, hubs are 1.0

        // Next state: updateHubNormalizeAuth
        break;
      }

      case State::updateHubNormalizeAuth: {
        // authorities are updated one iteration more than hubs,
        // authorities are not normalized, hubs are normalized

        // update local hub
        double nonNormalizedHub = 0.0;
        for (SenderMessage<double> const* message : messages) {
          nonNormalizedHub += message->value;  // auth from our out-neighbors
        }
        mutableVertexData()->nonNormalizedHub = nonNormalizedHub;

        aggregate<double>(hubAggregator, nonNormalizedHub * nonNormalizedHub);

        // normalize auth (hubDivisor is not ready yet, cannot normalize hubs)
        const auto nonNormalizedAuth = mutableVertexData()->nonNormalizedAuth;
        const auto normalizedUpdatedAuth = nonNormalizedAuth / ctx->authDivisor;
        const auto diff =
            fabs(mutableVertexData()->normalizedAuth - normalizedUpdatedAuth);
        mutableVertexData()->normalizedAuth = normalizedUpdatedAuth;

        aggregate<double>(maxDiffAuthAggregator, diff);
        sendHubToOutNeighbors(nonNormalizedHub);

        // authorities and hubs are updated to the same iteration,
        // authorities are normalized, hubs are not normalized

        // Next state: updateAuthNormalizeHub or finallyNormalizeHubs
        break;
      }

      case State::updateAuthNormalizeHub: {
        // authorities and hubs are updated to the same iteration,
        // authorities are normalized, hubs are not normalized
        updateStoreAndSendAuth(messages, ctx->hubDivisor);
        const auto nonNormalizedHub = mutableVertexData()->nonNormalizedHub;
        const auto normalizedUpdatedHub = nonNormalizedHub / ctx->hubDivisor;
        const auto diff =
            fabs(mutableVertexData()->normalizedHub - normalizedUpdatedHub);
        mutableVertexData()->normalizedHub = normalizedUpdatedHub;
        aggregate<double>(maxDiffHubAggregator, diff);

        // authorities are updated one iteration more than hubs,
        // authorities are not normalized, hubs are normalized

        // next state: updateHubNormalizeAuth
        break;
      }

      case State::finallyNormalizeHubs:
        // authorities and hubs are updated to the same iteration,
        // authorities are normalized, hubs are not normalized
        // last iteration
        mutableVertexData()->normalizedHub =
            mutableVertexData()->nonNormalizedHub / ctx->hubDivisor;
        break;

      case State::finallyNormalizeAuths:
        // authorities and hubs are updated to the same iteration,
        // authorities are not normalized, hubs are normalized
        // last iteration
        mutableVertexData()->normalizedAuth =
            mutableVertexData()->nonNormalizedAuth / ctx->authDivisor;
        break;
    }
  }

 private:
  // At the beginning, we don't have differences between the current and the
  // previous values yet. If we don't report any difference, the default
  // difference 0 will be taken and the process terminates.
  void reportFakeDifference() {
    // so that 0 != diff < epsilon
    aggregate(maxDiffAuthAggregator, epsilon + 1.0);
  }

  void updateStoreAndSendAuth(
      MessageIterator<SenderMessage<double>> const& messages,
      double hubDivisor) {
    // compute the new auth
    double auth = 0.0;
    for (SenderMessage<double> const* message : messages) {
      auth += message->value / hubDivisor;  // normalized hub from in-neighbors
    }
    // note: auth are preliminary values of an iteration of the
    // HITS algorithm, the correct ranks will be obtained in the next
    // global super-step when the norms based on the current values are
    // computed and we can divide the current values by the norms to
    // obtain the scores of the iteration

    mutableVertexData()->nonNormalizedAuth = auth;
    aggregate<double>(authAggregator, auth * auth);
    sendAuthToInNeighbors(messages, auth);
  }
};

VertexComputation<VertexType, int8_t, SenderMessage<double>>*
HITSKleinberg::createComputation(WorkerConfig const* config) const {
  return new HITSKleinbergComputation();
}

struct HITSKleinbergGraphFormat : public GraphFormat<VertexType, int8_t> {
  const std::string _resultField;

  explicit HITSKleinbergGraphFormat(
      application_features::ApplicationServer& server,
      std::string const& result)
      : GraphFormat<VertexType, int8_t>(server), _resultField(result) {}

  size_t estimatedEdgeSize() const override { return 0; }

  void copyVertexData(arangodb::velocypack::Options const&,
                      std::string const& /*documentId*/,
                      arangodb::velocypack::Slice /*document*/,
                      VertexType& /*targetPtr*/,
                      uint64_t& /*vertexIdRange*/) override {}

  bool buildVertexDocument(arangodb::velocypack::Builder& b,
                           VertexType const* value) const override {
    b.add(_resultField + "_auth", VPackValue(value->normalizedAuth));
    b.add(_resultField + "_hub", VPackValue(value->normalizedHub));
    return true;
  }
};

GraphFormat<VertexType, int8_t>* HITSKleinberg::inputFormat() const {
  return new HITSKleinbergGraphFormat(_server, _resultField);
}

WorkerContext* HITSKleinberg::workerContext(VPackSlice userParams) const {
  return new HITSKleinbergWorkerContext(maxGSS, numIterations);
}

struct HITSKleinbergMasterContext : public MasterContext {
  HITSKleinbergMasterContext() = default;

  bool postGlobalSuperstep() override {
    double const authMaxDiff =
        *getAggregatedValue<double>(maxDiffAuthAggregator);
    double const hubMaxDiff = *getAggregatedValue<double>(maxDiffHubAggregator);
    const double diff = std::max(authMaxDiff, hubMaxDiff);

    bool converged = diff < epsilon;

    // default (if no messages have been sent) is false
    bool const stop = *getAggregatedValue<bool>(isLastIterationAggregator);
    return !converged && !stop;
  }
};

MasterContext* HITSKleinberg::masterContext(VPackSlice userParams) const {
  return new HITSKleinbergMasterContext();
}

IAggregator* HITSKleinberg::aggregator(std::string const& name) const {
  if (name == hubAggregator || name == authAggregator) {
    return new SumAggregator<double>(false);  // non-permanent
  }
  if (name == maxDiffAuthAggregator) {
    return new MaxAggregator<double>(false);  // non-permanent
  }
  if (name == maxDiffHubAggregator) {
    return new MaxAggregator<double>(false);  // non-permanent
  }
  // this is a temporary hack (until MasterContext is available from the
  // algorithm) to report from WorkerContext to MasterContext that we have to
  // stop
  if (name == isLastIterationAggregator) {
    return new BoolOrAggregator(false);  // non-permanent
  }
  return nullptr;
}
