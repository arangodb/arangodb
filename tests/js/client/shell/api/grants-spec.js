/* jshint globalstrict:false, strict:false, maxlen: 5000 */
/* global describe, after, afterEach, before, beforeEach, it, db, arango */
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
const users = require('@arangodb/users');

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
    let resp = arango.PUT_RAW('/_admin/server/mode',
                              {'mode': 'default'});
    expect(resp.code).to.equal(200);
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
    let resp = arango.GET_RAW('/_api/user/hans/database/_system');
    expect(resp.parsedBody).to.have.property('result', 'rw');
  });

  it('should show the effective rights when readonly mode is on', function () {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');

    let resp;
    resp = arango.PUT_RAW(
      '/_admin/server/mode',
      {'mode': 'readonly'});
    resp = arango.GET_RAW('/_api/user/hans/database/_system');
    expect(resp.parsedBody).to.have.property('result', 'ro');
  });

  it('should show the configured rights when readonly mode is on and configured is requested', function () {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');

    let resp;
    resp = arango.PUT_RAW('/_admin/server/mode', {'mode': 'readonly'});
    resp = arango.GET_RAW('/_api/user/hans/database/_system?configured=true');
    expect(resp.parsedBody).to.have.property('result', 'rw');
  });

  it('should show the effective rights when readonly mode is on', function () {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    users.grantCollection('hans', '_system', 'grants');

    let resp;
    resp = arango.PUT_RAW('/_admin/server/mode', {'mode': 'readonly'});
    resp = arango.GET_RAW('/_api/user/hans/database/_system/grants');
    expect(resp.parsedBody).to.have.property('result', 'ro');
  });

  it('should show the configured rights when readonly mode is on and configured is requested', function () {
    users.save('hans');
    users.grantDatabase('hans', '_system', 'rw');
    users.grantCollection('hans', '_system', 'grants');

    let resp;
    resp = arango.PUT_RAW('/_admin/server/mode', {'mode': 'readonly'});
    resp = arango.GET_RAW('/_api/user/hans/database/_system/grants?configured=true');
    expect(resp.parsedBody).to.have.property('result', 'rw');
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

  const generateReadConfigUrl = (user) => {
    return `/_api/user/${user}/config`;
  };

  const generateWriteConfigUrl = (user, attribute) => {
    if (!attribute) {
      attribute = configPropertyAttribute;
    }
    return `/_api/user/${user}/config/${attribute}`;
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
    const cleanupRootUrl = generateWriteConfigUrl(rootUser);
    arango.DELETE_RAW(cleanupRootUrl);
  });

  after(function () {
        db._useDatabase('_system');
    db._dropDatabase(nonRootDB);
  });

  const testSuiteHelper = (user, database) => {
    db._useDatabase(database);
    const readUrl = generateReadConfigUrl(user);
    const writeUrl = generateWriteConfigUrl(user);
    let response;

    // should be empty by default
    response = arango.GET_RAW(readUrl);
    verifyEmptyConfig(response.parsedBody);

    // stores the init object
    response = arango.PUT_RAW(writeUrl,
                              generateConfigBody(initConfigObject));
    expect(response.code).to.equal(200);

    // should now be the same as init object
    response = arango.GET_RAW(readUrl);
    verifyInitObject(response.parsedBody);

    // now try to overwrite as well
    response = arango.PUT_RAW(writeUrl, generateConfigBody(overwriteConfigObject));
    expect(response.code).to.equal(200);

    response = arango.GET_RAW(readUrl);
    verifyOverwriteConfigObject(response.parsedBody);

    // now try a merge with another storage attribute
    const additionalWriteUrl = generateWriteConfigUrl(user, additionalPropertyAttribute);
    response = arango.PUT_RAW(additionalWriteUrl, generateConfigBody(mergeConfigObject));
    expect(response.code).to.equal(200);
    response = arango.GET_RAW(readUrl);
    verifyMergedConfigObject(response.parsedBody);

    // test delete (remove additional key)
    response = arango.DELETE_RAW(additionalWriteUrl);
    response = arango.GET_RAW(readUrl);
    verifyOverwriteConfigObject(response.parsedBody);

    // finally, test delete of graph attribute
    response = arango.DELETE_RAW(writeUrl);
    // should be fully empty now again
    response = arango.GET_RAW(readUrl);
    verifyEmptyConfig(response.parsedBody);
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
