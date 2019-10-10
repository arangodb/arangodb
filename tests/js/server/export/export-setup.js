/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */

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
/// @author Manuel Baesler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

{
  const db = require("@arangodb").db;

  try {
    db._dropDatabase("UnitTestsExport");
  } catch (e) {}

  db._createDatabase("UnitTestsExport");

  db._useDatabase("UnitTestsExport");

  const col = db._create("UnitTestsExport");
  for (let i = 0; i < 100; ++i) {
    col.save({ _key: "export" + i, value1: i, value2: "this is export", value3: "export" + i, value4: "%<>\"'" });
  }
  col.save({ _key: "special", value1: "abc \"def\" ghi", value2: [1, 2], value3: { foo: "bar" }, value4: "abc\r\ncd" }); 
}

return {
  status: true
};
