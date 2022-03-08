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

describe('Grants', function () {
  before(function () {
    db._drop('grants');
    db._create('grants');
  });
  beforeEach(function () {
    try {
      // remove any such user if it exists
      users.remove('hans');
    } catch (err) {
    }
  });
  afterEach(function () {
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
  after(function () {
    db._drop('grants');
  });
  it('should show the effective rights for a user', function () {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    let resp = request.get({
      url: '/_api/user/hans/database/_system',
      json: true,
    });
    expect(JSON.parse(resp.body)).to.have.property('result', 'rw');
  });

  it('should show the effective rights when readonly mode is on', function () {
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

  it('should show the configured rights when readonly mode is on and configured is requested', function () {
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

  it('should show the effective rights when readonly mode is on', function () {
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

  it('should show the configured rights when readonly mode is on and configured is requested', function () {
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

describe('UserProperties', function () {
  const rootUser = 'root';
  const rootDB = '_system';

  const nonRootDB = 'nonRootDB';
  const nonRootUser = 'notRoot';

  const configPropertyAttribute = 'graphs';
  const additionalPropertyAttribute = 'someAdditionalAttribute';

  const initConfigObject = {
    a: true,
    b: false,
    c: 1,
    d: 1.1,
    e: "aString",
    f: [1, 2, 3],
    g: {"anObjectKey": "anObjectValue"}
  };

  const overwriteConfigObject = {
    c: 1337,
    someNewParam: "MJ7"
  };

  const mergeConfigObject = {
    "thisMustStay": true
  };

  const verifyEmptyConfig = (result) => {
    expect(result.error).to.be.false;
    expect(result).to.have.property('code', 200);
    expect(result).to.have.property('result', null);
  };

  const verifyInitObject = (result) => {
    expect(result.error).to.be.false;
    expect(result).to.have.property('code', 200);

    result = result.result[configPropertyAttribute];
    expect(result.a).to.be.true;
    expect(result.b).to.be.false;
    expect(result).to.have.property('c', 1);
    expect(result).to.have.property('d', 1.1);
    expect(result).to.have.property('e', "aString");
    expect(result.f).to.be.an('array');
    expect(result.g).to.be.an('object');
  };

  const verifyOverwriteConfigObject = (result) => {
    // We do have one "global" property we can modify. If we write to that same top-level attribute again, it will be
    // fully replaced by the supplied object given. In case we write to another top-level attribute, it will be merged with
    // the other already available ones (This is tested in verifyMergedConfigObject).
    expect(result.error).to.be.false;
    expect(result).to.have.property('code', 200);

    result = result.result[configPropertyAttribute];
    expect(result).to.have.property('c', 1337);
    expect(result).to.have.property('someNewParam', "MJ7");
  };

  const verifyMergedConfigObject = (result) => {
    // needs to be called after we've written `overwriteConfigObject`
    verifyOverwriteConfigObject(result);
    const expectedMergeObject = result.result[additionalPropertyAttribute];
    expect(expectedMergeObject.thisMustStay).to.be.true;
  };

  const generateReadConfigUrl = (user, database) => {
    return `/_db/${database}/_api/user/${user}/config`;
  };

  const generateWriteConfigUrl = (user, database, attribute) => {
    if (!attribute) {
      attribute = configPropertyAttribute;
    }
    return `/_db/${database}/_api/user/${user}/config/${attribute}`;
  };

  const generateConfigBody = (object) => {
    return {
      value: object
    };
  };

  before(function () {
    db._createDatabase(nonRootDB);
  });

  beforeEach(function () {
    try {
      // remove any such user if it exists
      users.remove(nonRootUser);
    } catch (ignore) {
    }
  });

  afterEach(function () {
    try {
      users.remove(nonRootUser);
    } catch (ignore) {
    }

    // cleanup root users config we might have been modified in any of our tests
    const cleanupRootUrl = generateWriteConfigUrl(rootUser, rootDB);
    request.delete({
      url: cleanupRootUrl,
      json: true
    });
  });

  after(function () {
    db._dropDatabase(nonRootDB);
  });

  const testSuiteHelper = (user, database) => {
    const readUrl = generateReadConfigUrl(user, database);
    const writeUrl = generateWriteConfigUrl(user, database);
    let response;

    // should be empty by default
    response = request.get({
      url: readUrl,
      json: true
    });
    verifyEmptyConfig(JSON.parse(response.body));

    // stores the init object
    response = request.put({
      url: writeUrl,
      body: generateConfigBody(initConfigObject),
      json: true
    });
    expect(response.statusCode).to.equal(200);

    // should now be the same as init object
    response = request.get({
      url: readUrl,
      json: true
    });
    verifyInitObject(JSON.parse(response.body));

    // now try to overwrite as well
    response = request.put({
      url: writeUrl,
      body: generateConfigBody(overwriteConfigObject),
      json: true
    });
    expect(response.statusCode).to.equal(200);

    response = request.get({
      url: readUrl,
      json: true
    });
    verifyOverwriteConfigObject(JSON.parse(response.body));

    // now try a merge with another storage attribute
    const additionalWriteUrl = generateWriteConfigUrl(user, database, additionalPropertyAttribute);
    response = request.put({
      url: additionalWriteUrl,
      body: generateConfigBody(mergeConfigObject),
      json: true
    });
    expect(response.statusCode).to.equal(200);
    response = request.get({
      url: readUrl,
      json: true
    });
    verifyMergedConfigObject(JSON.parse(response.body));

    // test delete (remove additional key)
    response = request.delete({
      url: additionalWriteUrl,
      json: true
    });
    response = request.get({
      url: readUrl,
      json: true
    });
    verifyOverwriteConfigObject(JSON.parse(response.body));

    // finally, test delete of graph attribute
    response = request.delete({
      url: writeUrl,
      json: true
    });
    // should be fully empty now again
    response = request.get({
      url: readUrl,
      json: true
    });
    verifyEmptyConfig(JSON.parse(response.body));
  };

  it('should store/patch/delete root config data in _system database', function () {
    testSuiteHelper(rootUser, rootDB);
  });

  it('should store/patch/delete root config data in non-system database', function () {
    testSuiteHelper(rootUser, nonRootDB);
  });

  it('should store/patch/delete non-root user config data in _system database', function () {
    users.save(nonRootUser);
    users.grantDatabase(nonRootUser, rootDB, 'rw');
    testSuiteHelper(nonRootUser, rootDB);
  });

  it('should store/patch/delete non-root user config data in non-system database', function () {
    users.save(nonRootUser);
    users.grantDatabase(nonRootUser, nonRootDB, 'rw');
    testSuiteHelper(nonRootUser, nonRootDB);
  });

});
