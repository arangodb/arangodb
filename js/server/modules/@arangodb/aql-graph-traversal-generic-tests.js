/*jshint globalstrict:true, strict:true, esnext: true */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

/*
  TODO:
    - include js/server/modules/@arangodb/aql-graph-traversal-generic-graphs.js
    - move uniquenessTraversalSuite (mainly testDfsPathUniqueness) here, from
      enterprise/tests/js/server/aql/aql-smart-graph-traverser-enterprise-cluster.js
    - combine test(s) with their corresponding ProtoGraph and export them as pairs
      (maybe a "test" can just be a query plus the expected output or so?)
 */
