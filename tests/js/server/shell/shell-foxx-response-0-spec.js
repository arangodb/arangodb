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
  describe('headers', function () {
    it('exposes the headers of the native response', function () {
      require("console").log('exposes the headers of the native response');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.headers).to.equal(rawRes.headers);
    });
  });
  describe('statusCode', function () {
    it('exposes the responseCode of the native response', function () {
      require("console").log('exposes the responseCode of the native response');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.statusCode).to.equal(rawRes.responseCode);
    });
    it('allows setting the responseCode of the native response', function () {
      require("console").log('allows setting the responseCode of the native response');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      res.statusCode = 666;
      expect(res.statusCode).to.equal(rawRes.responseCode).and.to.equal(666);
    });
  });
  describe('body', function () {
    it('exposes the body of the native response', function () {
      require("console").log('exposes the body of the native response');
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.body).to.equal(rawRes.body);
    });
    it('allows setting the native response body to a string', function () {
      require("console").log('allows setting the native response body to a string');
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      res.body = 'potato';
      expect(rawRes.body).to.equal('potato');
    });
    it('allows setting the native response body to a buffer', function () {
      require("console").log('allows setting the native response body to a buffer');
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      res.body = new Buffer('potato');
      expect(rawRes.body).to.eql(new Buffer('potato'));
    });
    it('allows removing the native response body with null', function () {
      require("console").log('allows removing the native response body with null');
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      res.body = null;
      expect(rawRes).not.to.have.a.property('body');
    });
    it('allows removing the native response body with undefined', function () {
      require("console").log('allows removing the native response body with undefined');
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      res.body = undefined;
      expect(rawRes).not.to.have.a.property('body');
    });
    it('converts objects to JSON', function () {
      require("console").log('converts objects to JSON');
      const value = {hello: 'world'};
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      res.body = value;
      expect(rawRes.body).to.equal(JSON.stringify(value));
    });
    it('converts arrays to JSON', function () {
      require("console").log('converts arrays to JSON');
      const value = [1, 2, 3];
      const rawRes = {body: 'banana'};
      const res = new SyntheticResponse(rawRes, {});
      res.body = value;
      expect(rawRes.body).to.equal(JSON.stringify(value));
    });
    [0, 23, -1, false, true].forEach(function (value) {
      it(`converts ${value} to a string`, function () {
        require("console").log(`converts ${value} to a string`);
        const rawRes = {body: 'banana'};
        const res = new SyntheticResponse(rawRes, {});
        res.body = value;
        expect(rawRes.body).to.equal(String(value));
      });
    });
  });
  describe('getHeader', function () {
    it('returns the header by name', function () {
      require("console").log('returns the header by name');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.getHeader('hello')).to.equal('world');
    });
    it('converts the name to lowercase', function () {
      require("console").log('converts the name to lowercase');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.getHeader('Hello')).to.equal(res.getHeader('hello'));
    });
    it('intercepts content-type headers', function () {
      require("console").log('intercepts content-type headers');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.getHeader('content-type')).to.equal(rawRes.contentType);
    });
    it('intercepts content-type headers in any case', function () {
      require("console").log('intercepts content-type headers in any case');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      expect(res.getHeader('Content-Type')).to.equal(rawRes.contentType);
    });
  });
  describe('removeHeader', function () {
    it('removes the header by name', function () {
      require("console").log('removes the header by name');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      res.removeHeader('hello');
      expect(rawRes.headers).not.to.have.a.property('hello');
    });
    it('converts the name to lowercase', function () {
      require("console").log('converts the name to lowercase');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      res.removeHeader('Hello');
      expect(rawRes.headers).not.to.have.a.property('hello');
    });
    it('intercepts content-type headers', function () {
      require("console").log('intercepts content-type headers');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      res.removeHeader('content-type');
      expect(rawRes).not.to.have.a.property('contentType');
    });
    it('intercepts content-type headers in any case', function () {
      require("console").log('intercepts content-type headers in any case');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      res.removeHeader('Content-Type');
      expect(rawRes).not.to.have.a.property('contentType');
    });
  });
  describe('setHeader', function () {
    it('updates the header by name', function () {
      require("console").log('updates the header by name');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      res.setHeader('hello', 'pancakes');
      expect(rawRes.headers.hello).to.equal('pancakes');
    });
    it('converts the name to lowercase', function () {
      require("console").log('converts the name to lowercase');
      const rawRes = {headers: {hello: 'world'}};
      const res = new SyntheticResponse(rawRes, {});
      res.setHeader('Hello', 'pancakes');
      expect(rawRes.headers.hello).to.equal('pancakes');
    });
    it('intercepts content-type headers', function () {
      require("console").log('intercepts content-type headers');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      res.setHeader('content-type', 'application/x-woof');
      expect(rawRes.contentType).to.equal('application/x-woof');
    });
    it('intercepts content-type headers in any case', function () {
      require("console").log('intercepts content-type headers in any case');
      const rawRes = {contentType: 'application/x-meow'};
      const res = new SyntheticResponse(rawRes, {});
      res.setHeader('Content-Type', 'application/x-woof');
      expect(rawRes.contentType).to.equal('application/x-woof');
    });
    it('has no effect when called without a name', function () {
      require("console").log('has no effect when called without a name');
      const rawRes = {headers: {}, contentType: 'application/x-wat'};
      const res = new SyntheticResponse(rawRes, {});
      Object.freeze(rawRes.headers);
      Object.freeze(rawRes);
      expect(function () {
        res.setHeader();
      }).not.to.throw();
    });
  });
});
