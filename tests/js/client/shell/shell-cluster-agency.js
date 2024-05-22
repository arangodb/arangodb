/*jshint globalstrict:false, strict:false */
/*global fail, assertFalse, assertTrue, assertEqual, assertMatch, ArangoAgency, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
// /
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: agency
////////////////////////////////////////////////////////////////////////////////

function AgencySuite () {
  'use strict';
  
  return {

    setUp : function () {
      try {
        arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency", true);`);
      }
      catch (err) {
        // dir may not exist. this is not a problem
      }
      arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency");`);
    },
    
    tearDown : function () {
      try {
        arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency", true);`);
      }
      catch (err) {
      }
    },
    
    testVersion : function () {
      let result = arango.POST("/_admin/execute", `return global.ArangoAgency.version();`);
      assertMatch(/^\d\.\d/, result);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set
////////////////////////////////////////////////////////////////////////////////

    testSet : function () {
      // insert
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/foo", "test1"); `);
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/foo"); `);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test1");

      // overwrite
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/foo", "test2", 2); `);
      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/foo"); `);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test2");

      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/foo"); `));
      
      // re-insert
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/foo", "test3"); `);
      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/foo"); `);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test3");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cas
////////////////////////////////////////////////////////////////////////////////

    testCas : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/foo", "bar"); `));

      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.cas("UnitTestsAgency/foo", "bar", "baz"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.cas("UnitTestsAgency/foo", "baz", "bart"); `));
      assertFalse(arango.POST("/_admin/execute", `return global.ArangoAgency.cas("UnitTestsAgency/foo", "foo", "bar"); `));
      assertFalse(arango.POST("/_admin/execute", `return global.ArangoAgency.cas("UnitTestsAgency/boo", "foo", "bar"); `));
      
      try {
        arango.POST("/_admin/execute", `return global.ArangoAgency.cas("UnitTestsAgency/foo", "foo", "bar", 1, true); `);
        fail();
      }
      catch (err) {
      }
        
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.cas("UnitTestsAgency/foo", "bart", "baz", 0, 1, true); `));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test createDir
////////////////////////////////////////////////////////////////////////////////

    testCreateDir : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir2/bar"); `));

      try {
        // re-create an existing dir
        arango.POST("/_admin/execute", `return global.ArangoAgency.createDir("UnitTestsAgency/someDir"); `);
        fail();
      }
      catch (err1) {
      }
      
      try {
        // re-create an existing dir
        arango.POST("/_admin/execute", `return global.ArangoAgency.createDir("UnitTestsAgency/someDir2/bar"); `);
        fail();
      }
      catch (err2) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test createDir / remove
////////////////////////////////////////////////////////////////////////////////

    testCreateRemoveDir : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/someDir", true); `));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirNonRecursive : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      try {
        assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/someDir", false)); `));
        fail();
      }
      catch (err) {
        // not a file
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirRecursive1 : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/2/1"); `));
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/1/1/foo", "bar"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/2/1/foo", "baz"); `);

      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/1/1", true); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/1/2", true); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/1", true); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/2", true); `));
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency", true).arango.UnitTestsAgency; `);
      assertEqual({ }, values); // empty
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirRecursive2 : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/2/1"); `));
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/1/1/foo", "bar"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/2/1/foo", "baz"); `);

      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/1", true); `));

      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/1", true); `);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertFalse(values.arango.UnitTestsAgency.hasOwnProperty("1"));
        
      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/2", true); `);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("2"));
      assertEqual(values.arango.UnitTestsAgency["2"], {"1":{"foo":"baz"}}); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirRecursive3 : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2/1"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/1/2/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/2"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/2/1"); `));
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/1/1/foo", "bar"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/2/1/foo", "baz"); `);

      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency", true); `));

      var value = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency", true); `);
      assertTrue(value.hasOwnProperty("arango"));
      assertEqual(value.arango, {});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirNonExisting : function () {
      try {
        assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/someDir", true); `));
        fail();
      }
      catch (err) {
        // key not found
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove
////////////////////////////////////////////////////////////////////////////////

    testRemoveKeys : function () {
      var i;

      for (i = 0; i < 100; ++i) {
        assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/" + ${JSON.stringify(i)}, "value" + ${JSON.stringify(i)}); `));
      }
      
      for (i = 10; i < 90; ++i) {
        assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/" + ${JSON.stringify(i)}); `));
      }

      var values;
      for (i = 0; i < 100; ++i) {
        if (i >= 10 && i < 90) {
          values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/" + ${JSON.stringify(i)}); `);
          assertEqual(values, {"arango":{"UnitTestsAgency":{}}});
        }
        else {
          values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/" + ${JSON.stringify(i)}); `);
          assertTrue(values.hasOwnProperty("arango"));
          assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
          assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("" + i));
          assertEqual(values.arango.UnitTestsAgency[i], "value" + i);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGet : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo", "bar"); `);
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo"); `);
      assertEqual(values, {"arango":{"UnitTestsAgency":{"someDir":{"foo":"bar"}}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get recursive
////////////////////////////////////////////////////////////////////////////////

    testGetRecursive : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo/1/1/1", "bar1"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo/1/1/2", "bar2"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo/1/2/1", "bar3"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo/1/2/2", "bar4"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo/2/1/1", "bar5"); `);
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo/2/1/2", "bar6"); `);
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir").arango.UnitTestsAgency.someDir.foo; `);
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);

      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo; `);
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);
      
      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir", true).arango.UnitTestsAgency.someDir.foo; `);
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);

      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo", true).arango.UnitTestsAgency.someDir.foo; `);
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get multi
////////////////////////////////////////////////////////////////////////////////

    testGetMulti : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo", "bar"); `));
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/baz", "bart"); `));
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency", true).arango.UnitTestsAgency; `);
      assertEqual({ "someDir" : {"foo": "bar", "baz": "bart"}}, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUpdated : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo", "bar"); `);
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo; `);
      assertEqual(values, "bar");
      
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo", "baz"); `);
      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo; `);
      assertEqual(values, "baz");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetDeleted : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foo", "bar"); `);
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo; `);
      assertEqual(values, "bar");
      
      arango.POST("/_admin/execute", `return global.ArangoAgency.remove("UnitTestsAgency/someDir/foo"); `);

      values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir; `);
      assertEqual(values, {});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get
////////////////////////////////////////////////////////////////////////////////

    testGetNonExisting1 : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      var value = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foo"); `);
      assertEqual(value, {"arango":{"UnitTestsAgency":{"someDir":{}}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get
////////////////////////////////////////////////////////////////////////////////

    testGetNonExisting2 : function () {
      var value = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someOtherDir"); `);
      assertEqual(value, {"arango":{"UnitTestsAgency":{}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUrlEncodedValue : function () {
      assertTrue(arango.POST("/_admin/execute", `return global.ArangoAgency.createDirectory("UnitTestsAgency/someDir"); `));

      var value = "bar=BAT;foo=%47abc;degf=2343%20hihi aha\nabc";
      arango.POST("/_admin/execute", `return global.ArangoAgency.set("UnitTestsAgency/someDir/foobar", ${JSON.stringify(value)});`);
      
      var values = arango.POST("/_admin/execute", `return global.ArangoAgency.get("UnitTestsAgency/someDir/foobar"); `);
      assertEqual(values.arango.UnitTestsAgency.someDir.foobar, value);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (arango.POST("/_admin/execute", `return global.ArangoAgency.isEnabled();`)) {
  jsunity.run(AgencySuite);
}

return jsunity.done();
