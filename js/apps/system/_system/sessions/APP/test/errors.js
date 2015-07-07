/*global require, describe, it */
'use strict';
var expect = require('expect.js');
var errors = require('../errors');

describe('errors', function () {
  ['SessionNotFound', 'SessionExpired'].forEach(function (name) {
    describe(name, function () {
      var SessionError = errors[name];
      it('creates an Error', function () {
        expect(new SessionError()).to.be.an(Error);
      });
      it('has its name', function () {
        var err = new SessionError();
        expect(err.name).to.equal(name);
      });
      it('uses its argument in its message', function () {
        var err = new SessionError('potato');
        expect(err.message).to.contain('potato');
      });
      it('uses its message in its stack trace', function () {
        var err = new SessionError('potato');
        expect(err.stack).to.contain(err.message);
      });
      it('uses its name in its stack trace', function () {
        var err = new SessionError();
        expect(err.stack).to.contain(name);
      });
    });
  });
  describe('SessionExpired', function () {
    it('creates a SessionNotFound', function () {
      expect(new errors.SessionExpired()).to.be.an(errors.SessionNotFound);
    });
  });
});