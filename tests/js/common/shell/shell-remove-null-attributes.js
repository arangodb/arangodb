/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertNull, fail */

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

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;

function removeNullAttributesDocumentSuite() {
  'use strict';
  const cn = "UnitTestsCollection";

  let c;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
      c = null;
    },
    
    testDocumentInsertNullKey : function () {
      // removeNullAttributes not set
      try {
        c.insert({ _key: null });
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
      
      // removeNullAttributes = false
      try {
        c.insert({ _key: null }, { removeNullAttributes: false });
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
      
      assertEqual(0, c.count());
      // removeNullAttributes = true
      c.insert({ _key: null }, { removeNullAttributes: true });
      assertEqual(1, c.count());
    },

    testDocumentInsert : function () {
      // removeNullAttributes not set
      c.insert({ _key: "test0", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });
      let doc = c.document("test0");
      assertTrue(doc.hasOwnProperty("value1"));
      assertNull(doc.value1);
      assertTrue(doc.hasOwnProperty("value2"));
      assertEqual("not null", doc.value2);    
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertNull(doc.sub.value1);
      assertTrue(doc.sub.hasOwnProperty("value2"));
      assertNull(doc.sub.value2);
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertTrue(doc.sub.value3.hasOwnProperty("subsub"));
      assertNull(doc.sub.value3.subsub);

      // removeNullAttributes = false
      c.insert({ _key: "test1", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } }, { removeNullAttributes: false });
      doc = c.document("test1");
      assertTrue(doc.hasOwnProperty("value1"));
      assertNull(doc.value1);
      assertTrue(doc.hasOwnProperty("value2"));
      assertEqual("not null", doc.value2);    
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertNull(doc.sub.value1);
      assertTrue(doc.sub.hasOwnProperty("value2"));
      assertNull(doc.sub.value2);
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertTrue(doc.sub.value3.hasOwnProperty("subsub"));
      assertNull(doc.sub.value3.subsub);
      
      // removeNullAttributes = true
      c.insert({ _key: "test2", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } }, { removeNullAttributes: true });
      doc = c.document("test2");
      assertFalse(doc.hasOwnProperty("value1"));
      assertTrue(doc.hasOwnProperty("value2"));
      assertEqual("not null", doc.value2);    
      assertTrue(doc.hasOwnProperty("sub"));
      assertFalse(doc.sub.hasOwnProperty("value1"));
      assertFalse(doc.sub.hasOwnProperty("value2"));
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertFalse(doc.sub.value3.hasOwnProperty("subsub"));
      
    },
    
    testDocumentUpdate : function () {
      // removeNullAttributes not set
      c.insert({ _key: "test0", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

      c.update("test0", { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } });
      let doc = c.document("test0");
      assertTrue(doc.hasOwnProperty("value1"));
      assertNull(doc.value1);
      assertTrue(doc.hasOwnProperty("value2"));
      assertNull(doc.value2);
      assertTrue(doc.hasOwnProperty("value4"));
      assertEqual("not null", doc.value4);    
      assertTrue(doc.hasOwnProperty("value3"));
      assertTrue(doc.value3.hasOwnProperty("foo"));
      assertTrue(doc.value3.hasOwnProperty("subsub"));
      assertNull(doc.value3.foo);
      assertEqual(123, doc.value3.subsub);
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertEqual("not null", doc.sub.value1);
      assertTrue(doc.sub.hasOwnProperty("value2"));
      assertNull(doc.sub.value2);
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertNull(doc.sub.value3);
      
      // removeNullAttributes = false
      c.insert({ _key: "test1", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

      c.update("test1", { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } }, { removeNullAttributes: false });
      doc = c.document("test1");
      assertTrue(doc.hasOwnProperty("value1"));
      assertNull(doc.value1);
      assertTrue(doc.hasOwnProperty("value2"));
      assertNull(doc.value2);
      assertTrue(doc.hasOwnProperty("value4"));
      assertEqual("not null", doc.value4);    
      assertTrue(doc.hasOwnProperty("value3"));
      assertTrue(doc.value3.hasOwnProperty("foo"));
      assertNull(doc.value3.foo);
      assertTrue(doc.value3.hasOwnProperty("subsub"));
      assertEqual(123, doc.value3.subsub);
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertEqual("not null", doc.sub.value1);
      assertTrue(doc.sub.hasOwnProperty("value2"));
      assertNull(doc.sub.value2);
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertNull(doc.sub.value3);
      
      // removeNullAttributes = true
      c.insert({ _key: "test2", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

      c.update("test2", { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } }, { removeNullAttributes: true });
      doc = c.document("test2");
      assertTrue(doc.hasOwnProperty("value1"));
      assertNull(doc.value1);
      assertFalse(doc.hasOwnProperty("value2"));
      assertTrue(doc.hasOwnProperty("value4"));
      assertEqual("not null", doc.value4);    
      assertTrue(doc.hasOwnProperty("value3"));
      assertFalse(doc.value3.hasOwnProperty("foo"));
      assertTrue(doc.value3.hasOwnProperty("subsub"));
      assertEqual(123, doc.value3.subsub);
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertEqual("not null", doc.sub.value1);
      assertTrue(doc.sub.hasOwnProperty("value2"));
      assertNull(doc.sub.value2);
      assertFalse(doc.sub.hasOwnProperty("value3"));
    },
    
    testDocumentReplace : function () {
      // removeNullAttributes not set
      c.insert({ _key: "test0", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

      c.replace("test0", { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } });
      let doc = c.document("test0");
      assertFalse(doc.hasOwnProperty("value1"));
      assertTrue(doc.hasOwnProperty("value2"));
      assertNull(doc.value2);
      assertTrue(doc.hasOwnProperty("value4"));
      assertEqual("not null", doc.value4);    
      assertTrue(doc.hasOwnProperty("value3"));
      assertTrue(doc.value3.hasOwnProperty("foo"));
      assertTrue(doc.value3.hasOwnProperty("subsub"));
      assertNull(doc.value3.foo);
      assertEqual(123, doc.value3.subsub);
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertEqual("not null", doc.sub.value1);
      assertFalse(doc.sub.hasOwnProperty("value2"));
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertNull(doc.sub.value3);
      
      // removeNullAttributes = false
      c.insert({ _key: "test1", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

      c.replace("test1", { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } }, { removeNullAttributes: false });
      doc = c.document("test1");
      assertFalse(doc.hasOwnProperty("value1"));
      assertTrue(doc.hasOwnProperty("value2"));
      assertNull(doc.value2);
      assertTrue(doc.hasOwnProperty("value4"));
      assertEqual("not null", doc.value4);    
      assertTrue(doc.hasOwnProperty("value3"));
      assertTrue(doc.value3.hasOwnProperty("foo"));
      assertNull(doc.value3.foo);
      assertTrue(doc.value3.hasOwnProperty("subsub"));
      assertEqual(123, doc.value3.subsub);
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertEqual("not null", doc.sub.value1);
      assertFalse(doc.sub.hasOwnProperty("value2"));
      assertTrue(doc.sub.hasOwnProperty("value3"));
      assertNull(doc.sub.value3);
      
      // removeNullAttributes = true
      c.insert({ _key: "test2", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

      c.replace("test2", { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } }, { removeNullAttributes: true });
      doc = c.document("test2");
      assertFalse(doc.hasOwnProperty("value1"));
      assertFalse(doc.hasOwnProperty("value2"));
      assertTrue(doc.hasOwnProperty("value4"));
      assertEqual("not null", doc.value4);    
      assertTrue(doc.hasOwnProperty("value3"));
      assertFalse(doc.value3.hasOwnProperty("foo"));
      assertTrue(doc.value3.hasOwnProperty("subsub"));
      assertEqual(123, doc.value3.subsub);
      assertTrue(doc.hasOwnProperty("sub"));
      assertTrue(doc.sub.hasOwnProperty("value1"));
      assertEqual("not null", doc.sub.value1);
      assertFalse(doc.sub.hasOwnProperty("value2"));
      assertFalse(doc.sub.hasOwnProperty("value3"));
    },

  };
}

function removeNullAttributesQuerySuite() {
  'use strict';
  const cn = "UnitTestsCollection";

  let c;

  return {
    setUp : function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
      c = null;
    },
    
    testQueryInsertNullKey : function () {
      // removeNullAttributes not set
      try {
        db._query(`INSERT { _key: null } INTO ${cn}`);
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
      
      // removeNullAttributes = false
      try {
        db._query(`INSERT { _key: null } INTO ${cn} OPTIONS { removeNullAttributes: false }`);
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_KEY_BAD.code, err.errorNum);
      }
      
      assertEqual(0, c.count());
      // removeNullAttributes = true
      db._query(`INSERT { _key: null } INTO ${cn} OPTIONS { removeNullAttributes: true }`);
      assertEqual(1, c.count());
    },
    
    testQueryInsert : function () {
      // removeNullAttributes not set
      [true, false].forEach((disableRules) => {
        let rules = {};
        if (disableRules) {
          // particularly disable the single-remote-modification operation in the cluster!
          rules = { optimizer: { rules: ["-all"] } };
        }
        db._query(`INSERT { _key: "test0", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } } INTO ${cn}`, null, rules);
        let doc = c.document("test0");
        assertTrue(doc.hasOwnProperty("value1"));
        assertNull(doc.value1);
        assertTrue(doc.hasOwnProperty("value2"));
        assertEqual("not null", doc.value2);    
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertNull(doc.sub.value1);
        assertTrue(doc.sub.hasOwnProperty("value2"));
        assertNull(doc.sub.value2);
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertTrue(doc.sub.value3.hasOwnProperty("subsub"));
        assertNull(doc.sub.value3.subsub);

        c.remove("test0");
        
        // removeNullAttributes = false
        db._query(`INSERT { _key: "test1", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } } INTO ${cn} OPTIONS { removeNullAttributes: false }`, null, rules);
        doc = c.document("test1");
        assertTrue(doc.hasOwnProperty("value1"));
        assertNull(doc.value1);
        assertTrue(doc.hasOwnProperty("value2"));
        assertEqual("not null", doc.value2);    
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertNull(doc.sub.value1);
        assertTrue(doc.sub.hasOwnProperty("value2"));
        assertNull(doc.sub.value2);
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertTrue(doc.sub.value3.hasOwnProperty("subsub"));
        assertNull(doc.sub.value3.subsub);
        
        c.remove("test1");
        
        // removeNullAttributes = true
        db._query(`INSERT { _key: "test2", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } } INTO ${cn} OPTIONS { removeNullAttributes: true }`, null, rules);
        doc = c.document("test2");
        assertFalse(doc.hasOwnProperty("value1"));
        assertTrue(doc.hasOwnProperty("value2"));
        assertEqual("not null", doc.value2);    
        assertTrue(doc.hasOwnProperty("sub"));
        assertFalse(doc.sub.hasOwnProperty("value1"));
        assertFalse(doc.sub.hasOwnProperty("value2"));
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertFalse(doc.sub.value3.hasOwnProperty("subsub"));
        
        c.remove("test2");
      });
    },
    
    testQueryUpdate : function () {
      // removeNullAttributes not set
      [true, false].forEach((disableRules) => {
        let rules = {};
        if (disableRules) {
          // particularly disable the single-remote-modification operation in the cluster!
          rules = { optimizer: { rules: ["-all"] } };
        }
        c.insert({ _key: "test0", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

        db._query(`UPDATE "test0" WITH { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } } INTO ${cn}`, null, rules);
        let doc = c.document("test0");
        assertTrue(doc.hasOwnProperty("value1"));
        assertNull(doc.value1);
        assertTrue(doc.hasOwnProperty("value2"));
        assertNull(doc.value2);
        assertTrue(doc.hasOwnProperty("value4"));
        assertEqual("not null", doc.value4);    
        assertTrue(doc.hasOwnProperty("value3"));
        assertTrue(doc.value3.hasOwnProperty("foo"));
        assertTrue(doc.value3.hasOwnProperty("subsub"));
        assertNull(doc.value3.foo);
        assertEqual(123, doc.value3.subsub);
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertEqual("not null", doc.sub.value1);
        assertTrue(doc.sub.hasOwnProperty("value2"));
        assertNull(doc.sub.value2);
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertNull(doc.sub.value3);

        c.remove("test0");
      
        // removeNullAttributes = false
        c.insert({ _key: "test1", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

        db._query(`UPDATE "test1" WITH { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } } INTO ${cn} OPTIONS { removeNullAttributes: false }`, null, rules);
        doc = c.document("test1");
        assertTrue(doc.hasOwnProperty("value1"));
        assertNull(doc.value1);
        assertTrue(doc.hasOwnProperty("value2"));
        assertNull(doc.value2);
        assertTrue(doc.hasOwnProperty("value4"));
        assertEqual("not null", doc.value4);    
        assertTrue(doc.hasOwnProperty("value3"));
        assertTrue(doc.value3.hasOwnProperty("foo"));
        assertNull(doc.value3.foo);
        assertTrue(doc.value3.hasOwnProperty("subsub"));
        assertEqual(123, doc.value3.subsub);
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertEqual("not null", doc.sub.value1);
        assertTrue(doc.sub.hasOwnProperty("value2"));
        assertNull(doc.sub.value2);
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertNull(doc.sub.value3);
        
        c.remove("test1");
      
        // removeNullAttributes = true
        c.insert({ _key: "test2", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

        db._query(`UPDATE "test2" WITH { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } } INTO ${cn} OPTIONS { removeNullAttributes: true }`, null, rules);
        doc = c.document("test2");
        assertTrue(doc.hasOwnProperty("value1"));
        assertNull(doc.value1);
        assertFalse(doc.hasOwnProperty("value2"));
        assertTrue(doc.hasOwnProperty("value4"));
        assertEqual("not null", doc.value4);    
        assertTrue(doc.hasOwnProperty("value3"));
        assertFalse(doc.value3.hasOwnProperty("foo"));
        assertTrue(doc.value3.hasOwnProperty("subsub"));
        assertEqual(123, doc.value3.subsub);
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertEqual("not null", doc.sub.value1);
        assertTrue(doc.sub.hasOwnProperty("value2"));
        assertNull(doc.sub.value2);
        assertFalse(doc.sub.hasOwnProperty("value3"));
        
        c.remove("test2");
      });
    },
    
    testQueryReplace : function () {
      [true, false].forEach((disableRules) => {
        let rules = {};
        if (disableRules) {
          // particularly disable the single-remote-modification operation in the cluster!
          rules = { optimizer: { rules: ["-all"] } };
        }
        // removeNullAttributes not set
        c.insert({ _key: "test0", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

        db._query(`REPLACE "test0" WITH { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } } INTO ${cn}`, null, rules);
        let doc = c.document("test0");
        assertFalse(doc.hasOwnProperty("value1"));
        assertTrue(doc.hasOwnProperty("value2"));
        assertNull(doc.value2);
        assertTrue(doc.hasOwnProperty("value4"));
        assertEqual("not null", doc.value4);    
        assertTrue(doc.hasOwnProperty("value3"));
        assertTrue(doc.value3.hasOwnProperty("foo"));
        assertTrue(doc.value3.hasOwnProperty("subsub"));
        assertNull(doc.value3.foo);
        assertEqual(123, doc.value3.subsub);
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertEqual("not null", doc.sub.value1);
        assertFalse(doc.sub.hasOwnProperty("value2"));
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertNull(doc.sub.value3);
        
        c.remove("test0");
        
        // removeNullAttributes = false
        c.insert({ _key: "test1", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

        db._query(`REPLACE "test1" WITH { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } } INTO ${cn} OPTIONS { removeNullAttributes: false }`, null, rules);
        doc = c.document("test1");
        assertFalse(doc.hasOwnProperty("value1"));
        assertTrue(doc.hasOwnProperty("value2"));
        assertNull(doc.value2);
        assertTrue(doc.hasOwnProperty("value4"));
        assertEqual("not null", doc.value4);    
        assertTrue(doc.hasOwnProperty("value3"));
        assertTrue(doc.value3.hasOwnProperty("foo"));
        assertNull(doc.value3.foo);
        assertTrue(doc.value3.hasOwnProperty("subsub"));
        assertEqual(123, doc.value3.subsub);
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertEqual("not null", doc.sub.value1);
        assertFalse(doc.sub.hasOwnProperty("value2"));
        assertTrue(doc.sub.hasOwnProperty("value3"));
        assertNull(doc.sub.value3);
        
        c.remove("test1");
        
        // removeNullAttributes = true
        c.insert({ _key: "test2", value1: null, value2: "not null", sub: { value1: null, value2: null, value3: { subsub: null } } });

        db._query(`REPLACE "test2" WITH { value2: null, value4: "not null", value3: { foo: null, subsub: 123 }, sub: { value1: "not null", value3: null } } INTO ${cn} OPTIONS { removeNullAttributes: true }`, null, rules);
        doc = c.document("test2");
        assertFalse(doc.hasOwnProperty("value1"));
        assertFalse(doc.hasOwnProperty("value2"));
        assertTrue(doc.hasOwnProperty("value4"));
        assertEqual("not null", doc.value4);    
        assertTrue(doc.hasOwnProperty("value3"));
        assertFalse(doc.value3.hasOwnProperty("foo"));
        assertTrue(doc.value3.hasOwnProperty("subsub"));
        assertEqual(123, doc.value3.subsub);
        assertTrue(doc.hasOwnProperty("sub"));
        assertTrue(doc.sub.hasOwnProperty("value1"));
        assertEqual("not null", doc.sub.value1);
        assertFalse(doc.sub.hasOwnProperty("value2"));
        assertFalse(doc.sub.hasOwnProperty("value3"));
        
        c.remove("test2");
      });
    },
  
  };
}

jsunity.run(removeNullAttributesDocumentSuite);
jsunity.run(removeNullAttributesQuerySuite);

return jsunity.done();
