////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "ServerFeature.h"

#include "Aql/RestAqlHandler.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/process-utils.h"
#include "Cluster/RestShardHandler.h"
#include "HttpServer/HttpHandlerFactory.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestHandler/RestAdminLogHandler.h"
#include "RestHandler/RestBatchHandler.h"
#include "RestHandler/RestCursorHandler.h"
#include "RestHandler/RestDebugHandler.h"
#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestEdgeHandler.h"
#include "RestHandler/RestEdgesHandler.h"
#include "RestHandler/RestExportHandler.h"
#include "RestHandler/RestHandlerCreator.h"
#include "RestHandler/RestImportHandler.h"
#include "RestHandler/RestJobHandler.h"
#include "RestHandler/RestPleaseUpgradeHandler.h"
#include "RestHandler/RestQueryCacheHandler.h"
#include "RestHandler/RestQueryHandler.h"
#include "RestHandler/RestReplicationHandler.h"
#include "RestHandler/RestShutdownHandler.h"
#include "RestHandler/RestSimpleHandler.h"
#include "RestHandler/RestSimpleQueryHandler.h"
#include "RestHandler/RestUploadHandler.h"
#include "RestHandler/RestVersionHandler.h"
#include "RestHandler/WorkMonitorHandler.h"
#include "RestServer/DatabaseFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;
using namespace arangodb::rest;

ServerFeature::ServerFeature(application_features::ApplicationServer* server,
                             std::string const& authenticationRealm, int* res)
    : ApplicationFeature(server, "Server"),
      _defaultApiCompatibility(Version::getNumericServerVersion()),
      _allowMethodOverride(false),
      _authenticationRealm(authenticationRealm),
      _result(res),
      _handlerFactory(nullptr),
      _jobManager(nullptr) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("Dispatcher");
  startsAfter("Scheduler");
  startsAfter("WorkMonitor");
}

void ServerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(
      Section("server", "Server features", "server options", false, false));

  options->addHiddenOption("--server.default-api-compatibility",
                           "default API compatibility version",
                           new Int32Parameter(&_defaultApiCompatibility));

  options->addSection(
      Section("http", "HttpServer features", "http options", false, false));

  options->addHiddenOption("--http.allow-method-override",
                           "allow HTTP method override using special headers",
                           new BooleanParameter(&_allowMethodOverride));
}

void ServerFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  if (_defaultApiCompatibility < HttpRequest::MIN_COMPATIBILITY) {
    LOG(FATAL) << "invalid value for --server.default-api-compatibility. "
                  "minimum allowed value is "
               << HttpRequest::MIN_COMPATIBILITY;
    FATAL_ERROR_EXIT();
  }

  LOG(DEBUG) << "using default API compatibility: "
             << (long int)_defaultApiCompatibility;
}

void ServerFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

  buildHandlerFactory();
  HttpHandlerFactory::setMaintenance(true);
}

void ServerFeature::start() {
  DatabaseFeature* database = dynamic_cast<DatabaseFeature*>(
      application_features::ApplicationServer::lookupFeature("Database"));

  httpOptions._vocbase = database->vocbase();

  defineHandlers();
  HttpHandlerFactory::setMaintenance(false);
}

void ServerFeature::beginShutdown() {
  std::string msg =
      ArangoGlobalContext::CONTEXT->binaryName() + " [shutting down]";
  TRI_SetProcessTitle(msg.c_str());
}

void ServerFeature::stop() { *_result = EXIT_SUCCESS; }

static TRI_vocbase_t* LookupDatabaseFromRequest(HttpRequest* request,
                                                TRI_server_t* server) {
  // get database name from request
  std::string dbName = request->databaseName();

  if (dbName.empty()) {
    // if no databases was specified in the request, use system database name
    // as a fallback
    dbName = TRI_VOC_SYSTEM_DATABASE;
    request->setDatabaseName(dbName);
  }

  if (ServerState::instance()->isCoordinator()) {
    return TRI_UseCoordinatorDatabaseServer(server, dbName.c_str());
  }

  return TRI_UseDatabaseServer(server, dbName.c_str());
}

static bool SetRequestContext(HttpRequest* request, void* data) {
  TRI_server_t* server = static_cast<TRI_server_t*>(data);
  TRI_vocbase_t* vocbase = LookupDatabaseFromRequest(request, server);

  // invalid database name specified, database not found etc.
  if (vocbase == nullptr) {
    return false;
  }

  // database needs upgrade
  if (vocbase->_state == (sig_atomic_t)TRI_VOCBASE_STATE_FAILED_VERSION) {
    request->setRequestPath("/_msg/please-upgrade");
    return false;
  }

  VocbaseContext* ctx = new arangodb::VocbaseContext(request, server, vocbase);
  request->setRequestContext(ctx, true);

  // the "true" means the request is the owner of the context
  return true;
}

void ServerFeature::buildHandlerFactory() {
  _handlerFactory.reset(new HttpHandlerFactory(
      _authenticationRealm, _defaultApiCompatibility, _allowMethodOverride,
      &SetRequestContext, nullptr));
}

void ServerFeature::defineHandlers() {
#warning TODO
#if 0
  // ...........................................................................
  // /_msg
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_msg/please-upgrade",
      RestHandlerCreator<RestPleaseUpgradeHandler>::createNoData);

  // ...........................................................................
  // /_api
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::BATCH_PATH,
      RestHandlerCreator<RestBatchHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::CURSOR_PATH,
      RestHandlerCreator<RestCursorHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::DOCUMENT_PATH,
      RestHandlerCreator<RestDocumentHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGE_PATH,
      RestHandlerCreator<RestEdgeHandler>::createNoData);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EDGES_PATH,
      RestHandlerCreator<RestEdgesHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::EXPORT_PATH,
      RestHandlerCreator<RestExportHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::IMPORT_PATH,
      RestHandlerCreator<RestImportHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::REPLICATION_PATH,
      RestHandlerCreator<RestReplicationHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_QUERY_ALL_PATH,
      RestHandlerCreator<RestSimpleQueryHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_LOOKUP_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::SIMPLE_REMOVE_PATH,
      RestHandlerCreator<RestSimpleHandler>::createData<
          std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      RestVocbaseBaseHandler::UPLOAD_PATH,
      RestHandlerCreator<RestUploadHandler>::createNoData);

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      "/_api/shard-comm",
      RestHandlerCreator<RestShardHandler>::createNoData);
#endif

  _handlerFactory->addPrefixHandler(
      "/_api/aql", RestHandlerCreator<aql::RestAqlHandler>::createData<
                       std::pair<ApplicationV8*, aql::QueryRegistry*>*>,
      _pairForAqlHandler);

  _handlerFactory->addPrefixHandler(
      "/_api/query",
      RestHandlerCreator<RestQueryHandler>::createData<ApplicationV8*>,
      _applicationV8);

  _handlerFactory->addPrefixHandler(
      "/_api/query-cache",
      RestHandlerCreator<RestQueryCacheHandler>::createNoData);

  // And now some handlers which are registered in both /_api and /_admin
  _handlerFactory->addPrefixHandler(
      "/_api/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                       std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  _handlerFactory->addHandler(
      "/_api/version", RestHandlerCreator<RestVersionHandler>::createNoData,
      nullptr);

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/_admin/job", RestHandlerCreator<arangodb::RestJobHandler>::createData<
                         std::pair<Dispatcher*, AsyncJobManager*>*>,
      _pairForJobHandler);

  _handlerFactory->addHandler(
      "/_admin/version", RestHandlerCreator<RestVersionHandler>::createNoData,
      nullptr);

  // further admin handlers
  _handlerFactory->addHandler(
      "/_admin/log",
      RestHandlerCreator<arangodb::RestAdminLogHandler>::createNoData, nullptr);

  _handlerFactory->addPrefixHandler(
      "/_admin/work-monitor",
      RestHandlerCreator<WorkMonitorHandler>::createNoData, nullptr);

// This handler is to activate SYS_DEBUG_FAILAT on DB servers
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  _handlerFactory->addPrefixHandler(
      "/_admin/debug", RestHandlerCreator<RestDebugHandler>::createNoData,
      nullptr);
#endif

#warning TODO
#if 0
  _handlerFactory->addPrefixHandler(
      "/_admin/shutdown",
      RestHandlerCreator<arangodb::RestShutdownHandler>::createData<void*>,
      static_cast<void*>(_applicationServer));
#endif

  // ...........................................................................
  // /_admin
  // ...........................................................................

  _handlerFactory->addPrefixHandler(
      "/", RestHandlerCreator<RestActionHandler>::createData<
               RestActionHandler::action_options_t*>,
      (void*)&httpOptions);
#endif
}

#if 0

  // .............................................................................
  // try to figure out the thread affinity
  // .............................................................................

  size_t n = TRI_numberProcessors();

  if (n > 2 && _threadAffinity > 0) {
    size_t ns = _applicationScheduler->numberOfThreads();
    size_t nd = _applicationDispatcher->numberOfThreads();

    if (ns != 0 && nd != 0) {
      LOG(INFO) << "the server has " << n << " (hyper) cores, using " << ns
                << " scheduler threads, " << nd << " dispatcher threads";
    } else {
      _threadAffinity = 0;
    }

    switch (_threadAffinity) {
      case 1:
        if (n < ns + nd) {
          ns = static_cast<size_t>(round(1.0 * n * ns / (ns + nd)));
          nd = static_cast<size_t>(round(1.0 * n * nd / (ns + nd)));

          if (ns < 1) {
            ns = 1;
          }
          if (nd < 1) {
            nd = 1;
          }

          while (n < ns + nd) {
            if (1 < ns) {
              ns -= 1;
            } else if (1 < nd) {
              nd -= 1;
            } else {
              ns = 1;
              nd = 1;
            }
          }
        }

        break;

      case 2:
        if (n < ns) {
          ns = n;
        }

        if (n < nd) {
          nd = n;
        }

        break;

      case 3:
        if (n < ns) {
          ns = n;
        }

        nd = 0;

        break;

      case 4:
        if (n < nd) {
          nd = n;
        }

        ns = 0;

        break;

      default:
        _threadAffinity = 0;
        break;
    }

    if (_threadAffinity > 0) {
      TRI_ASSERT(ns <= n);
      TRI_ASSERT(nd <= n);

      std::vector<size_t> ps;
      std::vector<size_t> pd;

      for (size_t i = 0; i < ns; ++i) {
        ps.push_back(i);
      }

      for (size_t i = 0; i < nd; ++i) {
        pd.push_back(n - i - 1);
      }

      if (0 < ns) {
        _applicationScheduler->setProcessorAffinity(ps);
      }

      if (0 < nd) {
        _applicationDispatcher->setProcessorAffinity(pd);
      }

      if (0 < ns) {
        LOG(INFO) << "scheduler cores: " << ToString(ps);
      }
      if (0 < nd) {
        LOG(INFO) << "dispatcher cores: " << ToString(pd);
      }
    } else {
      LOG(INFO) << "the server has " << n << " (hyper) cores";
    }
  }

#endif
