/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertNull, assertNotNull, fail */

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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names-views': "true",
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('internal').errors;
const ArangoView = require("@arangodb").ArangoView;
const isCluster = require('internal').isCluster;

const traditionalName = "UnitTestsView";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";
      
const invalidNames = [
  "\u212b", // Angstrom, not normalized;
  "\u0041\u030a", // Angstrom, NFD-normalized;
  "\u0073\u0323\u0307", // s with two combining marks, NFD-normalized;
  "\u006e\u0303\u00f1", // non-normalized sequence;
];

function testSuite() {
  let checkTraditionalName = () => {
    let view = db._view(traditionalName);
    assertTrue(view instanceof ArangoView);
    
    if (!isCluster()) {
      view.rename(extendedName);
      assertNull(db._view(traditionalName));
      assertNotNull(db._view(extendedName));
      view.rename(traditionalName);
    }

    db._dropView(traditionalName);
  };

  let checkExtendedName = () => {
    let view = db._view(extendedName);
    assertTrue(view instanceof ArangoView);

    if (!isCluster()) {
      view.rename(traditionalName);
      assertNull(db._view(extendedName));
      assertNotNull(db._view(traditionalName));
      view.rename(extendedName);
    }

    db._dropView(extendedName);
  };

  return {
    tearDown: function() {
      try {
        db._dropView(traditionalName);
      } catch (err) {}
      try {
        db._dropView(extendedName);
      } catch (err) {}
    },

    testArangosearchTraditionalName: function() {
      let res = db._createView(traditionalName, "arangosearch", {});
      assertTrue(res);
      
      checkTraditionalName();
    },
    
    testArangosearchExtendedName: function() {
      let res = db._createView(extendedName, "arangosearch", {});
      assertTrue(res);

      checkExtendedName();
    },


    testArangosearchInvalidUtf8Names: function() {
      invalidNames.forEach((name) => {
        try {
          db._createView(name, "arangosearch", {});
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testSearchAliasTraditionalName: function() {
      let res = db._createView(traditionalName, "search-alias", {});
      assertTrue(res);

      checkTraditionalName();
    },
    
    testSearchAliasExtendedName: function() {
      let res = db._createView(extendedName, "search-alias", {});
      assertTrue(res);
      
      checkExtendedName();
    },
    
    testSearchAliasInvalidUtf8Names: function() {
      invalidNames.forEach((name) => {
        try {
          db._createView(name, "search-alias", {});
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
