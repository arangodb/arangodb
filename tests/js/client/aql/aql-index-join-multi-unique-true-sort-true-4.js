/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const joinHelper = require("@arangodb/testutils/joinHelper.js");

const unique = true;
const sort = true;

const parameters = joinHelper.createParameters(unique, sort, 4, 6);

for (const configs of Object.values(parameters)) {
  jsunity.run(function () {
    return joinHelper.createIndexJoinTestMultiSuite(configs);
  });
}
return jsunity.done();
