'use strict';

var path = require('path');
var utils = require('../utils');

module.exports = {
  obtainAssets: obtainAssets
};

function obtainAssets(assetPathRegEx, block, html, baseDir) {
  var assets = [];
  var assetpath, fileContent, match;

  if (block.asset) {
    assetpath = path.join(baseDir, block.asset);
    fileContent = utils.read(assetpath);

    return [fileContent];
  }

  while ((match = assetPathRegEx.exec(html)) !== null) {
    assetpath = path.join(baseDir, match[1]);
    fileContent = utils.read(assetpath);

    assets.push(fileContent);
  }

  return assets;
}
