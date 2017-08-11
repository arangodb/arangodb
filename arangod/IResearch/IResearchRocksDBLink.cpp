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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h" // required for RocksDBColumnFamily.h
#include "IResearchLink.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"

#include "IResearchRocksDBLink.h"

NS_LOCAL

////////////////////////////////////////////////////////////////////////////////
/// @brief return a reference to a static VPackSlice of an empty RocksDB index
///        definition
////////////////////////////////////////////////////////////////////////////////
VPackSlice const& emptyParentSlice() {
  static const struct EmptySlice {
    VPackBuilder _builder;
    VPackSlice _slice;
    EmptySlice() {
      VPackBuilder fieldsBuilder;

      fieldsBuilder.openArray();
      fieldsBuilder.close(); // empty array
      _builder.openObject();
      _builder.add("fields", fieldsBuilder.slice()); // empty array
      arangodb::iresearch::IResearchLink::setType(_builder); // the index type required by Index
      _builder.close(); // object with just one field required by the Index constructor
      _slice = _builder.slice();
    }
  } emptySlice;

  return emptySlice._slice;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchRocksDBLink::IResearchRocksDBLink(
    TRI_idx_iid_t iid,
    arangodb::LogicalCollection* collection
): RocksDBIndex(iid, collection, emptyParentSlice(), RocksDBColumnFamily::vpack(), false) {
}

IResearchRocksDBLink::~IResearchRocksDBLink() {
  // NOOP
}

bool IResearchRocksDBLink::allowExpansion() const {
  TRI_ASSERT(_link);
  return _link->allowExpansion();
}

bool IResearchRocksDBLink::canBeDropped() const {
  TRI_ASSERT(_link);
  return _link->canBeDropped();
}

bool IResearchRocksDBLink::hasSelectivityEstimate() const {
  TRI_ASSERT(_link);
  return _link->hasSelectivityEstimate();
}

arangodb::Result IResearchRocksDBLink::insertInternal(
    transaction::Methods* trx,
    arangodb::RocksDBMethods*,
    TRI_voc_rid_t revisionId,
    const arangodb::velocypack::Slice& doc
) {
  TRI_ASSERT(_link);
  return _link->insert(trx, revisionId, doc, false);
}

bool IResearchRocksDBLink::isSorted() const {
  TRI_ASSERT(_link);
  return _link->isSorted();
}

/*static*/ IResearchRocksDBLink::ptr IResearchRocksDBLink::make(
  TRI_idx_iid_t iid,
  arangodb::LogicalCollection* collection,
  VPackSlice const& definition
) noexcept {
  PTR_NAMED(IResearchRocksDBLink, ptr, iid, collection);

  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* link = dynamic_cast<arangodb::iresearch::IResearchRocksDBLink*>(ptr.get());
  #else
    auto* link = static_cast<arangodb::iresearch::IResearchRocksDBLink*>(ptr.get());
  #endif

  if (!link) {
    return nullptr;
  }

  link->_link = IResearchLink::make(iid, collection, definition);

  return link->_link ? ptr : nullptr;
}

arangodb::Result IResearchRocksDBLink::removeInternal(
    transaction::Methods* trx,
    arangodb::RocksDBMethods*,
    TRI_voc_rid_t revisionId,
    const arangodb::velocypack::Slice& doc
) {
  TRI_ASSERT(_link);
  return _link->remove(trx, revisionId, doc, false);
}

Index::IndexType IResearchRocksDBLink::type() const {
  TRI_ASSERT(_link);
  return _link->type();
}

char const* IResearchRocksDBLink::typeName() const {
  TRI_ASSERT(_link);
  return _link->typeName();
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------