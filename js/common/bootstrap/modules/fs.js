/* jshint -W051:true */
/* eslint-disable */
global.DEFINE_MODULE('fs', (function () {
  'use strict'
  /* eslint-enable */

  // //////////////////////////////////////////////////////////////////////////////
  // / DISCLAIMER
  // /
  // / Copyright 2016 ArangoDB GmbH, Cologne, Germany
  // / Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
  // /
  // / Licensed under the Apache License, Version 2.0 (the "License")
  // / you may not use this file except in compliance with the License.
  // / You may obtain a copy of the License at
  // /
  // /     http://www.apache.org/licenses/LICENSE-2.0
  // /
  // / Unless required by applicable law or agreed to in writing, software
  // / distributed under the License is distributed on an "AS IS" BASIS,
  // / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  // / See the License for the specific language governing permissions and
  // / limitations under the License.
  // /
  // / Copyright holder is triAGENS GmbH, Cologne, Germany
  // /
  // / @author Dr. Frank Celler
  // /
  // / Parts of the code are based on:
  // /
  // / Copyright Joyent, Inc. and other Node contributors.
  // /
  // / Permission is hereby granted, free of charge, to any person obtaining a
  // / copy of this software and associated documentation files (the
  // / "Software"), to deal in the Software without restriction, including
  // / without limitation the rights to use, copy, modify, merge, publish,
  // / distribute, sublicense, and/or sell copies of the Software, and to permit
  // / persons to whom the Software is furnished to do so, subject to the
  // / following conditions:
  // /
  // / The above copyright notice and this permission notice shall be included
  // / in all copies or substantial portions of the Software.
  // /
  // / THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  // / OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  // / MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
  // / NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
  // / DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
  // / OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
  // / USE OR OTHER DEALINGS IN THE SOFTWARE.
  // //////////////////////////////////////////////////////////////////////////////

  var exports = {};
  var isWindows = require('internal').platform.substr(0, 3) === 'win';

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief pathSeparator
  // //////////////////////////////////////////////////////////////////////////////

  exports.pathSeparator = '/';

  if (global.PATH_SEPARATOR) {
    exports.pathSeparator = global.PATH_SEPARATOR;
    delete global.PATH_SEPARATOR;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief normalizes path parts
  // /
  // / resolves . and .. elements in a path array with directory names there
  // / must be no slashes, empty elements, or device names (c:\) in the array
  // / (so also no leading and trailing slashes - it does not distinguish
  // / relative and absolute paths)
  // //////////////////////////////////////////////////////////////////////////////

  function normalizeArray (parts, allowAboveRoot) {
    // if the path tries to go above the root, `up` ends up > 0
    var up = 0;

    for (var i = parts.length - 1; i >= 0; i--) {
      var last = parts[i];

      if (last === '.') {
        parts.splice(i, 1);
      } else if (last === '..') {
        parts.splice(i, 1);
        up++;
      } else if (up) {
        parts.splice(i, 1);
        up--;
      }
    }

    // if the path is allowed to go above the root, restore leading ..s
    if (allowAboveRoot) {
      for (up; up--; up) {
        parts.unshift('..');
      }
    }

    return parts;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief normalizes a path string
  // //////////////////////////////////////////////////////////////////////////////

  const splitDeviceRe =
  /^([a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/]+[^\\\/]+)?([\\\/])?([\s\S]*?)$/;

  function normalizeUNCRoot (device) {
    return '\\\\' + device.replace(/^[\\\/]+/, '').replace(/[\\\/]+/g, '\\');
  }

  function normalizeWindows (path) {
    var result = splitDeviceRe.exec(path);
    var device = result[1] || '';
    var isUnc = device && device.charAt(1) !== ':';
    var isAbsolute = !!result[2] || isUnc; // UNC paths are always absolute
    var tail = result[3];
    var trailingSlash = /[\\\/]$/.test(tail);

    // If device is a drive letter, we'll normalize to lower case.
    if (device && device.charAt(1) === ':') {
      device = device[0].toLowerCase() + device.substr(1);
    }

    // Normalize the tail path
    tail = normalizeArray(tail.split(/[\\\/]+/).filter(function (p) {
      return !!p;
    }), !isAbsolute).join('\\');

    if (!tail && !isAbsolute) {
      tail = '.';
    }
    if (tail && trailingSlash) {
      tail += '\\';
    }

    // Convert slashes to backslashes when `device` points to an UNC root.
    // Also squash multiple slashes into a single one where appropriate.

    if (isUnc) {
      device = normalizeUNCRoot(device);
    }

    return device + (isAbsolute ? '\\' : '') + tail;
  }

  function normalizePosix (path) {
    var isAbsolute = path.charAt(0) === '/';
    var trailingSlash = path.substr(-1) === '/';

    // Normalize the path
    path = normalizeArray(path.split('/').filter(function (p) {
      return !!p;
    }), !isAbsolute).join('/');

    if (!path && !isAbsolute) {
      path = '.';
    }
    if (path && trailingSlash) {
      path += '/';
    }

    return (isAbsolute ? '/' : '') + path;
  }

  var normalize = isWindows ? normalizeWindows : normalizePosix;

  exports.normalize = normalize;

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief exists
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_EXISTS) {
    exports.exists = global.FS_EXISTS;
    delete global.FS_EXISTS;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief chmod
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_CHMOD) {
    exports.chmod = global.FS_CHMOD;
    delete global.FS_CHMOD;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief getTempFile
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_GET_TEMP_FILE) {
    exports.getTempFile = global.FS_GET_TEMP_FILE;
    delete global.FS_GET_TEMP_FILE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief getTempPath
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_GET_TEMP_PATH) {
    exports.getTempPath = global.FS_GET_TEMP_PATH;
    delete global.FS_GET_TEMP_PATH;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief home
  // //////////////////////////////////////////////////////////////////////////////

  var homeDirectory = '';

  if (global.HOME) {
    homeDirectory = global.HOME;
    delete global.HOME;
  }

  exports.home = function () {
    return homeDirectory;
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief isDirectory
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_IS_DIRECTORY) {
    exports.isDirectory = global.FS_IS_DIRECTORY;
    delete global.FS_IS_DIRECTORY;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief isFile
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_IS_FILE) {
    exports.isFile = global.FS_IS_FILE;
    delete global.FS_IS_FILE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief makeAbsolute
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_MAKE_ABSOLUTE) {
    exports.makeAbsolute = global.FS_MAKE_ABSOLUTE;
    delete global.FS_MAKE_ABSOLUTE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief join
  // //////////////////////////////////////////////////////////////////////////////

  if (isWindows) {
    exports.join = function () {
      function f (p) {
        if (typeof p !== 'string') {
          throw new TypeError('Arguments to path.join must be strings');
        }
        return p;
      }

      var paths = Array.prototype.filter.call(arguments, f);
      var joined = paths.join('\\');

      // Make sure that the joined path doesn't start with two slashes, because
      // normalize() will mistake it for an UNC path then.
      //
      // This step is skipped when it is very clear that the user actually
      // intended to point at an UNC path. This is assumed when the first
      // non-empty string arguments starts with exactly two slashes followed by
      // at least one more non-slash character.
      //
      // Note that for normalize() to treat a path as an UNC path it needs to
      // have at least 2 components, so we don't filter for that here.
      // This means that the user can use join to construct UNC paths from
      // a server name and a share name; for example:
      //   path.join('//server', 'share') -> '\\\\server\\share\')

      if (!/^[\\\/]{2}[^\\\/]/.test(paths[0])) {
        joined = joined.replace(/^[\\\/]{2,}/, '\\');
      }

      return normalize(joined);
    };
  } else {
    exports.join = function () {
      var paths = Array.prototype.slice.call(arguments, 0);

      return normalize(paths.filter(function (p) {
        if (typeof p !== 'string') {
          throw new TypeError('Arguments to path.join must be strings - have ' + JSON.stringify(p));
        }

        return p;
      }).join('/'));
    };
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief safe-join
  // //////////////////////////////////////////////////////////////////////////////

  var safeJoin;

  if (isWindows) {
    safeJoin = function (base, relative) {
      base = normalize(base + '/');
      var path = normalizeArray(relative.split(/[\\\/]+/), false).join('/');

      return base + path;
    };
  } else {
    safeJoin = function (base, relative) {
      base = normalize(base + '/');
      var path = normalizeArray(relative.split('/'), false).join('/');

      return base + path;
    };
  }

  exports.safeJoin = function () {
    var args = Array.prototype.slice.call(arguments);
    return args.reduce(function (base, relative) {
      return safeJoin(base, relative);
    }, args.shift());
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief list
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_LIST) {
    exports.list = global.FS_LIST;
    delete global.FS_LIST;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief listTree
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_LIST_TREE) {
    exports.listTree = global.FS_LIST_TREE;
    delete global.FS_LIST_TREE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief makeDirectory
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_MAKE_DIRECTORY) {
    exports.makeDirectory = global.FS_MAKE_DIRECTORY;
    delete global.FS_MAKE_DIRECTORY;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief makeDirectoryRecursive
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_MAKE_DIRECTORY_RECURSIVE) {
    exports.makeDirectoryRecursive = global.FS_MAKE_DIRECTORY_RECURSIVE;
    delete global.FS_MAKE_DIRECTORY_RECURSIVE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief mtime
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_MTIME) {
    exports.mtime = global.FS_MTIME;
    delete global.FS_MTIME;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief copy one file
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_LINK_FILE) {
    exports.linkFile = global.FS_LINK_FILE;
    delete global.FS_LINK_FILE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief copy one file
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_COPY_FILE) {
    exports.copyFile = global.FS_COPY_FILE;
    delete global.FS_COPY_FILE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief copy recursive
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_COPY_RECURSIVE) {
    exports.copyRecursive = global.FS_COPY_RECURSIVE;
    delete global.FS_COPY_RECURSIVE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief move
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_MOVE) {
    var move = global.FS_MOVE;
    // File system moving directories fallback function
    exports.move = function (source, target) {
      try {
        return move(source, target);
      } catch (x) {
        if (x.errorNum === 2 /* errors.ERROR_SYS_ERROR.code */) {
          if (exports.isDirectory(source)) {
            try {
              exports.copyRecursive(source, target);
              // yes, this only works from the temporary dir // or maybe not? -ap
              try {
                exports.removeDirectoryRecursive(source, true);
              } catch (z) {
                require('console').log(`failed to remove source directory: ${z.stack || z}`);
              }
            } catch (y) {
              try {
                // try to cleanup the mess we created...
                // if it doesn't work out... oh well...
                exports.removeDirectoryRecursive(target, true);
              } catch (v) {
                require('console').log(`failed to clean up target directory: ${v.stack || v}`);
              }
              throw y;
            }
          } else {
            exports.copyFile(source, target);
            try {
              exports.remove(source);
            } catch (e) {
              require('console').log(`failed to remove source directory: ${e.stack || e}`);
            }
          }
        } else {
          throw x;
        }
      }
      return true;
    };
    delete global.FS_MOVE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief read
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_READ) {
    exports.read = global.SYS_READ;
    delete global.SYS_READ;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief readBuffer and readFileSync
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_READ_BUFFER) {
    exports.readBuffer = global.SYS_READ_BUFFER;
    delete global.SYS_READ_BUFFER;

    exports.readFileSync = function (filename, encoding) {
      var buf = exports.readBuffer(filename);
      if (!encoding) {
        return buf;
      }
      if (encoding.encoding) {
        encoding = encoding.encoding;
      }
      return buf.toString(encoding);
    };
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief readGzip
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_READ_GZIP) {
    exports.readGzip = global.SYS_READ_GZIP;
    delete global.SYS_READ_GZIP;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief readDecrypt
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_READ_DECRYPT) {
    exports.readDecrypt = global.SYS_READ_DECRYPT;
    delete global.SYS_READ_DECRYPT;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief read64
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_READ64) {
    exports.read64 = global.SYS_READ64;
    delete global.SYS_READ64;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief remove
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_REMOVE) {
    exports.remove = global.FS_REMOVE;
    delete global.FS_REMOVE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief removeDirectory
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_REMOVE_DIRECTORY) {
    exports.removeDirectory = global.FS_REMOVE_DIRECTORY;
    delete global.FS_REMOVE_DIRECTORY;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief removeDirectoryRecursive
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_REMOVE_RECURSIVE_DIRECTORY) {
    exports.removeDirectoryRecursive = global.FS_REMOVE_RECURSIVE_DIRECTORY;
    delete global.FS_REMOVE_RECURSIVE_DIRECTORY;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief size
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_FILESIZE) {
    exports.size = global.FS_FILESIZE;
    delete global.FS_FILESIZE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief adler32
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_ADLER32) {
    exports.adler32 = global.FS_ADLER32;
    delete global.FS_ADLER32;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief unzipFile
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_UNZIP_FILE) {
    exports.unzipFile = global.FS_UNZIP_FILE;
    delete global.FS_UNZIP_FILE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief append
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_APPEND) {
    exports.append = global.SYS_APPEND;
    delete global.SYS_APPEND;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief write
  // //////////////////////////////////////////////////////////////////////////////

  if (global.SYS_WRITE) {
    exports.write = global.SYS_WRITE;
    delete global.SYS_WRITE;
  }

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief writeFileSync (node compatibility)
  // //////////////////////////////////////////////////////////////////////////////

  exports.writeFileSync = function (filename, content) {
    return exports.write(filename, content);
  };

  // //////////////////////////////////////////////////////////////////////////////
  // / @brief zipFile
  // //////////////////////////////////////////////////////////////////////////////

  if (global.FS_ZIP_FILE) {
    exports.zipFile = global.FS_ZIP_FILE;
    delete global.FS_ZIP_FILE;
  }

  exports.escapePath = function (s) { return s.replace(/\\/g,'\\\\'); };

  return exports;
}()));
