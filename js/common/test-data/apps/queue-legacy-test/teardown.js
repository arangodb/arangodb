'use strict';
var db = require('@arangodb').db;
var collectionName = 'test__queue_test_data';
if (db._collection(collectionName)) {
  db._collection(collectionName).drop();
}