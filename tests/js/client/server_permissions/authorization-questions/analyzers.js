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

// Authorization questions asked by the /_api/analyzer endpoint family.
//
// Observation-based counterpart of tests/api/apitests/analyzers.mjs.
//
// Handler: arangod/RestHandler/RestAnalyzerHandler.cpp
//          arangod/IResearch/IResearchAnalyzerFeature.cpp
//
// Every request first asks `UseApiVersion version=0` and then
// `UseDatabase name=<db> level=read`, where <db> is the database in the path
// (d for /_db/d/..., _system for the plain /_api/... routes).
//
// getAnalyzers (list): visits static analyzers (no question - null vocbase),
//   then, if canUseDatabase(<db>, Read) succeeds, visits that database's own
//   analyzers asking canSeeAnalyzer() per user-defined analyzer; then does the
//   same for the _system database if it differs from <db>. So the list from d
//   asks canUseDatabase(d) [== the base question] and canUseDatabase(_system).
// getAnalyzer (by name): built-in 'identity' is a static analyzer, so
//   IResearchAnalyzerFeature::canUse() short-circuits and asks NOTHING beyond
//   the base UseDatabase question.
// createAnalyzer / removeAnalyzer: IResearchAnalyzerFeature::canUse(Modify) ->
//   canUseAnalyzer(... level=modify). The emplace/remove then run internal
//   transactions on the _analyzers system collection.
//
// NOTE: analyzers require the iresearch/search feature; when it is not compiled
// in, all routes return 404 (and no questions are asked).

if (getOptions === true) {
  return {
    'server.authentication': 'true',
    'log.force-direct': 'true',
    // keep background threads from asking questions of their own
    'foxx.queues': 'false',
    // disable so it doesn't spoil the test output:
    'server.statistics': 'false'
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
  singleOnly
} = require('@arangodb/testutils/apitest-fixtures');

function analyzerApiAuthzSuite () {
  const NAME = 'apitest_analyzer';

  function dropAnalyzer (dbPrefix) {
    arango.DELETE_RAW(`${dbPrefix}/_api/analyzer/${NAME}`);
  }
  function createAnalyzer (dbPrefix) {
    arango.POST_RAW(`${dbPrefix}/_api/analyzer`, { name: NAME, type: 'identity' });
  }

  return {
    setUpAll: setUpApiTestData,
    tearDownAll: tearDownApiTestData,

    tearDown: function () {
      disableObserve();
      dropAnalyzer(`/_db/${DB}`);
      dropAnalyzer('');
    },

    // GET /_db/d/_api/analyzer - list from database d. Static analyzers ask
    // nothing; then canUseDatabase(d, Read) [duplicate of base] and
    // canUseDatabase(_system, Read) gate the two per-analyzer visits.
    // AUDIT: assumes no user-defined analyzers exist in d or _system; if any
    //        do, a `SeeAnalyzer db=<db> name=<a>` appears for each.
    testListAnalyzersD: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/analyzer`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "UseDatabase name=_system level=read",
        "UseCollection db=d name=_analyzers level=read",
        "UseCollection db=_system name=_analyzers level=read"
      ], endObserve());
    },

    // GET /_db/d/_api/analyzer/identity - built-in (static) analyzer, canUse()
    // short-circuits: only the base UseDatabase question is asked.
    testGetIdentityAnalyzerD: function () {
      beginObserve();
      arango.GET_RAW(`/_db/${DB}/_api/analyzer/identity`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read"
      ], endObserve());
    },

    // POST /_db/d/_api/analyzer - canUse(Modify) -> UseAnalyzer(... modify).
    // AUDIT: the emplace runs internal transactions on the _analyzers system
    //        collection in d; the read/writedata UseCollection questions below
    //        are a best guess (write transaction => writedata; a preceding
    //        load may add read) and may differ in count.
    testCreateAnalyzerD: function () {
      dropAnalyzer(`/_db/${DB}`);
      beginObserve();
      arango.POST_RAW(`/_db/${DB}/_api/analyzer`, { name: NAME, type: 'identity' });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseAnalyzer db=d name=apitest_analyzer level=modify",
        "UseCollection db=d name=_analyzers level=writedata",
        ...singleOnly([
          "UseCollection db=d name=_analyzers level=read"
        ])
      ], endObserve());
    },

    // DELETE /_db/d/_api/analyzer/{name} - canUse(Modify) -> UseAnalyzer(modify)
    // AUDIT: same _analyzers system-collection transaction caveat as create.
    testDeleteAnalyzerD: function () {
      dropAnalyzer(`/_db/${DB}`);
      createAnalyzer(`/_db/${DB}`);
      beginObserve();
      arango.DELETE_RAW(`/_db/${DB}/_api/analyzer/${NAME}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=d level=read",
        "IsReadOnly",
        "UseAnalyzer db=d name=apitest_analyzer level=modify",
        "UseCollection db=d name=_analyzers level=writedata",
        ...singleOnly([
          "UseCollection db=d name=_analyzers level=read"
        ])
      ], endObserve());
    },

    // GET /_api/analyzer - list from _system. Static + _system analyzers; the
    // system-vocbase branch is skipped because it equals the current vocbase.
    // AUDIT: assumes no user-defined analyzers exist in _system.
    testListAnalyzersSystem: function () {
      beginObserve();
      arango.GET_RAW(`/_api/analyzer`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // GET /_api/analyzer/identity - static analyzer, only base question.
    testGetIdentityAnalyzerSystem: function () {
      beginObserve();
      arango.GET_RAW(`/_api/analyzer/identity`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read"
      ], endObserve());
    },

    // POST /_api/analyzer - canUse(Modify) -> UseAnalyzer(_system ... modify)
    // AUDIT: _analyzers system-collection transaction caveat as above.
    testCreateAnalyzerSystem: function () {
      dropAnalyzer('');
      beginObserve();
      arango.POST_RAW(`/_api/analyzer`, { name: NAME, type: 'identity' });
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "IsReadOnly",
        "UseAnalyzer db=_system name=apitest_analyzer level=modify",
        "UseCollection db=_system name=_analyzers level=writedata",
        ...singleOnly([
          "UseCollection db=_system name=_analyzers level=read"
        ])
      ], endObserve());
    },

    // DELETE /_api/analyzer/{name} - canUse(Modify) -> UseAnalyzer(_system modify)
    // AUDIT: _analyzers system-collection transaction caveat as above.
    testDeleteAnalyzerSystem: function () {
      dropAnalyzer('');
      createAnalyzer('');
      beginObserve();
      arango.DELETE_RAW(`/_api/analyzer/${NAME}`);
      assertPermissions([
        "UseApiVersion version=0",
        "UseDatabase name=_system level=read",
        "IsReadOnly",
        "UseAnalyzer db=_system name=apitest_analyzer level=modify",
        "UseCollection db=_system name=_analyzers level=writedata",
        ...singleOnly([
          "UseCollection db=_system name=_analyzers level=read"
        ])
      ], endObserve());
    },
  };
}

jsunity.run(analyzerApiAuthzSuite);
return jsunity.done();
