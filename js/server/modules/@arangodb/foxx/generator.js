'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const inflect = require('i')();
const assert = require('assert');
const internal = require('internal');
const pluck = require('@arangodb/util').pluck;

const TEMPLATES = [
  'main', 'model', 'router', 'setup', 'teardown', 'test'
].reduce(function (obj, name) {
  obj[name] = _.template(fs.read(fs.join(
    internal.startupPath,
    'server', 'modules', '@arangodb', 'foxx', 'templates',
    `${name}.js.tmpl`
  )));
  return obj;
}, {});


exports.generate = function (opts) {
  const names = generateNames(opts.collectionNames);
  const files = [];
  const folders = [];

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
  const main = TEMPLATES.main({routePaths: pluck(names, 'routerFile')});
  files.push({name: 'main.js', content: main});

  folders.push('routes');
  folders.push('models');
  names.forEach(function (names) {
    const router = TEMPLATES.router(names);
    const model = TEMPLATES.model(names);
    files.push({name: fs.join('routes', `${names.routerFile}.js`), content: router});
    files.push({name: fs.join('models', `${names.modelFile}.js`), content: model});
  });

  folders.push('scripts');
  const setup = TEMPLATES.setup({collections: pluck(names, 'collection')});
  files.push({name: fs.join('scripts', 'setup.js'), content: setup});
  const teardown = TEMPLATES.teardown({collections: pluck(names, 'collection')});
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


function generateNames(collectionNames) {
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
