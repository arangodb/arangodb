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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "LogicalView.h"

#include "RestServer/ViewTypesFeature.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "velocypack/Iterator.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view properties
////////////////////////////////////////////////////////////////////////////////
const std::string PROPERTIES_FIELD("properties");

}

namespace arangodb {

// -----------------------------------------------------------------------------
// --SECTION--                                                       LogicalView
// -----------------------------------------------------------------------------

// @brief Constructor used in coordinator case.
// The Slice contains the part of the plan that
// is relevant for this view
LogicalView::LogicalView(
    TRI_vocbase_t& vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalDataSource(
     category(),
     LogicalDataSource::Type::emplace(
       arangodb::basics::VelocyPackHelper::getStringRef(
         definition, StaticStrings::DataSourceType, ""
       )
     ),
     vocbase,
     definition,
     planVersion
   ) {
  // ensure that the 'definition' was used as the configuration source
  if (!definition.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "got an invalid view definition while constructing LogicalView"
    );
  }

  if (!TRI_vocbase_t::IsAllowedName(definition)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_ILLEGAL_NAME);
  }

  if (!id()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
      TRI_ERROR_BAD_PARAMETER,
      "got invalid view identifier while constructing LogicalView"
    );
  }

  // update server's tick value
  TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id()));
}

/*static*/ LogicalDataSource::Category const& LogicalView::category() noexcept {
  static const Category category;

  return category;
}

/*static*/ std::shared_ptr<LogicalView> LogicalView::create(
    TRI_vocbase_t& vocbase,
    velocypack::Slice definition,
    bool isNew,
    uint64_t planVersion /*= 0*/,
    PreCommitCallback const& preCommit /*= PreCommitCallback()*/
) {
  auto const* viewTypes =
    application_features::ApplicationServer::getFeature<ViewTypesFeature>("ViewTypes");

  if (!viewTypes) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure to get 'ViewTypes' feature while creating LogicalView";

    return nullptr;
  }

  auto const viewType = basics::VelocyPackHelper::getStringRef(
    definition, StaticStrings::DataSourceType, ""
  );
  auto const& dataSourceType =
    arangodb::LogicalDataSource::Type::emplace(viewType);
  auto const& viewFactory = viewTypes->factory(dataSourceType);

  if (!viewFactory) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Found view type for which there is no factory, type: "
      << viewType.toString();

    return nullptr;
  }

  auto view = viewFactory(vocbase, definition, isNew, planVersion, preCommit);

  if (!view) {
    LOG_TOPIC(ERR, Logger::VIEWS)
      << "Failure to instantiate view of type: " << viewType.toString();

    return nullptr;
  }

  return view;
}

// -----------------------------------------------------------------------------
// --SECTION--                                               DBServerLogicalView
// -----------------------------------------------------------------------------

DBServerLogicalView::DBServerLogicalView(
    TRI_vocbase_t& vocbase,
    VPackSlice const& definition,
    uint64_t planVersion
): LogicalView(vocbase, definition, planVersion) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
}

DBServerLogicalView::~DBServerLogicalView() {
  if (deleted()) {
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    TRI_ASSERT(engine);

    engine->destroyView(vocbase(), *this);
  }
}

arangodb::Result DBServerLogicalView::appendVelocyPack(
    arangodb::velocypack::Builder& builder,
    bool detailed,
    bool forPersistence
) const {
  if (!builder.isOpenObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid builder provided for IResearchViewDBServer definition")
    );
  }

  builder.add(
    StaticStrings::DataSourceType,
    arangodb::velocypack::Value(type().name())
  );

  // note: includeSystem and forPersistence are not 100% synonymous,
  // however, for our purposes this is an okay mapping; we only set
  // includeSystem if we are persisting the properties
  if (forPersistence) {
    // storage engine related properties
    StorageEngine* engine = EngineSelectorFeature::ENGINE;

    if (!engine) {
      return TRI_ERROR_INTERNAL;
    }

    engine->getViewProperties(vocbase(), *this, builder);
  }

  if (detailed) {
    // implementation Information
    builder.add(
      PROPERTIES_FIELD,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
    );
    getPropertiesVPack(builder, forPersistence);
    builder.close();
  }

  // ensure that the object is still open
  if (!builder.isOpenObject()) {
    return TRI_ERROR_INTERNAL;
  }

  return arangodb::Result();
}

/*static*/ arangodb::Result DBServerLogicalView::create(
    DBServerLogicalView const& view
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  try {
    #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      // during recovery entry is being played back from the engine
      if (!engine->inRecovery()) {
        arangodb::velocypack::Builder builder;
        auto res = engine->getViews(view.vocbase(), builder);
        TRI_ASSERT(TRI_ERROR_NO_ERROR == res);
        auto slice  = builder.slice();
        TRI_ASSERT(slice.isArray());
        auto viewId = std::to_string(view.id());

        // We have not yet persisted this view
        for (auto entry: arangodb::velocypack::ArrayIterator(slice)) {
          auto id = arangodb::basics::VelocyPackHelper::getStringRef(
            entry,
            StaticStrings::DataSourceId,
            arangodb::velocypack::StringRef()
          );

          if (!id.compare(viewId)) {
            return arangodb::Result(
              TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
              std::string("view id '") + viewId + "already exists in the sotrage engine"
            );
          }
        }
      }
    #endif

    engine->createView(view.vocbase(), view.id(), view);

    return engine->persistView(view.vocbase(), view);
  } catch (std::exception const& e) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during storage engine persistance of view '") + view.name() + "': " + e.what()
    );
  } catch (...) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("caught exception during storage engine persistance of view '") + view.name() + "'"
    );
  }
}

arangodb::Result DBServerLogicalView::drop() {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);
  auto res = dropImpl();

  if (res.ok()) {
    deleted(true);
    engine->dropView(vocbase(), *this);
  }

  return res;
}

Result DBServerLogicalView::rename(std::string&& newName, bool doSync) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  auto oldName = name();

  try {
    name(std::move(newName));

    // store new view definition to disk
    if (!engine->inRecovery()) {
      engine->changeView(vocbase(), id(), *this, doSync);
    }
  } catch (basics::Exception const& ex) {
    name(std::move(oldName));

    return ex.code();
  } catch (...) {
    name(std::move(oldName));

    return TRI_ERROR_INTERNAL;
  }

  // write WAL 'rename' marker
  return engine->renameView(vocbase(), *this, oldName);
}

arangodb::Result DBServerLogicalView::updateProperties(
    VPackSlice const& slice,
    bool partialUpdate,
    bool doSync
) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto res = updateProperties(slice, partialUpdate);

  if (!res.ok()) {
    return res;
  }

  // after this call the properties are stored
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  TRI_ASSERT(engine);

  if (engine->inRecovery()) {
    return arangodb::Result(); // do not modify engine while in recovery
  }

  try {
    engine->changeView(vocbase(), id(), *this, doSync);
  } catch (arangodb::basics::Exception const& e) {
    return { e.code() };
  } catch (...) {
    return { TRI_ERROR_INTERNAL };
  }

  return {};
}

} // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------