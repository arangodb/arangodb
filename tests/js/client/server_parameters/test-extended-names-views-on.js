/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, assertNull, assertNotNull, assertUndefined, fail */

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
    'database.extended-names': "true",
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
      // renaming not supported in cluster
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
    assertTrue(extendedName, view.name());

    // check that we can retrieve properties for view
    let properties = view.properties();
    if (view.type() === "arangosearch") {
      assertTrue(properties.hasOwnProperty("links"));
    } else {
      assertTrue(properties.hasOwnProperty("indexes"));
    }
   
    // check that we can set the properties for view
    if (view.type() === "arangosearch") {
      let res = view.properties({ links: {} });
      assertEqual(properties, res);
    } else {
      let res = view.properties({ indexes: [] });
      assertEqual(properties, res);
    }

    if (!isCluster()) {
      // renaming not supported in cluster
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
      let view = db._createView(traditionalName, "arangosearch", {});
      assertTrue(view instanceof ArangoView);
      
      checkTraditionalName();
    },

    testArangosearchTraditionalNameWithExtendedCollectionNames: function() {
      let view = db._createView(traditionalName, "arangosearch", {});
      assertTrue(view instanceof ArangoView);

      // create a collection with an extended name
      db._create(extendedName);
      try {
        // and create a link on it
        let properties = view.properties({ links: { [extendedName]: { includeAllFields: true } } });
        assertTrue(properties.links.hasOwnProperty(extendedName));
      } finally {
        db._drop(extendedName);
      }
    },
    
    testArangosearchExtendedName: function() {
      let view = db._createView(extendedName, "arangosearch", {});
      assertTrue(view instanceof ArangoView);

      checkExtendedName();
    },
    
    testArangosearchExtendedNameWithExtendedCollectionNames: function() {
      let view = db._createView(extendedName, "arangosearch", {});
      assertTrue(view instanceof ArangoView);

      const otherExtendedName = extendedName + " !";
      // create a collection with an extended name
      db._create(otherExtendedName);
      try {
        // and create a link on it
        let properties = view.properties({ links: { [otherExtendedName]: { includeAllFields: true } } });
        assertTrue(properties.links.hasOwnProperty(otherExtendedName));
      } finally {
        db._drop(otherExtendedName);
      }
    },

    testArangosearchCreateInvalidUtf8Names: function() {
      invalidNames.forEach((name) => {
        try {
          db._createView(name, "arangosearch", {});
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testArangosearchRenameToInvalidUtf8: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }
      let view = db._createView(extendedName, "arangosearch", {});
      invalidNames.forEach((name) => {
        try {
          view.rename(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },

    testArangosearchExtendedViewNameUseInQuery1: function() {
      let c = db._create(traditionalName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value1: i, value2: "testmann" + i });
      }
      c.insert(docs);

      try {
        let view = db._createView(extendedName, "arangosearch", {});
        view.properties({ links: { [traditionalName]: { includeAllFields: true } } });

        let res = db._query("FOR doc IN `" + extendedName + "` SEARCH doc.value1 >= 0 OPTIONS { waitForSync: true } SORT doc.value1 RETURN doc").toArray();
        assertEqual(100, res.length);
        for (let i = 0; i < 100; ++i) {
          assertEqual(i, res[i].value1);
          assertTrue(res[i]._id.startsWith(traditionalName + "/"));
        }
        
        res = db._query("FOR doc IN `" + extendedName + "` SEARCH doc.value1 >= 0 OPTIONS { waitForSync: true } SORT doc.value1 RETURN doc.value1").toArray();
        assertEqual(100, res.length);
        for (let i = 0; i < 100; ++i) {
          assertEqual(i, res[i]);
        }
      } finally {
        db._drop(traditionalName);
      }
    },
    
    testArangosearchExtendedViewNameUseInQuery2: function() {
      let c = db._create(extendedName);
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value1: i, value2: "testmann" + i });
      }
      c.insert(docs);

      try {
        let view = db._createView(traditionalName, "arangosearch", {});
        view.properties({ links: { [extendedName]: { includeAllFields: true } } });

        let res = db._query("FOR doc IN `" + traditionalName + "` SEARCH doc.value1 >= 0 OPTIONS { waitForSync: true } SORT doc.value1 RETURN doc").toArray();
        assertEqual(100, res.length);
        for (let i = 0; i < 100; ++i) {
          assertEqual(i, res[i].value1);
          assertTrue(res[i]._id.startsWith(extendedName + "/"));
        }
        
        res = db._query("FOR doc IN `" + traditionalName + "` SEARCH doc.value1 >= 0 OPTIONS { waitForSync: true } SORT doc.value1 RETURN doc.value1").toArray();
        assertEqual(100, res.length);
        for (let i = 0; i < 100; ++i) {
          assertEqual(i, res[i]);
        }
      } finally {
        db._drop(extendedName);
      }
    },
    
    testSearchAliasTraditionalName: function() {
      let view = db._createView(traditionalName, "search-alias", {});
      assertTrue(view instanceof ArangoView);

      checkTraditionalName();
    },
    
    testSearchAliasTraditionalNameWithExtendedIndexName: function() {
      let view = db._createView(traditionalName, "search-alias", {});
      assertTrue(view instanceof ArangoView);

      // create an index with an extended name
      const otherTraditionalName = traditionalName + "2";

      let c = db._create(otherTraditionalName);
      try {
        c.ensureIndex({ fields: ["value"], type: "inverted", name: extendedName });
        let indexes = [{collection: otherTraditionalName, index: extendedName}];
        
        // and create indexes property
        let properties = view.properties({ indexes });
        assertEqual(indexes, properties.indexes);
      } finally {
        db._drop(otherTraditionalName);
      }
    },
    
    testSearchAliasExtendedName: function() {
      let view = db._createView(extendedName, "search-alias", {});
      assertTrue(view instanceof ArangoView);
      
      checkExtendedName();
    },
    
    testSearchAliasExtendedNameWithExtendedCollectionName: function() {
      let view = db._createView(extendedName, "search-alias", {});
      assertTrue(view instanceof ArangoView);

      // create a collection with an extended name
      const otherExtendedName = extendedName + "2!";

      let c = db._create(otherExtendedName);
      try {
        c.ensureIndex({ fields: ["value"], type: "inverted", name: extendedName });
        let indexes = [{collection: otherExtendedName, index: extendedName}];
        
        // and create indexes property
        let properties = view.properties({ indexes });
        assertEqual(indexes, properties.indexes);
      } finally {
        db._drop(otherExtendedName);
      }
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
    
    testSearchAliasRenameToInvalidUtf8: function() {
      if (isCluster()) {
        // renaming not supported in cluster
        return;
      }
      let view = db._createView(extendedName, "search-alias", {});
      invalidNames.forEach((name) => {
        try {
          view.rename(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testViewInvalidUtf8Names: function() {
      // db._view() returns null for non-existing views...
      invalidNames.forEach((name) => {
        // drop view
        assertNull(db._view(name));
      });
    },
    
    testDropViewInvalidUtf8Names: function() {
      // db._dropView() returns undefined for non-existing views...
      invalidNames.forEach((name) => {
        assertUndefined(db._dropView(name));
      });
    },
    
  };
}

jsunity.run(testSuite);
return jsunity.done();
