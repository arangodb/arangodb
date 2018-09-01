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

#include "IndexFactory.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Indexes/Index.h"
#include "RestServer/BootstrapFeature.h"

#include <velocypack/Slice.h>

namespace arangodb {

void IndexFactory::clear() {
  _factories.clear();
  _normalizers.clear();
}

Result IndexFactory::emplaceFactory(
  std::string const& type,
  IndexTypeFactory const& factory
) {
  if (!factory) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("index factory undefined during index factory registration for index type '") + type + "'"
    );
  }

  auto* feature =
    arangodb::application_features::ApplicationServer::lookupFeature("Bootstrap");
  auto* bootstrapFeature = dynamic_cast<BootstrapFeature*>(feature);

  // ensure new factories are not added at runtime since that would require additional locks
  if (bootstrapFeature && bootstrapFeature->isReady()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("index factory registration is only allowed during server startup")
    );
  }

  if (!_factories.emplace(type, factory).second) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
      std::string("index factory previously registered during index factory registration for index type '") + type + "'"
    );
  }

  return arangodb::Result();
}

Result IndexFactory::emplaceNormalizer(
  std::string const& type,
  IndexNormalizer const& normalizer
) {
  if (!normalizer) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("index normalizer undefined during index normalizer registration for index type '") + type + "'"
    );
  }

  auto* feature =
    arangodb::application_features::ApplicationServer::lookupFeature("Bootstrap");
  auto* bootstrapFeature = dynamic_cast<BootstrapFeature*>(feature);

  // ensure new normalizers are not added at runtime since that would require additional locks
  if (bootstrapFeature && bootstrapFeature->isReady()) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("index normalizer registration is only allowed during server startup")
    );
  }

  if (!_normalizers.emplace(type, normalizer).second) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
      std::string("index normalizer previously registered during index normalizer registration for index type '") + type + "'"
    );
  }

  return arangodb::Result();
}

Result IndexFactory::enhanceIndexDefinition(
  velocypack::Slice const definition,
  velocypack::Builder& normalized,
  bool isCreation,
  bool isCoordinator
) const {
  auto type = definition.get(StaticStrings::IndexType);

  if (!type.isString()) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  auto typeString = type.copyString();
  auto itr = _normalizers.find(typeString);

  if (itr == _normalizers.end()) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  TRI_ASSERT(ServerState::instance()->isCoordinator() == isCoordinator);
  TRI_ASSERT(normalized.isEmpty());

  try {
    velocypack::ObjectBuilder b(&normalized);
    auto idSlice = definition.get(StaticStrings::IndexId);
    uint64_t id = 0;

    if (idSlice.isNumber()) {
      id = idSlice.getNumericValue<uint64_t>();
    } else if (idSlice.isString()) {
      id = basics::StringUtils::uint64(idSlice.copyString());
    }

    if (id) {
      normalized.add(
        StaticStrings::IndexId,
        arangodb::velocypack::Value(std::to_string(id))
      );
    }

    return itr->second(normalized, definition, isCreation);
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (std::exception const&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

std::shared_ptr<Index> IndexFactory::prepareIndexFromSlice(
  velocypack::Slice definition,
  bool generateKey,
  LogicalCollection& collection,
  bool isClusterConstructor
) const {
  auto id = validateSlice(definition, generateKey, isClusterConstructor);
  auto type = definition.get(StaticStrings::IndexType);

  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER, "invalid index type definition"
    );
  }

  auto typeString = type.copyString();
  auto itr = _factories.find(typeString);

  if (itr == _factories.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_NOT_IMPLEMENTED,
      std::string("invalid or unsupported index type '") + typeString + "'"
    );
  }

  return itr->second(collection, definition, id, isClusterConstructor);
}

/// same for both storage engines
std::vector<std::string> IndexFactory::supportedIndexes() const {
  return std::vector<std::string>{"primary", "edge", "hash", "skiplist",
    "persistent", "geo",  "fulltext"};
}

TRI_idx_iid_t IndexFactory::validateSlice(arangodb::velocypack::Slice info, 
                                          bool generateKey, 
                                          bool isClusterConstructor) {
  if (!info.isObject()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }

  TRI_idx_iid_t iid = 0;
  auto value = info.get(StaticStrings::IndexId);

  if (value.isString()) {
    iid = basics::StringUtils::uint64(value.copyString());
  } else if (value.isNumber()) {
    iid = basics::VelocyPackHelper::getNumericValue<TRI_idx_iid_t>(
      info, StaticStrings::IndexId.c_str(), 0
    );
  } else if (!generateKey) {
    // In the restore case it is forbidden to NOT have id
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL, "cannot restore index without index identifier");
  }

  if (iid == 0 && !isClusterConstructor) {
    // Restore is not allowed to generate an id
    TRI_ASSERT(generateKey);
    iid = arangodb::Index::generateId();
  }

  return iid;
}

} // arangodb
