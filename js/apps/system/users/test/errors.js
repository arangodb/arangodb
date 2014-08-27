/*global require, describe, it */
(function () {
  'use strict';
  var expect = require('expect.js'),
    errors = require('../errors');

  describe('errors', function () {
    ['UserNotFound', 'UsernameNotAvailable'].forEach(function (name) {
      describe(name, function () {
        var UserError = errors[name];
        it('is a constructor', function () {
          expect(new UserError()).to.be.a(UserError);
        });
        it('creates an Error', function () {
          expect(new UserError()).to.be.an(Error);
        });
        it('uses its argument in its message', function () {
          var err = new UserError('potato');
          expect(err.message).to.contain('potato');
        });
        it('uses its message in its stack trace', function () {
          var err = new UserError('potato');
          expect(err.stack).to.contain(err.message);
        });
        it('uses its name in its stack trace', function () {
          var err = new UserError();
          expect(err.stack).to.contain(name);
        });
      });
    });
  });
}());