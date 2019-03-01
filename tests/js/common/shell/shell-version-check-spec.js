/*jshint globalstrict:false, strict:false, globalstrict: true */
/*global assertEqual, describe, it */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for routing
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 ArangoDB GmbH, Cologne, Germany
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
/// @author Heiko Kernbach
/// @author Copyright 2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const expect = require('chai').expect;
const arangodb = require("@arangodb");
const semver = require("semver");
const db = arangodb.db;

describe('Check if server version follows semantic versioning', function() {

  it('check and verify exposed version coming from the db module', function() {
    let version = db._version();

    expect(semver.valid(version)).to.equal(version);
    expect(semver.valid(version)).to.not.equal(null);
  });

  it('check and verify exposed version coming from the @arangodb module', function() {
    let version = arangodb.plainServerVersion();

    expect(semver.valid(version)).to.equal(version);
    expect(semver.valid(version)).to.not.equal(null);
  });
});
