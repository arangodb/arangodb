/*jshint globalstrict:false, strict:false */
/*global fail, assertFalse, assertTrue, assertEqual, ArangoAgency */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the agency communication layer
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: agency
////////////////////////////////////////////////////////////////////////////////

function AgencySuite () {
  'use strict';
  var agency = ArangoAgency;

  var cleanupLocks = function () {
    agency.set("UnitTestsAgency/Target/Lock", "UNLOCKED");
    agency.set("UnitTestsAgency/Plan/Lock", "UNLOCKED");
    agency.set("UnitTestsAgency/Current/Lock", "UNLOCKED");
  };
  
  return {

    setUp : function () {
      try {
        agency.remove("UnitTestsAgency", true);
      }
      catch (err) {
        // dir may not exist. this is not a problem
      }

      agency.createDirectory("UnitTestsAgency");
    },
    
    tearDown : function () {
      try {
        agency.remove("UnitTestsAgency", true);
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set
////////////////////////////////////////////////////////////////////////////////

    testSet : function () {
      // insert
      agency.set("UnitTestsAgency/foo", "test1");
      var values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test1");

      // overwrite
      agency.set("UnitTestsAgency/foo", "test2", 2);
      values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test2");

      assertTrue(agency.remove("UnitTestsAgency/foo"));
      
      // re-insert
      agency.set("UnitTestsAgency/foo", "test3");
      values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test3");

      // update with ttl
      agency.set("UnitTestsAgency/foo", "test4", 2);
      values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
      assertEqual(values.arango.UnitTestsAgency.foo, "test4");

      require("internal").wait(3);
      
      values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertFalse(values.arango.UnitTestsAgency.hasOwnProperty("foo"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test cas
////////////////////////////////////////////////////////////////////////////////

    testCas : function () {
      assertTrue(agency.set("UnitTestsAgency/foo", "bar"));

      assertTrue(agency.cas("UnitTestsAgency/foo", "bar", "baz"));
      assertTrue(agency.cas("UnitTestsAgency/foo", "baz", "bart"));
      assertFalse(agency.cas("UnitTestsAgency/foo", "foo", "bar"));
      assertFalse(agency.cas("UnitTestsAgency/boo", "foo", "bar"));
      
      try {
        agency.cas("UnitTestsAgency/foo", "foo", "bar", 1, true);
        fail();
      }
      catch (err) {
      }
        
      assertTrue(agency.cas("UnitTestsAgency/foo", "bart", "baz", 0, 1, true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test createDir
////////////////////////////////////////////////////////////////////////////////

    testCreateDir : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir2/bar"));

      try {
        // re-create an existing dir
        agency.createDir("UnitTestsAgency/someDir");
        fail();
      }
      catch (err1) {
      }
      
      try {
        // re-create an existing dir
        agency.createDir("UnitTestsAgency/someDir2/bar");
        fail();
      }
      catch (err2) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test createDir / remove
////////////////////////////////////////////////////////////////////////////////

    testCreateRemoveDir : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      assertTrue(agency.remove("UnitTestsAgency/someDir", true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirNonRecursive : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      try {
        assertTrue(agency.remove("UnitTestsAgency/someDir", false));
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
      assertTrue(agency.createDirectory("UnitTestsAgency/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/2/1"));
      agency.set("UnitTestsAgency/1/1/foo", "bar");
      agency.set("UnitTestsAgency/2/1/foo", "baz");

      assertTrue(agency.remove("UnitTestsAgency/1/1", true));
      assertTrue(agency.remove("UnitTestsAgency/1/2", true));
      assertTrue(agency.remove("UnitTestsAgency/1", true));
      assertTrue(agency.remove("UnitTestsAgency/2", true));
      
      var values = agency.get("UnitTestsAgency", true).arango.UnitTestsAgency;
      assertEqual({ }, values); // empty
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirRecursive2 : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/2/1"));
      agency.set("UnitTestsAgency/1/1/foo", "bar");
      agency.set("UnitTestsAgency/2/1/foo", "baz");

      assertTrue(agency.remove("UnitTestsAgency/1", true));

      var values = agency.get("UnitTestsAgency/1", true);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertFalse(values.arango.UnitTestsAgency.hasOwnProperty("1"));
        
      values = agency.get("UnitTestsAgency/2", true);
      assertTrue(values.hasOwnProperty("arango"));
      assertTrue(values.arango.hasOwnProperty("UnitTestsAgency"));
      assertTrue(values.arango.UnitTestsAgency.hasOwnProperty("2"));
      assertEqual(values.arango.UnitTestsAgency["2"], {"1":{"foo":"baz"}}); 
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirRecursive3 : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2/1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/1/2/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/2/1"));
      agency.set("UnitTestsAgency/1/1/foo", "bar");
      agency.set("UnitTestsAgency/2/1/foo", "baz");

      assertTrue(agency.remove("UnitTestsAgency", true));

      var value = agency.get("UnitTestsAgency", true);
      assertTrue(value.hasOwnProperty("arango"));
      assertEqual(value.arango, {});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test removeDir
////////////////////////////////////////////////////////////////////////////////

    testRemoveDirNonExisting : function () {
      try {
        assertTrue(agency.remove("UnitTestsAgency/someDir", true));
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
        assertTrue(agency.set("UnitTestsAgency/" + i, "value" + i));
      }
      
      for (i = 10; i < 90; ++i) {
        assertTrue(agency.remove("UnitTestsAgency/" + i));
      }

      var values;
      for (i = 0; i < 100; ++i) {
        if (i >= 10 && i < 90) {
          values = agency.get("UnitTestsAgency/" + i);
          assertEqual(values, {"arango":{"UnitTestsAgency":{}}});
        }
        else {
          values = agency.get("UnitTestsAgency/" + i);
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
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo", "bar");
      
      var values = agency.get("UnitTestsAgency/someDir/foo");
      assertEqual(values, {"arango":{"UnitTestsAgency":{"someDir":{"foo":"bar"}}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get recursive
////////////////////////////////////////////////////////////////////////////////

    testGetRecursive : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo/1/1/1", "bar1");
      agency.set("UnitTestsAgency/someDir/foo/1/1/2", "bar2");
      agency.set("UnitTestsAgency/someDir/foo/1/2/1", "bar3");
      agency.set("UnitTestsAgency/someDir/foo/1/2/2", "bar4");
      agency.set("UnitTestsAgency/someDir/foo/2/1/1", "bar5");
      agency.set("UnitTestsAgency/someDir/foo/2/1/2", "bar6");
      
      var values = agency.get("UnitTestsAgency/someDir").arango.UnitTestsAgency.someDir.foo;
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);

      values = agency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo;
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);
      
      values = agency.get("UnitTestsAgency/someDir", true).arango.UnitTestsAgency.someDir.foo;
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);

      values = agency.get("UnitTestsAgency/someDir/foo", true).arango.UnitTestsAgency.someDir.foo;
      assertEqual({"1":{"1":{"1":"bar1","2":"bar2"},
                        "2":{"1":"bar3","2":"bar4"}},
                   "2":{"1":{"1":"bar5","2":"bar6"}}}, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get multi
////////////////////////////////////////////////////////////////////////////////

    testGetMulti : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));
      assertTrue(agency.set("UnitTestsAgency/someDir/foo", "bar"));
      assertTrue(agency.set("UnitTestsAgency/someDir/baz", "bart"));
      
      var values = agency.get("UnitTestsAgency", true).arango.UnitTestsAgency;
      assertEqual({ "someDir" : {"foo": "bar", "baz": "bart"}}, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUpdated : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo", "bar");
      
      var values = agency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo;
      assertEqual(values, "bar");
      
      agency.set("UnitTestsAgency/someDir/foo", "baz");
      values = agency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo;
      assertEqual(values, "baz");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetDeleted : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo", "bar");
      
      var values = agency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir.foo;
      assertEqual(values, "bar");
      
      agency.remove("UnitTestsAgency/someDir/foo");

      values = agency.get("UnitTestsAgency/someDir/foo").arango.UnitTestsAgency.someDir;
      assertEqual(values, {});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get
////////////////////////////////////////////////////////////////////////////////

    testGetNonExisting1 : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      var value = agency.get("UnitTestsAgency/someDir/foo");
      assertEqual(value, {"arango":{"UnitTestsAgency":{"someDir":{}}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get
////////////////////////////////////////////////////////////////////////////////

    testGetNonExisting2 : function () {
      var value = agency.get("UnitTestsAgency/someOtherDir");
      assertEqual(value, {"arango":{"UnitTestsAgency":{}}});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUrlEncodedValue : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      var value = "bar=BAT;foo=%47abc;degf=2343%20hihi aha\nabc";
      agency.set("UnitTestsAgency/someDir/foobar", value);
      
      var values = agency.get("UnitTestsAgency/someDir/foobar").arango.UnitTestsAgency.someDir.foobar;
      assertEqual(values, value);
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (ArangoAgency.isEnabled()) {
  jsunity.run(AgencySuite);
}

return jsunity.done();

