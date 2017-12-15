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
#include "Basics/Common.h"

#include "Agency/AgencyComm.h"
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

static void createSystemCollection(UpgradeArgs const& args,
                                   std::string const& name) {
  Result res = methods::Collections::lookup(args.vocbase, name,
                                            [](LogicalCollection* coll) {});
  if (res.is(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND)) {
    auto cl = ApplicationServer::getFeature<ClusterFeature>("Cluster");
    VPackBuilder props;
    props.openObject();
    props.add("isSystem", VPackSlice::trueSlice());
    props.add("waitForSync", VPackSlice::falseSlice());
    props.add("journalSize", VPackValue(1024 * 1024));
    props.add("replicationFactor", VPackValue(cl->systemReplicationFactor()));
    if (name != "_graphs") {
      props.add("distributeShardsLike", VPackValue("_graphs"));
    }
    res =
        Collections::create(args.vocbase, name, TRI_COL_TYPE_DOCUMENT,
                            props.slice(), /*waitsForSyncReplication*/ true,
                            /*enforceReplicationFactor*/ true,
                            [](LogicalCollection* coll) { TRI_ASSERT(coll); });
  }
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

static void createIndex(UpgradeArgs const& args, std::string const& name,
                        Index::IndexType type,
                        std::vector<std::string> const& fields, bool unique,
                        bool sparse) {
  VPackBuilder output;
  Result res1, res2;
  res1 = methods::Collections::lookup(
      args.vocbase, name, [&](LogicalCollection* coll) {
        res2 =
            methods::Indexes::createIndex(coll, type, fields, unique, sparse);
      });
  if (res1.fail() || res2.fail()) {
    THROW_ARANGO_EXCEPTION(res1.fail() ? res1 : res2);
  }
}
void UpgradeTasks::setupGraphs(UpgradeArgs const& args) {
  createSystemCollection(args, "_graphs");
}
void UpgradeTasks::setupUsers(UpgradeArgs const& args) {
  createSystemCollection(args, "_users");
}
void UpgradeTasks::createUsersIndex(UpgradeArgs const& args) {
  createIndex(args, "_users", Index::TRI_IDX_TYPE_HASH_INDEX, {"user"},
              /*unique*/ true, /*sparse*/ true);
}
void UpgradeTasks::addDefaultUsers(UpgradeArgs const& args) {
  if (!args.users.isArray()) {
    return;
  }
  AuthInfo* auth = AuthenticationFeature::INSTANCE->authInfo();
  TRI_ASSERT(auth);
  for (VPackSlice slice : VPackArrayIterator(args.users)) {
    std::string user = VelocyPackHelper::getStringValue(slice, "username",
                                                        StaticStrings::Empty);
    TRI_ASSERT(!user.empty());
    std::string passwd = VelocyPackHelper::getStringValue(slice, "passwd", "");
    bool active = VelocyPackHelper::getBooleanValue(slice, "active", true);
    VPackSlice extra = slice.get("extra");
    Result res = auth->storeUser(false, user, passwd, active);
    if (res.fail() && !res.is(TRI_ERROR_USER_DUPLICATE)) {
      LOG_TOPIC(WARN, Logger::STARTUP) << "could not add database user "
                                       << user;
    } else if (extra.isObject() && !extra.isEmptyObject()) {
      auth->setUserData(user, extra);
    }
  }
}
void UpgradeTasks::updateUserModels(UpgradeArgs const&) {
  // TODO isn't this done on the fly ?
}
void UpgradeTasks::createModules(UpgradeArgs const& args) {
  createSystemCollection(args, "_modules");
}
void UpgradeTasks::createRouting(UpgradeArgs const& args) {
  createSystemCollection(args, "_routing");
}
void UpgradeTasks::insertRedirections(UpgradeArgs const& args) {
  auto ctx = transaction::StandaloneContext::Create(args.vocbase);
  SingleCollectionTransaction trx(ctx, "_routing", AccessMode::Type::WRITE);
  Result res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  std::vector<std::string> paths = {"/", "/_admin/html",
                                    "/_admin/html/index.html"};
  std::string destination =
      "/_db/" + args.vocbase->name() + "/_admin/aardvark/index.html";
  OperationOptions ops;
  ops.waitForSync = true;
  OperationResult opres;

  // FIXME: first, check for "old" redirects
  for (std::string const& path : paths) {
    VPackBuilder bb;
    bb.openObject();
    bb.add("url", VPackValue(path));
    bb.add("action", VPackValue(VPackValueType::Object));
    bb.add("do", VPackValue("@arangodb/actions/redirectRequest"));
    bb.add("options", VPackValue(VPackValueType::Object));
    bb.add("permanently", VPackSlice::trueSlice());
    bb.add("destination", VPackValue(destination));
    bb.close();
    bb.close();
    bb.add("priority", VPackValue(-1000000));
    bb.close();
    opres = trx.insert("_routing", bb.slice(), ops);
    if (opres.fail()) {
      THROW_ARANGO_EXCEPTION(opres.result);
    }
  }
  res = trx.finish(opres.result);
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
}
void UpgradeTasks::setupAqlFunctions(UpgradeArgs const& args) {
  createSystemCollection(args, "_aqlfunctions");
}
void UpgradeTasks::createFrontend(UpgradeArgs const& args) {
  createSystemCollection(args, "_frontend");
}
void UpgradeTasks::setupQueues(UpgradeArgs const& args) {
  createSystemCollection(args, "_queues");
}
void UpgradeTasks::setupJobs(UpgradeArgs const& args) {
  createSystemCollection(args, "_jobs");
}
void UpgradeTasks::createJobsIndex(UpgradeArgs const& args) {
  createSystemCollection(args, "_jobs");
  createIndex(args, "_jobs", Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
              {"queue", "status", "delayUntil"},
              /*unique*/ true, /*sparse*/ true);
  createIndex(args, "_jobs", Index::TRI_IDX_TYPE_SKIPLIST_INDEX,
              {"status", "queue", "delayUntil"},
              /*unique*/ true, /*sparse*/ true);
}
void UpgradeTasks::setupApps(UpgradeArgs const& args) {
  createSystemCollection(args, "_apps");
}
void UpgradeTasks::createAppsIndex(UpgradeArgs const& args) {
  createIndex(args, "_jobs", Index::TRI_IDX_TYPE_HASH_INDEX, {"mount"},
              /*unique*/ true, /*sparse*/ true);
}
void UpgradeTasks::setupAppBundles(UpgradeArgs const& args) {
  createSystemCollection(args, "_appbundles");
}
