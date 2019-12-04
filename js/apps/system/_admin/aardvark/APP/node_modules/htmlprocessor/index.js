/*
 * node-htmlprocessor
 * https://github.com/dciccale/node-htmlprocessor
 *
 * Copyright (c) 2013-2016 Denis Ciccale (@dciccale)
 * Licensed under the MIT license.
 * https://github.com/dciccale/node-htmlprocessor/blob/master/LICENSE-MIT
 */

'use strict';

var path = require('path');
var fs = require('fs');
var HTMLProcessor = require('./lib/htmlprocessor');
var utils = require('./lib/utils');

module.exports = function (files, options) {
  var html;

  if (options && options.customBlockTypes && options.customBlockTypes.length) {
    options.customBlockTypes = options.customBlockTypes.map(function (processor) {
      return path.resolve(processor);
    });
  }

  // create output directory if needed
  if (files.dest) {
    if (path.extname(files.dest)) {
      utils.mkdir(path.dirname(files.dest));
    } else {
      utils.mkdir(files.dest);
    }
  }

  // create options.list directory if needed
  if (options && options.list) {
    utils.mkdir(path.dirname(options.list));
  }

  html = new HTMLProcessor(options);

  files.src.forEach(function (filePath) {
    var content = html.process(filePath);
    var dest = getOutputPath(filePath);

    fs.writeFileSync(dest, content);
    console.log('File', '"' + dest + '"', 'created.');

    if (options && options.list) {
      console.log('File', '"' + options.list + '"', 'created.');
    }
  });

  function getOutputPath(filePath) {
    var dest = files.dest;
    var ext;

    if (!dest) {
      dest = getFileName(filePath, '.processed');
    } else if (!path.extname(dest)) {
      dest = path.join(dest, getFileName(filePath));
    }

    return dest;
  }

  function getFileName(filePath, postfix) {
    var ext = path.extname(filePath);
    return path.basename(filePath, ext) + (postfix || '') + ext;
  }

  // return processor instance
  return html;
};
