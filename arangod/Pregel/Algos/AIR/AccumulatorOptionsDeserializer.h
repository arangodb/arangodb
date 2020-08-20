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

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATOR_OPTIONS_DESERIALIZER_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATOR_OPTIONS_DESERIALIZER_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <map>
#include <string>

#include <VPackDeserializer/deserializer.h>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

enum class AccumulatorType {
  MIN,
  MAX,
  SUM,
  AND,
  OR,
  STORE,
  LIST,
  CUSTOM,
};

std::ostream& operator<<(std::ostream&, AccumulatorType const&);

enum class AccumulatorValueType {
  DOUBLES,
  INTS,
  STRINGS,
  BOOL,
  SLICE,
};

/* A *single* accumulator */
struct AccumulatorOptions {
  AccumulatorType type;
  AccumulatorValueType valueType;
  bool storeSender;
  std::optional<std::string> customType;
  std::optional<VPackBuilder> parameters;
};

struct DataAccessDefinition {
  std::optional<VPackBuilder> writeVertex;
  std::optional<VPackBuilder> readVertex;
  std::optional<VPackBuilder> readEdge;
};

struct CustomAccumulatorDefinition {
  VPackBuilder clearProgram;
  VPackBuilder updateProgram;
  VPackBuilder setProgram;
  VPackBuilder getProgram;
  VPackBuilder finalizeProgram;
};

// An accumulator declaration consists of a unique name
// and a struct of options
using AccumulatorsDeclaration = std::unordered_map<std::string, AccumulatorOptions>;
using BindingDeclarations = std::unordered_map<std::string, VPackBuilder>;
using CustomAccumulatorDefinitions = std::unordered_map<std::string, CustomAccumulatorDefinition>;
using DataAccessDefinitions = DataAccessDefinition;

struct AlgorithmPhase {
  std::string name;
  VPackBuilder initProgram;
  VPackBuilder updateProgram;
  VPackBuilder onHalt;
};

using PhaseDeclarations = std::vector<AlgorithmPhase>;

/* The Pregel Algorithm */
struct VertexAccumulatorOptions {
  std::string resultField;
  AccumulatorsDeclaration vertexAccumulators;
  AccumulatorsDeclaration globalAccumulators;
  CustomAccumulatorDefinitions customAccumulators;
  DataAccessDefinitions dataAccess;
  BindingDeclarations bindings;
  PhaseDeclarations phases;
  uint64_t maxGSS;
};

std::ostream& operator<<(std::ostream&, AccumulatorOptions const&);
std::ostream& operator<<(std::ostream&, DataAccessDefinition const&);
std::ostream& operator<<(std::ostream&, AccumulatorValueType const&);

deserializer_result<DataAccessDefinition> parseDataAccessOptions(VPackSlice slice);
deserializer_result<AccumulatorOptions> parseAccumulatorOptions(VPackSlice slice);
deserializer_result<VertexAccumulatorOptions> parseVertexAccumulatorOptions(VPackSlice slice);

}  // namespace accumulators
}  // namespace algos
}  // namespace pregel
}  // namespace arangodb
#endif
