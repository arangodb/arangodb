/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db,
    sessionsName = applicationContext.collectionName('sessions');

  if (db._collection(sessionsName) === null) {
    db._create(sessionsName, {isSystem: true});
  }
}());