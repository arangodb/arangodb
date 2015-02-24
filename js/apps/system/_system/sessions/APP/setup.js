/*global require, applicationContext */
(function () {
  'use strict';
  var db = require('org/arangodb').db,
      sessionsName;

  if (applicationContext.mount.indexOf('/_system/') === 0) {
    sessionsName = '_sessions';
  } else {
    sessionsName = applicationContext.collectionName('sessions');
  }

  if (db._collection(sessionsName) === null) {
    db._create(sessionsName, {isSystem: true});
  }
}());
