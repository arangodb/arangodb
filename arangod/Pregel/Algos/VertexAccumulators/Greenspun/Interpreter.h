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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_EVALUATOR_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATORS_EVALUATOR_H 1

#include <functional>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>


struct EvalContext {
  // Variables go here.
  std::unordered_map<std::string, VPackSlice> variables;

  std::string const& getThisId() const {
    return thisId;
  }

  VPackSlice getDocumentById(std::string_view id) {
    return VPackSlice::emptyObjectSlice();  // TODO
  }

  VPackSlice getAccumulatorValue(std::string_view id) {
    return VPackSlice::zeroSlice(); // TODO
  }

  void updateAccumulator(std::string_view accumId, std::string_view vertexId, VPackSlice value) {
    ; // TODO
  }

  void setAccumulator(std::string_view accumId, std::string_view vertexId, VPackSlice value) {
    ; // TODO
  }

  std::string thisId;
};

//
// ["varRef", "name"]
//
// assignment
//


void Evaluate(EvalContext& ctx, VPackSlice const slice, VPackBuilder& result);
void InitInterpreter();


#endif
