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

#include "RocksDBIndexFactory.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "RocksDBEngine/RocksDBEdgeIndex.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "VocBase/voc-types.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

int RocksDBIndexFactory::enhanceIndexDefinition(VPackSlice const definition,
                                                VPackBuilder& enhanced,
                                                bool create) const {
  THROW_ARANGO_NOT_YET_IMPLEMENTED(); 
  return 0;
}

std::shared_ptr<Index> RocksDBIndexFactory::prepareIndexFromSlice(
    arangodb::velocypack::Slice info, bool generateKey, LogicalCollection* col,
    bool isClusterConstructor) const {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  // extract type
  VPackSlice value = info.get("type");

  if (!value.isString()) {
    // Compatibility with old v8-vocindex.
    if (generateKey) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "invalid index type definition");
    }
  }

  std::string tmp = value.copyString();
  arangodb::Index::IndexType const type = arangodb::Index::type(tmp.c_str());

  std::shared_ptr<Index> newIdx;

  TRI_idx_iid_t iid = 0;
  value = info.get("id");
  if (value.isString()) {
    iid = basics::StringUtils::uint64(value.copyString());
  } else if (value.isNumber()) {
    iid =
        basics::VelocyPackHelper::getNumericValue<TRI_idx_iid_t>(info, "id", 0);
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot restore index without index identifier");
  }

  if (iid == 0 && !isClusterConstructor) {
    // Restore is not allowed to generate in id
    TRI_ASSERT(generateKey);
    iid = arangodb::Index::generateId();
  }

  switch (type) {
    case arangodb::Index::TRI_IDX_TYPE_UNKNOWN: {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid index type");
    }
    case arangodb::Index::TRI_IDX_TYPE_PRIMARY_INDEX: {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot create primary index");
      }
      newIdx.reset(new arangodb::RocksDBPrimaryIndex(col));
      break;
    }
    case arangodb::Index::TRI_IDX_TYPE_EDGE_INDEX: {
      if (!isClusterConstructor) {
        // this indexes cannot be created directly
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot create edge index");
      }
      newIdx.reset(new arangodb::RocksDBEdgeIndex(iid, col));
      break;
    }
    
    default: {
      THROW_ARANGO_NOT_YET_IMPLEMENTED();
      break;
    }
  }

  if (newIdx == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  return newIdx;
}

void RocksDBIndexFactory::fillSystemIndexes(
    arangodb::LogicalCollection* col,
    std::vector<std::shared_ptr<arangodb::Index>>& systemIndexes) const {
  // create primary index
  systemIndexes.emplace_back(
      std::make_shared<arangodb::RocksDBPrimaryIndex>(col));

  // create edges index
  if (col->type() == TRI_COL_TYPE_EDGE) {
    systemIndexes.emplace_back(
        std::make_shared<arangodb::RocksDBEdgeIndex>(1, col));
  }
}
