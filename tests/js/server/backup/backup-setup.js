/*jshint globalstrict:false, strict:false, maxlen:4000, unused:false */
/*global arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for backup tests
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const db = require("@arangodb").db;
const colname = 'UnitTestBkpCollection';
const user_rw_rw = 'UnitTestUserRWRW';
const user_rw_ro = 'UnitTestUserRWRO';
const user_rw_none = 'UnitTestUserRWNO';
const user_ro_rw = 'UnitTestUserRORW';
const user_none = 'UnitTestUserNONE';

const clearCollections = () => {
  db._drop(colname);
};

const generateUsers = () => {
  const users = require("@arangodb/users");
  users.save(user_rw_rw, user_rw_rw, true);
  users.grantDatabase(user_rw_rw, '_system', 'rw');
  users.grantCollection(user_rw_rw, '_system', colname, 'rw');
  users.save(user_rw_ro, user_rw_ro, true);
  users.grantDatabase(user_rw_ro, '_system', 'rw');
  users.grantCollection(user_rw_ro, '_system', colname, 'ro');
  users.save(user_rw_none, user_rw_none, true);
  users.grantDatabase(user_rw_none, '_system', 'rw');
  users.grantCollection(user_rw_none, '_system', colname, 'none');
  users.save(user_ro_rw, user_ro_rw, true);
  users.grantDatabase(user_ro_rw, '_system', 'ro');
  users.grantCollection(user_ro_rw, '_system', colname, 'rw');
  users.save(user_none, user_none, true);
  users.grantDatabase(user_none, '_system', 'none');
};

const createCollections = () => {
  clearCollections();
  db._create(colname, {numberOfShards: 4, shardKeys: ['_key'], replicationFactor: 2});
};

const fillCollections = () => {
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({foo: (i % 16), bar: i});
  }
  db._collection(colname).save(docs);
};

const createGraph = () => {
  const graphs = require("@arangodb/general-graph");
  graphs._create(
    'UnitTestGraph',
    [graphs._relation('UnitTestEdges', 'UnitTestVertices', 'UnitTestVertices')],
    [],
    {numberOfShards: 4, replicationFactor: 2}
  );
};

const setup = () => {
  createCollections();
  fillCollections();
  generateUsers();
  createGraph();
  return true;
};

return {
  status: setup()
};
