// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
// //////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const {assertEqual} = jsunity.jsUnity.assertions;

const db = require("@arangodb").db;

// AQL-equal objects (differing only in explicit nulls) must not merge across servers
function AqlItemBlockSerializationSuite () {
  const src = 'UnitTestsSerializationSrc';
  const dst = 'UnitTestsSerializationDst';
  const n = 1000;

  const distinctOverrides = (docs) => {
    return new Set(docs.map((d) => Object.keys(d).filter((k) => k.startsWith('override_')).join(','))).size;
  };

  return {
    setUpAll: function () {
      db._drop(src);
      db._drop(dst);
      const c = db._create(src, {numberOfShards: 1});
      db._create(dst, {numberOfShards: 3});
      let docs = [];
      for (let i = 0; i < n; ++i) {
        let settings = {theme: 'dark'};
        settings['override_' + i] = null;
        docs.push({settings});
      }
      c.insert(docs);
    },

    tearDownAll: function () {
      db._drop(src);
      db._drop(dst);
    },

    testReturnedValuesKeepTheirContent: function () {
      const res = db._query(`FOR d IN ${src} RETURN d.settings`, {}, {batchSize: n}).toArray();
      assertEqual(n, res.length);
      assertEqual(n, distinctOverrides(res));
    },

    testInsertedDocumentsKeepTheirContent: function () {
      db._query(`FOR d IN ${src} INSERT d.settings INTO ${dst}`);
      const stored = db._query(`FOR d IN ${dst} RETURN d`).toArray();
      assertEqual(n, stored.length);
      assertEqual(n, distinctOverrides(stored));
    },
  };
}

jsunity.run(AqlItemBlockSerializationSuite);
return jsunity.done();
