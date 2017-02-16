'use strict';
const joi = require('joi');
const configTypes = require('@arangodb/foxx/manifest').configTypes;

exports.mount = joi.string().regex(/(?:\/[-_0-9a-z]+)+/i).required()
.description(`Mount path relative to the database root`);

exports.flag = joi.alternatives().try(
  joi.boolean().description(`Boolean flag`),
  joi.number().integer().min(0).max(1).description(`Numeric flag for PHP compatibility`)
).default(false);

exports.shortInfo = joi.object({
  mount: exports.mount.description(`Mount path of the service`),
  name: joi.string().optional().description(`Name of the service`),
  version: joi.string().optional().description(`Version of the service`),
  development: joi.boolean().default(false).description(`Whether development mode is enabled`),
  legacy: joi.boolean().default(false).description(`Whether the service is running in legacy mode`)
}).required();

exports.fullInfo = exports.shortInfo.keys({
  path: joi.string().required().description(`File system path of the service`),
  manifest: joi.object().required().description(`Full manifest of the service`),
  options: joi.object().required().description(`Configuration and dependency option values`)
});

exports.configs = joi.object().pattern(/.+/, joi.object({
  value: joi.any().optional().description(`Current value of the configuration option`),
  default: joi.any().optional().description(`Default value of the configuration option`),
  type: joi.only(Object.keys(configTypes)).default('string').description(`Type of the configuration option`),
  description: joi.string().optional().description(`Human-readable description of the configuration option`),
  required: joi.boolean().default(true).description(`Whether the configuration option is required`)
}).required().description(`Configuration option`)).required();

exports.deps = joi.object().pattern(/.+/, joi.object({
  name: joi.string().default('*').description(`Name of the dependency`),
  version: joi.string().default('*').description(`Version of the dependency`),
  required: joi.boolean().default(true).description(`Whether the dependency is required`)
}).required().description(`Dependency option`)).required();

exports.service = joi.object({
  source: joi.alternatives(
    joi.string().description(`Local file path or URL of the service to be installed`),
    joi.object().type(Buffer).description(`Zip bundle of the service to be installed`)
  ).required().description(`Local file path, URL or zip bundle of the service to be installed`),
  configuration: joi.string().optional().description(`Configuration to use for the service (JSON)`),
  dependencies: joi.string().optional().description(`Dependency options to use for the service (JSON)`)
}).required();
