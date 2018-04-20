////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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

#include "ApplicationServerHelper.h"
#include "IResearchCommon.h"
#include "IResearchView.h"
#include "IResearchViewDBServer.h"
#include "VelocyPackHelper.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "VocBase/vocbase.h"

NS_LOCAL

typedef irs::async_utils::read_write_mutex::read_mutex ReadMutex;
typedef irs::async_utils::read_write_mutex::write_mutex WriteMutex;

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view id (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string ID_FIELD("id");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view id (from vocbase.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string IS_SYSTEM_FIELD("isSystem");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the iResearch View definition denoting the
///        corresponding link definitions
////////////////////////////////////////////////////////////////////////////////
const std::string LINKS_FIELD("links");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view name (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string NAME_FIELD("name");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view properties (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string PROPERTIES_FIELD("properties");

static std::string const VIEW_NAME_PREFIX("_iresearch_");

////////////////////////////////////////////////////////////////////////////////
/// @brief a key in the jSON definition that differentiates a view-cid container
///        from individual per-cid view implementation
///        (view types are identical)
////////////////////////////////////////////////////////////////////////////////
static std::string const VIEW_CONTAINER_MARKER("master");

////////////////////////////////////////////////////////////////////////////////
/// @brief compute the data path to user for iresearch persisted-store
///        get base path from DatabaseServerFeature (similar to MMFilesEngine)
///        the path is hardcoded to reside under:
///        <DatabasePath>/<IResearchView::type()>-<view id>
///        similar to the data path calculation for collections
////////////////////////////////////////////////////////////////////////////////
irs::utf8_path getPersistedPath(
    arangodb::DatabasePathFeature const& dbPathFeature, TRI_voc_cid_t id
) {
  irs::utf8_path dataPath(dbPathFeature.directory());
  static const std::string subPath("databases");

  dataPath /= subPath;
  dataPath /= arangodb::iresearch::DATA_SOURCE_TYPE.name();
  dataPath += "-";
  dataPath += std::to_string(id);

  return dataPath;
}

NS_END

NS_BEGIN(arangodb)
NS_BEGIN(iresearch)

IResearchViewDBServer::IResearchViewDBServer(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    arangodb::DatabasePathFeature const& dbPathFeature,
    uint64_t planVersion
): LogicalView(vocbase, info, planVersion),
   _meta(info),
   _persistedPath(getPersistedPath(dbPathFeature, id())) {
}

std::shared_ptr<arangodb::LogicalView> IResearchViewDBServer::create(
    TRI_voc_cid_t cid
) {
  {
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified
    auto itr = _collections.find(cid);

    if (itr != _collections.end()) {
      return itr->second;
    }
  }

  auto view_name = VIEW_NAME_PREFIX + std::to_string(id()) + "_" + name();
  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(ID_FIELD, arangodb::velocypack::Value("0")); // force unique ID
  builder.add(NAME_FIELD, toValuePair(view_name)); // mark the view definition as an internal per-cid instance
  builder.add(IS_SYSTEM_FIELD, arangodb::velocypack::Value(true)); // required to for use of VIEW_NAME_PREFIX

  if (!mergeSlice(builder, _meta.slice())) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure to generate definition while constructing IResearch View in database '" << vocbase().id() << "'";

      return nullptr;
  }

  builder.close();

  auto view = IResearchView::make(vocbase(), builder.slice(), 0);

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View for collection '" << cid << "' in database '" << vocbase().name() << "'";

    return nullptr;
  }

  // hold a reference to the original view in the deleter so that the view is still valid inside the deleter
  return std::shared_ptr<arangodb::LogicalView>(
    view.get(),
    [this, cid, view](arangodb::LogicalView* ptr)->void {
      ReadMutex mutex(_mutex);
      SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

      if (ptr
          && _collections.find(cid) == _collections.end()
          && !ptr->drop().ok()) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure while dropping an IResearch View '" << ptr->id() << "' in database '" << ptr->vocbase().name() << "'";
      }
    }
  );
}

std::function<void()> IResearchViewDBServer::emplace(
  TRI_voc_cid_t cid,
  std::shared_ptr<arangodb::LogicalView> const& view
) {
  if (!view) {
    return {}; // invalid argument
  }

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
  auto itr = _collections.find(cid);

  if (itr != _collections.end() && itr->second.get() != view.get()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "encountered a duplicate definition while registering View for collection '" << cid << "' in database '" << vocbase().name() << "'";

    return {}; // duplicate definition
  }

  _collections.emplace(cid, view);

  return [this, cid]()->void {
    WriteMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

    if (!_collections.erase(cid)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "view not found while removing View for collection '" << cid << "' in database '" << vocbase().name() << "'";
    }
  };
}

/*static*/ std::shared_ptr<LogicalView> IResearchViewDBServer::make(
  TRI_vocbase_t& vocbase,
  arangodb::velocypack::Slice const& info,
  uint64_t planVersion,
  LogicalView::PreCommitCallback const& preCommit /*= LogicalView::PreCommitCallback()*/
) {
  if (!info.isObject()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "non-object definition supplied while instantiating IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref name;
  bool seen;

  if (!getString(name, info, NAME_FIELD, seen, std::string()) || !seen) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "definition supplied without a 'name' while instantiating IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  // not a per-cid view instance (get here from ClusterInfo)
  if (!irs::starts_with(name, VIEW_NAME_PREFIX)) {
    auto wiew = vocbase.lookupView(name);

    // DBServer view already exists, treat as an update
    if (wiew) {
      return wiew->updateProperties(info.get(PROPERTIES_FIELD), false, true).ok() // 'false' because full view definition
        ? wiew : nullptr;
    }

    // if creation request not coming from the vocbase
    if (!info.hasKey(VIEW_CONTAINER_MARKER)) {
      arangodb::velocypack::Builder builder;

      builder.openObject();
      builder.add(
        VIEW_CONTAINER_MARKER,
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
      );

      if (!mergeSlice(builder, info)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to generate definition while constructing IResearch View in database '" << vocbase.id() << "'";

          return nullptr;
      }

      return vocbase.createView(builder.close().slice());
    }

    auto* feature =
      arangodb::iresearch::getFeature<arangodb::DatabasePathFeature>("DatabasePath");

    if (!feature) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'DatabasePath' while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    PTR_NAMED(IResearchViewDBServer, view, vocbase, info, *feature, planVersion);

    if (preCommit && !preCommit(view)) {
      LOG_TOPIC(ERR, arangodb::iresearch::TOPIC)
        << "failure during pre-commit while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    return view;
  }

  // ...........................................................................
  // a per-cid view instance (get here only from StorageEngine startup or WAL recovery)
  // ...........................................................................

  auto* begin = name.c_str() + VIEW_NAME_PREFIX.size();
  auto* end = (char*)::memchr(begin, '_', name.size() - VIEW_NAME_PREFIX.size());

  if (!end) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to determine view ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref view_id(begin, end - begin);
  irs::string_ref view_name(end + 1, name.c_str() + name.size() - (end + 1)); // +1 for '_'
  IResearchViewMeta meta;
  std::string error;

  if (!meta.init(info.get(PROPERTIES_FIELD), error)
      || 1 != meta._collections.size()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to determine collection ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  auto cid = *(meta._collections.begin());
  auto wiew = vocbase.lookupView(view_id);

  // create DBServer view
  if (!wiew) {
    arangodb::velocypack::Builder builder;

    builder.openObject();
    builder.add(
      VIEW_CONTAINER_MARKER,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
    );

    if (!mergeSlice(builder, info)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate definition while constructing IResearch View in database '" << vocbase.name() << "'";

        return nullptr;
    }

    // mark the view definition as a DBServer instance
    builder.add(ID_FIELD, arangodb::velocypack::Value(view_id));
    builder.add(NAME_FIELD, arangodb::velocypack::Value(view_name));
    builder.close();

    wiew = vocbase.createView(builder.slice());

    if (!wiew) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure while creating an IResearch View '" << std::string(name) << "' in database '" << vocbase.name() << "'";

      return nullptr;
    }
  }

  // TODO FIXME find a better way to look up an iResearch View
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* impl = dynamic_cast<IResearchViewDBServer*>(wiew.get());
  #else
    auto* impl = static_cast<IResearchViewDBServer*>(wiew.get());
  #endif

  auto view = impl->create(cid);
  auto result = impl->emplace(cid, view);

  return result ? view : nullptr;
}

arangodb::Result IResearchViewDBServer::drop() {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    auto res = entry.second->drop();

    if (!res.ok()) {
      return res; // fail on first failure
    }
  }

  return arangodb::Result();
}

void IResearchViewDBServer::open() {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    entry.second->open();
  }
}

arangodb::Result IResearchViewDBServer::rename(
    std::string&& newName,
    bool doSync
) {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    auto res =  entry.second->rename(std::string(newName), doSync);

    if (!res.ok()) {
      return res; // fail on first failure
    }
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();

  if (!mergeSlice(builder, _meta.slice())) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate definition while renaming IResearch View in database '") + vocbase().name() + "'"
    );
  }

  builder.add(NAME_FIELD, arangodb::velocypack::Value(std::move(newName)));
  builder.close();
  _meta = std::move(builder);

  return arangodb::Result();
}

void IResearchViewDBServer::toVelocyPack(
    arangodb::velocypack::Builder& result,
    bool /*includeProperties*/,
    bool /*includeSystem*/
) const {
  TRI_ASSERT(result.isOpenObject());
  mergeSlice(result, _meta.slice());
}

arangodb::Result IResearchViewDBServer::updateProperties(
  arangodb::velocypack::Slice const& properties,
  bool partialUpdate,
  bool doSync
) {
  if (!properties.isObject() || properties.hasKey(LINKS_FIELD)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid properties supplied while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  IResearchViewMeta meta;
  std::string error;

  if (partialUpdate) {
    IResearchViewMeta oldMeta;

    if (!oldMeta.init(_meta.slice().get(PROPERTIES_FIELD), error)
        || !meta.init(properties, error, oldMeta)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure parsing properties while updating IResearch View in database '") + vocbase().name() + "'"
      );
    }
  } else if (!meta.init(properties, error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failure parsing properties while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  arangodb::velocypack::Builder builder;

  builder.openObject();

  if (!mergeSlice(builder, _meta.slice())) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  builder.add(
    PROPERTIES_FIELD,
    arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
  );

  if (!meta.json(builder)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate 'properties' definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  builder.close();

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

  for (auto& entry: _collections) {
    auto res = entry.second->updateProperties(properties, partialUpdate, doSync);

    if (!res.ok()) {
      return res; // fail on first failure
    }
  }

  _meta = std::move(builder);

  return arangodb::Result();
}

bool IResearchViewDBServer::visitCollections(
    CollectionVisitor const& visitor
) const {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    if (!visitor(entry.first)) {
      return false;
    }
  }

  return true;
}

NS_END // iresearch
NS_END // arangodb

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------