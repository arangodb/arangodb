'use strict';
const dd = require('dedent');
const fs = require('fs');
const joi = require('joi');
const semver = require('semver');
const util = require('util');
const joinPath = require('path').join;
const {
  plainServerVersion,
  ArangoError,
  errors: {
    ERROR_INVALID_SERVICE_MANIFEST,
    ERROR_SERVICE_MANIFEST_NOT_FOUND,
    ERROR_MALFORMED_MANIFEST_FILE
  }
} = require('@arangodb');
const il = require('@arangodb/util').inline;

const CANONICAL_SCHEMA = 'http://json.schemastore.org/foxx-manifest';
exports.schemaUrl = CANONICAL_SCHEMA;

// Regular expressions for joi patterns
const RE_EMPTY = /^$/;
const RE_NOT_EMPTY = /./;
const RE_KEY = /^[_$a-z][-_$a-z0-9]*$/i;
const RE_PKG = /^@[_$a-z][-_$a-z0-9]*(\/[_$a-z][-_$a-z0-9]*)*$/i;

const legacyManifestFields = [
  'assets',
  'controllers',
  'exports',
  'isSystem'
];

const configTypes = {
  integer: joi.number().integer(),
  boolean: joi.boolean(),
  string: joi.string(),
  number: joi.number(),
  json (v) {
    return v && JSON.parse(v);
  }
};
configTypes.password = configTypes.string;
configTypes.int = configTypes.integer;
configTypes.bool = configTypes.boolean;

const manifestSchema = {
  $schema: joi.only(CANONICAL_SCHEMA).default(CANONICAL_SCHEMA),
  // FoxxStore metadata
  name: joi.string().regex(/^[-_a-z][-_a-z0-9]*$/i).optional(),
  version: joi.string().optional(),
  keywords: joi.array().optional(),
  license: joi.string().optional(),
  repository: (
  joi.object().optional()
    .keys({
      type: joi.string().required(),
      url: joi.string().required()
    })
  ),

  // Additional web interface metadata
  author: joi.string().allow('').default(''),
  contributors: joi.array().optional(),
  description: joi.string().allow('').default(''),
  thumbnail: joi.string().optional(),

  // Compatibility
  engines: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.string().required())
  ),

  // Index redirect
  defaultDocument: joi.string().allow('').optional(),

  // JS path
  lib: joi.string().default('.'),

  // Entrypoint
  main: joi.string().optional(),

  // Config
  configuration: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, (
      joi.object().required()
        .keys({
          default: joi.any().optional(),
          type: (
          joi.only(Object.keys(configTypes))
            .default('string')
          ),
          description: joi.string().optional(),
          required: joi.boolean().default(true)
        })
      ))
  ),

  // Dependencies supported
  dependencies: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.alternatives().try(
      joi.string().required(),
      joi.object().required()
        .keys({
          name: joi.string().default('*'),
          version: joi.string().default('*'),
          description: joi.string().optional(),
          required: joi.boolean().default(true),
          multiple: joi.boolean().default(false)
        })
    ))
  ),

  // Dependencies provided
  provides: (
  joi.alternatives().try(
    joi.string().optional(),
    joi.array().optional()
      .items(joi.string().required()),
    joi.object().optional()
      .pattern(RE_EMPTY, joi.forbidden())
      .pattern(RE_NOT_EMPTY, joi.string().required())
  )
  ),

  // Bundled assets
  files: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.alternatives().try(
      joi.string().required(),
      joi.object().required()
        .keys({
          path: joi.string().required(),
          gzip: joi.boolean().optional(),
          type: joi.string().optional()
        })
    ))
  ),

  // Scripts/queue jobs
  scripts: (
  joi.object().optional()
    .pattern(RE_EMPTY, joi.forbidden())
    .pattern(RE_NOT_EMPTY, joi.string().required())
    .default(Object, 'empty scripts object')
  ),

  // Foxx tests path
  tests: (
  joi.alternatives()
    .try(
      joi.string().required(),
      (
      joi.array().optional()
        .items(joi.string().required())
        .default(Array, 'empty test files array')
      )
  )
  )
};

function checkManifest (inputManifest, mount, complainAboutVersionMismatches) {
  const serverVersion = plainServerVersion();
  const errors = [];
  const manifest = {};
  let legacy = false;

  Object.keys(manifestSchema).forEach(function (key) {
    const schema = manifestSchema[key];
    const value = inputManifest[key];
    const result = joi.validate(value, schema);
    if (result.error) {
      if (key === '$schema') {
        manifest[key] = CANONICAL_SCHEMA;
        console.warnLines(il`
          Service at "${mount}" specifies manifest field "$schema"
          with invalid value "${util.format(value)}".
          Did you mean "${CANONICAL_SCHEMA}"?
        `);
      } else {
        const error = result.error.message.replace(/^"value"/, 'Value');
        errors.push(il`
          Service at "${mount}" specifies manifest field "${key}"
          with invalid value "${util.format(value)}": ${error}
        `);
      }
    } else {
      manifest[key] = result.value;
    }
  });

  if (manifest.engines && manifest.engines.arangodb) {
    if (semver.gtr('3.0.0', manifest.engines.arangodb)) {
      legacy = true;
      if (complainAboutVersionMismatches) {
        console.infoLines(il`
          Service at "${mount}" expects version "${manifest.engines.arangodb}"
          and will run in legacy compatibility mode.
        `);
      }
    } else if (!semver.satisfies(serverVersion, manifest.engines.arangodb)) {
      if (complainAboutVersionMismatches) {
        console.warnLines(il`
          Service at "${mount}" expects version "${manifest.engines.arangodb}"
          which is likely incompatible with installed version "${serverVersion}".
        `);
      }
    }
  }

  for (const key of Object.keys(inputManifest)) {
    if (manifestSchema[key]) {
      continue;
    }
    manifest[key] = inputManifest[key];
    if (key === 'engine' && !inputManifest.engines) {
      console.warnLines(il`
        Service at "${mount}" specifies unknown manifest field "engine".
        Did you mean "engines"?
      `);
    } else if (!legacy || legacyManifestFields.indexOf(key) === -1) {
      console.warnLines(il`
        Service at "${mount}" specifies unknown manifest field "${key}".
      `);
    }
  }

  if (manifest.version && !semver.valid(manifest.version)) {
    console.warnLines(il`
      Service at "${mount}" specifies manifest field "version"
      with invalid value "${manifest.version}".
    `);
  }

  if (manifest.provides) {
    if (typeof manifest.provides === 'string') {
      manifest.provides = [manifest.provides];
    }
    if (Array.isArray(manifest.provides)) {
      const provides = manifest.provides;
      manifest.provides = {};
      for (const provided of provides) {
        const tokens = provided.split(':');
        manifest.provides[tokens[0]] = tokens[1] || '*';
      }
    }

    for (const name of Object.keys(manifest.provides)) {
      if (!name.match(RE_KEY) && !name.match(RE_PKG)) {
        console.warnLines(il`
          Service at "${mount}" specifies manifest field "provides"
          with invalid key "${name}" which will no longer be supported in ArangoDB 4.
        `);
      }

      const version = manifest.provides[name];
      if (!semver.valid(version)) {
        errors.push(il`
          Service at "${mount}" specifies manifest field "provides"
          with "${name}" set to invalid value "${version}".
        `);
      }
    }
  }

  if (manifest.configuration) {
    for (const key of Object.keys(manifest.configuration)) {
      if (!key.match(RE_KEY)) {
        console.warnLines(il`
          Service at "${mount}" specifies manifest field "configuration"
          with invalid key "${key}" which will no longer be supported in ArangoDB 4.
        `);
      }
    }
  }

  if (manifest.dependencies) {
    for (const key of Object.keys(manifest.dependencies)) {
      if (!key.match(RE_KEY)) {
        console.warnLines(il`
          Service at "${mount}" specifies manifest field "dependencies"
          with invalid key "${key}" which will no longer be supported in ArangoDB 4.
        `);
      }

      if (typeof manifest.dependencies[key] === 'string') {
        const tokens = manifest.dependencies[key].split(':');
        manifest.dependencies[key] = {
          name: tokens[0] || '*',
          version: tokens[1] || '*',
          required: true
        };
      }

      const name = manifest.dependencies[key].name;
      if (name !== '*' && !name.match(RE_KEY) && !name.match(RE_PKG)) {
        console.warnLines(il`
          Service at "${mount}" specifies manifest field "dependencies"
          with "${key}" set to invalid name "${name}"
          which will no longer be supported in ArangoDB 4.
        `);
      }

      const version = manifest.dependencies[key].version;
      if (!semver.validRange(version)) {
        errors.push(il`
          Service at "${mount}" specifies manifest field "dependencies"
          with "${key}" set to invalid version "${version}".
        `);
      }
    }
  }

  if (errors.length) {
    for (const error of errors) {
      console.errorLines(error);
    }
    throw new ArangoError({
      errorNum: ERROR_INVALID_SERVICE_MANIFEST.code,
      errorMessage: dd`
        ${ERROR_INVALID_SERVICE_MANIFEST.message}
        Manifest for service at "${mount}":
        ${errors.join('\n')}
      `
    });
  }

  if (typeof manifest.tests === 'string') {
    manifest.tests = [manifest.tests];
  }

  if (legacy) {
    if (manifest.defaultDocument === undefined) {
      manifest.defaultDocument = 'index.html';
    }

    if (typeof manifest.controllers === 'string') {
      manifest.controllers = {'/': manifest.controllers};
    }
  } else if (manifest.lib) {
    const base = manifest.lib;
    delete manifest.lib;
    if (manifest.main) {
      manifest.main = joinPath(base, manifest.main);
    }
    if (manifest.tests) {
      manifest.tests = manifest.tests.map((path) => joinPath(base, path));
    }
    for (const key of Object.keys(manifest.scripts)) {
      manifest.scripts[key] = joinPath(base, manifest.scripts[key]);
    }
  }

  return manifest;
}

function validateManifestFile (filename, mount, complainAboutVersionMismatches) {
  let mf;
  if (!fs.exists(filename)) {
    throw new ArangoError({
      errorNum: ERROR_SERVICE_MANIFEST_NOT_FOUND.code,
      errorMessage: dd`
        ${ERROR_SERVICE_MANIFEST_NOT_FOUND.message}
        File: ${filename}
      `
    });
  }
  try {
    mf = JSON.parse(fs.read(filename));
  } catch (e) {
    throw Object.assign(
      new ArangoError({
        errorNum: ERROR_MALFORMED_MANIFEST_FILE.code,
        errorMessage: dd`
          ${ERROR_MALFORMED_MANIFEST_FILE.message}
          File: ${filename}
        `
      }), {cause: e}
    );
  }
  mf = checkManifest(mf, mount, complainAboutVersionMismatches);
  return mf;
}

exports.configTypes = configTypes;
exports.validate = validateManifestFile;
exports.validateJson = checkManifest;
