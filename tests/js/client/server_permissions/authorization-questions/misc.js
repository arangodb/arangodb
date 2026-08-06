/*jshint globalstrict:false, strict:false */
/* global getOptions, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

// Authorization questions asked by a grab-bag of miscellaneous /_api/* and
// /openapi.json endpoints.
//
// Observation-based counterpart of tests/api/apitests/misc.mjs.
//
// Every request first asks `UseDatabase name=<db> level=read` in
// RestHandler::checkUserCanAccess() (the base check fires for any authenticated
// request while authentication is on), where <db> is derived from the
// /_db/<name>/ path prefix. Individual handlers then ask further questions:
//   - canUseAdminAction(X)   -> logs X (always, RBAC not required)
//   - canUseHardenedAction(X) -> logs X ONLY with --server.harden=true; we do
//     not set harden, so those handlers (engine, version) ask nothing extra.
//   - canUseDatabase(db, Write) -> `UseDatabase name=<db> level=write`
//   - a read/write transaction over a collection -> UseCollection questions in
//     StorageEngine/TransactionState.cpp checkCollectionPermission.
//
// Many endpoints here are cluster-/replication2-only or V8-only; on a plain
// single server they may 404/501 before the derived question is reached. Such
// cases carry an // AUDIT note.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const {
  beginObserve,
  endObserve,
  disableObserve,
  assertPermissions
} = require('@arangodb/testutils/permissions-observer');
const {
  setUpApiTestData,
  tearDownApiTestData,
  DB,
  DOC_COLLECTION,
  singleOnly
} = require('@arangodb/testutils/apitest-fixtures');

function miscApiAuthzSuite () {
  const c = DOC_COLLECTION;

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // ── /_api/document-state ─────────────────────────────────────────────
    // RestDocumentStateHandler: GET -> canUseAdminAction(AdminReadReplicatedLog),
    // POST/DELETE -> canUseAdminAction(AdminWriteReplicatedLog).
    // AUDIT: only registered with replication2 in cluster mode; on a single
    // server the route is unknown and returns 404 before the handler runs, so
    // in that case only the base UseDatabase check (or nothing) is observed.
    testDocumentStateShards: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/document-state/99999/shards`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "AdminReadReplicatedLog"
      ], endObserve());
    },

    testDocumentStateStartSnapshot: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/document-state/99999/snapshot/start`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "AdminWriteReplicatedLog"
      ], endObserve());
    },

    testDocumentStateFinishSnapshot: function () {
      beginObserve();
      arango.DELETE_RAW(
        `/_db/${DB}/_api/document-state/99999/snapshot/finish/99999`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "AdminWriteReplicatedLog"
      ], endObserve());
    },

    // ── /_api/endpoint ───────────────────────────────────────────────────
    // RestEndpointHandler only checks _vocbase.isSystem(); it asks no can().
    testListEndpoints: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/endpoint`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // ── /_api/engine ─────────────────────────────────────────────────────
    // RestEngineHandler: canUseHardenedAction(MonitoringInternal). Not hardened
    // in this suite -> asks nothing beyond the base check.
    testEngine: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/engine`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testEngineStats: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/engine/stats`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── /_api/explain ────────────────────────────────────────────────────
    // RestExplainHandler builds a query over c; explaining sets up a read
    // transaction on c -> UseCollection(read).
    // AUDIT: explain only plans (does not execute); the read permission check
    // still fires while resolving the collection.
    testExplain: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/explain`, { query: `FOR d IN ${c} RETURN d` });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseCollection db=d name=c level=read"
      ], endObserve());
    },

    // ── /_api/key-generators ─────────────────────────────────────────────
    // RestKeyGeneratorsHandler asks no can(); base check only.
    testKeyGenerators: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/key-generators`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── /_api/log ────────────────────────────────────────────────────────
    // RestLogHandler: GET -> AdminReadReplicatedLog, POST/DELETE ->
    // AdminWriteReplicatedLog.
    // AUDIT: replication2 + cluster only; single server returns 404 before the
    // handler runs (then only the base check, or nothing, is observed).
    testListReplicatedLogs: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/log`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "AdminReadReplicatedLog"
      ], endObserve());
    },

    testCreateReplicatedLog: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/log`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "AdminWriteReplicatedLog"
      ], endObserve());
    },

    testDeleteReplicatedLog: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/log`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "AdminWriteReplicatedLog"
      ], endObserve());
    },

    // ── /_api/log-internal ───────────────────────────────────────────────
    // RestLogInternalHandler gates on isSuperuserOrDisabled() (no can() logged)
    // and only accepts POST. A GET yields 405 after the superuser gate.
    // AUDIT: replication2 + cluster only; single server returns 404. No
    // authorization question beyond the base check is asked in any case.
    testLogInternal: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/log-internal`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── /_api/query/* ────────────────────────────────────────────────────
    // RestQueryHandler asks no can() for any of these (registry / all=true use
    // isSuperuserOrDisabled(), which is not logged).
    testSlowQueries: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query/slow`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testCurrentQueries: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query/current`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testQueryProperties: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query/properties`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // AUDIT: registry uses isSuperuserOrDisabled() (no can() logged); root over
    // basic auth is not a superuser, so this returns 403 but asks nothing.
    testQueryRegistry: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query/registry`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testQueryRules: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query/rules`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // POST /_api/query only parses (does not resolve collections) -> base only.
    testValidateQuery: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/query`, { query: `FOR d IN ${c} RETURN d` });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testKillNonexistentQuery: function () {
      beginObserve();
      arango.DELETE_RAW(
        `/_db/${DB}/_api/query/nonexistent-query-apitester-99999`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testClearSlowQueryLog: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/query/slow`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── /_api/query-cache/* ──────────────────────────────────────────────
    // Reads ask nothing beyond the base check.
    testQueryCacheEntries: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query-cache/entries`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testQueryCacheProperties: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query-cache/properties`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // PUT /_api/query-cache/properties and DELETE /_api/query-cache check
    // canUseAdminAction(AdminQueryCache) only when requestedApiVersion > 0.
    // AUDIT: the default API version is V0 (== 0), so the AdminQueryCache
    // question is NOT asked for the plain path used here; only the base check
    // (in the _system database, which the handler additionally requires)
    // fires. With an explicit /_arango/v1 prefix, AdminQueryCache would appear.
    testUpdateQueryCacheProperties: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/query-cache/properties`, { mode: 'off' });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    testClearQueryCache: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/query-cache`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // ── /_api/query-plan-cache ───────────────────────────────────────────
    // GET filters cached plans; for each cached plan it asks
    // canUseCollection(read) for every referenced collection.
    // AUDIT: the plan cache is empty in this fresh database (explain/parse do
    // not populate it), so no per-collection question is asked here.
    testQueryPlanCacheEntries: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/query-plan-cache`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // DELETE clears the plan cache -> canUseDatabase(Write).
    testClearQueryPlanCache: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/query-plan-cache`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseDatabase name=d level=write"
      ], endObserve());
    },

    // ── /_api/ttl/* ──────────────────────────────────────────────────────
    // RestTtlHandler only requires the _system database (no can()).
    testTtlProperties: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/ttl/properties`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    testTtlStatistics: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/ttl/statistics`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    testUpdateTtlProperties: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/ttl/properties`,
                     { enable: true, frequency: 30000 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // ── /_api/upload ─────────────────────────────────────────────────────
    // RestUploadHandler asks no can(); base check only.
    testUpload: function () {
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/upload`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── /_api/version ────────────────────────────────────────────────────
    // RestVersionHandler falls through to the base check, then
    // canUseHardenedAction(MonitoringInternal) for full details. Not hardened
    // here -> asks nothing beyond the base check.
    testVersion: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/version`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // ── /_api/wal/* ──────────────────────────────────────────────────────
    // RestWalAccessHandler: canUseAdminAction(AdminWalAccess) (always logged).
    // AUDIT: on a coordinator the handler returns 501 before the check; on a
    // single server or DBServer the AdminWalAccess question is asked.
    testWalLastTick: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/wal/lastTick`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...singleOnly([
          "AdminWalAccess"
        ])
      ], endObserve());
    },

    testWalOpenTransactions: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/wal/open-transactions`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...singleOnly([
          "AdminWalAccess"
        ])
      ], endObserve());
    },

    testWalRange: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/wal/range`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...singleOnly([
          "AdminWalAccess"
        ])
      ], endObserve());
    },

    // tailing the WAL resolves the collection of every operation it reports,
    // which after the startup activity is every collection of the database
    testWalTailRead: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/wal/tail`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...singleOnly([
          "AdminWalAccess",
          "UseCollection db=_system name=_analyzers level=read",
          "UseCollection db=_system name=_appbundles level=read",
          "UseCollection db=_system name=_apps level=read",
          "UseCollection db=_system name=_aqlfunctions level=read",
          "UseCollection db=_system name=_frontend level=read",
          "UseCollection db=_system name=_graphs level=read",
          "UseCollection db=_system name=_jobs level=read",
          "UseCollection db=_system name=_queues level=read",
          "UseCollection db=_system name=_statistics level=read",
          "UseCollection db=_system name=_statistics15 level=read",
          "UseCollection db=_system name=_statisticsRaw level=read",
          "UseCollection db=_system name=_users level=read"
        ])
      ], endObserve());
    },

    testWalTailAcknowledge: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/_system/_api/wal/tail`, {});
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...singleOnly([
          "AdminWalAccess"
        ])
      ], endObserve());
    },

    testWalTailRelease: function () {
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/wal/tail`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        ...singleOnly([
          "AdminWalAccess"
        ])
      ], endObserve());
    },

    // ── /openapi.json ────────────────────────────────────────────────────
    // RestOpenApiHandler (extends RestBaseHandler) does not override
    // checkUserCanAccess, so the base UseDatabase check fires; the request has
    // no /_db/ prefix, so it addresses the _system database. execute() itself
    // asks no can().
    // AUDIT: documented as OPEN (unauthenticated -> 200), but an authenticated
    // root request still triggers the base UseDatabase(_system, read) check.
    testOpenApiSpec: function () {
      beginObserve();
      arango.GET_RAW(`/openapi.json`);
      assertPermissions([
      ], endObserve());
    },

    // ── /_api/tasks ──────────────────────────────────────────────────────
    // RestTasksHandler: GET asks no can(); POST/DELETE ask
    // canUseDatabase(Write). AUDIT: requires V8; without V8 execute() returns
    // 501 before the write check, so only the base check would be observed.
    testListTasks: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/tasks`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testGetNonexistentTask: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/tasks/nonexistent-task-apitester-99999`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    testCreateTask: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/${DB}/_api/tasks`,
        { name: 'apitester-task', command: '1+1;', offset: 0 });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseDatabase name=d level=write"
      ], endObserve());
      if (res.parsedBody && res.parsedBody.id) {
        arango.DELETE_RAW(`/_db/${DB}/_api/tasks/${res.parsedBody.id}`);
      }
    },

    testDeleteTask: function () {
      const created = arango.POST_RAW(`/_db/${DB}/_api/tasks`,
        { name: 'apitester-task', command: '1+1;', offset: 0 });
      const id = created.parsedBody ? created.parsedBody.id : undefined;
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/tasks/${id}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseDatabase name=d level=write"
      ], endObserve());
    },

    // ── /_api/token ──────────────────────────────────────────────────────
    // RestAccessTokenHandler overrides checkUserCanAccess() to require only
    // authentication (no base UseDatabase question). GET -> canReadUser(user),
    // POST/DELETE -> canModifyUserProfile(user). The target user is "root",
    // which equals the connected (authenticated) user, so both short-circuit
    // to OK WITHOUT calling can(). Reading the tokens hence asks NOTHING,
    // while creating/deleting one persists it and thus reads _users.
    // canModifyUserProfile() consults the read-only gate after the (skipped)
    // permission question, so POST/DELETE additionally ask `IsReadOnly` -
    // on a coordinator as well, the gate is in the ExecContext, not in the
    // storage path.
    testListAccessTokens: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_api/token/root`);
      assertPermissions(["UseApiVersion version=0"], endObserve());
    },

    testCreateAccessToken: function () {
      beginObserve();
      const res = arango.POST_RAW(`/_db/_system/_api/token/root`,
                                  { name: 'apitester-token' });
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
      if (res.parsedBody && res.parsedBody.id) {
        arango.DELETE_RAW(`/_db/_system/_api/token/root/${res.parsedBody.id}`);
      }
    },

    testDeleteAccessToken: function () {
      const created = arango.POST_RAW(`/_db/_system/_api/token/root`,
                                      { name: 'apitester-token' });
      const id = created.parsedBody ? created.parsedBody.id : undefined;
      beginObserve();
      arango.DELETE_RAW(`/_db/_system/_api/token/root/${id}`);
      assertPermissions([
        "UseApiVersion version=0",
        "IsReadOnly",
        ...singleOnly([
          "UseCollection db=_system name=_users level=read",
          "UseCollection db=_system name=_users level=writedata"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(miscApiAuthzSuite);
return jsunity.done();
