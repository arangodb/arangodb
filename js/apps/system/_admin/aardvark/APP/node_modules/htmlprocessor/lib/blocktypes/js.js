'use strict';

var path = require('path');
var helpers = require('./helpers');

module.exports = js;

function js(content, block, blockLine, blockContent, filePath) {
  var scripts = [];
  var replacement;

  if (block.inline) {
    scripts = getScripts(block, blockContent, this.options.includeBase || path.dirname(filePath));

    replacement = block.indent + '<script>' + this.linefeed +
                  scripts.join(this.linefeed) +
                  block.indent + '</script>';

    return content.split(blockLine).join(replacement);
  }

  return content.replace(blockLine, block.indent + '<script src="' + block.asset + '"><\/script>');
}

function getScripts(block, html, baseDir) {
  var srcRegEx = /.*src=[\'"]([^\'"]*)[\'"].*/gi;
  return helpers.obtainAssets(srcRegEx, block, html, baseDir);
}
