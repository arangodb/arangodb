/*global describe, it, before, beforeEach, afterEach */
'use strict';
const expect = require('chai').expect;
const fm = require('@arangodb/foxx/manager');
const request = require('@arangodb/request');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.pathForTesting('common'), 'test-data', 'apps'));
const arango = require('@arangodb').arango;
const origin = arango.getEndpoint().replace(/\+vpp/, '').replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:').replace(/^vst:/, 'http:').replace(/^h2:/, 'http:');
const baseUrl = origin + '/_db/_system';

require("@arangodb/test-helper").waitForFoxxInitialized();

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
    const res = request.get(`${baseUrl}/${mount}/hello/world`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({raw: 'world'});
  });

  it('decodes encoded parts', () => {
    const res = request.get(`${baseUrl}/${mount}/${encodeURIComponent('hellö')}/world`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({encoded: 'world'});
  });

  it('does not double-decode encoded parts', () => {
    const res = request.get(`${baseUrl}/${mount}/${encodeURIComponent(encodeURIComponent('hellö'))}/world`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({crazy: 'world'});
  });

  it('does not decode plus signs in parts', () => {
    const res = request.get(`${baseUrl}/${mount}/h+llo/world`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({plus: 'world'});
  });

  it('decodes encoded path params', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/${encodeURIComponent('wörld')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({raw: 'wörld'});
  });

  it('does not double-decode encoded path params', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/${encodeURIComponent(encodeURIComponent('wörld'))}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({raw: encodeURIComponent('wörld')});
  });

  it('does not decode plus signs in path params', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/w+rld`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({raw: 'w+rld'});
  });

  it('distinguishes between real slashes and encoded slashes', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/${encodeURIComponent('not/slash')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({raw: 'not/slash'});
  });

  it('does not decode suffixes', () => {
    const res = request.get(`${baseUrl}/${mount}/suffix/${encodeURIComponent('hello/world')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({suffix: encodeURIComponent('hello/world')});
  });

  it('does not decode plus signs in suffixes', () => {
    const res = request.get(`${baseUrl}/${mount}/suffix/${encodeURIComponent('hello+world')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({suffix: encodeURIComponent('hello+world')});
  });

  it('does not try to resolve dots in suffixes', () => {
    const res = request.get(`${baseUrl}/${mount}/suffix/../..`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({suffix: '../..'});
  });

  it('does not try to resolve dots during routing', () => {
    const res = request.get(`${baseUrl}/${mount}/../${mount}/hello`);
    expect(res).to.have.property('status', 404);
  });

  it('serves static files', () => {
    const res = request.get(`${baseUrl}/${mount}/static/public.txt`, {json: false});
    expect(res.body).to.equal('Hello');
  });

  it('does not serve static files outside statics path using dots', () => {
    const res = request.get(`${baseUrl}/${mount}/static/../secret.txt`, {json: false});
    expect(res).to.have.property('status', 404);
  });

  it('does not serve static files outside statics path using escaped dots', () => {
    const DOTDOT = '%2E%2E';
    expect(decodeURIComponent(DOTDOT)).to.equal('..');
    const res = request.get(`${baseUrl}/${mount}/static/${DOTDOT}/secret.txt`, {json: false});
    expect(res).to.have.property('status', 404);
  });
});
