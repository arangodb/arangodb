/*global applicationContext */
'use strict';
var db = require('org/arangodb').db;
var usersName = applicationContext.collectionName('users');

if (db._collection(usersName) === null) {
  db._create(usersName, {isSystem: true});
}
