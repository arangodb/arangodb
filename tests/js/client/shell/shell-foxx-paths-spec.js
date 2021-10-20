/*global describe, it, before, beforeEach, afterEach */
'use strict';
const expect = require('chai').expect;
const fm = require('@arangodb/foxx/manager');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
const arango = require('@arangodb').arango;

describe('Foxx service path handling', () => {
  const mount = '/unittest/paths';
  before(function () {
    expect(encodeURIComponent('ö')).not.to.equal('ö');
    expect(encodeURIComponent('+')).not.to.equal('+');
    expect(encodeURIComponent('/')).not.to.equal('/');
    expect(encodeURIComponent(encodeURIComponent('/'))).not.to.equal(encodeURIComponent('/'));

    try {
      fm.uninstall(mount, {force: true});
    } catch (e) {}
    fm.install(fs.join(basePath, 'paths'), mount);
    internal.sleep(1);
  });

  beforeEach(function () { });
  afterEach(function () { });

  it('supports plain URLs', () => {
    const res = arango.GET_RAW(`${mount}/hello/world`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({raw: 'world'});
  });

  it('decodes encoded parts', () => {
    const res = arango.GET_RAW(`${mount}/${encodeURIComponent('hellö')}/world`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({encoded: 'world'});
  });

  it('does not double-decode encoded parts', () => {
    const res = arango.GET_RAW(`${mount}/${encodeURIComponent(encodeURIComponent('hellö'))}/world`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({crazy: 'world'});
  });

  it('does not decode plus signs in parts', () => {
    const res = arango.GET_RAW(`${mount}/h+llo/world`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({plus: 'world'});
  });

  it('decodes encoded path params', () => {
    const res = arango.GET_RAW(`${mount}/hello/${encodeURIComponent('wörld')}`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({raw: 'wörld'});
  });

  it('does not double-decode encoded path params', () => {
    const res = arango.GET_RAW(`${mount}/hello/${encodeURIComponent(encodeURIComponent('wörld'))}`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({raw: encodeURIComponent('wörld')});
  });

  it('does not decode plus signs in path params', () => {
    const res = arango.GET_RAW(`${mount}/hello/w+rld`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({raw: 'w+rld'});
  });

  it('distinguishes between real slashes and encoded slashes', () => {
    const res = arango.GET_RAW(`${mount}/hello/${encodeURIComponent('not/slash')}`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({raw: 'not/slash'});
  });

  it('does not decode suffixes', () => {
    const res = arango.GET_RAW(`${mount}/suffix/${encodeURIComponent('hello/world')}`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({suffix: encodeURIComponent('hello/world')});
  });

  it('does not decode plus signs in suffixes', () => {
    const res = arango.GET_RAW(`${mount}/suffix/${encodeURIComponent('hello+world')}`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({suffix: encodeURIComponent('hello+world')});
  });

  it('does not try to resolve dots in suffixes', () => {
    const res = arango.GET_RAW(`${mount}/suffix/../..`);
    expect(res).to.have.property('code', 200);
    expect(res.parsedBody).to.eql({suffix: '../..'});
  });

  it('does not try to resolve dots during routing', () => {
    const res = arango.GET_RAW(`${mount}/../${mount}/hello`);
    expect(res).to.have.property('code', 404);
  });

  it('serves static files', () => {
    const res = arango.GET_RAW(`${mount}/static/public.txt`, {'accept-content-type': 'text/plain'});
    expect(res.body).to.equal('Hello');
  });

  it('does not serve static files outside statics path using dots', () => {
    const res = arango.GET_RAW(`${mount}/static/../secret.txt`, {json: false});
    expect(res).to.have.property('code', 404);
  });

  it('does not serve static files outside statics path using escaped dots', () => {
    const DOTDOT = '%2E%2E';
    expect(decodeURIComponent(DOTDOT)).to.equal('..');
    const res = arango.GET_RAW(`${mount}/static/${DOTDOT}/secret.txt`, {json: false});
    expect(res).to.have.property('code', 404);
  });
});
