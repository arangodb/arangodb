'use strict';

const db = require('@arangodb').db;
db['foxx_queue_test'].save({job: true});
