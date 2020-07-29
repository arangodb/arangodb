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

using namespace arangodb::pregel;

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

void VertexComputationEvalContext::getAccumulatorValue(std::string_view accumId,
                                                       VPackBuilder& builder) const {
  vertexData().accumulatorByName(accumId)->getValueIntoBuilder(builder);
}

void VertexComputationEvalContext::setAccumulator(std::string_view accumId, VPackSlice value) {
  vertexData().accumulatorByName(accumId)->setBySlice(value);
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

void VertexComputationEvalContext::updateAccumulator(std::string_view accumId,
                                                     std::string_view toId,
                                                     VPackSlice value) {
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
      return;
    }
  }
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

}  // namespace arangodb::pregel::algos::accumulators
