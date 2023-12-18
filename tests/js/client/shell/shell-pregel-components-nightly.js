/*jshint globalstrict:false, strict:false */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / @brief Pregel Tests
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2022 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
let pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");

const wccTestSuite = pregelTestHelpers.makeHeavyWCCTestSuite(false, "", 4);

jsunity.run(wccTestSuite);
return jsunity.done();
