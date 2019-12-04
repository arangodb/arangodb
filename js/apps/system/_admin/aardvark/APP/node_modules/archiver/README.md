# Archiver v1.3.0

[![Build Status](https://travis-ci.org/archiverjs/node-archiver.svg?branch=master)](https://travis-ci.org/archiverjs/node-archiver) [![Build status](https://ci.appveyor.com/api/projects/status/38kqu3yp159nodxe/branch/master?svg=true)](https://ci.appveyor.com/project/ctalkington/node-archiver/branch/master)

a streaming interface for archive generation

Visit the [API documentation](http://archiverjs.com/docs) for a list of all methods available.

## Install

```bash
npm install archiver --save
```

## Quick Start

```js
// require modules
var fs = require('fs');
var archiver = require('archiver');

// create a file to stream archive data to.
var output = fs.createWriteStream(__dirname + '/example.zip');
var archive = archiver('zip', {
    store: true // Sets the compression method to STORE.
});

// listen for all archive data to be written
output.on('close', function() {
  console.log(archive.pointer() + ' total bytes');
  console.log('archiver has been finalized and the output file descriptor has closed.');
});

// good practice to catch this error explicitly
archive.on('error', function(err) {
  throw err;
});

// pipe archive data to the file
archive.pipe(output);

// append a file from stream
var file1 = __dirname + '/file1.txt';
archive.append(fs.createReadStream(file1), { name: 'file1.txt' });

// append a file from string
archive.append('string cheese!', { name: 'file2.txt' });

// append a file from buffer
var buffer3 = new Buffer('buff it!');
archive.append(buffer3, { name: 'file3.txt' });

// append a file
archive.file('file1.txt', { name: 'file4.txt' });

// append files from a directory
archive.directory('subdir/');

// append files from a glob pattern
archive.glob('subdir/*.txt');

// finalize the archive (ie we are done appending files but streams have to finish yet)
archive.finalize();
```

## Formats

Archiver ships with out of the box support for TAR and ZIP archives.

You can register additional formats with `registerFormat`.

_Formats will be changing in the next few releases to implement a middleware approach._