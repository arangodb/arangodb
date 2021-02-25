////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Lars Maier
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATOR_OPTIONS_H
#define ARANGODB_PREGEL_ALGOS_VERTEX_ACCUMULATOR_OPTIONS_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <map>
#include <unordered_set>
#include <string>

#include <VPackDeserializer/errors.h>
#include <VPackDeserializer/types.h>

namespace arangodb {
namespace pregel {
namespace algos {
namespace accumulators {

using PregelProgram = VPackBuilder;

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
  DOUBLE,
  INT,
  STRING,
  BOOL,
  ANY,
};

/* A *single* accumulator */
struct AccumulatorOptions {
  AccumulatorType type;
  AccumulatorValueType valueType;
  std::optional<std::string> customType;
  std::optional<VPackBuilder> parameters;
};

using KeyPath = std::vector<std::string>;
using KeyOrPath = std::variant<KeyPath, std::string>;
using PathList = std::vector<KeyOrPath>;

struct DataAccessDefinition {
  std::optional<VPackBuilder> writeVertex;
  std::optional<PathList> readVertex;
  std::optional<PathList> readEdge;
};

struct CustomAccumulatorDefinition {
  PregelProgram clearProgram;
  PregelProgram setProgram;
  PregelProgram getProgram;
  PregelProgram updateProgram;

  PregelProgram setStateProgram;
  PregelProgram getStateProgram;
  PregelProgram getStateUpdateProgram;
  PregelProgram aggregateStateProgram;

  PregelProgram finalizeProgram;
};

// An accumulator declaration consists of a unique name
// and a struct of options
using AccumulatorsDeclaration = std::unordered_map<std::string, AccumulatorOptions>;
using BindingDeclarations = std::unordered_map<std::string, VPackBuilder>;
using CustomAccumulatorDefinitions = std::unordered_map<std::string, CustomAccumulatorDefinition>;
using DataAccessDefinitions = DataAccessDefinition;

struct AlgorithmPhase {
  std::string name;
  PregelProgram initProgram;
  PregelProgram updateProgram;

  PregelProgram onHalt;
  PregelProgram onPreStep;
  PregelProgram onPostStep;
};

using PhaseDeclarations = std::vector<AlgorithmPhase>;

using IdentifierList = std::unordered_set<std::string>;

struct TraceMessagesFilterOptions {
  IdentifierList bySender;
  IdentifierList byAccumulator;
};

struct TraceMessagesOptions {
  std::optional<TraceMessagesFilterOptions> filter;
};

using TraceMessageVertexList = std::unordered_map<std::string, TraceMessagesOptions>;

struct DebugInformation {
  TraceMessageVertexList traceMessages;
};

/* The Pregel Algorithm */
struct VertexAccumulatorOptions {
  std::string resultField;
  std::size_t parallelism;
  AccumulatorsDeclaration vertexAccumulators;
  AccumulatorsDeclaration globalAccumulators;
  CustomAccumulatorDefinitions customAccumulators;
  DataAccessDefinitions dataAccess;
  BindingDeclarations bindings;
  PhaseDeclarations phases;
  uint64_t maxGSS;
  std::optional<DebugInformation> debug;
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
