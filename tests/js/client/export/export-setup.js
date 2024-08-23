/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */

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
/// @author Manuel Baesler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

'use strict';

{
  const db = require("@arangodb").db;

  let databases = [
    "UnitTestsExport",
    "اسم قاعدة بيانات يونيكود",
    "имя базы данных юникода !\"23 ää"
  ];

  databases.forEach((name) => {
    db._useDatabase("_system");

    try {
      db._dropDatabase(name);
    } catch (e) {}

    db._createDatabase(name);

    db._useDatabase(name);

    const col = db._create("UnitTestsExport");
    let docs = [];
    for (let i = 0; i < 100; ++i) {
      docs.push({ _key: "export" + i, value1: i, value2: "this is export", value3: "export" + i, value4: "%<>\"'" });
    }
    docs.push({ _key: "special", value1: "abc \"def\" ghi", value2: [1, 2], value3: { foo: "bar" }, value4: "abc\r\ncd" }); 
    col.insert(docs);
  });
}

return {
  status: true
};
