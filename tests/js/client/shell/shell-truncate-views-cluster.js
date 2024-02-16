/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertFalse, assertEqual, assertNotEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const analyzers = require("@arangodb/analyzers");
const {
  clearAllFailurePoints,
  debugCanUseFailAt,
  debugSetFailAt,
  getEndpointById,
  getDBServers,
} = require('@arangodb/test-helper');
  
const cn = "UnitTestsCollection";
const servers = getDBServers();

const createAnalyzers = () => {
  try {
    analyzers.remove("testAnalyzer", true);
  } catch (e) {
  }
  analyzers.save("testAnalyzer", "delimiter", { delimiter: "/" });
};

const viewName = (name, isSearchAlias) => {
  if (isSearchAlias) {
    return name + "_sa";
  } else {
    return name + "_as";
  }
};
  
const removeViews = (isSearchAlias) => {
  let vn = viewName(cn + "_view", isSearchAlias);
  db._dropView(vn);
  vn = viewName(cn + "_view_ps", isSearchAlias);
  db._dropView(vn);
  vn = viewName(cn + "_view_sv", isSearchAlias);
  db._dropView(vn);
  vn = viewName(cn + "_view_ps_sv", isSearchAlias);
  db._dropView(vn);
};
  
const createViews = (isSearchAlias) => {
  removeViews(isSearchAlias);
  let vn = viewName(cn + "_view", isSearchAlias);
  if (isSearchAlias) {
    let i = db._collection(cn).ensureIndex({
      type: "inverted",
      includeAllFields: true,
      analyzer: "testAnalyzer",
    });
    db._createView(vn, "search-alias", {
      indexes: [{collection: cn, index: i.name}]
    });
  } else {
    db._createView(vn, "arangosearch", {
      links: {[cn]: {analyzers: ["testAnalyzer", "identity"], includeAllFields: true}}
    });
  }

  vn = viewName(cn + "_view_ps", isSearchAlias);
  if (isSearchAlias) {
    let i = db._collection(cn).ensureIndex({
      type: "inverted",
      includeAllFields: true,
      analyzer: "testAnalyzer",
      primarySort: {fields: [{field: "_id", asc: false}, {field: "_key", asc: true}]},
    });
    db._createView(vn, "search-alias", {
      indexes: [{collection: cn, index: i.name}]
    });
  } else {
    db._createView(vn, "arangosearch", {
      links: {[cn]: {includeAllFields: true}},
      primarySort: [{field: "_id", asc: false}, {field: "_key", asc: true}],
    });
  }

  vn = viewName(cn + "_view_sv", isSearchAlias);
  if (isSearchAlias) {
    let i = db._collection(cn).ensureIndex({
      type: "inverted",
      includeAllFields: true,
      analyzer: "testAnalyzer",
      storedValues: [["_id"], ["_key"]],
    });
    db._createView(vn, "search-alias", {
      indexes: [{collection: cn, index: i.name}]
    });
  } else {
    db._createView(vn, "arangosearch", {
      links: {[cn]: {includeAllFields: true}},
      storedValues: [["_id"], ["_key"]],
    });
  }

  vn = viewName(cn + "_view_ps_sv", isSearchAlias);
  if (isSearchAlias) {
    let i = db._collection(cn).ensureIndex({
      type: "inverted",
      includeAllFields: true,
      analyzer: "testAnalyzer",
      storedValues: [["_id"], ["_key"]],
      primarySort: {fields: [{field: "_id", asc: false}, {field: "_key", asc: true}]},
    });
    db._createView(vn, "search-alias", {
      indexes: [{collection: cn, index: i.name}]
    });
  } else {
    db._createView(vn, "arangosearch", {
      links: {[cn]: {includeAllFields: true}},
      storedValues: [["_id"], ["_key"]],
      primarySort: [{field: "_id", asc: false}, {field: "_key", asc: true}],
    });
  }
};
          
const checkViewConsistency = (cn, isSearchAlias, isMaterialize, viewCounts) => {
  let runQuery = (query) => {
    return db._query(query).toArray();
  };
  let expected;
  if (!isMaterialize) {
    expected = runQuery("FOR d IN " + cn + " COLLECT WITH COUNT INTO length RETURN length")[0];
  } else {
    expected = runQuery("FOR d IN " + cn + " RETURN d").length;
  }
  let vn;
  const checkView = (isMaterialize) => {
    let result;
    if (isMaterialize) {
      result = runQuery("FOR d IN " + vn + " OPTIONS {waitForSync: true} RETURN d._key").length;
    } else {
      result = runQuery("FOR d IN " + vn + " OPTIONS {waitForSync: true} COLLECT WITH COUNT INTO length RETURN length")[0];
    }
    viewCounts.push({viewName:vn, viewCount:result, collectionName:cn, collectionCount:expected, isMaterialize: isMaterialize});
  };

  vn = viewName(cn + "_view", isSearchAlias);
  checkView(isMaterialize);

  vn = viewName(cn + "_view_ps", isSearchAlias);
  checkView(isMaterialize);

  vn = viewName(cn + "_view_sv", isSearchAlias);
  checkView(isMaterialize);

  vn = viewName(cn + "_view_ps_sv", isSearchAlias);
  checkView(isMaterialize);
};

function TruncateSuite () {
  'use strict';
  
  return {
    setUp : function () {
      createAnalyzers();

      db._create(cn, { numberOfShards: 6, replicationFactor: 1 });
      
      createViews(true);
      createViews(false);
    },

    tearDown : function () {
      clearAllFailurePoints();

      removeViews(true);
      removeViews(false);

      db._drop(cn);
      
      analyzers.remove('testAnalyzer');
    },

    testTruncateViewWithRangeDelete : function () {
      for (const server of servers) {
        debugSetFailAt(getEndpointById(server.id), "TruncateRangeDeleteLowValue");
      }

      const n = 2000;

      let c = db._collection(cn);
      
      let docs = [];
      for (let i = 0; i < n; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: "test" + i });
      }
      c.insert(docs);

      assertEqual(n, c.count());
      assertEqual(n, c.toArray().length);
      
      c.truncate();
      
      assertEqual(0, c.count());
      assertEqual(0, c.toArray().length);

      let viewCounts = [];
      checkViewConsistency(cn, false, false, viewCounts);
      checkViewConsistency(cn, true, false, viewCounts);
      checkViewConsistency(cn, false, true, viewCounts);
      checkViewConsistency(cn, true, true, viewCounts);
      
      for (const c of viewCounts) {
        assertEqual(c.viewCount, c.collectionCount, c);
      }
    },

  };
}

if (debugCanUseFailAt(getEndpointById(servers[0].id))) {
  jsunity.run(TruncateSuite);
}
return jsunity.done();
