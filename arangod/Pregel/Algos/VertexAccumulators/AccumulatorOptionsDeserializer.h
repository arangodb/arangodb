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

#include <string>
#include <map>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <VPackDeserializer/deserializer.h>

enum class AccumulatorType {
  MIN, MAX, SUM, AND, OR
};

std::ostream& operator<<(std::ostream&, AccumulatorType const&);

enum class AccumulatorValueType {
  DOUBLES,
  INTS,
  STRINGS,
  BOOL,
};

/* A *single* accumulator */
struct AccumulatorOptions {
  AccumulatorType type;
  AccumulatorValueType valueType;
  bool storeSender;
};

// An accumulator declaration consists of a unique name
// and a struct of options
using AccumulatorsDeclaration = std::map<std::string, AccumulatorOptions>;

/* The Pregel Algorithm */
struct VertexAccumulatorOptions {
  std::string resultField;
  AccumulatorsDeclaration accumulatorsDeclaration;
  VPackSlice initProgram;
  VPackSlice updateProgram;
};

std::ostream& operator<<(std::ostream&, AccumulatorOptions const&);
std::ostream& operator<<(std::ostream&, AccumulatorValueType const&);

deserializer_result<AccumulatorOptions> parseAccumulatorOptions(VPackSlice slice);
deserializer_result<VertexAccumulatorOptions> parseVertexAccumulatorOptions(VPackSlice slice);

#endif
