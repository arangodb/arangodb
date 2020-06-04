////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_API_INDEXES_H
#define ARANGOD_VOC_BASE_API_INDEXES_H 1

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/Result.h"
#include "Indexes/Index.h"
#include "VocBase/Identifiers/IndexId.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
class CollectionNameResolver;
namespace methods {

/// Common code for ensureIndexes and api-index.js
struct Indexes {
  static arangodb::Result getIndex(LogicalCollection const* collection,
                                   velocypack::Slice const& indexId, velocypack::Builder&,
                                   transaction::Methods* trx = nullptr);

  /// @brief get all indexes, skips view links
  static arangodb::Result getAll(LogicalCollection const* collection,
                                 std::underlying_type<Index::Serialize>::type,
                                 bool withHidden, arangodb::velocypack::Builder&,
                                 transaction::Methods* trx = nullptr);

  static arangodb::Result createIndex(LogicalCollection*, Index::IndexType,
                                      std::vector<std::string> const&,
                                      bool unique, bool sparse);

  static arangodb::Result ensureIndex(LogicalCollection* collection,
                                      velocypack::Slice const& definition,
                                      bool create, velocypack::Builder& output);

  static arangodb::Result drop(LogicalCollection* collection,
                               velocypack::Slice const& indexArg);

  static arangodb::Result extractHandle(LogicalCollection const* collection,
                                        CollectionNameResolver const* resolver,
                                        velocypack::Slice const& val,
                                        IndexId& iid, std::string& name);

 private:
  static arangodb::Result ensureIndexCoordinator(LogicalCollection const* collection,
                                                 velocypack::Slice const& indexDef, bool create,
                                                 velocypack::Builder& resultBuilder);

#ifdef USE_ENTERPRISE
  static arangodb::Result ensureIndexCoordinatorEE(
      arangodb::LogicalCollection const* collection, arangodb::velocypack::Slice const slice,
      bool create, arangodb::velocypack::Builder& resultBuilder);
  static arangodb::Result dropCoordinatorEE(arangodb::LogicalCollection const* collection,
                                            IndexId const iid);
#endif
};
}  // namespace methods
}  // namespace arangodb

#endif
