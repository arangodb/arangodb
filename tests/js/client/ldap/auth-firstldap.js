/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication (dual ldap)
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Frank Celler
/// @author Copyright 2021, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const {
  FORBIDDEN,
  OK,
  createSuite
} = require('@arangodb/testutils/ldap-generic-failover.js');

// username, password, database, result
const dualLdapResults = {
  User1NoPwLdap1: ["user1", "", "test", FORBIDDEN],
  User1PwLdap1: ["user1", "abc", "test", OK],
  User1NoPwLdap2: ["user1,dc=arangodb", "", "test", FORBIDDEN],
  User1PwLdap2: ["user1,dc=arangodb", "abc", "test", FORBIDDEN],

  DonaldNoPwLdap1: ["The Donald", "", "test", FORBIDDEN],
  DonaldPwLdap1: ["The Donald", "abc", "test", OK],
  DonaldNoPwLdap2: ["The Donald,dc=arangodb", "", "test", FORBIDDEN],
  DonaldPwLdap: ["The Donald,dc=arangodb", "abc", "test", FORBIDDEN],

  MerkelNoPwLdap1: ["Angela Merkel", "", "test", FORBIDDEN],
  MerkelPwLdap1: ["Angela Merkel", "abc", "test", FORBIDDEN],
  MerkelNoPwLdap2: ["Angela Merkel,dc=arangodb", "", "test", FORBIDDEN],
  MerkelPwLdap2: ["Angela Merkel,dc=arangodb", "abc", "test", FORBIDDEN]
};

const DualLdapSuite = createSuite(dualLdapResults);

jsunity.run(DualLdapSuite);
return jsunity.done();
