/*global applicationContext */
'use strict';
var crypto = require('org/arangodb/crypto'),
  cfg = applicationContext.configuration;

function verifyPassword(authData, password) {
  if (!authData) {
    authData = {};
  }
  var hashMethod = authData.method || cfg.hashMethod,
    salt = authData.salt || '',
    storedHash = authData.hash || '',
    generatedHash = crypto[hashMethod](salt + password);
  // non-lazy comparison to avoid timing attacks
  return crypto.constantEquals(storedHash, generatedHash);
}

function hashPassword(password) {
  var hashMethod = cfg.hashMethod,
    salt = crypto.genRandomAlphaNumbers(cfg.saltLength),
    hash = crypto[hashMethod](salt + password);
  return {method: hashMethod, salt: salt, hash: hash};
}

exports.verifyPassword = verifyPassword;
exports.hashPassword = hashPassword;
