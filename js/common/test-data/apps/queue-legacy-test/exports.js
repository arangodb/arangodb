'use strict';
var joi = require('joi');
var schema = joi.object().required();
var queues = require('org/arangodb/foxx').queues;

queues.registerJobType('queue-legacy-test', {
  schema: schema,
  execute: function (data) {
    var result = schema.validate(data);
    if (result.error) {
      throw result.error;
    }
    return result.data;
  }
});