'use strict';
const db = require('@arangodb').db;
const name = module.context.collectionName('server_security');

if (!db._collection(name)) {
  db._create(name);
}

