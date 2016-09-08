////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TRAVERSAL_BLOCK_H
#define ARANGOD_AQL_TRAVERSAL_BLOCK_H 1

#include "Aql/ExecutionBlock.h"
#include "Aql/TraversalNode.h"
#include "Basics/VelocyPackHelper.h"
#include "VocBase/Traverser.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace aql {

class TraversalBlock : public ExecutionBlock {
 public:
  TraversalBlock(ExecutionEngine* engine, TraversalNode const* ep);

  ~TraversalBlock();

  /// @brief initialize, here we fetch all docs from the database
  int initialize() override;

  /// @brief initializeCursor
  int initializeCursor(AqlItemBlock* items, size_t pos) override;

  /// @brief shutdown send destroy to all engines.
  int shutdown(int errorCode) override final;

  /// @brief getSome
  AqlItemBlock* getSome(size_t atLeast, size_t atMost) override final;

  // skip between atLeast and atMost, returns the number actually skipped . . .
  // will only return less than atLeast if there aren't atLeast many
  // things to skip overall.
  size_t skipSome(size_t atLeast, size_t atMost) override final;

 private:

  /// @brief cleanup, here we clean up all internally generated values
  void freeCaches();

  /// @brief vertices buffer
  std::vector<arangodb::aql::AqlValue> _vertices;

  /// @brief edges buffer
  std::vector<arangodb::aql::AqlValue> _edges;

  /// @brief path buffer
  std::vector<arangodb::aql::AqlValue> _paths;

  /// @brief current position in _paths, _edges, _vertices
  size_t _posInPaths;

  /// @brief Options for the travereser
  arangodb::traverser::TraverserOptions* _opts;

  /// @brief Traverser object
  std::unique_ptr<arangodb::traverser::Traverser> _traverser;

  /// @brief The information to get the starting point, when a register id is
  /// used
  arangodb::aql::RegisterId _reg;

  /// @brief Keep a copy of the start vertex id-string. Can be freed if this
  /// start vertex is not in use any more.
  std::string _vertexId;

  /// @brief Indicator if we use a register for input variable.
  ///        Invariant: _useRegister == true <=> _reg != undefined
  bool _useRegister;

  /// @brief Indicator if we have used the constant input parameter for
  /// computation
  ///        We use it to check if we are done with enumerating.
  bool _usedConstant;

  /// @brief Variable for the vertex output
  Variable const* _vertexVar;

  /// @brief Register for the vertex output
  RegisterId _vertexReg;

  /// @brief Variable for the edge output
  Variable const* _edgeVar;

  /// @brief Register for the edge output
  RegisterId _edgeReg;

  /// @brief Variable for the full path output
  Variable const* _pathVar;

  /// @brief Register for the full path output
  RegisterId _pathReg;

  /// @brief _inVars, a vector containing all variables required
  ///        for the filtering conditions.
  std::vector<Variable const*> _inVars;

  /// @brief _inRegs, a vector containing for each expression above
  /// a vector of RegisterId, used to execute the expression
  std::vector<RegisterId> _inRegs;

  std::unordered_map<ServerID, traverser::TraverserEngineID> const* _engines;

  /// @brief continue fetching of paths
  bool morePaths(size_t hint);

  /// @brief skip the next paths
  size_t skipPaths(size_t hint);

  /// @brief Initialize the filter expressions
  void initializeExpressions(AqlItemBlock const*, size_t pos);

  /// @brief Initialize the path enumerator
  void initializePaths(AqlItemBlock const*, size_t pos);

  /// @brief Checks if we output the vertex
  bool usesVertexOutput() { return _vertexVar != nullptr; }

  /// @brief Checks if we output the edge
  bool usesEdgeOutput() { return _edgeVar != nullptr; }

  /// @brief Checks if we output the path
  bool usesPathOutput() { return _pathVar != nullptr; }

};
}  // namespace arangodb::aql
}  // namespace arangodb

#endif
