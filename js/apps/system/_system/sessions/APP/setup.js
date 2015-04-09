/*global applicationContext */
'use strict';
var db = require('org/arangodb').db;
var collectionName = 'sessions';

if (applicationContext.mount.indexOf('/_system/') === 0) {
  collectionName = '_' + collectionName;
} else {
  collectionName = applicationContext.collectionName(collectionName);
}

if (db._collection(collectionName) === null) {
  db._create(collectionName, {isSystem: true});
}
