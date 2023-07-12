////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "CreateCollectionBody.h"

#include "VocBase/Properties/DatabaseConfiguration.h"
#include "Inspection/VPack.h"

#include <velocypack/Collection.h>
#include <velocypack/Slice.h>

using namespace arangodb;

CreateCollectionBody::CreateCollectionBody() {}

ResultT<CreateCollectionBody> parseAndValidate(
    DatabaseConfiguration const& config, VPackSlice input,
    std::function<void(CreateCollectionBody&)> applyDefaults) {
  try {
    CreateCollectionBody res;
    applyDefaults(res);
    auto status =
        velocypack::deserializeWithStatus(input, res, {}, InspectUserContext{});
    if (status.ok()) {
      // Inject default values, and finally check if collection is allowed
      auto result = res.applyDefaultsAndValidateDatabaseConfiguration(config);
      if (result.fail()) {
        return result;
      }
      return res;
    }
    if (status.path() == "name") {
      // Special handling to be backwards compatible error reporting
      // on "name"
      return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
    }

    if (status.path().rfind("keyOptions", 0) == 0) {
      // Special handling to be backwards compatible error reporting
      // on "keyOptions"
      return Result{TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR, status.error()};
    }
    if (status.path() == StaticStrings::SmartJoinAttribute) {
      return Result{TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE, status.error()};
    }
    return Result{
        TRI_ERROR_BAD_PARAMETER,
        status.error() +
            (status.path().empty() ? "" : " on path " + status.path())};
  } catch (basics::Exception const& e) {
    return Result{e.code(), e.message()};
  } catch (std::exception const& e) {
    return Result{TRI_ERROR_INTERNAL, e.what()};
  }
}

ResultT<CreateCollectionBody> CreateCollectionBody::fromCreateAPIBody(
    VPackSlice input, DatabaseConfiguration const& config) {
  if (!input.isObject()) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }
  return ::parseAndValidate(config, input, [](CreateCollectionBody& col) {});
}

ResultT<CreateCollectionBody> CreateCollectionBody::fromCreateAPIV8(
    VPackSlice properties, std::string const& name, TRI_col_type_e type,
    DatabaseConfiguration const& config) {
  if (name.empty()) {
    // Special handling to be backwards compatible error reporting
    // on "name"
    return Result{TRI_ERROR_ARANGO_ILLEGAL_NAME};
  }
  return ::parseAndValidate(config, properties,
                            [&name, &type](CreateCollectionBody& col) {
                              // Inject the default values given by V8 in a
                              // separate parameter
                              col.type = type;
                              col.name = name;
                            });
}

arangodb::velocypack::Builder
CreateCollectionBody::toCreateCollectionProperties(
    std::vector<CreateCollectionBody> const& collections) {
  arangodb::velocypack::Builder builder;
  VPackArrayBuilder guard{&builder};
  for (auto const& c : collections) {
    // TODO: This is copying multiple times.
    // Nevermind this is only temporary code.
    builder.add(c.toCollectionsCreate().slice());
  }
  return builder;
}

arangodb::velocypack::Builder CreateCollectionBody::toCollectionsCreate()
    const {
  arangodb::velocypack::Builder builder;
  arangodb::velocypack::serializeWithContext(builder, *this,
                                             InspectUserContext{});
  // TODO: This is a hack to erase attributes that are not expected by follow up
  // APIS, it should be obsolete after refactoring is completed
  std::vector<std::string> attributesToErase{};
  if (builder.slice().hasKey(StaticStrings::SmartJoinAttribute) &&
      builder.slice()
          .get(StaticStrings::SmartJoinAttribute)
          .isEqualString("")) {
    // TODO: This is a hack to erase the SmartJoin attribute if it was not set
    // We need this to satisfy the API checks in LogicalCollection
    // `initializeSmartAttributes`
    attributesToErase.emplace_back(StaticStrings::SmartJoinAttribute);
  }
  if (builder.slice().hasKey(StaticStrings::ShardingStrategy) &&
      builder.slice().get(StaticStrings::ShardingStrategy).isEqualString("")) {
    // TODO: This is a hack to erase the ShardingStrategy attribute if it was
    // not set
    attributesToErase.emplace_back(StaticStrings::ShardingStrategy);
  }

  if (builder.slice().hasKey(StaticStrings::GraphSmartGraphAttribute) &&
      builder.slice()
          .get(StaticStrings::GraphSmartGraphAttribute)
          .isEqualString("")) {
    // TODO: This is a hack to erase the SmartGraphAttribute attribute if it was
    // not set
    attributesToErase.emplace_back(StaticStrings::GraphSmartGraphAttribute);
  }
  if (!attributesToErase.empty()) {
    return VPackCollection::remove(builder.slice(), attributesToErase);
  }
  return builder;
}
