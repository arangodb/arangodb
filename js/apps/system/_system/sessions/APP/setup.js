'use strict';
var db = require('org/arangodb').db;
var collectionName = '_sessions';

if (db._collection(collectionName) === null) {
  db._create(collectionName, {isSystem: true});
}