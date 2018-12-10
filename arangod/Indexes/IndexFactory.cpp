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
#include "VocBase/LogicalCollection.h"

#include <velocypack/Slice.h>

namespace {

struct InvalidIndexFactory: public arangodb::IndexTypeFactory {
  virtual bool equal(
      arangodb::velocypack::Slice const&,
      arangodb::velocypack::Slice const&
  ) const override {
    return false; // invalid definitions are never equal
  }

  virtual arangodb::Result instantiate(
      std::shared_ptr<arangodb::Index>&,
      arangodb::LogicalCollection&,
      arangodb::velocypack::Slice const& definition,
      TRI_idx_iid_t,
      bool
  ) const override {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failure to instantiate index without a factory for definition: ") + definition.toString()
    );
  }

  virtual arangodb::Result normalize(
      arangodb::velocypack::Builder&,
      arangodb::velocypack::Slice definition,
      bool
  ) const override {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failure to normalize index without a factory for definition: ") + definition.toString()
    );
  }
};

InvalidIndexFactory const INVALID;

} // namespace

namespace arangodb {

void IndexFactory::clear() {
  _factories.clear();
}

Result IndexFactory::emplace(
  std::string const& type,
  IndexTypeFactory const& factory
) {
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

  if (!_factories.emplace(type, &factory).second) {
    return arangodb::Result(
      TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
      std::string("index factory previously registered during index factory registration for index type '") + type + "'"
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

  auto& factory = IndexFactory::factory(type.copyString());

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

    return factory.normalize(normalized, definition, isCreation);
  } catch (basics::Exception const& ex) {
    return ex.code();
  } catch (std::exception const&) {
    return TRI_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return TRI_ERROR_INTERNAL;
  }
}

const IndexTypeFactory& IndexFactory::factory(
    std::string const& type
) const noexcept {
  auto itr = _factories.find(type);
  TRI_ASSERT(itr == _factories.end() || false == !(itr->second)); // IndexFactory::emplace(...) inserts non-nullptr

  return itr == _factories.end() ? INVALID : *(itr->second);
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

  auto& factory = IndexFactory::factory(type.copyString());
  std::shared_ptr<Index> index;
  auto res = factory.instantiate(
    index, collection, definition, id, isClusterConstructor
  );

  if (!res.ok()) {
    TRI_set_errno(res.errorNumber());
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "failed to instantiate index, error: " << res.errorNumber() << " " << res.errorMessage();

    return nullptr;
  }

  if (!index) {
    TRI_set_errno(TRI_ERROR_INTERNAL);
    LOG_TOPIC(ERR, arangodb::Logger::ENGINES)
      << "failed to instantiate index, factory returned null instance";

    return nullptr;
  }

  return index;
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
    VPackSlice type = info.get(StaticStrings::IndexType);
    // dont generate ids for indexes of type "primary"
    // id 0 is expected for primary indexes
    if (!type.isString() || !type.isEqualString("primary")) {
      TRI_ASSERT(generateKey);
      iid = arangodb::Index::generateId();
    }
  }

  return iid;
}

} // arangodb
