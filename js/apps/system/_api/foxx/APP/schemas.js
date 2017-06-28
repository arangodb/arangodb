'use strict';
const joi = require('joi');
const configTypes = require('@arangodb/foxx/manifest').configTypes;

exports.mount = joi.string().regex(/(?:\/[-_0-9a-z]+)+/i).required();

exports.flag = joi.alternatives().try(
  joi.boolean(),
  joi.number().integer().min(0).max(1)
).default(false);

exports.shortInfo = joi.object({
  mount: exports.mount,
  name: joi.string().optional(),
  version: joi.string().optional(),
  development: joi.boolean().default(false),
  legacy: joi.boolean().default(false)
}).required();

exports.fullInfo = exports.shortInfo.keys({
  path: joi.string().required(),
  manifest: joi.object().required(),
  options: joi.object().required()
});

exports.configs = joi.object().pattern(/.+/, joi.object({
  value: joi.any().optional(),
  default: joi.any().optional(),
  type: joi.only(Object.keys(configTypes)).default('string'),
  description: joi.string().optional(),
  required: joi.boolean().default(true)
}).required()).required();

exports.deps = joi.object().pattern(/.+/, joi.object({
  name: joi.string().default('*'),
  version: joi.string().default('*'),
  required: joi.boolean().default(true)
}).required()).required();

exports.service = joi.alternatives(
  joi.object().type(Buffer).required(),
  joi.object({
    source: joi.alternatives(
      joi.string(),
      joi.object().type(Buffer)
    ).required(),
    configuration: joi.object().optional(),
    dependencies: joi.object().optional()
  }).required()
);
