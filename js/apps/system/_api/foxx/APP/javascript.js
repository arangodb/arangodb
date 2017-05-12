'use strict';
const fs = require('fs');
const joinPath = require('path').join;
const fmu = require('@arangodb/foxx/manager-utils');

module.exports = {
  fromClient (body) {
    const manifest = JSON.stringify({main: 'index.js'}, null, 4);
    const tempPath = fs.getTempFile('uploads', false);
    fs.makeDirectoryRecursive(tempPath);
    fs.writeFileSync(joinPath(tempPath, 'index.js'), body);
    fs.writeFileSync(joinPath(tempPath, 'manifest.json'), manifest);
    const tempFile = fmu.zipDirectory(tempPath);
    const source = fs.readFileSync(tempFile);
    try {
      fs.removeDirectoryRecursive(tempPath, true);
    } catch (e) {
      console.warn('Failed to remove temporary directory for single-file service');
    }
    try {
      fs.remove(tempFile);
    } catch (e) {
      console.warn('Failed to remove temporary bundle for single-file service');
    }
    return {source};
  }
};
