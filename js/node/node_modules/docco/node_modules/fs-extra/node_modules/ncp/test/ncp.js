

var assert = require('assert'),
    path = require('path'),
    rimraf = require('rimraf'),
    readDirFiles = require('read-dir-files'),
    ncp = require('../').ncp;


var fixtures = path.join(__dirname, 'fixtures'),
    src = path.join(fixtures, 'src'),
    out = path.join(fixtures, 'out');

describe('ncp', function () {
  before(function (cb) {
    rimraf(out, function() {
      ncp(src, out, cb);
    });
  });

  describe('when copying a directory of files', function () {
    it('files are copied correctly', function (cb) {
      readDirFiles(src, 'utf8', function (srcErr, srcFiles) {
        readDirFiles(out, 'utf8', function (outErr, outFiles) {
          assert.ifError(srcErr);
          assert.deepEqual(srcFiles, outFiles);
          cb();
        });
      });
    });
  });

  describe('when copying files using filter', function () {
    before(function (cb) {
      var filter = function(name) {
        return name.substr(name.length - 1) != 'a';
      };
      rimraf(out, function () {
        ncp(src, out, {filter: filter}, cb);
      });
    });

    it('files are copied correctly', function (cb) {
      readDirFiles(src, 'utf8', function (srcErr, srcFiles) {
        function filter(files) {
          for (var fileName in files) {
            var curFile = files[fileName];
            if (curFile instanceof Object)
              return filter(curFile);
            if (fileName.substr(fileName.length - 1) == 'a')
              delete files[fileName];
          }
        }
        filter(srcFiles);
        readDirFiles(out, 'utf8', function (outErr, outFiles) {
          assert.ifError(outErr);
          assert.deepEqual(srcFiles, outFiles);
          cb();
        });
      });
    });
  });

  describe('when using clobber=false', function () {
    it('the copy is completed successfully', function (cb) {
      ncp(src, out, function() {
        ncp(src, out, {clobber: false}, function(err) {
          assert.ifError(err);
          cb();
        });
      });
    });
  });

  describe('when using transform', function () {
    it('file descriptors are passed correctly', function (cb) {
      ncp(src, out, {
         transform: function(read,write,file) {
            assert.notEqual(file.name, undefined);
            assert.strictEqual(typeof file.mode,'number');
            read.pipe(write);
         }
      }, cb);
    });
  });
});
