'use strict';

var path = require('path');
var url = require('url');

module.exports = attr;

function attr(content, block, blockLine, blockContent) {
  var re = new RegExp('(.*' + block.attr + '=[\'"])([^\'"]*)([\'"].*)', 'gi');
  var replaced = false;

  // Only run attr replacer for the block content
  var replacedBlock = blockContent.replace(re, function (wholeMatch, start, asset, end) {

    // Check if only the path was provided to leave the original asset name intact
    asset = (!path.extname(block.asset) && /\//.test(block.asset)) ? url.resolve(block.asset, path.basename(asset)) : block.asset;

    replaced = true;

    return start + asset + end;
  });

  // If the attribute doesn't exist, add it.
  if (!replaced) {
    replacedBlock = blockContent.replace(/>/, ' ' + block.attr + '="' + block.asset + '">');
  }

  return content.replace(blockLine, replacedBlock);
}
