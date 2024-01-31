/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2020, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const errors = require("internal").errors;
const { deriveTestSuite } = require('@arangodb/test-helper');
const isEnterprise = require("internal").isEnterprise();
  
const vn = "UnitTestsVertex";
const on = "UnitTestsOrphan";
const en = "UnitTestsEdges";
const gn = "UnitTestsGraph";
const smartGraphAttribute = 'smart';

function BaseTestConfig() {
  return {
    testWithPoisonedEdge : function () {
      const n = "doesnotexist";
      db._create(n);
      try {
        db[en + "1"].insert({ _from: vn + "/42:test1", _to: n + "/42:test2", smartGraphAttribute: "42" });
        
        try {
          db._query(`FOR v IN ANY "${vn}/42:test1" GRAPH "${gn}" RETURN v`);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_QUERY_COLLECTION_LOCK_FAILED.code, err.errorNum);
        }
      } finally {
        db._drop(n);
      }
    },
  };
}

function GraphCollectionRestrictionGeneralGraph() {
  'use strict';
    
  const graphs = require("@arangodb/general-graph");

  let suite = {
    setUp: function () {
      graphs._create(gn, [graphs._relation(en + "1", vn, vn), graphs._relation(en + "2", vn, vn)], [on], { numberOfShards: 4 });
    },

    tearDown : function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {}

      db._drop(vn);
      db._drop(on);
      db._drop(en + "1");
      db._drop(en + "2");
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_GeneralGraph');
  return suite;
}

function GraphCollectionRestrictionSmartGraph() {
  'use strict';
    
  const graphs = require("@arangodb/smart-graph");

  let suite = {
    setUp: function () {
      graphs._create(gn, [graphs._relation(en + "1", vn, vn), graphs._relation(en + "2", vn, vn)], [on], { numberOfShards: 4, smartGraphAttribute });
    },

    tearDown : function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {}

      db._drop(vn);
      db._drop(on);
      db._drop(en + "1");
      db._drop(en + "2");
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_SmartGraph');
  return suite;
}
  
function GraphCollectionRestrictionDisjointSmartGraph() {
  'use strict';
  
  const graphs = require("@arangodb/smart-graph");

  let suite = {
    setUp: function () {
      graphs._create(gn, [graphs._relation(en + "1", vn, vn), graphs._relation(en + "2", vn, vn)], [on], { numberOfShards: 4, smartGraphAttribute, isDisjoint: true });
    },

    tearDown : function () {
      try {
        graphs._drop(gn, true);
      } catch (err) {}

      db._drop(vn);
      db._drop(on);
      db._drop(en + "1");
      db._drop(en + "2");
    },
  };

  deriveTestSuite(BaseTestConfig(), suite, '_DisjointSmartGraph');
  return suite;
}

jsunity.run(GraphCollectionRestrictionGeneralGraph);

if (isEnterprise) {
  jsunity.run(GraphCollectionRestrictionSmartGraph);
  jsunity.run(GraphCollectionRestrictionDisjointSmartGraph);
}

return jsunity.done();
