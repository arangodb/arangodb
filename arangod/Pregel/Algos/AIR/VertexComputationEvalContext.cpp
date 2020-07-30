////////////////////////////////////////////////////////////////////////////////
///
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
///
////////////////////////////////////////////////////////////////////////////////

#include "VertexComputationEvalContext.h"

#include "VertexComputation.h"
#include "AccumulatorAggregator.h"

using namespace arangodb::pregel;
using namespace arangodb::greenspun;

namespace arangodb::pregel::algos::accumulators {

VertexComputationEvalContext::VertexComputationEvalContext(VertexComputation& computation)
    : _computation(computation){};

VertexComputation& VertexComputationEvalContext::computation() const {
  return _computation;
}

VertexData& VertexComputationEvalContext::vertexData() const {
  return computation().vertexData();
}

std::string const& VertexComputationEvalContext::getThisId() const {
  return computation().vertexData()._documentId;
}

std::size_t VertexComputationEvalContext::getVertexUniqueId() const {
  return vertexData()._vertexId;
}

void VertexComputationEvalContext::printCallback(const std::string& msg) const {
  LOG_DEVEL << msg;
}

EvalResult VertexComputationEvalContext::getAccumulatorValue(std::string_view accumId,
                                                             VPackBuilder& builder) const {
  if (auto iter = vertexData()._accumulators.find(std::string{accumId});
      iter != std::end(vertexData()._accumulators)) {
    iter->second->getValueIntoBuilder(builder);
    return {};
  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    auto accum = _computation.getAggregatedValue<VertexAccumulatorAggregator>(globalName);
    if (accum != nullptr) {
      accum->getAccumulator().getValueIntoBuilder(builder);
      return {};
    }
  }
  return EvalError("accumulator `" + std::string{accumId} + "` not found");
}

EvalResult VertexComputationEvalContext::setAccumulator(std::string_view accumId,
                                                        VPackSlice value) {
  // FIXME: oh. my. god. in 2020. we copy string views.
  if (auto iter = vertexData()._accumulators.find(std::string{accumId});
      iter != std::end(vertexData()._accumulators)) {
    iter->second->setBySlice(value);
    return {};
  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    auto accum = _computation.getAggregatedValue<VertexAccumulatorAggregator>(globalName);
    if (accum != nullptr) {
      accum->getAccumulator().updateByMessageSlice(value);
      return {};
    }
  }

  return EvalError("accumulator `" + std::string{accumId} + "` not found");
}

EvalResult VertexComputationEvalContext::getPregelId(VPackBuilder& result) const {
  auto id = _computation.pregelId();
  {
    VPackObjectBuilder ob(&result);
    result.add("key", VPackValue(id.key));
    result.add("shard", VPackValue(id.shard));
  }
  return {};
}

EvalResult VertexComputationEvalContext::updateAccumulator(std::string_view accumId,
                                                           std::string_view toId,
                                                           VPackSlice value) {
  auto const& accums = _computation.algorithm().options().vertexAccumulators;
  if (auto i = accums.find(std::string{accumId}); i != accums.end()) {
    MessageData msg;
    msg.reset(std::string{accumId}, value, getThisId());

    // FIXME: this is extremely stupid to do,
    //        once we have proper variables, we should
    //        carry Edge with us and use that directly
    auto edgeIter = _computation.getEdges();
    for (; edgeIter.hasMore(); ++edgeIter) {
      auto edge = *edgeIter;
      if (edge->data()._toId == toId) {
        _computation.sendMessage(edge, msg);
        return {};
      }
    }

  } else {
    std::string globalName = "[global]-";
    globalName += accumId;
    _computation.aggregate<VPackSlice>(globalName, value);
    return {};
  }

  return EvalError("vertex accumulator `" + std::string{accumId} + "` not found");
}

EvalResult VertexComputationEvalContext::updateAccumulatorById(std::string_view accumId,
                                                               VPackSlice toVertex,
                                                               VPackSlice value) {
  MessageData msg;
  msg.reset(std::string{accumId}, value, getThisId());

  const auto pregelIdFromSlice = [](VPackSlice slice) -> PregelID {
    if (slice.isObject()) {
      VPackSlice key = slice.get("key");
      VPackSlice shard = slice.get("shard");
      if (key.isString() && shard.isNumber<PregelShard>()) {
        return PregelID(shard.getNumber<PregelShard>(), key.copyString());
      }
    }

    return {};
  };

  PregelID id = pregelIdFromSlice(toVertex);
  _computation.sendMessage(id, msg);
  return {};
}

EvalResult VertexComputationEvalContext::enumerateEdges(
    std::function<EvalResult(VPackSlice edge)> cb) const {
  RangeIterator<Edge<EdgeData>> edgeIter = _computation.getEdges();
  for (; edgeIter.hasMore(); ++edgeIter) {
    VPackSlice edgeDoc = (*edgeIter)->data()._document.slice();
    if (auto e = cb(edgeDoc); e.fail()) {
      return e.wrapError(
          [&](EvalError& err) { err.wrapMessage("during edge enumeration"); });
    }
  }

  return {};
}

EvalResult VertexComputationEvalContext::getBindingValue(std::string_view id,
                                                         VPackBuilder& result) const {
  if (_computation.algorithm().getBindParameter(id, result)) {
    return {};
  }
  return EvalError("bind parameter `" + std::string{id} + "` not found");
}

EvalResult VertexComputationEvalContext::getGlobalSuperstep(VPackBuilder& result) const {
  result.add(VPackValue(_computation.phaseGlobalSuperstep()));
  return {};
}

EvalResult VertexComputationEvalContext::getVertexCount(VPackBuilder& result) const {
  result.add(VPackValue(computation().context()->vertexCount()));
  return {};
}

EvalResult VertexComputationEvalContext::getOutgoingEdgesCount(VPackBuilder& result) const {
  result.add(VPackValue(computation().getEdgeCount()));
  return {};
}


}  // namespace arangodb::pregel::algos::accumulators
