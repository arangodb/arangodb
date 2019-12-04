'use strict';

var utils = require('../utils');

module.exports = template;

function template(content, block, blockLine, blockContent) {
  var compiledTmpl = utils._.template(blockContent, this.data, this.options.templateSettings);

  // Clean template output and fix indent
  compiledTmpl = block.indent + compiledTmpl.trim().replace(/(\r\n|\n)\s*/g, '$1' + block.indent);

  return content.replace(blockLine, compiledTmpl.replace(/\$/g, '$$$'));
}
