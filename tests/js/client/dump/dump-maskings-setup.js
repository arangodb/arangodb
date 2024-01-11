/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */
/*global arango */

// /////////////////////////////////////////////////////////////////////////////
// @brief tests for dump/reload
//
// @file
//
// DISCLAIMER
//
// Copyright 2019 ArangoDB GmbH, Cologne, Germany
// Copyright 2010-2012 triagens GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is ArangoDB GmbH, Cologne, Germany
//
// @author Frank Celler
// /////////////////////////////////////////////////////////////////////////////

(function () {
  'use strict';
  var db = require("@arangodb").db;
  var i, c;

  try {
    db._dropDatabase("UnitTestsDumpSrc");
  } catch (err1) {
  }

  db._createDatabase("UnitTestsDumpSrc");
  db._useDatabase("UnitTestsDumpSrc");

  db._create("maskings1");

  db.maskings1.save({
    _key: "1",

    name: "Hallo World! This is a t0st a top-level",

    blub: {
      name: "Hallo World! This is a t0st in a sub-object",
    },

    email: [
      "testing arrays",
      "this is another one",
      { something: "something else" },
      { email: "within a subject" },
      { name: [ "emails within a subject", "as list" ] }
    ],

    sub: {
       name: "this is a name leaf attribute",
       email: [ "in this case as list", "with more than one entry" ]
    }
  });

  db._create("maskings2");

  db.maskings2.save({
    _key: "2",

    random: "a",
    zip: "12345",
    date: "2018-01-01",
    integer: 100,
    decimal: 100.12,
    ccard: "1234 1234 1234 1234",
    phone: "abcd 1234",
    email: "me@you.here"
  });
})();

return {
  status: true
};

