////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

#include "PrimEvalContext.h"
#include <iostream>

#include "EvalResult.h"

namespace arangodb {
namespace greenspun {

std::string const& PrimEvalContext::getThisId() const { std::abort(); }

EvalResult PrimEvalContext::getPregelId(VPackBuilder& result) const {
  return EvalError("not implemented");
}

EvalResult PrimEvalContext::getAccumulatorValue(std::string_view id, VPackBuilder& result) const {
  return EvalError("not implemented");
}
EvalResult PrimEvalContext::updateAccumulator(std::string_view accumId,
                                              std::string_view toId, VPackSlice value) {
  return EvalError("not implemented");
}
EvalResult PrimEvalContext::updateAccumulatorById(std::string_view accumId,
                                                  VPackSlice toVertex, VPackSlice value) {
  return EvalError("not implemented");
}
EvalResult PrimEvalContext::setAccumulator(std::string_view accumId, VPackSlice value) {
  return EvalError("not implemented");
}

EvalResult PrimEvalContext::sendToAllNeighbors(std::string_view accumId, VPackSlice value) {
  return EvalError("not implemented");
}

std::size_t PrimEvalContext::getVertexUniqueId() const { return 0; }

EvalResult PrimEvalContext::enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const {
  return EvalError("not impemented");
}
EvalResult PrimEvalContext::getBindingValue(std::string_view id, VPackBuilder& result) const {
  return EvalError("not impemented");
}
EvalResult PrimEvalContext::getGlobalSuperstep(VPackBuilder& result) const {
  return EvalError("not impemented");
}
EvalResult PrimEvalContext::gotoPhase(std::string_view nextPhase) const {
  return EvalError("not impemented");
}
EvalResult PrimEvalContext::finishAlgorithm() const {
  return EvalError("not impemented");
}

EvalResult PrimEvalContext::getVertexCount(VPackBuilder& result) const {
  return EvalError("not implemented");
}
EvalResult PrimEvalContext::getOutgoingEdgesCount(VPackBuilder& result) const {
  return EvalError("not implemented");
}

void PrimEvalContext::printCallback(const std::string& msg) const {
  std::cout << msg << std::endl;
}

}  // namespace greenspun
}  // namespace arangodb
