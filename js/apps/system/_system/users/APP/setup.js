/*global applicationContext */
'use strict';
var db = require('@arangodb').db;
var usersName = applicationContext.collectionName('users');

if (db._collection(usersName) === null) {
  db._create(usersName, {isSystem: true});
}
