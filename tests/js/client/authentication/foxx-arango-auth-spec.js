/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('org/arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps', 'arango-auth'));
const url = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('Foxx arangoUser', function () {
  let mount;

  beforeEach(function () {
    mount = '/unittest/arango-auth';
    try {
      FoxxManager.uninstall(mount, {force: true});
    } catch (err) {
    }
    FoxxManager.install(basePath, mount);
  });

  afterEach(function () {
    FoxxManager.uninstall(mount, {force: true});
  });

  it('defaults to null if omitted', function () {
    const result = internal.download(url + mount, '');
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('defaults to authenticated user when present', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('root:').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: 'root'}));
  });

  it('should not set the arangoUser object if not authenticated correctly - used invalid password', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('root:invalidpassword').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - used invalid username', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('iamnotavaliduser:').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - used invalid username and password', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('iamnotavaliduser:noriamavalidpassword').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - empty username and empty password', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer(':').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - empty password', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('iamnotavaliduser:').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - empty user', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer(':somerandompass').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - empty string', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - str boolean true', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('true').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - str boolean false', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('false').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - str array', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('[]').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - str obj', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer('{}').toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

  it('should not set the arangoUser object if not authenticated correctly - str empty', function () {
    const opts = { headers: {
      authorization: (
        'Basic ' + new Buffer([]).toString('base64')
      )
    }};
    const result = internal.download(url + mount, '', opts);
    expect(result.code).to.equal(200);
    expect(result.body).to.eql(JSON.stringify({user: null}));
  });

});
