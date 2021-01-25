/* global describe, it */
'use strict';
const expect = require('chai').expect;
const statuses = require('statuses');
const path = require('path');
const fs = require('fs');
const internal = require('internal');
const crypto = require('@arangodb/crypto');
const SyntheticResponse = require('@arangodb/foxx/router/response');

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('SyntheticResponse', function () {
  describe('sendFile', function () {
    const filename = fs.normalize(fs.makeAbsolute(path.join(
      internal.pathForTesting('common'),
      'test-data',
      'foxx',
      'toomanysecrets.txt'
    )));
    const body = fs.readBuffer(filename);
    const lastModified = new Date(fs.mtime(filename) * 1000).toUTCString();
    it('sets the native request body to the file contents', function () {
      require("console").log('sets the native request body to the file contents');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendFile(filename);
      expect(rawRes.body).to.eql(body);
    });
    it('sets the last-modified header by default', function () {
      require("console").log('sets the last-modified header by default');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendFile(filename);
      expect(rawRes.headers).to.have.a.property('last-modified', lastModified);
    });
    it('does not override existing last-modified header', function () {
      require("console").log('does not override existing last-modified header');
      const rawRes = {headers: {'last-modified': 'not today'}};
      const res = new SyntheticResponse(rawRes, {});
      res.sendFile(filename);
      expect(rawRes.headers).to.have.a.property('last-modified', 'not today');
    });
    it('overrides last-modified header if lastModified is true', function () {
      require("console").log('overrides last-modified header if lastModified is true');
      const rawRes = {headers: {'last-modified': 'not today'}};
      const res = new SyntheticResponse(rawRes, {});
      res.sendFile(filename, {lastModified: true});
      expect(rawRes.headers).to.have.a.property('last-modified', lastModified);
    });
    it('does not set the last-modified header if lastModified is false', function () {
      require("console").log('does not set the last-modified header if lastModified is false');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendFile(filename, {lastModified: false});
      expect(rawRes).not.to.have.a.property('headers');
    });
    it('treats options boolean as lastModified', function () {
      require("console").log('treats options boolean as lastModified');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendFile(filename, false);
      expect(rawRes).not.to.have.a.property('headers');
    });
  });
  describe('sendStatus', function () {
    it('sets the responseCode of the native request', function () {
      require("console").log('sets the responseCode of the native request');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendStatus(400);
      expect(rawRes.responseCode).to.equal(400);
    });
    it('sets the response body to the matching message', function () {
      require("console").log('sets the response body to the matching message');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendStatus(400);
      expect(rawRes.body).to.equal(statuses[400]);
    });
    it('sets the response body to the status code if no message exists', function () {
      require("console").log('sets the response body to the status code if no message exists');
      expect(statuses).not.to.have.a.property('999');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.sendStatus(999);
      expect(rawRes.body).to.equal('999');
    });
  });
  describe('set', function () {
    it('updates the header by name', function () {
      require("console").log('updates the header by name');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      res.set('hello', 'pancakes');
      expect(rawRes.headers.hello).to.equal('pancakes');
    });
    it('converts the name to lowercase', function () {
      require("console").log('converts the name to lowercase');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      res.set('Hello', 'pancakes');
      expect(rawRes.headers.hello).to.equal('pancakes');
    });
    it('intercepts content-type headers', function () {
      require("console").log('intercepts content-type headers');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      res.set('content-type', 'application/x-woof');
      expect(rawRes.contentType).to.equal('application/x-woof');
    });
    it('intercepts content-type headers in any case', function () {
      require("console").log('intercepts content-type headers in any case');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      res.set('Content-Type', 'application/x-woof');
      expect(rawRes.contentType).to.equal('application/x-woof');
    });
    it('has no effect when called without a name', function () {
      require("console").log('has no effect when called without a name');
      const rawRes = {headers: {}, contentType: 'application/x-wat'};
      const res = new SyntheticResponse(rawRes, {});
      Object.freeze(rawRes.headers);
      Object.freeze(rawRes);
      expect(function () {
        res.set();
      }).not.to.throw();
    });
    it('accepts a headers object', function () {
      require("console").log('accepts a headers object');
      const rawRes = {headers: {a: '1'}};
      const res = new SyntheticResponse(rawRes, {});
      res.set({b: '2', c: '3'});
      expect(rawRes.headers).to.eql({a: '1', b: '2', c: '3'});
    });
  });
  describe('status', function () {
    it('allows setting the responseCode of the native response', function () {
      require("console").log('allows setting the responseCode of the native response');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      res.status(666);
      expect(rawRes.responseCode).to.equal(666);
    });
  });
  describe('vary', function () {
    it('manipulates the vary header', function () {
      require("console").log('manipulates the vary header');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.vary('Origin');
      expect(res.headers).to.have.a.property('vary', 'Origin');
      res.vary('User-Agent');
      expect(res.headers).to.have.a.property('vary', 'Origin, User-Agent');
    });
    it('ignores duplicates', function () {
      require("console").log('ignores duplicates');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.vary('x, y');
      expect(res.headers).to.have.a.property('vary', 'x, y');
      res.vary('x, z');
      expect(res.headers).to.have.a.property('vary', 'x, y, z');
    });
    it('understands arrays', function () {
      require("console").log();
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.vary('x');
      expect(res.headers).to.have.a.property('vary', 'x');
      res.vary(['y', 'z']);
      expect(res.headers).to.have.a.property('vary', 'x, y, z');
    });
    it('understands wildcards', function () {
      require("console").log('understands wildcards');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.vary('*');
      expect(res.headers).to.have.a.property('vary', '*');
      res.vary('User-Agent');
      expect(res.headers).to.have.a.property('vary', '*');
    });
  });
});
