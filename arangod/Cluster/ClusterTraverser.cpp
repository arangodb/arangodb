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

#include "ClusterTraverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ClusterMethods.h"
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/EdgeCursor.h"
#include "Graph/PathEnumerator.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/WeightedEnumerator.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"

using namespace arangodb;
using namespace arangodb::graph;

using ClusterTraverser = arangodb::traverser::ClusterTraverser;

ClusterTraverser::ClusterTraverser(arangodb::traverser::TraverserOptions* opts,
                                   std::unordered_map<ServerID, aql::EngineId> const* engines,
                                   std::string const& dbname)
    : Traverser(opts), _dbname(dbname), _engines(engines) {
  _opts->linkTraverser(this);

  createEnumerator();
  TRI_ASSERT(_enumerator != nullptr);
}

void ClusterTraverser::setStartVertex(std::string const& vid) {
  // There will be no idString of length above uint32_t
  arangodb::velocypack::HashedStringRef const s(vid.c_str(), static_cast<uint32_t>(vid.length()));

  auto it = _vertices.find(s);
  if (it == _vertices.end()) {
    size_t firstSlash = vid.find('/');
    if (firstSlash == std::string::npos ||
        vid.find('/', firstSlash + 1) != std::string::npos) {
      // We can stop here. The start vertex is not a valid _id
      traverserCache()->increaseFilterCounter();
      _done = true;
      return;
    }
  }

  if (!vertexMatchesConditions(s.stringRef(), 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }
 
  arangodb::velocypack::HashedStringRef persId = traverserCache()->persistString(s);
  _vertexGetter->reset(persId.stringRef());
  _enumerator->setStartVertex(persId.stringRef());
  _done = false;
}

void ClusterTraverser::clear() {
  traverserCache()->clear();

  _vertices.clear();
  _verticesToFetch.clear();
}

bool ClusterTraverser::getVertex(VPackSlice edge, arangodb::traverser::EnumeratedPath& path) {
  bool res = _vertexGetter->getVertex(edge, path);
  if (res) {
    auto const& last = path.lastVertex();
    // There will be no idString of length above uint32_t
    arangodb::velocypack::HashedStringRef other(last.data(),
                                                static_cast<uint32_t>(last.length()));
    if (_vertices.find(other) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(other);
    }
  }
  return res;
}

bool ClusterTraverser::getSingleVertex(arangodb::velocypack::Slice edge,
                                       arangodb::velocypack::StringRef sourceVertexId,
                                       uint64_t depth,
                                       arangodb::velocypack::StringRef& targetVertexId) {
  bool res = _vertexGetter->getSingleVertex(edge, sourceVertexId, depth, targetVertexId);
  if (res) {
    // There will be no idString of length above uint32_t
    arangodb::velocypack::HashedStringRef other(targetVertexId.data(),
                                                static_cast<uint32_t>(
                                                    targetVertexId.length()));
    if (_vertices.find(other) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(other);
    }
  }
  return res;
}

void ClusterTraverser::fetchVertices() {
  if (_opts->produceVertices()) {
    auto ch = static_cast<ClusterTraverserCache*>(traverserCache());
    ch->insertedDocuments() += _verticesToFetch.size();
    fetchVerticesFromEngines(*_trx, *ch, _verticesToFetch, _vertices, /*forShortestPath*/ false);

    _enumerator->incHttpRequests(_engines->size());
  } else {
    for (auto const& it : _verticesToFetch) {
      _vertices.emplace(it, VPackSlice::nullSlice());
    }
  }
  _verticesToFetch.clear();
}

aql::AqlValue ClusterTraverser::fetchVertexData(arangodb::velocypack::StringRef idString) {
  // There will be no idString of length above uint32_t
  arangodb::velocypack::HashedStringRef hashedIdString(idString.data(),
                                                       static_cast<uint32_t>(
                                                           idString.length()));
  auto cached = _vertices.find(hashedIdString);
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.

    // we need to make sure the idString remains valid afterwards
    hashedIdString = _opts->cache()->persistString(hashedIdString);
    _verticesToFetch.emplace(hashedIdString);
    fetchVertices();
    cached = _vertices.find(hashedIdString);
  }
  // Now all vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  uint8_t const* ptr = cached->second.begin();
  return aql::AqlValue(ptr);  // non-copying constructor
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Function to add the real data of a vertex into a velocypack builder
//////////////////////////////////////////////////////////////////////////////

void ClusterTraverser::addVertexToVelocyPack(arangodb::velocypack::StringRef vid, VPackBuilder& result) {
  // There will be no idString of length above uint32_t
  arangodb::velocypack::HashedStringRef hashedIdString(vid.data(),
                                                       static_cast<uint32_t>(vid.length()));
  auto cached = _vertices.find(hashedIdString);
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.
    _verticesToFetch.emplace(hashedIdString);
    fetchVertices();
    cached = _vertices.find(hashedIdString);
  }
  // Now all vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  result.add(cached->second);
}

void ClusterTraverser::destroyEngines() {
  // We have to clean up the engines in Coordinator Case.
  NetworkFeature const& nf = _trx->vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    return;
  }
  // nullptr only happens on controlled server shutdown

  _enumerator->incHttpRequests(_engines->size());

  VPackBuffer<uint8_t> body;

  network::RequestOptions options;
  options.database = _trx->vocbase().name();
  options.timeout = network::Timeout(30.0);
  options.skipScheduler = true;  // hack to speed up future.get()

  // TODO: use collectAll to parallelize shutdown ?
  for (auto const& it : *_engines) {
    auto res = network::sendRequest(pool, "server:" + it.first, fuerte::RestVerb::Delete,
                                    "/_internal/traverser/" +
                                        arangodb::basics::StringUtils::itoa(it.second),
                                    body, options);
    res.wait();

    if (!res.hasValue() || res.get().fail()) {
      // Note If there was an error on server side we do not have ok()
      std::string message("Could not destroy all traversal engines");
      if (res.hasValue()) {
        message += ": " + network::fuerteToArangoErrorMessage(res.get());
      }
      LOG_TOPIC("8a7a0", ERR, arangodb::Logger::FIXME) << message;
    }
  }
}

void ClusterTraverser::createEnumerator() {
  TRI_ASSERT(_enumerator == nullptr);
  switch (_opts->mode) {
    case TraverserOptions::Order::DFS:
      // normal, depth-first enumerator
      _enumerator = std::make_unique<DepthFirstEnumerator>(this, _opts);
      break;
    case TraverserOptions::Order::BFS:
      // default breadth-first enumerator
      _enumerator = std::make_unique<BreadthFirstEnumerator>(this, _opts);
      break;
    case TraverserOptions::Order::WEIGHTED:
      _enumerator = std::make_unique<WeightedEnumerator>(this, _opts);
      break;
  }
}

bool ClusterTraverser::getVertex(arangodb::velocypack::StringRef vertex, size_t depth) {
  bool res = _vertexGetter->getVertex(vertex, depth);
  if (res) {
    // There will be no idString of length above uint32_t
    arangodb::velocypack::HashedStringRef hashedIdString(vertex.data(),
                                                         static_cast<uint32_t>(
                                                             vertex.length()));
    if (_vertices.find(hashedIdString) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(hashedIdString);
    }
  }
  return res;
}
