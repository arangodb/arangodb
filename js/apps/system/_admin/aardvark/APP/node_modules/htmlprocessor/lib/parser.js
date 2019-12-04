/*
 * node-htmlprocessor
 * https://github.com/dciccale/node-htmlprocessor
 *
 * Copyright (c) 2013-2016 Denis Ciccale (@dciccale)
 * Licensed under the MIT license.
 * https://github.com/dciccale/node-htmlprocessor/blob/master/LICENSE-MIT
 */

'use strict';

var fs = require('fs');

module.exports = Parser;

function Parser(options) {
  this.options = options || {};
  this.listFile = null;

  if (this.options && this.options.list) {
    // Set our log file as a writestream variable with the 'a' flag
    this.listFile = fs.createWriteStream(this.options.list, {
      flags: 'a',
      encoding: 'utf8',
      mode: parseInt('744', 8)
    });
  }

  // Set up regular expressions
  /*
   * <!-- build:<type>[:target] [value] -->
   * - type (required) js, css, attr, remove, template, include
   * - target|attribute (optional) i.e. dev, prod, release or attributes like [href] [src]
   * - value (optional) i.e. script.min.js
  */
  this.regStart = new RegExp('<!--\\s*' + this.options.commentMarker + ':(\\[?[\\w-]+\\]?)(?::([\\w,]+))?(?:\\s*(inline)\\s)?(?:\\s*(scoped)\\s)?(?:\\s*([^\\s]+)\\s*-->)*');

  // <!-- /build -->
  this.regEnd = new RegExp('(?:<!--\\s*)*\\/' + this.options.commentMarker + '\\s*-->');

  // <link rel="stylesheet" href="js/bower_components/bootstrap/dist/css/bootstrap.css" />
  this.cssHref = /^\s*<\s*link.*href=["\']([^"']*)["\'].*$/i;

  // <script src="js/bower_components/xdate/src/xdate.js"></script>
  this.jsSrc = /^\s*<\s*script.*src=["\']([^"']*)["\'].*$/i;
}

Parser.prototype.getBlocks = function (content, filePath) {
  // Normalize line endings and split in lines
  var lines = content.replace(/\r\n/g, '\n').split(/\n/);
  var inside = false;
  var sections = [];
  var block;

  lines.forEach(function (line) {
    var build = line.match(this.regStart);
    var endbuild = this.regEnd.test(line);
    var attr;

    if (build) {
      inside = true;
      attr = build[1].match(/(?:\[([\w\-]+)\])*/)[1];
      block = {
        type: attr ? 'attr': build[1],
        attr: attr,
        targets: !!build[2] ? build[2].split(',') : null,
        inline: !!build[3],
        scoped: !!build[4],
        asset: build[5],
        indent: /^\s*/.exec(line)[0],
        raw: []
      };
    }

    if (inside && block) {
      block.raw.push(line);

      if (this.listFile) {
        this._writeToList(line, filePath);
      }
    }

    if (inside && endbuild) {
      inside = false;
      sections.push(block);
    }
  }, this);

  if (this.listFile) {
    this.listFile.end();
  }

  return sections;

};

Parser.prototype._writeToList = function (line, filePath) {
  var match = this._getMatch(line);
  if (match) {
    this.listFile.write(filePath + ':' + match[1] + '\n');
  }
}

Parser.prototype._getMatch = function (line) {
  var matchCss = line.match(this.cssHref);
  var matchJs = line.match(this.jsSrc);
  return matchCss || matchJs || null;
}
