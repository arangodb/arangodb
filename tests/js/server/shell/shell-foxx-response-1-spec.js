/* global describe, it */
'use strict';
const expect = require('chai').expect;
const sinon = require('sinon');
const statuses = require('statuses');
const path = require('path');
const fs = require('fs');
const internal = require('internal');
const crypto = require('@arangodb/crypto');
const SyntheticResponse = require('@arangodb/foxx/router/response');

require("@arangodb/test-helper").waitForFoxxInitialized();

describe('SyntheticResponse', function () {
  describe('write', function () {
    describe('when the native response has no body', function () {
      it('allows setting the native response body to a string', function () {
        require("console").log('allows setting the native response body to a string');
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.write('potato');
        expect(rawRes.body).to.equal('potato');
      });
      it('allows setting the native response body to a buffer', function () {
        require("console").log('allows setting the native response body to a buffer');
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.write(new Buffer('potato'));
        expect(rawRes.body).to.eql(new Buffer('potato'));
      });
      it('ignores null values', function () {
        require("console").log('ignores null values');
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.write(null);
        expect(rawRes).not.to.have.a.property('body');
      });
      it('ignores undefined values', function () {
        require("console").log('ignores undefined values');
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.write(undefined);
        expect(rawRes).not.to.have.a.property('body');
      });
      it('converts objects to JSON', function () {
        require("console").log('converts objects to JSON');
        const value = {hello: 'world'};
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.write(value);
        expect(rawRes.body).to.equal(JSON.stringify(value));
      });
      it('converts arrays to JSON', function () {
        require("console").log('converts arrays to JSON');
        const value = [1, 2, 3];
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.write(value);
        expect(rawRes.body).to.equal(JSON.stringify(value));
      });
      [0, 23, -1, false, true].forEach(function (value) {
        it(`converts ${value} to a string`, function () {
          require("console").log(`converts ${value} to a string`);
          const rawRes = {};
          const res = new SyntheticResponse(rawRes, {});
          res.write(value);
          expect(rawRes.body).to.equal(String(value));
        });
      });
    });
    describe('when the native response has a string body', function () {
      const body = 'banana';
      it('allows adding a string to the native response body', function () {
        require("console").log('allows adding a string to the native response body');
        const rawRes = {body: body};
        const res = new SyntheticResponse(rawRes, {});
        res.write('potato');
        expect(rawRes.body).to.equal(body + 'potato');
      });
      it('allows adding a buffer to the native response body', function () {
        require("console").log('allows adding a buffer to the native response body');
        const rawRes = {body: body};
        const res = new SyntheticResponse(rawRes, {});
        res.write(new Buffer('potato'));
        expect(rawRes.body).to.eql(new Buffer(body + 'potato'));
      });
      it('ignores null values', function () {
        require("console").log('ignores null values');
        const rawRes = {body: body};
        const res = new SyntheticResponse(rawRes, {});
        res.write(null);
        expect(rawRes.body).to.equal(body);
      });
      it('ignores undefined values', function () {
        require("console").log('ignores undefined values');
        const rawRes = {body: body};
        const res = new SyntheticResponse(rawRes, {});
        res.write(undefined);
        expect(rawRes.body).to.equal(body);
      });
      it('converts objects to JSON', function () {
        require("console").log('converts objects to JSON');
        const value = {hello: 'world'};
        const rawRes = {body: body};
        const res = new SyntheticResponse(rawRes, {});
        res.write(value);
        expect(rawRes.body).to.equal(body + JSON.stringify(value));
      });
      it('converts arrays to JSON', function () {
        require("console").log('converts arrays to JSON');
        const value = [1, 2, 3];
        const rawRes = {body: body};
        const res = new SyntheticResponse(rawRes, {});
        res.write(value);
        expect(rawRes.body).to.equal(body + JSON.stringify(value));
      });
      [0, 23, -1, false, true].forEach(function (value) {
        it(`converts ${value} to a string`, function () {
          require("console").log(`converts ${value} to a string`);
          const rawRes = {body: body};
          const res = new SyntheticResponse(rawRes, {});
          res.write(value);
          expect(rawRes.body).to.equal(body + String(value));
        });
      });
    });
    describe('when the native response has a buffer body', function () {
      const body = 'banana';
      it('allows adding a string to the native response body', function () {
        require("console").log('allows adding a string to the native response body');
        const rawRes = {body: new Buffer(body)};
        const res = new SyntheticResponse(rawRes, {});
        res.write('potato');
        expect(rawRes.body).to.eql(new Buffer(body + 'potato'));
      });
      it('allows adding a buffer to the native response body', function () {
        require("console").log('allows adding a buffer to the native response body');
        const rawRes = {body: new Buffer(body)};
        const res = new SyntheticResponse(rawRes, {});
        res.write(new Buffer('potato'));
        expect(rawRes.body).to.eql(new Buffer(body + 'potato'));
      });
      it('ignores null values', function () {
        require("console").log('ignores null values');
        const buf = new Buffer(body);
        const rawRes = {body: buf};
        const res = new SyntheticResponse(rawRes, {});
        res.write(null);
        expect(rawRes.body).to.equal(buf);
      });
      it('ignores undefined values', function () {
        require("console").log('ignores undefined values');
        const buf = new Buffer(body);
        const rawRes = {body: buf};
        const res = new SyntheticResponse(rawRes, {});
        res.write(undefined);
        expect(rawRes.body).to.equal(buf);
      });
      it('converts objects to JSON', function () {
        require("console").log('converts objects to JSON');
        const value = {hello: 'world'};
        const rawRes = {body: new Buffer(body)};
        const res = new SyntheticResponse(rawRes, {});
        res.write(value);
        expect(rawRes.body).to.eql(new Buffer(body + JSON.stringify(value)));
      });
      it('converts arrays to JSON', function () {
        require("console").log('converts arrays to JSON');
        const value = [1, 2, 3];
        const rawRes = {body: new Buffer(body)};
        const res = new SyntheticResponse(rawRes, {});
        res.write(value);
        expect(rawRes.body).to.eql(new Buffer(body + JSON.stringify(value)));
      });
      [0, 23, -1, false, true].forEach(function (value) {
        it(`converts ${value} to a string`, function () {
          require("console").log(`converts ${value} to a string`);
          const rawRes = {body: new Buffer(body)};
          const res = new SyntheticResponse(rawRes, {});
          res.write(value);
          expect(rawRes.body).to.eql(new Buffer(body + String(value)));
        });
      });
    });
  });
  describe('attachment', function () {
    it('adds a content-disposition header', function () {
      require("console").log('adds a content-disposition header');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.attachment('lol.js');
      expect(rawRes.headers).to.have.a.property(
        'content-disposition',
        'attachment; filename="lol.js"'
      );
    });
    it('only exposes the basename', function () {
      require("console").log('only exposes the basename');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.attachment('/hello/world/lol.js');
      expect(rawRes.headers).to.have.a.property(
        'content-disposition',
        'attachment; filename="lol.js"'
      );
    });
    it('escapes quotation marks in the filename', function () {
      require("console").log('escapes quotation marks in the filename');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.attachment('"lol".js');
      expect(rawRes.headers).to.have.a.property(
        'content-disposition',
        'attachment; filename="\\"lol\\".js"'
      );
    });
    it('escapes special characters', function () {
      require("console").log('escapes special characters');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.attachment('l\rl.js');
      expect(rawRes.headers).to.have.a.property(
        'content-disposition',
        'attachment; filename="l?l.js"; filename*=UTF-8\'\'l%0Dl.js'
      );
    });
    [
      ['js', 'application/javascript'],
      ['json', 'application/json'],
      ['txt', 'text/plain'],
      ['unknown mime type', 'application/octet-stream']
    ].forEach(function (t) {
      it(`sets the content-type header to ${t[1]} for ${t[0]} files`, function () {
        require("console").log(`sets the content-type header to ${t[1]} for ${t[0]} files`);
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.attachment(`lol.${t[0]}`);
        expect(rawRes.contentType).to.equal(t[1]);
      });
    });
    it('does not override existing content-type', function () {
      require("console").log('does not override existing content-type');
      const rawRes = {contentType: 'application/json'};
      const res = new SyntheticResponse(rawRes, {});
      res.attachment('hello.txt');
      expect(rawRes.contentType).to.equal('application/json');
    });
    it('overrides existing content-disposition headers', function () {
      require("console").log('overrides existing content-disposition headers');
      const rawRes = {headers: {'content-disposition': 'lolcat'}};
      const res = new SyntheticResponse(rawRes, {});
      res.attachment('hello.txt');
      expect(rawRes.headers).to.have.a.property(
        'content-disposition',
        'attachment; filename="hello.txt"'
      );
    });
  });
  describe('download', function () {
    it('passes the arguments to attachment and sendFile', function () {
      require("console").log('passes the arguments to attachment and sendFile');
      const path = '/hello/world.js';
      const filename = 'lol.js';
      const res = new SyntheticResponse({}, {});
      res.attachment = sinon.spy();
      res.sendFile = sinon.spy();
      res.download(path, filename);
      expect(res.attachment.calledOnce).to.equal(true);
      expect(res.attachment.args[0]).to.eql([filename]);
      expect(res.sendFile.calledOnce).to.equal(true);
      expect(res.sendFile.args[0]).to.eql([path]);
    });
    it('falls back to path if filename is omitted', function () {
      require("console").log('falls back to path if filename is omitted');
      const path = '/hello/world.js';
      const res = new SyntheticResponse({}, {});
      res.attachment = sinon.spy();
      res.sendFile = sinon.spy();
      res.download(path);
      expect(res.attachment.calledOnce).to.equal(true);
      expect(res.attachment.args[0]).to.eql([path]);
      expect(res.sendFile.calledOnce).to.equal(true);
      expect(res.sendFile.args[0]).to.eql([path]);
    });
  });
  describe('json', function () {
    [{hello: 'world'}, [1, 2, 3], 'a string', 23, null, false, true, -1].forEach(function (value) {
      it(`converts ${value} to JSON`, function () {
        require("console").log(`converts ${value} to JSON`);
        const rawRes = {};
        const res = new SyntheticResponse(rawRes, {});
        res.json(value);
        expect(rawRes.body).to.equal(JSON.stringify(value));
      });
    });
    it('sets the content-type to JSON', function () {
      require("console").log('sets the content-type to JSON');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.json({some: 'json'});
      expect(rawRes.contentType).to.equal('application/json; charset=utf-8');
    });
    it('does not override the existing content-type', function () {
      require("console").log('does not override the existing content-type');
      const rawRes = {contentType: 'application/x-lolcat'};
      const res = new SyntheticResponse(rawRes, {});
      res.json({some: 'json'});
      expect(rawRes.contentType).to.equal('application/x-lolcat');
    });
  });
  describe('redirect', function () {
    it('sets the responseCode of the native request', function () {
      require("console").log('sets the responseCode of the native request');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      res.redirect(303, '/lol/cat');
      expect(rawRes.responseCode).to.equal(303);
    });
    it('sets the location header of the native request', function () {
      require("console").log('sets the location header of the native request');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      res.redirect(303, '/lol/cat');
      expect(rawRes.headers).to.have.a.property('location', '/lol/cat');
    });
    it('defaults to code 302 if no code is provided', function () {
      require("console").log('defaults to code 302 if no code is provided');
      const rawRes = {};
      const res = new SyntheticResponse(rawRes, {});
      res.redirect('/lol/cat');
      expect(rawRes.responseCode).to.equal(302);
    });
    it('sets responseCode to 301 if code is "permanent"', function () {
      require("console").log('sets responseCode to 301 if code is "permanent"');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      res.redirect('permanent', '/lol/cat');
      expect(rawRes.responseCode).to.equal(301);
    });
    it('does not override responseCode if no code is provided', function () {
      require("console").log('does not override responseCode if no code is provided');
      const rawRes = {responseCode: 999};
      const res = new SyntheticResponse(rawRes, {});
      res.redirect('/lol/cat');
      expect(rawRes.responseCode).to.equal(999);
    });
  });
});
