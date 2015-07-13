/*global require, describe, it */
'use strict';
var expect = require('expect.js');
var errors = require('../errors');

describe('errors', function () {
  ['UserNotFound', 'UsernameNotAvailable'].forEach(function (name) {
    describe(name, function () {
      var UserError = errors[name];
      it('creates an Error', function () {
        expect(new UserError()).to.be.an(Error);
      });
      it('has its name', function () {
        var err = new UserError();
        expect(err.name).to.equal(name);
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