/*jslint indent: 2, nomen: true, maxlen: 120 */
/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db,
    sessionsName = applicationContext.collectionName('sessions');

  if (db._collection(sessionsName) === null) {
    db._create(sessionsName, {isSystem: true});
  }
}());