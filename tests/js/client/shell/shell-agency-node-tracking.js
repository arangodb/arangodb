/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertNull, ArangoClusterInfo */

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
const internal = require("internal");
const { getCompleteMetricsValues } = require('@arangodb/test-helper');
const clusterInfo = global.ArangoClusterInfo;
const isCluster = require("internal").isCluster();

  
const cn = "UnitTestsCollection";
const n = 10000;

function AgencyNodeTracking () {
  'use strict';

  let insertDocuments = (collName) => {
    let c = db._collection(collName);
    let docs = [];
    for (let i = 0; i < n; ++i) {
      docs.push({value: i, value1: i, value2: i});
      if (docs.length === 5000) {
        c.insert(docs);
        docs = [];
      }
    }
    if (docs.length > 0) {
      c.insert(docs);
      docs = [];
    }
  };
        
  return {
    setUp: function () {},

    tearDown: function () {
      for(let i = 0; i < 5; i++) {
        db._drop(`c${i}`);
        db._dropView(`view0${i}`);
        db._dropView(`view1${i}`);
        db._dropView(`view2${i}`);
        db._dropView(`viewSa0${i}`);
        db._dropView(`viewSa1${i}`);
        db._dropView(`viewSa2${i}`);
      }
    },
    
    testMemoryUsageShouldIncreaseCollections: function () {

        let initial_metric = getCompleteMetricsValues("arangodb_agency_node_memory_usage");
        assertTrue(initial_metric > 0);

        // UNCOMMENT AFTER FIXING BTS 1584

        // let metric_before = initial_metric;
        // let metric_after;
        // for(let i = 0; i < 2; i++) {
        //   // create collection
        //   let cn = `c${i}`;
        //   db._create(cn);
        //   metric_after = getCompleteMetricsValues("arangodb_agency_node_memory_usage");
        //   if(isCluster) {
        //     assertTrue(metric_after > metric_before);
        //   } else {
        //     assertEqual(metric_before, initial_metric);
        //     assertEqual(metric_after, initial_metric);
        //   }

        //   metric_before = metric_after;
        //   insertDocuments(cn);

        //   // create few indexes
        //   let coll = db._collection(cn);
          
        //   coll.ensureIndex({type: "inverted", name: "inverted", fields: ["value"]});
        //   coll.ensureIndex({type: "inverted", name: "inverted1", fields: ["value1"]});
        //   coll.ensureIndex({type: "inverted", name: "inverted2", fields: ["value2"]});

        //   coll.ensureIndex({ type: "persistent", fields: ["value"], estimates: true });
        //   coll.ensureIndex({ type: "persistent", fields: ["value1"], estimates: true });
        //   coll.ensureIndex({ type: "persistent", fields: ["value2"], estimates: true });

        //   metric_after = getCompleteMetricsValues("arangodb_agency_node_memory_usage");
        //   if(isCluster) {
        //     assertTrue(metric_after > metric_before);
        //   } else {
        //     assertEqual(metric_before, initial_metric);
        //     assertEqual(metric_after, initial_metric);
        //   }
        //   metric_before = metric_after;

        //   // create few arangosearch views
        //   db._createView(`view0${i}`, 'arangosearch', { links: { [cn]: {value: {}} } });
        //   db._createView(`view1${i}`, 'arangosearch', { links: { [cn]: {value1: {}} } });
        //   db._createView(`view2${i}`, 'arangosearch', { links: { [cn]: {value2: {}} } });

        //   metric_after = getCompleteMetricsValues("arangodb_agency_node_memory_usage");
        //   if(isCluster) {
        //     assertTrue(metric_after > metric_before);
        //   } else {
        //     assertEqual(metric_before, initial_metric);
        //     assertEqual(metric_after, initial_metric);
        //   }
        //   metric_before = metric_after;

        //   // create few search-alias views
        //   db._createView(`viewSa0${i}`, 'search-alias', { indexes: [ {collection: `${cn}`, index: "inverted"}]} );
        //   db._createView(`viewSa1${i}`, 'search-alias', { indexes: [ {collection: `${cn}`, index: "inverted1"}]});
        //   db._createView(`viewSa2${i}`, 'search-alias', { indexes: [ {collection: `${cn}`, index: "inverted2"}]});

        //   metric_after = getCompleteMetricsValues("arangodb_agency_node_memory_usage");
        //   if(isCluster) {
        //     assertTrue(metric_after > metric_before);
        //   } else {
        //     assertEqual(metric_before, initial_metric);
        //     assertEqual(metric_after, initial_metric);
        //   }
        //   metric_before = metric_after;
        // }
      }
  };
}

jsunity.run(AgencyNodeTracking);
return jsunity.done();
