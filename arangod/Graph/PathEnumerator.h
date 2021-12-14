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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Graph/EdgeDocumentToken.h"

#include <velocypack/Slice.h>
#include <vector>

namespace arangodb {
struct ResourceMonitor;

namespace aql {
struct AqlValue;
class PruneExpressionEvaluator;
}

namespace velocypack {
class Builder;
}

namespace graph {
class EdgeCursor;
}

namespace traverser {
class Traverser;
struct TraverserOptions;

class EnumeratedPath {
 public:
  explicit EnumeratedPath(arangodb::ResourceMonitor& resourceMonitor);
  ~EnumeratedPath();

  void pushVertex(std::string_view v);
  void pushEdge(graph::EdgeDocumentToken const& e);
  void popVertex() noexcept;
  void popEdge() noexcept;
  void clear();
  size_t numVertices() const noexcept;
  size_t numEdges() const noexcept;
  std::vector<std::string_view> const& vertices() const noexcept;
  std::vector<graph::EdgeDocumentToken> const& edges() const noexcept;
  std::string_view lastVertex() const noexcept;
  graph::EdgeDocumentToken const& lastEdge() const noexcept;

 private:
  template <typename T>
  void growStorage(std::vector<T>& data);

 private: 
  arangodb::ResourceMonitor& _resourceMonitor;
  std::vector<graph::EdgeDocumentToken> _edges;
  std::vector<std::string_view> _vertices;
};

class PathEnumerator {
 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief This is the class that knows the details on how to
  ///        load the data and how to return data in the expected format
  ///        NOTE: This class does not known the traverser.
  //////////////////////////////////////////////////////////////////////////////

  traverser::Traverser* _traverser;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Options used in the traversal
  //////////////////////////////////////////////////////////////////////////////

  TraverserOptions* _opts;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief List of the last path is used to
  //////////////////////////////////////////////////////////////////////////////

  EnumeratedPath _enumeratedPath;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Number of HTTP requests made
  //////////////////////////////////////////////////////////////////////////////

  size_t _httpRequests;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Indicates if we issue next() the first time.
  ///        It shall return an empty path in this case.
  //////////////////////////////////////////////////////////////////////////////

  bool _isFirst;

  bool keepEdge(graph::EdgeDocumentToken& eid, velocypack::Slice edge,
                std::string_view sourceVertex, size_t depth, size_t cursorId);

 public:
  PathEnumerator(Traverser* traverser, TraverserOptions* opts);

  virtual ~PathEnumerator();

  virtual void clear() = 0;

  /// @brief set start vertex and reset
  /// note that the caller *must* guarantee that the string data pointed to by
  /// startVertex remains valid even after the call to reset()!!
  virtual void setStartVertex(std::string_view startVertex) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Compute the next Path element from the traversal.
  ///        Returns false if there is no next path element.
  ///        Only if this is true one can compute the AQL values.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool next() = 0;

  virtual aql::AqlValue lastVertexToAqlValue() = 0;
  virtual aql::AqlValue lastEdgeToAqlValue() = 0;
  virtual aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&) = 0;


  /// @brief validate post filter statement
  /// returns true if the path should be returned
  /// returns false if the path should not be returned
  bool usePostFilter(aql::PruneExpressionEvaluator* evaluator);

  /// @brief return number of HTTP requests made, and reset it to 0
  size_t getAndResetHttpRequests() {
    size_t value = _httpRequests;
    _httpRequests = 0;
    return value;
  }

  void incHttpRequests(size_t requests) { _httpRequests += requests; }

 protected:
  graph::EdgeCursor* getCursor(std::string_view nextVertex, uint64_t currentDepth);

  /// @brief The vector of EdgeCursors to walk through.
  std::vector<std::unique_ptr<graph::EdgeCursor>> _cursors;
};

// cppcheck-suppress noConstructor
class DepthFirstEnumerator final : public PathEnumerator {
 private:
  size_t _activeCursors;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Flag if we need to prune the next path
  //////////////////////////////////////////////////////////////////////////////
  bool _pruneNext;

 public:
  DepthFirstEnumerator(Traverser* traverser, TraverserOptions* opts);

  ~DepthFirstEnumerator();

  void clear() override {}

  /// @brief set start vertex and reset
  void setStartVertex(std::string_view startVertex) override;

  /// @brief Get the next Path element from the traversal.
  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;

  aql::AqlValue lastEdgeToAqlValue() override;

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

 private:
  bool shouldPrune();
  bool validDisjointPath() const;

  velocypack::Slice pathToSlice(arangodb::velocypack::Builder& result, bool fromPrune);
};
}  // namespace traverser
}  // namespace arangodb

