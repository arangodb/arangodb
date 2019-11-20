/*jshint globalstrict:false, strict:false, maxlen : 4000 */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for dump/reload tests
///
/// @file
///
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
/// @author Jan Christoph Uhde
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const db = require("@arangodb").db;

function create(name, minReplicationFactor, replicationFactor, sharding){
  try {
    db._dropDatabase(name);
  } catch (err1) {
  }
  db._createDatabase(name, {
    "minReplicationFactor": minReplicationFactor,
    "replicationFactor": replicationFactor,
    "sharding": sharding,
  });
}

create("UnitTestsDumpProperties1", 2, 2, "");
create("UnitTestsDumpProperties2", 2, 2, "single");
create("UnitTestsDumpProperties3", 2, 3, "");

return {
  status: true
};
