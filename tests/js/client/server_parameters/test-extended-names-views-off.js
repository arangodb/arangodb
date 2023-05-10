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
    'database.extended-names': "false",
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('@arangodb').errors;
const ArangoView = require("@arangodb").ArangoView;
const isCluster = require("internal").isCluster;

const traditionalName = "UnitTestsView";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";

function testSuite() {
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
      let view = db._createView(traditionalName, "arangosearch", {});
      assertTrue(view instanceof ArangoView);

      view = db._view(traditionalName);
      assertTrue(view instanceof ArangoView);
    },
    
    testArangosearchExtendedName: function() {
      try {
        db._createView(extendedName, "arangosearch", {});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
      
      let v = db._view(extendedName);
      assertNull(v);
    },
    
    testArangosearchExtendedCollectionName: function() {
      let view = db._createView(traditionalName, "arangosearch", {});
      let links = { [extendedName]: { includeAllFields: true } };
      try {
        view.properties({ links });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
    },
    
    testArangosearchRenameToExtendedName: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }
        
      let view = db._createView(traditionalName, "arangosearch", {});
      try {
        view.rename(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },
    
    testSearchAliasTraditionalName: function() {
      let view = db._createView(traditionalName, "search-alias", {});
      assertTrue(view instanceof ArangoView);

      view = db._view(traditionalName);
      assertTrue(view instanceof ArangoView);
    },
    
    testSearchAliasExtendedName: function() {
      try {
        db._createView(extendedName, "search-alias", {});
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
      
      let v = db._view(extendedName);
      assertNull(v);
    },
    
    testSearchAliasRenameToExtendedName: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }
        
      let view = db._createView(traditionalName, "search-alias", {});
      try {
        view.rename(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
