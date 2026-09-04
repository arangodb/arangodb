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
///
////////////////////////////////////////////////////////////////////////////////

#include "IndexDefinitionRegistry.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Indexes/Index.h"
#include "Indexes/IndexFactory.h"
#include "RestServer/BootstrapFeature.h"
#include "Utilities/NameValidator.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

#include <absl/strings/numbers.h>
#include <absl/strings/str_cat.h>

namespace {

using namespace arangodb;

struct InvalidIndexDefinition : public IndexDefinition {
  explicit InvalidIndexDefinition(application_features::ApplicationServer& server)
      : IndexDefinition(server) {}

  bool equal(velocypack::Slice, velocypack::Slice,
            std::string const&) const override {
    return false;  // invalid definitions are never equal
  }

  Result normalize(velocypack::Builder&, velocypack::Slice definition, bool,
                   Database const&) const override {
    std::string type = basics::VelocyPackHelper::getStringValue(
        definition, StaticStrings::IndexType, "");
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid index type '" + type + "'");
  }
};

}  // namespace

namespace arangodb {

IndexDefinitionRegistry::IndexDefinitionRegistry(
    application_features::ApplicationServer& server)
    : _server(server),
      _definitions(),
      _invalid(std::make_unique<InvalidIndexDefinition>(server)) {}

Result IndexDefinitionRegistry::emplace(std::string const& type,
                                        IndexDefinition const& definition) {
  if (_server.hasFeature<BootstrapFeature>()) {
    auto& feature = _server.getFeature<BootstrapFeature>();
    // ensure new definitions are not added at runtime since that would
    // require additional locks
    if (feature.isReady()) {
      return Result(TRI_ERROR_INTERNAL,
                    std::string("index definition registration is only "
                                "allowed during server startup"));
    }
  }

  if (!_definitions.try_emplace(type, &definition).second) {
    return Result(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                  std::string("index definition previously registered "
                              "during index definition registration for "
                              "index type '") +
                      type + "'");
  }

  return Result();
}

IndexDefinition const& IndexDefinitionRegistry::definition(
    std::string const& type) const noexcept {
  auto itr = _definitions.find(type);
  TRI_ASSERT(itr == _definitions.end() ||
            false == !(itr->second));  // emplace(...) inserts non-nullptr

  return itr == _definitions.end() ? *_invalid : *(itr->second);
}

Result IndexDefinitionRegistry::enhanceIndexDefinition(
    velocypack::Slice definition, velocypack::Builder& normalized,
    bool isCreation, Database const& vocbase) const {
  auto type = definition.get(StaticStrings::IndexType);

  if (!type.isString()) {
    return Result(TRI_ERROR_BAD_PARAMETER, "invalid index type");
  }

  auto& def = IndexDefinitionRegistry::definition(type.copyString());

  TRI_ASSERT(normalized.isEmpty());

  try {
    velocypack::ObjectBuilder b(&normalized);
    auto const id = helpers::extractId(definition);
    if (id.isSet()) {
      absl::AlphaNum toStr{id.id()};
      normalized.add(StaticStrings::IndexId, velocypack::Value{toStr.Piece()});
    }

    std::string name{helpers::extractName(definition)};

    if (name.empty()) {
      // we should set the name for special types explicitly elsewhere,
      // but just in case...
      if (auto t = Index::type(type.stringView()); t == IndexType::Primary) {
        name = StaticStrings::IndexNamePrimary;
      } else if (t == IndexType::Edge) {
        name = StaticStrings::IndexNameEdge;
      } else {
        // generate a name
        name = absl::StrCat("idx_", TRI_HybridLogicalClock());
      }
    }

    if (!name.empty()) {
      bool extendedNames = vocbase.extendedNames();
      if (auto res = IndexNameValidator::validateName(extendedNames, name);
          res.fail()) {
        return res;
      }
    }

    normalized.add(StaticStrings::IndexName, velocypack::Value(name));

    return def.normalize(normalized, definition, isCreation, vocbase);
  } catch (basics::Exception const& ex) {
    return Result(ex.code(), ex.what());
  } catch (std::exception const& ex) {
    return Result(TRI_ERROR_INTERNAL, ex.what());
  } catch (...) {
    return Result(TRI_ERROR_INTERNAL, "unknown exception");
  }
}

std::vector<std::pair<std::string_view, std::string_view>>
IndexDefinitionRegistry::indexAliases(uint32_t /*apiVersion*/) const {
  return {};
}

}  // namespace arangodb
