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
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                            agency
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: agency
////////////////////////////////////////////////////////////////////////////////

function AgencySuite () {
  var agency = ArangoAgency;
  var oldPrefix = agency.prefix(true);

  var cleanupLocks = function () {
    agency.set("UnitTestsAgency/Target/Lock", "UNLOCKED");
    agency.set("UnitTestsAgency/Plan/Lock", "UNLOCKED");
    agency.set("UnitTestsAgency/Current/Lock", "UNLOCKED");
  };
  
  return {

    setUp : function () {
      agency.setPrefix("UnitTestsAgency");
      assertEqual("UnitTestsAgency", agency.prefix(true));
      
      try {
        agency.remove("UnitTestsAgency", true);
        //agency.remove("UnitTestsAgency", true);
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

      agency.setPrefix(oldPrefix);
      assertEqual(oldPrefix, agency.prefix(true));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test version
////////////////////////////////////////////////////////////////////////////////

    testVersion : function () {
      assertMatch(/^etcd/, agency.version());
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test list
////////////////////////////////////////////////////////////////////////////////

    testList : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/foo"));
      assertTrue(agency.set("UnitTestsAgency/foo/1", "foo1"));
      assertTrue(agency.set("UnitTestsAgency/foo/2", "foo2"));
      assertTrue(agency.set("UnitTestsAgency/foo/3", "foo3"));
      assertTrue(agency.createDirectory("UnitTestsAgency/foo/bam"));
      assertTrue(agency.createDirectory("UnitTestsAgency/foo/bar"));
      assertTrue(agency.set("UnitTestsAgency/foo/bar/1", "bar1"));
      assertTrue(agency.set("UnitTestsAgency/foo/bar/2", "bar2"));
      assertTrue(agency.createDirectory("UnitTestsAgency/foo/bar/baz"));
      assertTrue(agency.set("UnitTestsAgency/foo/bar/baz/1", "baz1"));
      assertTrue(agency.createDirectory("UnitTestsAgency/foo/bar/baz/bam"));

      var values = agency.list("UnitTestsAgency/foo");
      assertEqual({ 
        "UnitTestsAgency/foo" : true,
        "UnitTestsAgency/foo/1" : false, 
        "UnitTestsAgency/foo/2" : false, 
        "UnitTestsAgency/foo/3" : false, 
        "UnitTestsAgency/foo/bam" : true, 
        "UnitTestsAgency/foo/bar" : true 
      }, values);

      values = agency.list("UnitTestsAgency/foo", true);
      assertEqual({ 
        "UnitTestsAgency/foo" : true, 
        "UnitTestsAgency/foo/1" : false, 
        "UnitTestsAgency/foo/2" : false, 
        "UnitTestsAgency/foo/3" : false, 
        "UnitTestsAgency/foo/bam" : true, 
        "UnitTestsAgency/foo/bar" : true, 
        "UnitTestsAgency/foo/bar/1" : false, 
        "UnitTestsAgency/foo/bar/2" : false, 
        "UnitTestsAgency/foo/bar/baz" : true, 
        "UnitTestsAgency/foo/bar/baz/1" : false, 
        "UnitTestsAgency/foo/bar/baz/bam" : true 
      }, values);
      
      // insert a new directory (above the others, sort order is important)
      assertTrue(agency.createDirectory("UnitTestsAgency/foo/abc"));
      values = agency.list("UnitTestsAgency/foo");

      assertEqual({ 
        "UnitTestsAgency/foo" : true,
        "UnitTestsAgency/foo/1" : false, 
        "UnitTestsAgency/foo/2" : false, 
        "UnitTestsAgency/foo/3" : false, 
        "UnitTestsAgency/foo/abc" : true, 
        "UnitTestsAgency/foo/bam" : true, 
        "UnitTestsAgency/foo/bar" : true 
      }, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lockRead
////////////////////////////////////////////////////////////////////////////////

    testLockRead : function () {
      cleanupLocks();

      assertTrue(agency.lockRead("UnitTestsAgency/Target"));
      assertTrue(agency.unlockRead("UnitTestsAgency/Target"));

      assertTrue(agency.lockRead("UnitTestsAgency/Plan"));
      assertTrue(agency.unlockRead("UnitTestsAgency/Plan"));
      
      assertTrue(agency.lockRead("UnitTestsAgency/Current"));
      assertTrue(agency.unlockRead("UnitTestsAgency/Current"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lockRead
////////////////////////////////////////////////////////////////////////////////

    testLockReadNotExisting : function () {
      cleanupLocks();

      assertTrue(agency.remove("UnitTestsAgency/Target/Lock"));
      assertTrue(agency.remove("UnitTestsAgency/Plan/Lock"));
      assertTrue(agency.remove("UnitTestsAgency/Current/Lock"));

      assertTrue(agency.lockRead("UnitTestsAgency/Target"));
      assertTrue(agency.unlockRead("UnitTestsAgency/Target"));

      assertTrue(agency.lockRead("UnitTestsAgency/Plan"));
      assertTrue(agency.unlockRead("UnitTestsAgency/Plan"));
      
      assertTrue(agency.lockRead("UnitTestsAgency/Current"));
      assertTrue(agency.unlockRead("UnitTestsAgency/Current"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lockRead
////////////////////////////////////////////////////////////////////////////////

    testLockReadDouble : function () {
      cleanupLocks();

      assertTrue(agency.lockRead("UnitTestsAgency/Target", 5));

      try {
        // this will fail because of a duplicate lock
        assertTrue(agency.lockRead("UnitTestsAgency/Target", 1, 1));
        fail();
      }
      catch (err) {
      }
      
      assertTrue(agency.unlockRead("UnitTestsAgency/Target"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lockRead
////////////////////////////////////////////////////////////////////////////////

    testLockReadWrongType : function () {
      cleanupLocks();

      assertTrue(agency.lockRead("UnitTestsAgency/Target", 5));

      try {
        // unlock of a wrong type
        agency.unlockWrite("UnitTestsAgency/Target", 1);
        fail();
      }
      catch (err) {
      }
      
      assertTrue(agency.unlockRead("UnitTestsAgency/Target"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lockWrite
////////////////////////////////////////////////////////////////////////////////

    testLockWrite : function () {
      cleanupLocks();

      assertTrue(agency.lockWrite("UnitTestsAgency/Target"));
      assertTrue(agency.unlockWrite("UnitTestsAgency/Target"));

      assertTrue(agency.lockWrite("UnitTestsAgency/Plan"));
      assertTrue(agency.unlockWrite("UnitTestsAgency/Plan"));
      
      assertTrue(agency.lockWrite("UnitTestsAgency/Current"));
      assertTrue(agency.unlockWrite("UnitTestsAgency/Current"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test lockWrite
////////////////////////////////////////////////////////////////////////////////

    testLockWriteDouble : function () {
      cleanupLocks();

      assertTrue(agency.lockWrite("UnitTestsAgency/Target", 5));

      try {
        // this will fail because of a duplicate lock
        assertTrue(agency.lockWrite("UnitTestsAgency/Target", 1, 1));
        fail();
      }
      catch (err) {
      }

      assertTrue(agency.unlockWrite("UnitTestsAgency/Target"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test re-lock 
////////////////////////////////////////////////////////////////////////////////

    testLockRelock : function () {
      cleanupLocks();

      assertTrue(agency.lockRead("UnitTestsAgency/Target", 5));

      var start = require("internal").time();
      assertTrue(agency.lockWrite("UnitTestsAgency/Target", 5, 10));
      var end = require("internal").time();

      assertTrue(Math.round(end - start) >= 3);
      assertTrue(agency.unlockWrite("UnitTestsAgency/Target"));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set
////////////////////////////////////////////////////////////////////////////////

    testSet : function () {
      // insert
      agency.set("UnitTestsAgency/foo", "test1");
      var values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/foo"));
      assertEqual(values["UnitTestsAgency/foo"], "test1");

      // overwrite
      agency.set("UnitTestsAgency/foo", "test2", 2);
      var values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/foo"));
      assertEqual(values["UnitTestsAgency/foo"], "test2");

      assertTrue(agency.remove("UnitTestsAgency/foo"));
      
      // re-insert
      agency.set("UnitTestsAgency/foo", "test3");
      var values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/foo"));
      assertEqual(values["UnitTestsAgency/foo"], "test3");

      // update with ttl
      agency.set("UnitTestsAgency/foo", "test4", 2);
      var values = agency.get("UnitTestsAgency/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/foo"));
      assertEqual(values["UnitTestsAgency/foo"], "test4");

      require("internal").wait(3);
      
      try {
        values = agency.get("UnitTestsAgency/foo");
        fail();
      }
      catch (e) {
        assertEqual(404, e.code); 
        assertEqual(100, e.errorNum);  // not found
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test watch
////////////////////////////////////////////////////////////////////////////////

    testWatchTimeout : function () {
      assertTrue(agency.set("UnitTestsAgency/foo", "bar"));

      var wait = 3;
      var start = require("internal").time();
      assertFalse(agency.watch("UnitTestsAgency/foo", 0, wait));
      var end = require("internal").time();
      assertEqual(wait, Math.round(end - start));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test watch
////////////////////////////////////////////////////////////////////////////////

    testWatchChange : function () {
      assertTrue(agency.set("UnitTestsAgency/foo", "bar"));

      var wait = 1;
      var start = require("internal").time();

      assertFalse(agency.watch("UnitTestsAgency/foo", 0, wait));
      var end = require("internal").time();
      assertEqual(wait, Math.round(end - start));

      assertTrue(agency.set("UnitTestsAgency/foo", "baz"));
      assertTrue(agency.set("UnitTestsAgency/foo", "bart"));

      var idx = agency.get("UnitTestsAgency/foo", false, true)["UnitTestsAgency/foo"].index;
      start = require("internal").time();
      var result = agency.watch("UnitTestsAgency/foo", idx, wait);
      end = require("internal").time();

      assertEqual(0, Math.round(end - start));

      idx = agency.get("UnitTestsAgency/foo", false, true)["UnitTestsAgency/foo"].index;
      start = require("internal").time();
      result = agency.watch("UnitTestsAgency/foo", idx + 100000, wait);
      end = require("internal").time();

      assertEqual(wait, Math.round(end - start));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test recursive watch
////////////////////////////////////////////////////////////////////////////////

    testWatchRecursive : function () {
      assertTrue(agency.set("UnitTestsAgency/foo/1", "bar"));
      assertTrue(agency.set("UnitTestsAgency/foo/2", "baz"));

      var wait = 1;
      var start = require("internal").time();
      assertFalse(agency.watch("UnitTestsAgency/foo", 0, wait));
      var end = require("internal").time();
      assertEqual(wait, Math.round(end - start));

      assertTrue(agency.set("UnitTestsAgency/foo/3", "bart"));
      var idx = agency.get("UnitTestsAgency/foo", false, true)["UnitTestsAgency/foo/3"].index;
      start = require("internal").time();
      var result = agency.watch("UnitTestsAgency/foo", idx - 10, wait);
      end = require("internal").time();

      assertEqual(0, Math.round(end - start));
      
      idx = agency.get("UnitTestsAgency/foo", false, true)["UnitTestsAgency/foo/3"].index;
      start = require("internal").time();
      result = agency.watch("UnitTestsAgency/foo", idx - 5, wait, true);
      end = require("internal").time();
      assertEqual(0, Math.round(end - start));
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

      try {
        // re-create an existing dir
        agency.createDir("UnitTestsAgency/someDir");
        fail();
      }
      catch (err) {
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
      
      var values = agency.get("UnitTestsAgency", true);
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

      try {
        agency.get("UnitTestsAgency/1", true);
        fail();
      }
      catch (err) {
        // key not found
      }
        
      var values = agency.get("UnitTestsAgency/2", true);
      assertEqual({ "UnitTestsAgency/2/1/foo" : "baz" }, values); 
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

      try {
        agency.get("UnitTestsAgency", true);
        fail();
      }
      catch (err) {
        // key not found
      }
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
          try {
            values = agency.get("UnitTestsAgency/" + i);
            fail();
          }
          catch (err) {
          }
        }
        else {
          values = agency.get("UnitTestsAgency/" + i);
          assertTrue(values.hasOwnProperty("UnitTestsAgency/" + i));
          assertEqual(values["UnitTestsAgency/" + i], "value" + i);
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
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo"));
      assertEqual(values["UnitTestsAgency/someDir/foo"], "bar");
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
      
      var values = agency.get("UnitTestsAgency/someDir");
      assertEqual({ }, values);
      values = agency.get("UnitTestsAgency/someDir/foo");
      assertEqual({ }, values);
      
      values = agency.get("UnitTestsAgency/someDir", true);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo/1/1/1"));
      assertEqual("bar1", values["UnitTestsAgency/someDir/foo/1/1/1"]);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo/1/1/2"));
      assertEqual("bar2", values["UnitTestsAgency/someDir/foo/1/1/2"]);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo/1/2/1"));
      assertEqual("bar3", values["UnitTestsAgency/someDir/foo/1/2/1"]);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo/1/2/2"));
      assertEqual("bar4", values["UnitTestsAgency/someDir/foo/1/2/2"]);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo/2/1/1"));
      assertEqual("bar5", values["UnitTestsAgency/someDir/foo/2/1/1"]);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo/2/1/2"));
      assertEqual("bar6", values["UnitTestsAgency/someDir/foo/2/1/2"]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get w/ indexes
////////////////////////////////////////////////////////////////////////////////

    testGetIndexes : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo", "bar");
      agency.set("UnitTestsAgency/someDir/bar", "baz");
      
      var values = agency.get("UnitTestsAgency/someDir", true, true);
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo"));
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/bar"));
      assertEqual(values["UnitTestsAgency/someDir/foo"].value, "bar");
      assertEqual(values["UnitTestsAgency/someDir/bar"].value, "baz");
      assertTrue(values["UnitTestsAgency/someDir/foo"].hasOwnProperty("index"));
      assertTrue(values["UnitTestsAgency/someDir/bar"].hasOwnProperty("index"));

      assertNotEqual(values["UnitTestsAgency/someDir/foo"].index, values["UnitTestsAgency/someDir/bar"].index);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get directory
////////////////////////////////////////////////////////////////////////////////

    testGetDirectory : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));
      assertTrue(agency.set("UnitTestsAgency/someDir/foo", "bar"));

      var values = agency.get("UnitTestsAgency", false);
      assertEqual({ }, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get multi
////////////////////////////////////////////////////////////////////////////////

    testGetMulti : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));
      assertTrue(agency.set("UnitTestsAgency/someDir/foo", "bar"));
      assertTrue(agency.set("UnitTestsAgency/someDir/baz", "bart"));
      
      var values = agency.get("UnitTestsAgency", true);
      assertEqual({ "UnitTestsAgency/someDir/baz" : "bart", "UnitTestsAgency/someDir/foo" : "bar" }, values);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUpdated : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo", "bar");
      
      var values = agency.get("UnitTestsAgency/someDir/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo"));
      assertEqual(values["UnitTestsAgency/someDir/foo"], "bar");
      
      agency.set("UnitTestsAgency/someDir/foo", "baz");
      values = agency.get("UnitTestsAgency/someDir/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo"));
      assertEqual(values["UnitTestsAgency/someDir/foo"], "baz");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetDeleted : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      agency.set("UnitTestsAgency/someDir/foo", "bar");
      
      var values = agency.get("UnitTestsAgency/someDir/foo");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foo"));
      assertEqual(values["UnitTestsAgency/someDir/foo"], "bar");
      
      agency.remove("UnitTestsAgency/someDir/foo");

      try {
        values = agency.get("UnitTestsAgency/someDir/foo");
        fail();
      }
      catch (err) {
        // key not found
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get
////////////////////////////////////////////////////////////////////////////////

    testGetNonExisting1 : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      try {
        agency.get("UnitTestsAgency/someDir/foo");
        fail();
      }
      catch (err) {
        // key not found
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test get
////////////////////////////////////////////////////////////////////////////////

    testGetNonExisting2 : function () {
      try {
        agency.get("UnitTestsAgency/someOtherDir");
        fail();
      }
      catch (err) {
        // key not found
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUrlEncodedValue : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      var value = "bar=BAT;foo=%47abc;degf=2343%20hihi aha\nabc";
      agency.set("UnitTestsAgency/someDir/foobar", value);
      
      var values = agency.get("UnitTestsAgency/someDir/foobar");
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/foobar"));
      assertEqual(values["UnitTestsAgency/someDir/foobar"], value);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test set / get
////////////////////////////////////////////////////////////////////////////////

    testGetUrlEncodedKey : function () {
      assertTrue(agency.createDirectory("UnitTestsAgency/someDir"));

      var key = "foo bar baz / hihi";
      agency.set("UnitTestsAgency/someDir/" + encodeURIComponent(key), "something");
     
      var values = agency.get("UnitTestsAgency/someDir/" + encodeURIComponent(key));
      assertTrue(values.hasOwnProperty("UnitTestsAgency/someDir/" + key));
      assertEqual(values["UnitTestsAgency/someDir/" + key], "something");
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (ArangoAgency.isEnabled()) {
  jsunity.run(AgencySuite);
}

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

