/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, after, afterEach, before, beforeEach, it */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
// / @author Andreas Streichardt
// //////////////////////////////////////////////////////////////////////////////

const expect = require('chai').expect;
const request = require('@arangodb/request');
const users = require('@arangodb/users');
const db = require('@arangodb').db;

describe('Grants', function() {
  before(function() {
    db._drop('grants');
    db._create('grants');
  });
  beforeEach(function() {
    try {
      // remove any such user if it exists
      users.remove('hans');
    } catch (err) {}
  });
  afterEach(function() {
    let resp = request.put({
      url: '/_admin/server/mode',
      body: {'mode': 'default'},
      json: true,
    });
    expect(resp.statusCode).to.equal(200);
    // wait for readonly mode to reset
    require('internal').wait(5.0);
    users.remove('hans');
  });
  after(function() {
    db._drop('grants');
  });
  it('should show the effective rights for a user', function() {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    let resp = request.get({
      url: '/_api/user/hans/database/_system',
      json: true,
    });
    expect(JSON.parse(resp.body)).to.have.property('result', 'rw');
  });
  
  it('should show the effective rights when readonly mode is on', function() {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    
    let resp;
    resp = request.put({
      url: '/_admin/server/mode',
      body: {'mode': 'readonly'},
      json: true,
    });
    resp = request.get({
      url: '/_api/user/hans/database/_system',
      json: true,
    });
    expect(JSON.parse(resp.body)).to.have.property('result', 'ro');
  });

  it('should show the configured rights when readonly mode is on and configured is requested', function() {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    
    let resp;
    resp = request.put({
      url: '/_admin/server/mode',
      body: {'mode': 'readonly'},
      json: true,
    });
    resp = request.get({
      url: '/_api/user/hans/database/_system?configured=true',
      json: true,
    });
    expect(JSON.parse(resp.body)).to.have.property('result', 'rw');
  });

  it('should show the effective rights when readonly mode is on', function() {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    users.grantCollection('hans', '_system', 'grants');
    
    let resp;
    resp = request.put({
      url: '/_admin/server/mode',
      body: {'mode': 'readonly'},
      json: true,
    });
    resp = request.get({
      url: '/_api/user/hans/database/_system/grants',
      json: true,
    });
    expect(JSON.parse(resp.body)).to.have.property('result', 'ro');
  });

  it('should show the configured rights when readonly mode is on and configured is requested', function() {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    users.grantCollection('hans', '_system', 'grants');
    
    let resp;
    resp = request.put({
      url: '/_admin/server/mode',
      body: {'mode': 'readonly'},
      json: true,
    });
    resp = request.get({
      url: '/_api/user/hans/database/_system/grants?configured=true',
      json: true,
    });
    expect(JSON.parse(resp.body)).to.have.property('result', 'rw');
  });
});
