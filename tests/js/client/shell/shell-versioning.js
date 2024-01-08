/*jshint globalstrict:false, strict:false, maxlen:1000*/
/*global assertEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const ERRORS = arangodb.errors;

const cn = "UnitTestsVersioning";

function VersioningSuite () {
  'use strict';

  return {
    setUp: function () {
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },

    testUpdateNoVersionAttributeProvided : function () {
      const values = [
        { _key: "test1", version: 2 },
        { _key: "test1", version: 3 },
        { _key: "test1", version: 1 },
        { _key: "test2", version: 1 },
        { _key: "test2", version: 9 },
        { _key: "test2", version: 42 },
        { _key: "test2", version: 0 },
      ];

      let state = {};
      values.forEach((v) => {
        if (state.hasOwnProperty(v._key)) {
          let res = db[cn].update(v._key, v);
          assertEqual(res._oldRev, state[v._key]._rev);
          delete res._oldRev;
          state[v._key] = Object.assign(state[v._key], Object.assign(res, v));
        } else {
          let res = db[cn].insert(v);
          state[v._key] = Object.assign(res, v);
        }

        let doc = db[cn].document(v._key);
        assertEqual(doc, state[v._key]);
      });
    },
    
    testUpdateWithVersionAttribute : function () {
      const values = [
        { _key: "test1", version: 2 },
        { _key: "test1", version: 3 },
        { _key: "test1", version: 1 },
        { _key: "test2", version: 1 },
        { _key: "test2", version: 9 },
        { _key: "test2", version: 42 },
        { _key: "test2", version: 0 },
        { _key: "test3" },
        { _key: "test3", version: 5 },
        { _key: "test3", version: 4 },
        { _key: "test3", value: 2 },
      ];

      let state = {};
      values.forEach((v) => {
        if (state.hasOwnProperty(v._key)) {
          let res = db[cn].update(v._key, v, {versionAttribute: "version"});
          assertEqual(res._oldRev, state[v._key]._rev);
          delete res._oldRev;
          if (!state[v._key].hasOwnProperty("version") || !v.hasOwnProperty("version") || state[v._key].version < v.version) {
            state[v._key] = Object.assign(state[v._key], Object.assign(res, v));
          }
        } else {
          let res = db[cn].insert(v);
          state[v._key] = Object.assign(res, v);
        }

        let doc = db[cn].document(v._key);
        assertEqual(doc, state[v._key]);
      });
    },
    
    testReplaceNoVersionAttributeProvided : function () {
      const values = [
        { _key: "test1", version: 2 },
        { _key: "test1", version: 3 },
        { _key: "test1", version: 1 },
        { _key: "test2", version: 1 },
        { _key: "test2", version: 9 },
        { _key: "test2", version: 42 },
        { _key: "test2", version: 0 },
      ];

      let state = {};
      values.forEach((v) => {
        if (state.hasOwnProperty(v._key)) {
          let res = db[cn].replace(v._key, v);
          assertEqual(res._oldRev, state[v._key]._rev);
          delete res._oldRev;
          state[v._key] = Object.assign(state[v._key], Object.assign(res, v));
        } else {
          let res = db[cn].insert(v);
          state[v._key] = Object.assign(res, v);
        }

        let doc = db[cn].document(v._key);
        assertEqual(doc, state[v._key]);
      });
    },
    
    testReplaceWithVersionAttribute : function () {
      const values = [
        { _key: "test1", version: 2 },
        { _key: "test1", version: 3 },
        { _key: "test1", version: 1 },
        { _key: "test2", version: 1 },
        { _key: "test2", version: 9 },
        { _key: "test2", version: 42 },
        { _key: "test2", version: 0 },
        { _key: "test3" },
        { _key: "test3", version: 5 },
        { _key: "test3", version: 4 },
        { _key: "test3", value: 2 },
      ];

      let state = {};
      values.forEach((v) => {
        if (state.hasOwnProperty(v._key)) {
          let res = db[cn].replace(v._key, v, {versionAttribute: "version"});
          assertEqual(res._oldRev, state[v._key]._rev);
          delete res._oldRev;
          if (!state[v._key].hasOwnProperty("version") || !v.hasOwnProperty("version") || state[v._key].version < v.version) {
            state[v._key] = Object.assign(res, v);
          }
        } else {
          let res = db[cn].insert(v);
          state[v._key] = Object.assign(res, v);
        }

        let doc = db[cn].document(v._key);
        assertEqual(doc, state[v._key]);
      });
    },
  
    // assertEqual(ERRORS.ERROR_CURSOR_NOT_FOUND.code, e.errorNum);
  };
}

jsunity.run(VersioningSuite);
return jsunity.done();
