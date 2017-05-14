'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const dd = require('dedent');
const fs = require('fs');
const joinPath = require('path').join;
const inflect = require('i')();
const assert = require('assert');
const arangodb = require('@arangodb');
const ArangoError = arangodb.ArangoError;
const errors = arangodb.errors;

const template = (filename) => _.template(
  fs.readFileSync(joinPath(
    __dirname,
    'templates',
    `${filename}.tmpl`
  ))
);

const TEMPLATES = [
  'main',
  'dcModel',
  'ecModel',
  'dcRouter',
  'ecRouter',
  'setup',
  'teardown',
  'test'
].reduce(function (obj, name) {
  obj[name] = template(`${name}.js`);
  return obj;
}, {readme: template('README.md')});

exports.generate = function (opts) {
  var invalidOptions = [];
  // Set default values:
  opts.documentCollections = opts.documentCollections || [];
  opts.edgeCollections = opts.edgeCollections || [];
  if (typeof opts.name !== 'string') {
    invalidOptions.push('name has to be a string.');
  }
  if (typeof opts.author !== 'string') {
    invalidOptions.push('author has to be a string.');
  }
  if (typeof opts.description !== 'string') {
    invalidOptions.push('description has to be a string.');
  }
  if (typeof opts.license !== 'string') {
    invalidOptions.push('license has to be a string.');
  }
  if (!Array.isArray(opts.documentCollections)) {
    invalidOptions.push('documentCollections has to be an array.');
  }
  if (!Array.isArray(opts.edgeCollections)) {
    invalidOptions.push('edgeCollections has to be an array.');
  }
  if (invalidOptions.length > 0) {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_FOXX_OPTIONS.code,
      errorMessage: dd`
        ${errors.ERROR_INVALID_FOXX_OPTIONS.message}
        Options: ${JSON.stringify(invalidOptions, undefined, 2)}
      `
    });
  }
  const dcNames = generateNames(opts.documentCollections);
  const ecNames = generateNames(opts.edgeCollections);
  const files = [];
  const folders = [];

  for (const names1 of dcNames) {
    for (const names2 of ecNames) {
      assert(
        names1.routerFile !== names2.routerFile,
        `Collection names ${names1.collection} and ${names2.collection} are indistinguishable`
      );
    }
  }

  const manifest = JSON.stringify({
    name: opts.name,
    version: '0.0.0',
    description: opts.description,
    engines: {
      arangodb: '^3.0.0'
    },
    author: opts.author,
    license: opts.license,
    main: 'main.js',
    scripts: {
      setup: 'scripts/setup.js',
      teardown: 'scripts/teardown.js'
    },
    tests: 'test/**/*.js'
  }, null, 4);
  files.push({name: 'manifest.json', content: manifest});

  const main = TEMPLATES.main({routePaths: [].concat(
      dcNames.map(names => names.routerFile),
      ecNames.map(names => names.routerFile)
  )});
  files.push({name: 'main.js', content: main});

  const readme = TEMPLATES.readme(opts);
  files.push({name: 'README.md', content: readme});

  folders.push('routes');
  folders.push('models');
  dcNames.forEach(function (names) {
    const router = TEMPLATES.dcRouter(names);
    const model = TEMPLATES.dcModel(names);
    files.push({name: fs.join('routes', `${names.routerFile}.js`), content: router});
    files.push({name: fs.join('models', `${names.modelFile}.js`), content: model});
  });
  ecNames.forEach(function (names) {
    const router = TEMPLATES.ecRouter(names);
    const model = TEMPLATES.ecModel(names);
    files.push({name: fs.join('routes', `${names.routerFile}.js`), content: router});
    files.push({name: fs.join('models', `${names.modelFile}.js`), content: model});
  });

  folders.push('scripts');
  const setup = TEMPLATES.setup({
    documentCollections: dcNames.map(names => names.collection),
    edgeCollections: ecNames.map(names => names.collection)
  });
  files.push({name: fs.join('scripts', 'setup.js'), content: setup});
  const teardown = TEMPLATES.teardown({collections: [].concat(
      dcNames.map(names => names.collection),
      ecNames.map(names => names.collection)
  )});
  files.push({name: fs.join('scripts', 'teardown.js'), content: teardown});

  const test = TEMPLATES.test({});
  folders.push('test');
  files.push({name: fs.join('test', 'example.js'), content: test});

  return {files, folders};
};

exports.write = function (path, files, folders) {
  fs.makeDirectory(path);
  for (const folder of folders) {
    fs.makeDirectory(fs.join(path, folder));
  }
  for (const file of files) {
    fs.write(fs.join(path, file.name), file.content);
  }
};

function generateNames (collectionNames) {
  return collectionNames.map(function (collectionName, i) {
    const routerFileName = collectionName.toLowerCase();
    collectionNames.forEach(function (next, j) {
      if (i === j) {
        return;
      }
      assert(
        routerFileName !== next.toLowerCase(),
        `Collection names "${collectionName}" and "${next}" are indistinguishable`
      );
    });

    const documentName = inflect.singularize(collectionName);
    let documentsName = collectionName;
    const initial = collectionName.charAt(0);
    assert(
      initial.toLowerCase() !== initial.toUpperCase(),
      `Collection name "${collectionName}" starts with a case-insensitive character`
    );
    if (documentsName === documentName) {
      documentsName += 'Items';
    }
    return {
      collection: collectionName,
      model: initial.toUpperCase() + documentName.slice(1),
      document: initial.toLowerCase() + documentName.slice(1),
      documents: documentsName,
      routerFile: routerFileName,
      modelFile: documentName.toLowerCase()
    };
  });
}
