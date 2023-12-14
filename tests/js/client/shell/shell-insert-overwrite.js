/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertNull, assertTrue, assertEqual, assertUndefined, assertNotUndefined */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the insert/overwrite behavior
///
/// @file
///
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

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let ERRORS = arangodb.errors;
let db = arangodb.db;

function InsertOverwriteSuite () {
  'use strict';
  let cn = "UnitTestsCollection";

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testInsertNoOverwrite : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        c.insert({ _key: "test", value: 2 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
    },
    
    testInsertWithOverwriteConflict : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "conflict" });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
    },
    
    testInsertWithOverwriteOnlyConflict : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        c.insert({ _key: "test", value: 2 }, { overwriteMode: "conflict" });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
    },

    testInsertWithOverwrite : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwrite: true });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteNewDocument : function () {
      let c = db._collection(cn);
      
      assertEqual(0, c.count());

      c.insert({ _key: "test", value: 2 }, { overwrite: true });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },

    testInsertWithOverwriteReplace : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteOnlyReplace : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteReplaceNewDocument : function () {
      let c = db._collection(cn);
      
      assertEqual(0, c.count());

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteReplaceReturnOld : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      let result = c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "replace", returnOld: true });
      assertUndefined(result.old);
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      result = c.insert({ _key: "test", value: 3 }, { overwrite: true, overwriteMode: "replace", returnOld: true });
      assertNotUndefined(result.old);
      assertEqual("test", result.old._key);
      assertEqual(2, result.old.value);
      assertEqual(1, c.count());
      assertEqual(3, c.document("test").value);
    },
    
    testInsertWithOverwriteUpdate : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      c.insert({ _key: "test", foo: "bar" }, { overwrite: true, overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      
      c.insert({ _key: "test", bar: "baz" }, { overwrite: true, overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      assertEqual("baz", c.document("test").bar);
      
      c.insert({ _key: "test", value: 7 }, { overwrite: true, overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteOnlyUpdate : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      c.insert({ _key: "test", foo: "bar" }, { overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      
      c.insert({ _key: "test", bar: "baz" }, { overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      assertEqual("baz", c.document("test").bar);
      
      c.insert({ _key: "test", value: 7 }, { overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteUpdateNewDocument : function () {
      let c = db._collection(cn);
      
      assertEqual(0, c.count());

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "update" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteIgnore : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      
      c.insert({ _key: "test", foo: "bar" }, { overwrite: true, overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      
      c.insert({ _key: "test", bar: "baz" }, { overwrite: true, overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
      
      c.insert({ _key: "test", value: 7 }, { overwrite: true, overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteOnlyIgnore : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      
      c.insert({ _key: "test", foo: "bar" }, { overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      
      c.insert({ _key: "test", bar: "baz" }, { overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
      
      c.insert({ _key: "test", value: 7 }, { overwriteMode: "replace" });
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteIgnoreNewDocument : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "ignore" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteIgnoreNoKey : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      c.insert({ value: 2 }, { overwrite: true, overwriteMode: "ignore" });
      assertEqual(1, c.count());
    },
    
    testInsertWithOverwriteIgnoreReturnOld : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      let result = c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "ignore", returnOld: true });
      assertUndefined(result.old);
      assertEqual("test", result._key);
      let oldRev = result._rev;
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      result = c.insert({ _key: "test", value: 3 }, { overwrite: true, overwriteMode: "ignore", returnOld: true });
      assertUndefined(result.old);
      assertEqual("test", result._key);
      assertEqual(oldRev, result._rev);
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteIgnoreReturnNew : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      let result = c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "ignore", returnNew: true });
      assertEqual("test", result.new._key);
      assertEqual(2, result.new.value);
      assertEqual("test", result._key);
      let oldRev = result._rev;
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      result = c.insert({ _key: "test", value: 3 }, { overwrite: true, overwriteMode: "ignore", returnNew: true });
      assertUndefined(result.new);
      assertEqual("test", result._key);
      assertEqual(oldRev, result._rev);
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertInvalidOverwriteMode : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      c.insert({ _key: "test", value: 2 }, { overwrite: true, overwriteMode: "fleisch" });
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      c.insert({ _key: "test", value: 3 }, { overwrite: true, overwriteMode: "" });
      assertEqual(1, c.count());
      assertEqual(3, c.document("test").value);
      assertUndefined(c.document("test").foo);
      
      c.insert({ _key: "test", rats: true }, { overwrite: true, overwriteMode: false });
      assertEqual(1, c.count());
      assertUndefined(c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertTrue(c.document("test").rats);
    },
    
    testInsertOnlyInvalidOverwriteMode : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        c.insert({ _key: "test", value: 2, foo: "bar" }, { overwriteMode: "hundmann" });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
     
      try {
        c.insert({ _key: "test", value: 3 }, { overwriteMode: "" });
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
      
      try {
        c.insert({ _key: "test", rats: true }, { overwriteMode: false });
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

  };
}

function InsertOverwriteAqlSuite () {
  'use strict';
  let cn = "UnitTestsCollection";

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testInsertNoOverwriteAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        db._query("INSERT { _key: 'test', value: 2 } INTO " + cn);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
    },
    
    testInsertWithOverwriteConflictAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'conflict' }");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
    },
    
    testInsertWithOverwriteOnlyConflictAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwriteMode: 'conflict' }");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
    },

    testInsertWithOverwriteAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteNewDocumentAql : function () {
      let c = db._collection(cn);
      
      assertEqual(0, c.count());

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },

    testInsertWithOverwriteReplaceAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteOnlyReplaceAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteReplaceNewDocumentAql : function () {
      let c = db._collection(cn);
      
      assertEqual(0, c.count());

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteUpdateAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      db._query("INSERT { _key: 'test', foo: 'bar' } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      
      db._query("INSERT { _key: 'test', bar: 'baz' } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      assertEqual("baz", c.document("test").bar);
      
      db._query("INSERT { _key: 'test', value: 7 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteOnlyUpdateAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      db._query("INSERT { _key: 'test', foo: 'bar' } INTO " + cn + " OPTIONS { overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      
      db._query("INSERT { _key: 'test', bar: 'baz' } INTO " + cn + " OPTIONS { overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      assertEqual("bar", c.document("test").foo);
      assertEqual("baz", c.document("test").bar);
      
      db._query("INSERT { _key: 'test', value: 7 } INTO " + cn + " OPTIONS { overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteUpdateNewDocumentAql : function () {
      let c = db._collection(cn);
      
      assertEqual(0, c.count());

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'update' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteIgnoreAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      
      db._query("INSERT { _key: 'test', foo: 'bar' } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      
      db._query("INSERT { _key: 'test', bar: 'baz' } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
      
      db._query("INSERT { _key: 'test', value: 7 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteOnlyIgnoreAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      
      db._query("INSERT { _key: 'test', foo: 'bar' } INTO " + cn + " OPTIONS { overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      
      db._query("INSERT { _key: 'test', bar: 'baz' } INTO " + cn + " OPTIONS { overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(1, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
      
      db._query("INSERT { _key: 'test', value: 7 } INTO " + cn + " OPTIONS { overwriteMode: 'replace' }");
      assertEqual(1, c.count());
      assertEqual(7, c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertUndefined(c.document("test").bar);
    },
    
    testInsertWithOverwriteIgnoreNewDocumentAql : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertWithOverwriteIgnoreNoKeyAql : function () {
      let c = db._collection(cn);

      assertEqual(0, c.count());

      db._query("INSERT { value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' }");
      assertEqual(1, c.count());
    },
    
    testInsertWithOverwriteIgnoreReturnOldAql : function () {
      let c = db._collection(cn);

      try {
        db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN OLD");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
      
      assertEqual(0, c.count());

      c.insert({ _key: "test", value: 2 });

      try {
        db._query("INSERT { _key: 'test', value: 3 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN OLD");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
      
      assertEqual(2, c.document("test").value);
      assertEqual(1, c.count());
    },
    
    testInsertWithOverwriteIgnoreReturnOldAqlNoSingleOperation : function () {
      let disableRule = { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } };
      let c = db._collection(cn);

      try {
        db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN OLD", null, disableRule);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
      
      assertEqual(0, c.count());

      c.insert({ _key: "test", value: 2 });

      try {
        db._query("INSERT { _key: 'test', value: 3 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN OLD", null, disableRule);
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code, err.errorNum);
      }
      
      assertEqual(2, c.document("test").value);
      assertEqual(1, c.count());
    },
    
    testInsertWithOverwriteIgnoreReturnNewAql : function () {
      let c = db._collection(cn);
        
      let result = db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN NEW").toArray();
      assertEqual("test", result[0]._key);
      assertEqual(2, result[0].value);

      result = db._query("INSERT { _key: 'test', value: 3 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN NEW").toArray();
      assertNull(result[0]);
      
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },

    testInsertWithOverwriteIgnoreReturnNewAqlNoSingleOperation : function () {
      let disableRule = { optimizer: { rules: ["-optimize-cluster-single-document-operations"] } };
      let c = db._collection(cn);
        
      let result = db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN NEW", null, disableRule).toArray();
      assertEqual("test", result[0]._key);
      assertEqual(2, result[0].value);

      result = db._query("INSERT { _key: 'test', value: 3 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'ignore' } RETURN NEW", null, disableRule).toArray();
      assertNull(result[0]);
      
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
    },
    
    testInsertInvalidOverwriteModeAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      db._query("INSERT { _key: 'test', value: 2 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: 'fleisch' }");
      assertEqual(1, c.count());
      assertEqual(2, c.document("test").value);
      
      db._query("INSERT { _key: 'test', value: 3 } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: '' }");
      assertEqual(1, c.count());
      assertEqual(3, c.document("test").value);
      assertUndefined(c.document("test").foo);
      
      db._query("INSERT { _key: 'test', rats: true } INTO " + cn + " OPTIONS { overwrite: true, overwriteMode: false }");
      assertEqual(1, c.count());
      assertUndefined(c.document("test").value);
      assertUndefined(c.document("test").foo);
      assertTrue(c.document("test").rats);
    },
    
    testInsertOnlyInvalidOverwriteModeAql : function () {
      let c = db._collection(cn);

      c.insert({ _key: "test", value: 1 });

      try {
        db._query("INSERT { _key: 'test', value: 2, foo: 'bar' } INTO " + cn + " OPTIONS { overwriteMode: 'hundmann' }");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
     
      try {
        db._query("INSERT { _key: 'test', value: 3 } INTO " + cn + " OPTIONS { overwriteMode: '' }");
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
      
      try {
        db._query("INSERT { _key: 'test', rats: true } INTO " + cn + " OPTIONS { overwriteMode: false }");
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }
    },

  };
}

jsunity.run(InsertOverwriteSuite);
jsunity.run(InsertOverwriteAqlSuite);

return jsunity.done();
