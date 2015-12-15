/*global describe, it, beforeEach */
(function () {
  'use strict';
  var sinon = require('sinon'),
    expect = require('expect.js'),
    mockuire;

  mockuire = require('mockuire')(module, {
    'js': {compile: function (src) {
      return 'var applicationContext = require("applicationContext");\n' + src;
    }}
  });

  describe('verifyPassword', function () {
    var cfg = {}, crypto = {}, verifyPassword;

    verifyPassword = mockuire('../auth', {
      applicationContext: {configuration: cfg},
      '@arangodb/crypto': crypto
    }).verifyPassword;

    beforeEach(function () {
      cfg.hashMethod = 'bogohash';
      crypto.bogohash = sinon.stub();
      crypto.constantEquals = sinon.stub();
    });

    it('uses the hash method defined by the auth data', function () {
      crypto.rot13 = sinon.spy();
      verifyPassword({method: 'rot13'});
      expect(crypto.rot13.callCount).to.equal(1);
      expect(crypto.bogohash.callCount).to.equal(0);
    });

    it('falls back to the hash method defined by the configuration', function () {
      verifyPassword({});
      expect(crypto.bogohash.callCount).to.equal(1);
      crypto.bogohash.reset();
      verifyPassword();
      expect(crypto.bogohash.callCount).to.equal(1);
    });

    it('prefixes the password with the salt', function () {
      verifyPassword({salt: 'keyboardcat'}, 'secret');
      expect(crypto.bogohash.callCount).to.equal(1);
      expect(crypto.bogohash.args[0]).to.eql(['keyboardcatsecret']);
    });

    it('passes the hashed password and the stored hash to constantEquals', function () {
      crypto.bogohash.returns('deadbeef');
      verifyPassword({hash: '8badf00d'});
      expect(crypto.constantEquals.callCount).to.equal(1);
      expect(crypto.constantEquals.args[0].length).to.equal(2);
      expect(crypto.constantEquals.args[0]).to.contain('deadbeef');
      expect(crypto.constantEquals.args[0]).to.contain('8badf00d');
    });

    it('returns the result of constantEquals', function () {
      crypto.constantEquals.returns('magic');
      var result = verifyPassword();
      expect(result).to.equal('magic');
    });
  });
}());