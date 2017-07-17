/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */
/*global arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for dump/reload tests
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

(function () {
  'use strict';
  var db = require("@arangodb").db;
  var i, c;

  try {
    db._dropDatabase("UnitTestsDumpSrc");
  }
  catch (err1) {
  }
  db._createDatabase("UnitTestsDumpSrc");

  // create user in _system database
  var users = require("@arangodb/users");
  users.save("foobaruser", "foobarpasswd", true);
  users.grantDatabase("foobaruser", "_system");
  users.grantCollection("foobaruser", "_system", "*");
  users.grantDatabase("foobaruser", "UnitTestsDumpSrc");
  users.grantCollection("foobaruser", "UnitTestsDumpSrc", "*");
  db._useDatabase("UnitTestsDumpSrc");

  var endpoint = arango.getEndpoint();

  arango.reconnect(endpoint, "UnitTestsDumpSrc", "foobaruser", "foobarpasswd");


  // this remains empty
  db._create("UnitTestsDumpEmpty", { waitForSync: true, indexBuckets: 256 });

  // create lots of documents
  c = db._create("UnitTestsDumpMany");
  for (i = 0; i < 100000; ++i) {
    c.save({ _key: "test" + i, value1: i, value2: "this is a test", value3: "test" + i });
  }

  c = db._createEdgeCollection("UnitTestsDumpEdges");
  for (i = 0; i < 10; ++i) {
    c.save("UnitTestsDumpMany/test" + i,
           "UnitTestsDumpMany/test" + (i + 1),
           { _key: "test" + i, what: i + "->" + (i + 1) });
  }

  // we update & modify the order
  c = db._create("UnitTestsDumpOrder", { indexBuckets: 16 });
  c.save({ _key: "one", value: 1 });
  c.save({ _key: "two", value: 2 });
  c.save({ _key: "three", value: 3 });
  c.save({ _key: "four", value: 4 });

  c.update("three", { value: 3, value2: 123 });
  c.update("two", { value: 2, value2: 456 });
  c.update("one", { value: 1, value2: 789 });
  c.remove("four");
  
})();

return {
  status: true
};

