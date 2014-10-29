/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db,
    usersName = applicationContext.collectionName('users');

  if (db._collection(usersName) === null) {
    db._create(usersName, {isSystem: true});
  }
}());
