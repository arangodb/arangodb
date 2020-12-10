/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual, assertNotEqual, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
/// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// Some tests skips cluster until ticket TG-1110 (Some fullCount seems to not be propagated well enough)
// is fixed. Or all cluster tests with fullCount will hang

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");
var assertQueryError = helper.assertQueryError;
const isCluster = require("@arangodb/cluster").isCluster();

function arangoSearchCountOptimization () {
  return {
    setUpAll : function () {
      db._dropView("UnitTestView");
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
      let c = db._create("UnitTestsCollection", { numberOfShards: 3 });

      let docs = [];
      for (let i = 0; i < 10010; ++i) {
        docs.push({ value: "test" + (i % 10), count: i });
      }
      c.insert(docs);
      
      db._createView("UnitTestView", "arangosearch", {links:{
                                                       UnitTestsCollection:{
                                                         includeAllFields:false,
                                                         fields:{
                                                           value:{
                                                             analyzers:["identity"]},
                                                           count:{
                                                             analyzers:["identity"]}}}}});
      db._createView("UnitTestViewSorted", "arangosearch", {primarySort: [{"field": "count", "direction": "asc"}],
                      links:{
                        UnitTestsCollection:{
                          includeAllFields:false,
                          fields:{
                            value:{analyzers:["identity"]},
                            count:{analyzers:["identity"]}}}}});
      // sync the views
      db._query("FOR d IN UnitTestView OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
      db._query("FOR d IN UnitTestViewSorted OPTIONS {waitForSync:true} LIMIT 1 RETURN d");
    },

    tearDownAll : function () {
      db._dropView("UnitTestView");
      db._dropView("UnitTestViewSorted");
      db._drop("UnitTestsCollection");
    },
    
    testNoSortWithoutFiltersExact() {
      let res = db._query("FOR d IN UnitTestView  COLLECT WITH COUNT INTO c RETURN c").toArray(); 
      assertEqual(10010, res[0]);
    },
    testNoSortWithoutFiltersCost() {
      let res = db._query("FOR d IN UnitTestView OPTIONS{countApproximate:'cost'} " + 
                          "COLLECT WITH COUNT INTO c RETURN c").toArray(); 
      assertEqual(10010, res[0]);
    },
    testNoSortLimitFullCountExact() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestView LIMIT 10, 100 RETURN d", {}, {fullCount:true}); 
      assertEqual(10010, res.getExtra().stats.fullCount);
    },
    testNoSortLimitFullCountCost() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestView OPTIONS{countApproximate:'cost'} " + 
                          "LIMIT 10, 100 RETURN d", {}, {fullCount:true}); 
      assertEqual(10010, res.getExtra().stats.fullCount);
    },
    testNoSortLimitFiltersFullCountExact() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestView SEARCH d.value == 'test1' OR " +
                          "d.value == 'test2' LIMIT 10, 100 RETURN d", {}, {fullCount:true}); 
      assertEqual(2002, res.getExtra().stats.fullCount);
    },
    testNoSortLimitFiltersFullCountCost() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestView SEARCH d.value == 'test1' OR " +
                          "d.value == 'test2' OPTIONS{countApproximate:'cost'} LIMIT 10, 100 RETURN d",
                          {}, {fullCount:true});
      assertEqual(2002, res.getExtra().stats.fullCount);
    },
    testNoSortCountFiltersExact() {
      let res = db._query("FOR d IN UnitTestView SEARCH d.value == 'test1' OR " + 
                          " d.value == 'test2'  COLLECT WITH COUNT INTO c RETURN c").toArray();
      assertEqual(2002, res[0]);
    },
    testNoSortCountFiltersCost() {
      let res = db._query("FOR d IN UnitTestView SEARCH d.value == 'test1' OR " + 
                          " d.value == 'test2' OPTIONS{countApproximate:'cost'} " +
                          " COLLECT WITH COUNT INTO c RETURN c").toArray();
      assertEqual(2002, res[0]);
    },
    testSortLimitWithFullCountExact() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestViewSorted SORT d.count ASC LIMIT 10, 100 RETURN d",
                          {}, {fullCount:true});
      assertEqual(10010, res.getExtra().stats.fullCount);
    },
    testSortLimitWithFullCountCost() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestViewSorted OPTIONS{countApproximate:'cost'} " +
                          "SORT d.count ASC LIMIT 10, 100 RETURN d", {}, {fullCount:true});
      assertEqual(10010, res.getExtra().stats.fullCount);
    },
    testSortLimitFiltersFullCountExact() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestViewSorted SEARCH d.value == 'test1' OR " +
                          "d.value == 'test2' OPTIONS{countApproximate:'exact'} " + 
                          "SORT d.count ASC LIMIT 10, 100 RETURN d",
                          {}, {fullCount:true});
      assertEqual(2002, res.getExtra().stats.fullCount);
    },
    testSortLimitFiltersFullCountCost() {
      if (isCluster) {
        return; // TG-1110
      }
      let res = db._query("FOR d IN UnitTestViewSorted SEARCH d.value == 'test1' OR " +
                          "d.value == 'test2' OPTIONS{countApproximate:'cost'} " + 
                          "SORT d.count ASC LIMIT 10, 100 RETURN d",
                          {}, {fullCount:true});
      assertEqual(2002, res.getExtra().stats.fullCount);
    },
  };
}

jsunity.run(arangoSearchCountOptimization);

return jsunity.done();

