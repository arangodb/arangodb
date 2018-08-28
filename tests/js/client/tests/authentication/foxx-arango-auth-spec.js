/* global arango, describe, beforeEach, afterEach, it*/
'use strict';

const expect = require('chai').expect;
const FoxxManager = require('org/arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps', 'arango-auth'));
const url = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');

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
});
