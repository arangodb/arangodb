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
/// @author Simon Gräter
////////////////////////////////////////////////////////////////////////////////

#include "UpgradeTasks.h"
#include "Agency/AgencyComm.h"
#include "Basics/Common.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/OperationOptions.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Methods/Indexes.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;
using application_features::ApplicationServer;
using basics::VelocyPackHelper;

// Note: this entire file should run with superuser rights

/// create a collection if it does not exists.
static bool createSystemCollection(TRI_vocbase_t* vocbase,
                                   std::string const& name) {
  Result res = methods::Collections::lookup(vocbase, name,
                                            [](LogicalCollection* coll) {});
  if (res.is(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) {
    uint32_t defaultReplFactor = 1;
    ClusterFeature* cl = ApplicationServer::getFeature<ClusterFeature>("Cluster");
    if (cl != nullptr) {
      defaultReplFactor = cl->systemReplicationFactor();
    }
    VPackBuilder bb;
    bb.openObject();
    bb.add("isSystem", VPackSlice::trueSlice());
    bb.add("waitForSync", VPackSlice::falseSlice());
    bb.add("journalSize", VPackValue(1024 * 1024));
    bb.add("replicationFactor", VPackValue(defaultReplFactor));
    if (name != "_graphs") {
      bb.add("distributeShardsLike", VPackValue("_graphs"));
    }
    bb.close();
    res = Collections::create(vocbase, name, TRI_COL_TYPE_DOCUMENT, bb.slice(),
                              /*waitsForSyncReplication*/ true,
                              /*enforceReplicationFactor*/ true,
                              [](LogicalCollection* coll) { TRI_ASSERT(coll); });
  }
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return true;
}

/// create an index if it does not exist
static bool createIndex(TRI_vocbase_t* vocbase, std::string const& name,
                        Index::IndexType type,
                        std::vector<std::string> const& fields, bool unique,
                        bool sparse) {
  VPackBuilder output;
  Result res1, res2;
  res1 =
      methods::Collections::lookup(vocbase, name, [&](LogicalCollection* coll) {
        res2 =
            methods::Indexes::createIndex(coll, type, fields, unique, sparse);
      });
  if (res1.fail() || res2.fail()) {
    THROW_ARANGO_EXCEPTION(res1.fail() ? res1 : res2);
  }
  return true;
}
bool UpgradeTasks::setupGraphs(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_graphs");
}
bool UpgradeTasks::setupUsers(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_users");
}
bool UpgradeTasks::createUsersIndex(TRI_vocbase_t* vocbase, VPackSlice const&) {
  TRI_ASSERT(vocbase->isSystem());
  return createIndex(vocbase, "_users", Index::TRI_IDX_TYPE_HASH_INDEX,
                     {"user"},
                     /*unique*/ true, /*sparse*/ true);
}
bool UpgradeTasks::addDefaultUserOther(TRI_vocbase_t* vocbase,
                                       VPackSlice const& params) {
  TRI_ASSERT(!vocbase->isSystem());
  TRI_ASSERT(params.isObject());
  VPackSlice users = params.get("users");
  if (users.isNone()) {
    return true; // exit, no users were specified
  } else if (!users.isArray()) {
    LOG_TOPIC(ERR, Logger::STARTUP) << "addDefaultUserOther: users is invalid";
    return false;
  }
  auth::UserManager* um = AuthenticationFeature::instance()->userManager();
  TRI_ASSERT(um != nullptr);
  for (VPackSlice slice : VPackArrayIterator(users)) {
    std::string user = VelocyPackHelper::getStringValue(slice, "username",
                                                        StaticStrings::Empty);
    if (user.empty()) {
      continue;
    }
    std::string passwd = VelocyPackHelper::getStringValue(slice, "passwd", "");
    bool active = VelocyPackHelper::getBooleanValue(slice, "active", true);
    VPackSlice extra = slice.get("extra");
    Result res =
        um->storeUser(false, user, passwd, active, VPackSlice::noneSlice());
    if (res.fail() && !res.is(TRI_ERROR_USER_DUPLICATE)) {
      LOG_TOPIC(WARN, Logger::STARTUP) << "could not add database user "
                                       << user;
    } else if (extra.isObject() && !extra.isEmptyObject()) {
      um->updateUser(user, [&](auth::User& user) {
        user.setUserData(VPackBuilder(extra));
        return TRI_ERROR_NO_ERROR;
      });
    }
  }
  return true;
}
bool UpgradeTasks::updateUserModels(TRI_vocbase_t* vocbase, VPackSlice const&) {
  TRI_ASSERT(vocbase->isSystem());
  // TODO isn't this done on the fly ?
  return true;
}
bool UpgradeTasks::createModules(TRI_vocbase_t* vocbase, VPackSlice const& s) {
  return createSystemCollection(vocbase, "_modules");
}
bool UpgradeTasks::setupAnalyzers(TRI_vocbase_t* vocbase, VPackSlice const& s) {
  return createSystemCollection(vocbase, "_iresearch_analyzers");
}
bool UpgradeTasks::createRouting(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_routing");
}
bool UpgradeTasks::insertRedirections(TRI_vocbase_t* vocbase,
                                      VPackSlice const&) {
  std::vector<std::string> toRemove;  // remove in a different trx
  auto cb = [&toRemove](VPackSlice const& doc) {
    TRI_ASSERT(doc.isObject());
    VPackSlice url = doc.get("url"), action = doc.get("action");
    if (url.isObject() && action.isObject() &&
        action.get("options").isObject()) {
      VPackSlice v = action.get("options").get("destination");
      if (v.isString()) {
        std::string path = v.copyString();
        if (path.find("_admin/html") != std::string::npos ||
            path.find("_admin/aardvark") != std::string::npos) {
          toRemove.push_back(doc.get(StaticStrings::KeyString).copyString());
        }
      }
    }
  };
  Result res = methods::Collections::all(vocbase, "_routing", cb);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  auto ctx = transaction::StandaloneContext::Create(vocbase);
  SingleCollectionTransaction trx(ctx, "_routing", AccessMode::Type::WRITE);
  res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  OperationOptions opts;
  opts.waitForSync = true;
  for (std::string const& key : toRemove) {
    VPackBuilder b;
    b(VPackValue(VPackValueType::Object))(StaticStrings::KeyString,
                                          VPackValue(key))();
    trx.remove("_routing", b.slice(), opts);  // check results
  }

  std::vector<std::string> paths = {"/", "/_admin/html", "/_admin/html/index.html"};
  std::string dest = "/_db/" + vocbase->name() + "/_admin/aardvark/index.html";
  OperationResult opres;
  for (std::string const& path : paths) {
    VPackBuilder bb;
    bb.openObject();
    bb.add("url", VPackValue(path));
    bb.add("action", VPackValue(VPackValueType::Object));
    bb.add("do", VPackValue("@arangodb/actions/redirectRequest"));
    bb.add("options", VPackValue(VPackValueType::Object));
    bb.add("permanently", VPackSlice::trueSlice());
    bb.add("destination", VPackValue(dest));
    bb.close();
    bb.close();
    bb.add("priority", VPackValue(-1000000));
    bb.close();
    opres = trx.insert("_routing", bb.slice(), opts);
    if (opres.fail()) {
      THROW_ARANGO_EXCEPTION(opres.result);
    }
  }
  res = trx.finish(opres.result);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return true;
}

bool UpgradeTasks::setupAqlFunctions(TRI_vocbase_t* vocbase,
                                     VPackSlice const&) {
  return createSystemCollection(vocbase, "_aqlfunctions");
}

bool UpgradeTasks::createFrontend(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_frontend");
}

bool UpgradeTasks::setupQueues(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_queues");
}

bool UpgradeTasks::setupJobs(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_jobs");
}

bool UpgradeTasks::createJobsIndex(TRI_vocbase_t* vocbase, VPackSlice const&) {
  createSystemCollection(vocbase, "_jobs");
  createIndex(vocbase, "_jobs", Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
              {"queue", "status", "delayUntil"},
              /*unique*/ true, /*sparse*/ true);
  createIndex(vocbase, "_jobs", Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
              {"status", "queue", "delayUntil"},
              /*unique*/ true, /*sparse*/ true);
  return true;
}

bool UpgradeTasks::setupApps(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_apps");
}

bool UpgradeTasks::createAppsIndex(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createIndex(vocbase, "_apps", Index::TRI_IDX_TYPE_HASH_INDEX,
                     {"mount"}, /*unique*/ true, /*sparse*/ true);
}

bool UpgradeTasks::setupAppBundles(TRI_vocbase_t* vocbase, VPackSlice const&) {
  return createSystemCollection(vocbase, "_appbundles");
}
