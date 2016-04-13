'use strict';
const db = require('@arangodb').db;
const collections = ['_sessions', '_users'];

collections.forEach(name => {
  if (!db._collection(name)) {
    db._createDocumentCollection(name, {isSystem: true});
  }
});
