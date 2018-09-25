/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, assertTypeOf */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the binary document interface
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

let jsunity = require("jsunity");
let arangodb = require("@arangodb");
let ERRORS = arangodb.errors;
let db = arangodb.db;
let internal = require("internal");
let fs = require("fs");
let path = require("path");
let wait = internal.wait;
const pathForTesting = internal.pathForTesting;

function CollectionBinaryDocumentSuite () {
  'use strict';
  let cn = "UnitTestsCollectionBinary";
  let collection = null;

  return {
    setUp : function () {
      db._drop(cn);
      collection = db._create(cn);
    },

    tearDown : function () {
      if (collection) {
        collection.unload();
        collection.drop();
        collection = null;
      }
      wait(0.0);
    },

    testBinaryDocument : function () {
      let filename1 = path.resolve('js/apps/system/_admin/aardvark/APP/frontend/img/arangodb_logo_letter.png'.split('/').join(path.sep));
      if (!fs.exists(filename1)) {
        // look for version-specific file
        filename1 = path.resolve('js/' + db._version().replace(/-.*$/, '') + '/apps/system/_admin/aardvark/APP/frontend/img/arangodb_logo_letter.png'.split('/').join(path.sep));
      }
      const content = fs.readFileSync(filename1);

      const d1 = collection._binaryInsert({_key: "test", meta: "hallo"}, filename1);

      assertTypeOf("string", d1._id);
      assertTypeOf("string", d1._key);
      assertTypeOf("string", d1._rev);
      assertEqual("UnitTestsCollectionBinary/test", d1._id);
      assertEqual("test", d1._key);

      const d3 = collection.document("test");

      assertTypeOf("string", d3._id);
      assertTypeOf("string", d3._key);
      assertTypeOf("string", d3._rev);
      assertTypeOf("string", d3._attachment);
      assertTypeOf("string", d3.meta);
      assertEqual("hallo", d3.meta);
      assertEqual(content.toString('base64'), d3._attachment);

      const filename2 = fs.getTempFile('binary', false);
      const d2 = collection._binaryDocument("test", filename2);

      assertTypeOf("string", d2._id);
      assertTypeOf("string", d2._key);
      assertTypeOf("string", d2._rev);
      assertTypeOf("string", d2.meta);
      assertEqual("hallo", d2.meta);

      const s = fs.readFileSync(filename2);
      assertEqual(content, s);
    }
  };
}

jsunity.run(CollectionBinaryDocumentSuite);

return jsunity.done();
