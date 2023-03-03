////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Roman Rabinovich
////////////////////////////////////////////////////////////////////////////////

#include "HITSKleinberg.h"
#include <cmath>
#include <utility>
#include "Pregel/Aggregator.h"
#include "Pregel/Algorithm.h"
#include "Pregel/Worker/GraphStore.h"
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

namespace {
double const epsilon = 0.00001;

/**
 * If userParams has a threshold value, return it, otherwise return epsilon.
 * @param userParams
 * @return
 */
double getThreshold(VPackSlice userParams) {
  if (userParams.hasKey(Utils::threshold)) {
    if (!userParams.get(Utils::threshold).isNumber()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The threshold parameter should be a number.");
    }
    return userParams.get(Utils::threshold).getNumber<double>();
  }
  return epsilon;
}
}  // namespace

enum class State {
  sendInitialHubs,
  updateAuth,
  updateAuthNormalizeHub,
  updateHubNormalizeAuth,
  finallyNormalizeHubs,
  finallyNormalizeAuths
};

struct HITSKleinbergWorkerContext : public WorkerContext {
  HITSKleinbergWorkerContext(size_t maxGSS, size_t numIterations,
                             double threshold)
      : numIterations(numIterations), threshold(threshold){};

  double authDivisor = 0;
  double hubDivisor = 0;
  State state = State::sendInitialHubs;
  size_t numIterations;
  size_t currentIteration = 0;
  double const threshold;

  void preGlobalSuperstep(uint64_t gss) override {
    // todo: we only need this if the global super-step is odd
    //  difficulty: the parent class WorkerContext doesn't know the current gss
    auto const* authNorm = getAggregatedValue<double>(authAggregator);
    auto const* hubNorm = getAggregatedValue<double>(hubAggregator);
    ADB_PROD_ASSERT(authNorm != nullptr);
    ADB_PROD_ASSERT(hubNorm != nullptr);
    authDivisor = std::sqrt(*authNorm);
    hubDivisor = std::sqrt(*hubNorm);

    double const authMaxDiff =
        *getAggregatedValue<double>(maxDiffAuthAggregator);
    double const hubMaxDiff = *getAggregatedValue<double>(maxDiffHubAggregator);
    double const diff = std::max(authMaxDiff, hubMaxDiff);

    if (diff < threshold) {
      if (state == State::updateAuthNormalizeHub) {
        state = State::finallyNormalizeHubs;
      } else if (state == State::updateHubNormalizeAuth) {
        state = State::finallyNormalizeAuths;
      }
    }
  }

  void postGlobalSuperstep(uint64_t gss) override {
    switch (state) {
      case State::sendInitialHubs: {
        state = State::updateAuth;
      } break;
      case State::updateAuth:
      case State::updateAuthNormalizeHub: {
        state = State::updateHubNormalizeAuth;
      } break;
      case State::updateHubNormalizeAuth: {
        ++currentIteration;
        if (currentIteration == numIterations) {
          state = State::finallyNormalizeHubs;
        } else {
          state = State::updateAuthNormalizeHub;
        }
      } break;
      case State::finallyNormalizeHubs:
      case State::finallyNormalizeAuths: {
        aggregate(isLastIterationAggregator, true);
      } break;
    }
  }
};

struct HITSKleinbergComputation
    : public VertexComputation<VertexType, int8_t, SenderMessage<double>> {
  HITSKleinbergComputation() = default;

  // sends auth to all in-neighbors. note that all vertices send messages
  // to all out-neighbors in all iterations (there are no inactive vertices),
  // so the set of in-neighbors can be determined by iterating over received
  // messages
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
        // these are not normalized, but according to the description of the
        // algorithm in the paper, the 1.0's are used in place of normalized
        // values
        mutableVertexData()->normalizedAuth = 1.0;
        mutableVertexData()->normalizedHub = 1.0;
        reportFakeDifference(ctx->threshold);

        sendHubToOutNeighbors(1.0);
      } break;

      case State::updateAuth: {
        // We enter this state when all authorities and all hubs are 1.0
        updateStoreAndSendAuth(messages, 1.0);
        reportFakeDifference(ctx->threshold);
        // authorities are one iteration before hubs
        // authorities are not normalized, hubs are 1.0

        // Next state: updateHubNormalizeAuth
      } break;

      case State::updateHubNormalizeAuth: {
        // authorities are updated one iteration more than hubs,
        // authorities are not normalized, hubs are normalized

        // update local hub
        double nonNormalizedHub = 0.0;
        for (SenderMessage<double> const* message : messages) {
          TRI_ASSERT(message != nullptr);
          nonNormalizedHub += message->value;  // auth from our out-neighbors
        }
        mutableVertexData()->nonNormalizedHub = nonNormalizedHub;

        aggregate<double>(hubAggregator, nonNormalizedHub * nonNormalizedHub);

        // normalize auth (hubDivisor is not ready yet, cannot normalize hubs)
        auto const nonNormalizedAuth = mutableVertexData()->nonNormalizedAuth;
        auto const normalizedUpdatedAuth = nonNormalizedAuth / ctx->authDivisor;
        auto const diff =
            fabs(mutableVertexData()->normalizedAuth - normalizedUpdatedAuth);
        mutableVertexData()->normalizedAuth = normalizedUpdatedAuth;

        aggregate<double>(maxDiffAuthAggregator, diff);
        sendHubToOutNeighbors(nonNormalizedHub);

        // authorities and hubs are updated to the same iteration,
        // authorities are normalized, hubs are not normalized

        // Next state: updateAuthNormalizeHub or finallyNormalizeHubs
      } break;

      case State::updateAuthNormalizeHub: {
        // authorities and hubs are updated to the same iteration,
        // authorities are normalized, hubs are not normalized
        updateStoreAndSendAuth(messages, ctx->hubDivisor);
        auto const nonNormalizedHub = mutableVertexData()->nonNormalizedHub;
        auto const normalizedUpdatedHub = nonNormalizedHub / ctx->hubDivisor;
        auto const diff =
            fabs(mutableVertexData()->normalizedHub - normalizedUpdatedHub);
        mutableVertexData()->normalizedHub = normalizedUpdatedHub;
        aggregate<double>(maxDiffHubAggregator, diff);

        // authorities are updated one iteration more than hubs,
        // authorities are not normalized, hubs are normalized

        // next state: updateHubNormalizeAuth
      } break;

      case State::finallyNormalizeHubs: {
        // authorities and hubs are updated to the same iteration,
        // authorities are normalized, hubs are not normalized
        // last iteration
        mutableVertexData()->normalizedHub =
            mutableVertexData()->nonNormalizedHub / ctx->hubDivisor;
      } break;

      case State::finallyNormalizeAuths: {
        // authorities and hubs are updated to the same iteration,
        // authorities are not normalized, hubs are normalized
        // last iteration
        mutableVertexData()->normalizedAuth =
            mutableVertexData()->nonNormalizedAuth / ctx->authDivisor;
      } break;
    }
  }

 private:
  // At the beginning, we don't have differences between the current and the
  // previous values yet. If we don't report any difference, the default
  // difference 0 will be taken and the process terminates.
  void reportFakeDifference(double threshold) {
    // so that 0 != diff < threshold
    aggregate(maxDiffAuthAggregator, threshold + 1000.0);
  }

  void updateStoreAndSendAuth(
      MessageIterator<SenderMessage<double>> const& messages,
      double hubDivisor) {
    // compute the new auth
    double auth = 0.0;
    for (SenderMessage<double> const* message : messages) {
      TRI_ASSERT(message != nullptr);
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
  std::string const _resultField;

  explicit HITSKleinbergGraphFormat(
      application_features::ApplicationServer& server, std::string result)
      : GraphFormat<VertexType, int8_t>(server),
        _resultField(std::move(result)) {}

  [[nodiscard]] size_t estimatedEdgeSize() const override { return 0; }

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
  double const threshold = getThreshold(userParams);
  return new HITSKleinbergWorkerContext(maxGSS, numIterations, threshold);
}

struct HITSKleinbergMasterContext : public MasterContext {
  double const threshold;

  explicit HITSKleinbergMasterContext(VPackSlice userParams)
      : threshold(getThreshold(userParams)) {}

  bool postGlobalSuperstep() override {
    double const authMaxDiff =
        *getAggregatedValue<double>(maxDiffAuthAggregator);
    double const hubMaxDiff = *getAggregatedValue<double>(maxDiffHubAggregator);
    double const diff = std::max(authMaxDiff, hubMaxDiff);

    bool const converged = diff < threshold;

    // default (if no messages have been sent) is false
    bool const stop = *getAggregatedValue<bool>(isLastIterationAggregator);
    return !converged && !stop;
  }
};

MasterContext* HITSKleinberg::masterContext(VPackSlice userParams) const {
  return new HITSKleinbergMasterContext(userParams);
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
