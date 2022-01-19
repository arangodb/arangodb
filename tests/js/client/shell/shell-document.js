/*jshint globalstrict:false, strict:false, maxlen: 5000 */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the document interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const cn = "UnitTestsCollection";
const jsunity = require("jsunity");
const {generateTestSuite} = require("@arangodb/testutils/collect-crud-api-generic-tests");
const {CollectionWrapper} = require("@arangodb/testutils/collection-wrapper-util");

const wrap = new CollectionWrapper(cn);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////
function CollectionDocumentKeysSuite() {
  return generateTestSuite(wrap);
}

jsunity.run(CollectionDocumentKeysSuite);

return jsunity.done();

