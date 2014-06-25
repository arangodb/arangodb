/*jslint indent: 2, nomen: true, maxlen: 120, vars: true, es5: true */
/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db;
  var usersName = applicationContext.collectionName('users');

  if (db._collection(usersName) === null) {
    db._create(usersName);
  }
}());