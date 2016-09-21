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


#ifndef ARANGOD_CLUSTER_TRAVERSER_ENGINE_H
#define ARANGOD_CLUSTER_TRAVERSER_ENGINE_H 1

#include "Basics/Common.h"
#include "Aql/Collections.h"

struct TRI_vocbase_t;

namespace arangodb {

class Transaction;
class TransactionContext;

namespace aql {
class Collections;
class Query;
}
namespace velocypack {
class Builder;
class Slice;
}
namespace traverser {
struct TraverserOptions;

class BaseTraverserEngine {
  friend class TraverserEngineRegistry;
  protected:
  // These are private on purpose.
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly
    BaseTraverserEngine(TRI_vocbase_t*, arangodb::velocypack::Slice);

  public:
    virtual ~BaseTraverserEngine();

   // The engine is NOT copyable.
   BaseTraverserEngine(BaseTraverserEngine const&) = delete;

   void getEdges(arangodb::velocypack::Slice, size_t,
                 arangodb::velocypack::Builder&);

   void getVertexData(arangodb::velocypack::Slice,
                      arangodb::velocypack::Builder&);

   void getVertexData(arangodb::velocypack::Slice, size_t,
                      arangodb::velocypack::Builder&);

   virtual void smartSearch(arangodb::velocypack::Slice,
                            arangodb::velocypack::Builder&) = 0;

   virtual void smartSearchBFS(arangodb::velocypack::Slice,
                               arangodb::velocypack::Builder&) = 0;

   bool lockCollection(std::string const&);

   std::shared_ptr<TransactionContext> context() const;

  protected:
    std::unique_ptr<TraverserOptions> _opts;
    arangodb::aql::Query* _query;
    arangodb::Transaction* _trx;
    arangodb::aql::Collections _collections;
    std::unordered_set<std::string> _locked;
    std::unordered_map<std::string, std::vector<std::string>> _vertexShards;
};

class TraverserEngine : public BaseTraverserEngine {
  friend class TraverserEngineRegistry;
  private:
  // These are private on purpose.
  // Only the Registry (friend) is allowed
  // to create and destroy engines.
  // We can get into undefined state if sth.
  // deletes an engine but the registry
  // does not get informed properly
 
    TraverserEngine(TRI_vocbase_t*, arangodb::velocypack::Slice);
  public:
    ~TraverserEngine();

    void smartSearch(arangodb::velocypack::Slice,
                     arangodb::velocypack::Builder&) override;

    void smartSearchBFS(arangodb::velocypack::Slice,
                       arangodb::velocypack::Builder&) override;
};

} // namespace traverser
} // namespace arangodb

#endif
