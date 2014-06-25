/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, es5: true */
/*global require, exports, applicationContext */
(function () {
  'use strict';
  var crypto = require('org/arangodb/crypto');
  var cfg = applicationContext.configuration;

  function verifyPassword(authData, password) {
    if (!authData) {
      authData = {};
    }
    var hashMethod = authData.method || cfg.hashMethod;
    var salt = authData.salt || '';
    var storedHash = authData.hash || '';
    var generatedHash = crypto[hashMethod](salt + password);
    // non-lazy comparison to avoid timing attacks
    return crypto.constantEquals(storedHash, generatedHash);
  }

  function hashPassword(password) {
    var hashMethod = cfg.hashMethod;
    var salt = crypto.genRandomAlphaNumbers(cfg.saltLength);
    var hash = crypto[hashMethod](salt + password);
    return {method: hashMethod, salt: salt, hash: hash};
  }

  exports.verifyPassword = verifyPassword;
  exports.hashPassword = hashPassword;
}());