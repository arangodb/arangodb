'use strict';
const db = require('@arangodb').db;
const name = module.context.collectionName('server_security');

db._drop(name);
