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

#ifndef ARANGODB_GREENSPUN_PRIMEVALCONTEXT_H
#define ARANGODB_GREENSPUN_PRIMEVALCONTEXT_H 1

#include "Interpreter.h"

namespace arangodb {
namespace greenspun {

struct PrimEvalContext : EvalContext {
  virtual std::string const& getThisId() const;
  virtual EvalResult getPregelId(VPackBuilder& result) const;
  virtual EvalResult getAccumulatorValue(std::string_view id, VPackBuilder& result) const;
  virtual EvalResult updateAccumulator(std::string_view accumId,
                                       std::string_view toId, VPackSlice value);
  virtual EvalResult updateAccumulatorById(std::string_view accumId,
                                           VPackSlice toVertex, VPackSlice value);
  virtual EvalResult sendToAllNeighbors(std::string_view accumId, VPackSlice value);
  virtual EvalResult setAccumulator(std::string_view accumId, VPackSlice value);
  virtual std::size_t getVertexUniqueId() const;

  virtual EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const;
  virtual EvalResult getBindingValue(std::string_view id, VPackBuilder& result) const;
  virtual EvalResult getGlobalSuperstep(VPackBuilder& result) const;
  virtual EvalResult gotoPhase(std::string_view nextPhase) const;
  virtual EvalResult finishAlgorithm() const;
  virtual EvalResult getVertexCount(VPackBuilder& result) const;
  virtual EvalResult getOutgoingEdgesCount(VPackBuilder& result) const;
  virtual void printCallback(std::string const& msg) const;
};

}  // namespace greenspun
}  // namespace arangodb

#endif
