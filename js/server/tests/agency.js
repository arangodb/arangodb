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
  var agency;

  return {

    setUp : function () {
      agency = new ArangoAgency();
      
      try {
        agency.remove("UnitTestsAgency", true);
      }
      catch (err) {
        // dir may not exist. this is not a problem
      }

      agency.createDirectory("UnitTestsAgency");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test version
////////////////////////////////////////////////////////////////////////////////

    testVersion : function () {
      assertMatch(/^etcd/, agency.version());
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
      start = require("internal").time();
      var result = agency.watch("UnitTestsAgency/foo", 1, wait);
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
      
      try {
        agency.cas("UnitTestsAgency/foo", "foo", "bar", true);
        fail();
      }
      catch (err) {
      }
        
      assertTrue(agency.cas("UnitTestsAgency/foo", "bart", "baz", true));
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

jsunity.run(AgencySuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

