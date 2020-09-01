////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_VOCBASE_PATHENUMERATOR_H
#define ARANGODB_VOCBASE_PATHENUMERATOR_H 1

#include <vector>

#include "Basics/Common.h"
#include "Graph/EdgeDocumentToken.h"

#include <velocypack/Slice.h>

namespace arangodb {
namespace aql {
struct AqlValue;
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

struct EnumeratedPath {
  std::vector<graph::EdgeDocumentToken> edges;
  std::vector<arangodb::velocypack::StringRef> vertices;
  EnumeratedPath() {}
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
                velocypack::StringRef sourceVertex, size_t depth, size_t cursorId);

 public:
  PathEnumerator(Traverser* traverser, TraverserOptions* opts);

  virtual ~PathEnumerator();

  /// @brief set start vertex and reset
  /// note that the caller *must* guarantee that the string data pointed to by
  /// startVertex remains valid even after the call to reset()!!
  virtual void setStartVertex(arangodb::velocypack::StringRef startVertex) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Compute the next Path element from the traversal.
  ///        Returns false if there is no next path element.
  ///        Only if this is true one can compute the AQL values.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool next() = 0;

  virtual aql::AqlValue lastVertexToAqlValue() = 0;
  virtual aql::AqlValue lastEdgeToAqlValue() = 0;
  virtual aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&) = 0;

  /// @brief return number of HTTP requests made, and reset it to 0
  size_t getAndResetHttpRequests() {
    size_t value = _httpRequests;
    _httpRequests = 0;
    return value;
  }

  void incHttpRequests(size_t requests) { _httpRequests += requests; }

 protected:
  graph::EdgeCursor* getCursor(arangodb::velocypack::StringRef nextVertex, uint64_t currentDepth);

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

  /// @brief set start vertex and reset
  void setStartVertex(arangodb::velocypack::StringRef startVertex) override;

  /// @brief Get the next Path element from the traversal.
  bool next() override;

  aql::AqlValue lastVertexToAqlValue() override;

  aql::AqlValue lastEdgeToAqlValue() override;

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& result) override;

 private:
  bool shouldPrune();

  velocypack::Slice pathToSlice(arangodb::velocypack::Builder& result);
};
}  // namespace traverser
}  // namespace arangodb

#endif
