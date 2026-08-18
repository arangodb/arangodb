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

// Authorization questions asked by the catchall prefix handler mounted at "/".
//
// Unlike its sibling files this one has NO counterpart in tests/api/apitests/;
// the catchall is not an API family but the fallback for everything no explicit
// C++ handler claimed.
//
// Handler: arangod/Actions/RestActionHandler.cpp, registered as
//
//   // GeneralServerFeature.cpp
//   f.addPrefixHandler("/", RestHandlerCreator<RestActionHandler>::createNoData,
//                      {0});
//
// i.e. for API version 0 only - under /_arango/v1 an unknown path finds no
// handler at all (RestHandlerFactory.cpp) and is answered with a 404 without any
// ExecContext involvement. That is not covered here; see api-version.js.
//
// This is the only handler in the server that overrides all three authorization
// hooks of RestHandler, so it is the only one where the base checks can be
// skipped or turned into a superuser escalation:
//
//   checkUserAuthentication()  GRANTED_EARLY for isPublicAardvarkPath(), which
//                              makes RestHandler::handleAuthorizationChecks()
//                              return before BOTH remaining checks
//   checkApiVersionAccess()    skipped for hasAllowedUnauthenticatedPath()
//   checkDatabaseAccess()      on failure, sets _mustEscalateToSuperuser for
//                              hasAllowedUnauthenticatedPath(), which execute()
//                              turns into an ExecContextSuperuserScope
//
// Note that hasAllowedUnauthenticatedPath() does NOT test whether the request is
// unauthenticated - despite its name it only tests authenticationSystemOnly()
// (default true) and that the path does not start with /_. It therefore holds for
// authenticated requests too, so the API version question is skipped for the
// whole non-/_ path space, including the bare "/".
//
// Everything the handler fronts - the JS action framework (js/actions/*.js) and,
// through js/actions/api-system.js (url: '', prefix: true -> routeRequest), the
// whole Foxx routing layer including the system services /_admin/aardvark and
// /_api/foxx - performs no identity checks of its own. defineHttp()'s isSystem /
// allowUseDatabase flags are JavaScript capability flags, not permissions. So
// whatever this handler asks IS the authorization for that entire surface.
//
// Baseline for every request that goes through the normal checks:
//
//   UseApiVersion version=0
//   UseDatabase name=<db> level=read
//
// The database is the one from the /_db/<name> path prefix; we spell it out
// explicitly. Note that requestPath() has that prefix stripped before the
// aardvark allowlist and the /_ test of hasAllowedUnauthenticatedPath() look at
// it, so /_db/_system/_admin/aardvark/whoAmI and /_admin/aardvark/whoAmI are the
// same path as far as those two are concerned.
//
// One class of question is filtered out of every observation, see
// FOXX_REGISTRY below: actions.routeRequest() builds a routing list per database
// on first use, and buildRouting(dbname, false) reads the _apps collection
// (FoxxManager initLocalServiceMap) under the CALLER's ExecContext - the JS
// action path has no superuser escalation. Those reads are cached per V8
// context, but the server recycles V8 contexts by age (60s by default,
// V8DealerFeatureOptions::maxExecutorAge) and spreads requests over several of
// them, so whether a given request rebuilds that cache is not a property of the
// request and cannot be asserted on.
//
// Known gap, deliberately not covered here: /_admin/execute is an exact handler
// (RestAdminExecuteHandler), not the catchall, and asks nothing beyond the base
// pair before running arbitrary server-side JavaScript.

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false'
  };
}

const jsunity = require('jsunity');
const request = require('@arangodb/request');
const { endpointToURL } = require('@arangodb/test-helper-common');
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
  DOC_COLLECTION
} = require('@arangodb/testutils/apitest-fixtures');

function catchallAuthzSuite () {
  const c = DOC_COLLECTION;

  // the base questions of a request that goes through the normal checks
  const base = (db) => [
    "UseApiVersion version=0",
    `UseDatabase name=${db} level=read`
  ];

  // AUDIT: the same, minus the API version question. hasAllowedUnauthenticatedPath()
  // is a PURE PATH TEST - it does not look at whether the request is
  // authenticated, only at authenticationSystemOnly() and at the path not
  // starting with /_ - and checkApiVersionAccess() returns OK early whenever it
  // holds. So no path outside /_ is subject to API version restrictions at all,
  // not even for an authenticated user, and that includes the bare "/" of the
  // redirect branch. Only the database question is asked there.
  const baseOutsideUnderscore = (db) => [
    `UseDatabase name=${db} level=read`
  ];

  // //////////////////////////////////////////////////////////////////////////
  // / @brief a request without any Authorization header
  // /
  // / The `arango` object always attaches the connection's credentials, so it
  // / cannot express an unauthenticated request; @arangodb/request adds no
  // / Authorization header unless asked to. The URL is derived from the shell's
  // / own endpoint, because that is the instance the observer reads the log of.
  // //////////////////////////////////////////////////////////////////////////

  const anonymousGet = (path) =>
    request.get({ url: endpointToURL(arango.getEndpoint()) + path });

  // //////////////////////////////////////////////////////////////////////////
  // / @brief the Foxx service registry reads, which are not per-request
  // /
  // / The JS layer reads _apps / _appbundles to learn which services are mounted
  // / and caches the answer per V8 context. Because contexts are recycled by age
  // / and requests are spread over several of them, whether a particular request
  // / performs that read is not a property of the request - so these questions
  // / are dropped instead of being asserted on. Everything else is asserted
  // / exactly, including any other collection this surface might touch.
  // //////////////////////////////////////////////////////////////////////////

  const FOXX_REGISTRY = /^UseCollection db=\S+ name=(_apps|_appbundles) /;

  const observe = () => endObserve().filter((q) => !FOXX_REGISTRY.test(q));

  // //////////////////////////////////////////////////////////////////////////
  // / @brief send an un-observed request, waiting out Foxx self-heal
  // /
  // / A freshly created database may still be waiting for the initial Foxx
  // / self-heal, in which case routeRequest() answers 503 before reaching the
  // / route. Retry until it is through, so that no observation starts against a
  // / half-initialized database.
  // //////////////////////////////////////////////////////////////////////////

  const warmUp = (path) => {
    for (let tries = 0; tries < 120; ++tries) {
      let res = arango.GET_RAW(path);
      if (res.code !== 503) {
        return;
      }
      require('internal').sleep(0.5);
    }
    throw new Error(`routing space of ${path} did not become available`);
  };

  return {
    setUpAll: function () {
      setUpApiTestData();
      // one request per routing space this suite observes, plus a first
      // execution of the two system Foxx services
      warmUp(`/_db/${DB}/_api/warm-up-unknown-route`);
      warmUp(`/_db/${DB}/warm-up-unknown-route`);
      warmUp(`/_db/_system/_admin/warm-up-unknown-route`);
      warmUp(`/_db/_system/_admin/aardvark/foxxes`);
      warmUp(`/_db/${DB}/_api/foxx`);
    },

    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
    },

    // ── A. the "/" redirect branch ────────────────────────────────────────
    // executeAction() answers a bare GET / (and GET /_admin/html) with a
    // redirect to /_db/<db><redirectRootTo()> without executing the action -
    // but the checks have already run at that point.
    //
    // The two triggers of this one branch differ in what they ask, because "/"
    // does not start with /_ while "/_admin/html" does: only the latter is asked
    // the API version question. See baseOutsideUnderscore above.

    testRootRedirectSystem: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/`);
      assertPermissions(baseOutsideUnderscore('_system'), observe());
    },

    testRootRedirectOtherDatabase: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/`);
      assertPermissions(baseOutsideUnderscore(DB), observe());
    },

    // the second trigger of the same branch: suffixes ["_admin", "html"]
    testAdminHtmlRedirect: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/html`);
      assertPermissions(base('_system'), observe());
    },

    // ── B. the public aardvark allowlist ─────────────────────────────────
    // isPublicAardvarkPath() makes checkUserAuthentication() return
    // GRANTED_EARLY, so handleAuthorizationChecks() returns before the API
    // version AND the database check: NOTHING is asked. Note that the handler
    // deliberately does NOT escalate to superuser for these - the comment in
    // RestActionHandler.cpp records that granting superuser to anything merely
    // prefixed with /_admin/aardvark/ previously allowed unauthenticated AQL
    // execution.
    //
    // (The isPublicAardvarkPath() branch in checkDatabaseAccess() is dead code:
    // checkUserAuthentication() already short-circuited for those paths.)

    testAardvarkIndexHtml: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/index.html`);
      assertPermissions([], observe());
    },

    // AUDIT: this route's handler reads db._engine(), cluster.isCluster() and
    // friends while running as the unescalated request context, and still asks
    // nothing.
    testAardvarkConfigJs: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/config.js`);
      assertPermissions([], observe());
    },

    testAardvarkWhoAmI: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/whoAmI`);
      assertPermissions([], observe());
    },

    // prefix entry: the asset need not exist, the allowlist is what we observe
    testAardvarkStaticPrefix: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/static/css/no-such-file.css`);
      assertPermissions([], observe());
    },

    testAardvarkImgPrefix: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/img/arango-icon.svg`);
      assertPermissions([], observe());
    },

    // Negative guard for the vulnerability quoted above: a path that only
    // *resembles* an allowlist entry must go through the normal checks. Near
    // miss on an exact entry ...
    testAardvarkNearMissExactEntry: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/whoAmI2`);
      assertPermissions(base('_system'), observe());
    },

    // ... and near miss on a prefix entry.
    testAardvarkNearMissPrefixEntry: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/staticx/no-such-file.css`);
      assertPermissions(base('_system'), observe());
    },

    // ── C. system Foxx services behind the catchall ──────────────────────

    // an authRouter route of the aardvark service, i.e. past the allowlist
    testAardvarkFoxxes: function () {
      beginObserve();
      arango.GET_RAW(`/_db/_system/_admin/aardvark/foxxes`);
      assertPermissions(base('_system'), observe());
    },

    // /_api/foxx is not a C++ handler: it is the system Foxx service mounted at
    // /_api/foxx, reached through this catchall. Its own checks are mount-path
    // checks (mount.startsWith('/_')), not identity checks.
    testFoxxManagementApi: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/foxx`);
      assertPermissions(base(DB), observe());
    },

    // ── D. plain JS actions behind the catchall ──────────────────────────

    testAdminEcho: function () {
      beginObserve();
      arango.POST_RAW(`/_db/_system/_admin/echo`, {});
      assertPermissions(base('_system'), observe());
    },

    // /_api/simple/any has no C++ handler (unlike all, all-keys, by-example,
    // lookup-by-keys and remove-by-keys, which simple.js covers); it is the JS
    // action in js/actions/api-simple.js. db._collection(c).any() opens a read
    // transaction on c.
    testSimpleAnyJsAction: function () {
      beginObserve();
      arango.PUT_RAW(`/_db/${DB}/_api/simple/any`, { collection: c });
      assertPermissions([
        ...base(DB),
        `UseCollection db=${DB} name=${c} level=read`
      ], observe());
    },

    // ── E. unknown paths ─────────────────────────────────────────────────
    // routeRequest() finds no route and answers 404. The three cases exercise
    // the two routing spaces it distinguishes (internal, matched by
    // /^\/_(admin|api)\//, and custom). A 404 must ask exactly what its path
    // space asks - no more, and no less.

    testUnknownApiPath: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/no-such-endpoint`);
      assertPermissions(base(DB), observe());
    },

    testUnknownAdminPath: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_admin/no-such-endpoint`);
      assertPermissions(base(DB), observe());
    },

    // outside /_, so no API version question - the request is authenticated and
    // the database check succeeds, which is what distinguishes this from
    // testUnauthenticatedCustomPathEscalates below. The two observations are
    // identical; the difference is invisible from the outside.
    testUnknownCustomPath: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/no-such-custom-route`);
      assertPermissions(baseOutsideUnderscore(DB), observe());
    },

    // ── F. the unauthenticated superuser escalation ──────────────────────
    // --server.authentication-system-only defaults to true, so
    // hasAllowedUnauthenticatedPath() is live for every path that does not
    // start with /_ once the /_db/<name> prefix has been stripped.
    //
    // AUDIT: this is the security statement about this handler. An
    // unauthenticated request to a non-/_ path is answered with SUPERUSER
    // rights: authentication is DENIED but turned into GRANTED, the API version
    // question is skipped entirely, the database question is asked and denied
    // (the context is AuthMode::Unauthenticated), and that denial is what
    // triggers _mustEscalateToSuperuser. So exactly one question is asked on the
    // way to running the Foxx route as superuser.
    //
    // AUDIT: the observed set is the same as the authenticated
    // testUnknownCustomPath above, because the API version question is skipped
    // for the non-/_ path space either way and the database question is asked
    // (and traced) whether it is answered yes or no. An observation therefore
    // cannot tell "authenticated, allowed" from "anonymous, denied, escalated to
    // superuser" - the escalation leaves no trace of its own.

    testUnauthenticatedCustomPathEscalates: function () {
      beginObserve();
      anonymousGet(`/_db/${DB}/no-such-custom-route`);
      assertPermissions([
        `UseDatabase name=${DB} level=read`
      ], observe());
    },

    // The same request under a /_ path keeps authentication DENIED, so the 401
    // is generated before any check runs and nothing is asked at all.
    testUnauthenticatedInternalPathDenied: function () {
      beginObserve();
      anonymousGet(`/_db/${DB}/_api/no-such-endpoint`);
      assertPermissions([], observe());
    },
  };
}

jsunity.run(catchallAuthzSuite);
return jsunity.done();
