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

#ifndef ARANGODB_PREGEL_GREENSPUN_PRIMITIVES_H
#define ARANGODB_PREGEL_GREENSPUN_PRIMITIVES_H 1

#include "Interpreter.h"

struct PrimEvalContext : EvalContext {
  virtual std::string const& getThisId() const { std::abort(); };
  virtual EvalResult getPregelId(VPackBuilder& result) const { return EvalError("not implemented"); }
  virtual void getAccumulatorValue(std::string_view id, VPackBuilder& result) const {};
  virtual void updateAccumulator(std::string_view accumId,
                                 std::string_view toId, VPackSlice value) {};
  virtual EvalResult updateAccumulatorById(std::string_view accumId,
                                 VPackSlice toVertex, VPackSlice value) { return EvalError("not implemented"); };
  virtual void setAccumulator(std::string_view accumId, VPackSlice value) {};

  virtual std::size_t getVertexUniqueId() const { return 0; }

  virtual EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge)> cb) const {
    return EvalError("not impemented");
  }
  virtual EvalResult getBindingValue(std::string_view id, VPackBuilder& result) const {
    return EvalError("not impemented");
  }
  virtual EvalResult getGlobalSuperstep(VPackBuilder& result) const {
    return EvalError("not impemented");
  }
  virtual EvalResult gotoPhase(std::string_view nextPhase) const {
    return EvalError("not impemented");
  }
  virtual EvalResult finishAlgorithm() const {
    return EvalError("not impemented");
  }

  virtual void printCallback(std::string const& msg) const;
};

extern std::unordered_map<std::string, std::function<EvalResult(PrimEvalContext& ctx, VPackSlice const slice, VPackBuilder& result)>> primitives;
void RegisterPrimitives();

#endif
