'use strict';
const db = require('@arangodb').db;
const name = module.context.collectionName('setup_teardown');

db._drop(name);
