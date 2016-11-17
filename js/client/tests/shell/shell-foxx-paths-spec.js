/*global describe, it, beforeEach, afterEach */
'use strict';
const expect = require('chai').expect;
const fm = require('@arangodb/foxx/manager');
const request = require('@arangodb/request');
const fs = require('fs');
const internal = require('internal');
const basePath = fs.makeAbsolute(fs.join(internal.startupPath, 'common', 'test-data', 'apps'));
const arango = require('@arangodb').arango;
const baseUrl = arango.getEndpoint().replace('tcp://', 'http://') + '/_db/_system';

describe('Foxx service path handling', () => {
  const mount = '/unittest/paths';
  beforeEach(function () {
    try {
      fm.uninstall(mount, {force: true});
    } catch (e) {}
    fm.install(basePath, mount);
  });

  afterEach(function () {
    fm.uninstall(mount, {force: true});
  });

  it('supports plain URLs', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/world`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({name: 'world'});
  });

  it('decodes encoded parts', () => {
    const res = request.get(`${baseUrl}/${mount}/${encodeURIComponent('hello')}/world`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({name: 'world'});
  });

  it('decodes encoded path params', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/${encodeURIComponent('world')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({name: 'world'});
  });

  it('distinguishes between real slashes and encoded slashes', () => {
    const res = request.get(`${baseUrl}/${mount}/hello/${encodeURIComponent('not/slash')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).to.eql({name: 'not/slash'});
  });

  it('does not decode suffixes', () => {
    const res = request.get(`${baseUrl}/${mount}/suffix/${encodeURIComponent('hello/world')}`);
    expect(res).to.have.property('status', 200);
    expect(res.json).not.to.eql({suffix: 'hello/world'});
    expect(res.json).to.eql({suffix: encodeURIComponent('hello/world')});
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
