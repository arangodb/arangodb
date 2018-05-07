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
///        view deletion marker (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string DELETED_FIELD("deleted");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view globaly-unique id (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string GLOBALLY_UNIQUE_ID_FIELD("globallyUniqueId");

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
/// @brief the name of the field in the IResearch View definition denoting the
///        view name (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string NAME_FIELD("name");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view plan ID (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string PLAN_ID_FIELD("planId");

////////////////////////////////////////////////////////////////////////////////
/// @brief the name of the field in the IResearch View definition denoting the
///        view type (from LogicalView.cpp)
////////////////////////////////////////////////////////////////////////////////
const std::string TYPE_FIELD("type");

////////////////////////////////////////////////////////////////////////////////
/// @brief the view name prefix of per-cid view instances
////////////////////////////////////////////////////////////////////////////////
std::string const VIEW_NAME_PREFIX("_iresearch_");

////////////////////////////////////////////////////////////////////////////////
/// @brief a key in the jSON definition that differentiates a view-cid container
///        from individual per-cid view implementation
///        (view types are identical)
////////////////////////////////////////////////////////////////////////////////
std::string const VIEW_CONTAINER_MARKER("master");

////////////////////////////////////////////////////////////////////////////////
/// @brief generate the name used for the per-cid views
///        must be unique to avoid view collisions in vocbase
////////////////////////////////////////////////////////////////////////////////
std::string generateName(
    std::string const& viewName,
    TRI_voc_cid_t viewId,
    TRI_voc_cid_t collectionId
) {
  return VIEW_NAME_PREFIX
   + std::to_string(collectionId)
   + "_" + std::to_string(viewId)
   + "_" + viewName
   ;
}

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
   _meta(
     info.isObject() && info.get(StaticStrings::PropertiesField).isObject()
     ? info.get(StaticStrings::PropertiesField) : emptyObjectSlice()
   ),
   _persistedPath(getPersistedPath(dbPathFeature, id())) {
}

IResearchViewDBServer::~IResearchViewDBServer() {
  _collections.clear(); // ensure view distructors called before mutex is deallocated
}

arangodb::Result IResearchViewDBServer::drop() {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

  for (auto itr = _collections.begin(); itr != _collections.end();) {
    auto res = vocbase().dropView(itr->second->id(), true); // per-cid collections always system

    if (!res.ok()) {
      return res; // fail on first failure
    }

    itr = _collections.erase(itr);
  }

  return arangodb::Result();
}

arangodb::Result IResearchViewDBServer::drop(TRI_voc_cid_t cid) {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
  auto itr = _collections.find(cid);

  if (itr == _collections.end()) {
    return arangodb::Result(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  auto res = vocbase().dropView(itr->second->id(), true); // per-cid collections always system

  if (res.ok()) {
    _collections.erase(itr);
  }

  return res;
}

std::shared_ptr<arangodb::LogicalView> IResearchViewDBServer::ensure(
    TRI_voc_cid_t cid
) {
  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read
  auto itr = _collections.find(cid);

  if (itr != _collections.end()) {
    return itr->second;
  }

  static const std::function<bool(irs::string_ref const& key)> acceptor = [](
      irs::string_ref const& key
  )->bool {
    return key != StaticStrings::CollectionsField && key != StaticStrings::LinksField; // ignored fields
  };
  arangodb::velocypack::Builder builder;

  builder.openObject();
  builder.add(IS_SYSTEM_FIELD, arangodb::velocypack::Value(true)); // required to for use of VIEW_NAME_PREFIX
  builder.add(NAME_FIELD, toValuePair(generateName(name(), id(), cid))); // mark the view definition as an internal per-cid instance
  builder.add(TYPE_FIELD, toValuePair(DATA_SOURCE_TYPE.name())); // type required for proper factory selection

  {
    builder.add(
      StaticStrings::PropertiesField,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
    );

    if (!mergeSliceSkipKeys(builder, _meta.slice(), acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate properties definition while constructing IResearch View in database '" << vocbase().id() << "'";

        return nullptr;
    }

    builder.close(); // close StaticStrings::PropertiesField
  }

  builder.close();

  auto view = vocbase().createView(builder.slice());

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View for collection '" << cid << "' in database '" << vocbase().name() << "'";

    return nullptr;
  }

  auto id = view->id();

  // hold a reference to the original view in the deleter so that the view is still valid inside the deleter
  return std::shared_ptr<arangodb::LogicalView>(
    view.get(),
    [this, id, cid](arangodb::LogicalView* ptr)->void {
      static const auto visitor = [](TRI_voc_cid_t)->bool { return false; };

      // same view in vocbase and with no collections
      if (ptr
          && ptr == vocbase().lookupView(id).get() // avoid double dropView(...)
          && ptr->visitCollections(visitor)) {
        drop(cid);
      }
    }
  );
}

/*static*/ std::shared_ptr<LogicalView> IResearchViewDBServer::make(
    TRI_vocbase_t& vocbase,
    arangodb::velocypack::Slice const& info,
    bool isNew,
    uint64_t planVersion,
    LogicalView::PreCommitCallback const& preCommit /*= {}*/
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
      return wiew->updateProperties(info.get(StaticStrings::PropertiesField), false, true).ok() // 'false' because full view definition
        ? wiew : nullptr;
    }

    // if creation request not coming from the vocbase
    if (!info.hasKey(VIEW_CONTAINER_MARKER)) {
      arangodb::velocypack::Builder builder;

      builder.openObject();

      if (!mergeSlice(builder, info)) {
        LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
          << "failure to generate definition while constructing IResearch View in database '" << vocbase.id() << "'";

          return nullptr;
      }

      builder.add(
        VIEW_CONTAINER_MARKER,
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
      );
      builder.close();

      return vocbase.createView(builder.slice());
    }

    auto* feature =
      arangodb::iresearch::getFeature<arangodb::DatabasePathFeature>("DatabasePath");

    if (!feature) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to find feature 'DatabasePath' while constructing IResearch View in database '" << vocbase.id() << "'";

      return nullptr;
    }

    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      return key != VIEW_CONTAINER_MARKER; // ignored internally injected filed
    };
    arangodb::velocypack::Builder builder;

    builder.openObject();

    if (!mergeSliceSkipKeys(builder, info, acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate definition while constructing IResearch View in database '" << vocbase.name() << "'";

      return nullptr;
    }

    builder.close();

    PTR_NAMED(IResearchViewDBServer, view, vocbase, builder.slice(), *feature, planVersion);

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

  // parse view names created by generateName(...)
  auto* begin = name.c_str() + VIEW_NAME_PREFIX.size();
  auto size = name.size() - VIEW_NAME_PREFIX.size();
  auto* end = (char*)::memchr(begin, '_', size);

  if (!end) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to determine collection ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref collectionId(begin, end - begin);

  begin = end + 1; // +1 for '_'
  size -= collectionId.size() + 1; // +1 for '_'
  end = (char*)::memchr(begin, '_', size);

  if (!end) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to determine view ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  irs::string_ref viewId(begin, end - begin);

  begin = end + 1; // +1 for '_'
  size -= viewId.size() + 1; // +1 for '_'

  irs::string_ref viewName(begin, size);
  char* suffix;
  auto cid = std::strtoull(collectionId.c_str(), &suffix, 10); // 10 for base-10

  if (suffix != collectionId.c_str() + collectionId.size()) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failed to parse collection ID while constructing IResearch View in database '" << vocbase.name() << "'";

    return nullptr;
  }

  auto wiew = vocbase.lookupView(viewId); // always look up by view ID since it cannot change

  // create DBServer view
  if (!wiew) {
    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      return key != ID_FIELD && key != IS_SYSTEM_FIELD && key != NAME_FIELD; // ignored fields
    };
    arangodb::velocypack::Builder builder;

    builder.openObject();
    builder.add(ID_FIELD, arangodb::velocypack::Value(viewId));
    builder.add(NAME_FIELD, arangodb::velocypack::Value(viewName));
    builder.add(// mark the view definition as a DBServer instance
      VIEW_CONTAINER_MARKER,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Null)
    );

    if (!mergeSliceSkipKeys(builder, info, acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate definition while constructing IResearch View in database '" << vocbase.name() << "'";

        return nullptr;
    }

    builder.close();
    wiew = vocbase.createView(builder.slice());
  }

  // TODO FIXME find a better way to look up an iResearch View
  #ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    auto* view = dynamic_cast<IResearchViewDBServer*>(wiew.get());
  #else
    auto* view = static_cast<IResearchViewDBServer*>(wiew.get());
  #endif

  if (!view) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View '" << std::string(name) << "' in database '" << vocbase.name() << "'";

    return nullptr;
  }

  auto impl = IResearchView::make(vocbase, info, isNew, planVersion, preCommit);

  if (!impl) {
    LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
      << "failure while creating an IResearch View for collection '" << cid << "' in database '" << vocbase.name() << "'";

    return nullptr;
  }

  WriteMutex mutex(view->_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously read

  return view->_collections.emplace(cid, impl).first->second;
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
    bool /*doSync*/
) {
  ReadMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // 'collections_' can be asynchronously modified

  for (auto& entry: _collections) {
    auto res = vocbase().renameView(
      entry.second, generateName(newName, id(), entry.first)
    );

    if (TRI_ERROR_NO_ERROR != res) {
      return res; // fail on first failure
    }
  }

  name(std::move(newName));

  return arangodb::Result();
}

void IResearchViewDBServer::toVelocyPack(
    arangodb::velocypack::Builder& result,
    bool includeProperties,
    bool includeSystem
) const {
  TRI_ASSERT(result.isOpenObject());

  result.add(ID_FIELD, arangodb::velocypack::Value(std::to_string(id())));
  result.add(NAME_FIELD, arangodb::velocypack::Value(name()));
  result.add(TYPE_FIELD, arangodb::velocypack::Value(type().name()));

  if (includeSystem) {
    result.add(DELETED_FIELD, arangodb::velocypack::Value(deleted()));
    result.add(GLOBALLY_UNIQUE_ID_FIELD, arangodb::velocypack::Value(guid()));
    result.add(IS_SYSTEM_FIELD, arangodb::velocypack::Value(system()));
    result.add(PLAN_ID_FIELD, arangodb::velocypack::Value(std::to_string(planId())));
  }

  if (includeProperties) {
    static const std::function<bool(irs::string_ref const& key)> acceptor = [](
        irs::string_ref const& key
    )->bool {
      return key != StaticStrings::CollectionsField && key != StaticStrings::LinksField; // ignored fields
    };
    ReadMutex mutex(_mutex);
    SCOPED_LOCK(mutex); // '_collections'/'_meta' can be asynchronously modified

    result.add(
      StaticStrings::PropertiesField,
      arangodb::velocypack::Value(arangodb::velocypack::ValueType::Object)
    );

    {
      result.add(
        StaticStrings::CollectionsField,
        arangodb::velocypack::Value(arangodb::velocypack::ValueType::Array)
      );

      for (auto& entry: _collections) {
        result.add(arangodb::velocypack::Value(entry.first));
      }

      result.close(); // close StaticStrings::CollectionsField
    }

    if (!mergeSliceSkipKeys(result, _meta.slice(), acceptor)) {
      LOG_TOPIC(WARN, arangodb::iresearch::TOPIC)
        << "failure to generate definition while properties generating jSON IResearch View in database '" << vocbase().name() << "'";
    }

    result.close(); // close StaticStrings::PropertiesField
  }
}

arangodb::Result IResearchViewDBServer::updateProperties(
  arangodb::velocypack::Slice const& properties,
  bool partialUpdate,
  bool doSync
) {
  if (!properties.isObject()) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("invalid properties supplied while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  // ...........................................................................
  // sanitize update slice
  // ...........................................................................

  static const std::function<bool(irs::string_ref const& key)> propsAcceptor = [](
      irs::string_ref const& key
  )->bool {
    return key != StaticStrings::CollectionsField && key != StaticStrings::LinksField; // ignored fields
  };
  arangodb::velocypack::Builder props;

  props.openObject();

  if (!mergeSliceSkipKeys(props, properties, propsAcceptor)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  props.close();

  IResearchViewMeta meta;
  std::string error;

  if (partialUpdate) {
    IResearchViewMeta oldMeta;

    if (!oldMeta.init(_meta.slice(), error)
        || !meta.init(props.slice(), error, oldMeta)) {
      return arangodb::Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failure parsing properties while updating IResearch View in database '") + vocbase().name() + "'"
      );
    }
  } else if (!meta.init(props.slice(), error)) {
    return arangodb::Result(
      TRI_ERROR_BAD_PARAMETER,
      std::string("failure parsing properties while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  WriteMutex mutex(_mutex);
  SCOPED_LOCK(mutex); // '_meta' can be asynchronously read

  meta._collections.clear();

  for (auto& entry: _collections) {
    meta._collections.emplace(entry.first);
  }

  // ...........................................................................
  // prepare replacement '_meta'
  // ...........................................................................

  arangodb::velocypack::Builder builder;

  builder.openObject();

  if (!meta.json(builder)) {
    return arangodb::Result(
      TRI_ERROR_INTERNAL,
      std::string("failure to generate 'properties' definition while updating IResearch View in database '") + vocbase().name() + "'"
    );
  }

  builder.close();

  // ...........................................................................
  // update per-cid views
  // ...........................................................................

  for (auto& entry: _collections) {
    auto res =
      entry.second->updateProperties(props.slice(), partialUpdate, doSync);

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
