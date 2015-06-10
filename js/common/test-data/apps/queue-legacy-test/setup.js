'use strict';
var db = require('org/arangodb').db;
var collectionName = 'test__queue_test_data';
if (db._collection(collectionName)) {
  require('console').warn('Collection already exists:', collectionName);
  db._collection(collectionName).truncate();
} else {
  db._createDocumentCollection(collectionName);
}