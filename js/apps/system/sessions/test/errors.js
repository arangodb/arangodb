/*global require, describe, it */
(function () {
  'use strict';
  var expect = require('expect.js'),
    errors = require('../errors');

  describe('errors', function () {
    ['SessionNotFound', 'SessionExpired'].forEach(function (name) {
      describe(name, function () {
        var SessionError = errors[name];
        it('is a constructor', function () {
          expect(new SessionError()).to.be.a(SessionError);
        });
        it('creates an Error', function () {
          expect(new SessionError()).to.be.an(Error);
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
  });
}());