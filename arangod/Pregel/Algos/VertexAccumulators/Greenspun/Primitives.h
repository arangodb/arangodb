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
  virtual std::string const& getThisId() const = 0;
  virtual void getAccumulatorValue(std::string_view id, VPackBuilder& result) const = 0;
  virtual void updateAccumulator(std::string_view accumId,
                                 std::string_view edgeId, VPackSlice value) = 0;
  virtual void setAccumulator(std::string_view accumId, VPackSlice value) = 0;
  virtual EvalResult enumerateEdges(std::function<EvalResult(VPackSlice edge, VPackSlice vertex)> cb) const = 0;

};

extern std::unordered_map<std::string, std::function<EvalResult(PrimEvalContext& ctx, VPackSlice const slice, VPackBuilder& result)>> primitives;
void RegisterPrimitives();

#endif
