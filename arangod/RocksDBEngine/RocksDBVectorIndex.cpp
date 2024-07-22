////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBVectorIndex.h"
#include "Basics/voc-errors.h"
#include "Inspection/VPack.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Vector/LocalSensitiveHashing.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/Value.h>
#include "Indexes/Index.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {

RocksDBVectorIndex::RocksDBVectorIndex(IndexId iid, LogicalCollection& coll,
                                       arangodb::velocypack::Slice info)
    : RocksDBIndex(iid, coll, info,
                   RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::VectorIndex),
                   /*useCache*/ false,
                   /*cacheManager*/ nullptr,
                   /*engine*/
                   coll.vocbase().engine<RocksDBEngine>()) {
  TRI_ASSERT(type() == Index::TRI_IDX_TYPE_VECTOR_INDEX);
  velocypack::deserialize(info.get("params"), _definition);
}

/// @brief Test if this index matches the definition
bool RocksDBVectorIndex::matchesDefinition(VPackSlice const& info) const {
  return false;
}

void RocksDBVectorIndex::toVelocyPack(
    arangodb::velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  VPackObjectBuilder objectBuilder(&builder);
  RocksDBIndex::toVelocyPack(builder, flags);
  builder.add(VPackValue("params"));
  velocypack::serialize(builder, _definition);
}

// TODO Move away, also remove from MDIIndex.cpp
auto accessDocumentPath(VPackSlice doc,
                        std::vector<basics::AttributeName> const& path)
    -> VPackSlice {
  for (auto&& attrib : path) {
    TRI_ASSERT(attrib.shouldExpand == false);
    if (!doc.isObject()) {
      return VPackSlice::noneSlice();
    }

    doc = doc.get(attrib.name);
  }

  return doc;
}

template<typename F>
Result RocksDBVectorIndex::processDocument(velocypack::Slice doc,
                                           LocalDocumentId documentId, F func) {
  TRI_ASSERT(_fields.size() == 1);
  VPackSlice value = accessDocumentPath(doc, _fields[0]);
  std::vector<double> input;
  input.reserve(_definition.dimensions);
  if (auto res = velocypack::deserializeWithStatus(value, input); !res.ok()) {
    return {TRI_ERROR_BAD_PARAMETER, res.error()};
  }

  if (input.size() != _definition.dimensions) {
    // TODO Find better error code
    return {TRI_ERROR_BAD_PARAMETER};
  }
  // TODO Maybe check all values withing <min, max>
  auto hashes = calculateHashedStrings(_definition, input);
  // prefix + hash + documentId
  for (auto const& hashedString : hashes) {
    RocksDBKey rocksdbKey;
    rocksdbKey.constructVectorIndexValue(objectId(), hashedString, documentId);

    auto status = func(rocksdbKey);
    if (!status.ok()) {
      return rocksutils::convertStatus(status);
    }
  }

  return Result{};
}

/// @brief inserts a document into the index
Result RocksDBVectorIndex::insert(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options,
                                  bool performChecks) {
#ifdef USE_ENTERPRISE
  return processDocument(doc, documentId, [this, &methods](auto const& key) {
    auto value = RocksDBValue::VectorIndexValue();
    return methods->PutUntracked(this->_cf, key, value.string());
  });
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

/// @brief removes a document from the index
Result RocksDBVectorIndex::remove(transaction::Methods& trx,
                                  RocksDBMethods* methods,
                                  LocalDocumentId documentId,
                                  velocypack::Slice doc,
                                  OperationOptions const& options) {
#ifdef USE_ENTERPRISE
  return processDocument(doc, documentId, [this, &methods](auto const& key) {
    return methods->Delete(this->_cf, key);
  });
#else
  return Result(TRI_ERROR_ONLY_ENTERPRISE);
#endif
}

}  // namespace arangodb
