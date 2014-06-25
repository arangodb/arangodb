/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, es5: true */
/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db;
  var sessionsName = applicationContext.collectionName('sessions');

  if (db._collection(sessionsName) === null) {
    db._create(sessionsName);
  }
}());