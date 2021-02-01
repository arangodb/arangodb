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
  describe('cookie', function () {
    it('adds a cookie', function () {
      require("console").log('adds a cookie');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.cookie('hello', 'banana');
      expect(rawRes.cookies).to.eql([
        {name: 'hello', value: 'banana'}
      ]);
    });
    it('optionally adds a TTL', function () {
      require("console").log('optionally adds a TTL');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.cookie('hello', 'banana', {ttl: 22});
      expect(rawRes.cookies).to.eql([
        {name: 'hello', value: 'banana', lifeTime: 22}
      ]);
    });
    it('optionally adds some metadata', function () {
      require("console").log('optionally adds some metadata');
      const rawRes = {};
      require("console").log("1");
      const res = new SyntheticResponse(rawRes, {});
      require("console").log("2");
      res.cookie('hello', 'banana', {
        path: '/path',
        domain: 'cats.example',
        secure: true,
        httpOnly: true
      });
      require("console").log("3");
      expect(rawRes.cookies).to.eql([
        {
          name: 'hello',
          value: 'banana',
          path: '/path',
          domain: 'cats.example',
          secure: true,
          httpOnly: true
        }
      ]);
      require("console").log("4");
    });
    it('supports signed cookies when a secret is provided', function () {
      require("console").log('supports signed cookies when a secret is provided');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.cookie('hello', 'banana', {secret: 'potato'});
      expect(rawRes.cookies).to.eql([
        {name: 'hello', value: 'banana'},
        {name: 'hello.sig', value: crypto.hmac('potato', 'banana')}
      ]);
    });
    it('supports signed cookies with different algorithms', function () {
      require("console").log('supports signed cookies with different algorithms');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.cookie('hello', 'banana', {
        secret: 'potato',
        algorithm: 'sha512'
      });
      expect(rawRes.cookies).to.eql([
        {name: 'hello', value: 'banana'},
        {name: 'hello.sig', value: crypto.hmac('potato', 'banana', 'sha512')}
      ]);
    });
    it('treats options string as a secret', function () {
      require("console").log('treats options string as a secret');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.cookie('hello', 'banana', 'potato');
      expect(rawRes.cookies).to.eql([
        {name: 'hello', value: 'banana'},
        {name: 'hello.sig', value: crypto.hmac('potato', 'banana')}
      ]);
    });
    it('treats options number as a TTL value', function () {
      require("console").log('treats options number as a TTL value');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.cookie('hello', 'banana', 22);
      expect(rawRes.cookies).to.eql([
        {name: 'hello', value: 'banana', lifeTime: 22}
      ]);
    });
  });
});
