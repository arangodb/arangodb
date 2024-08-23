/*eslint curly:0, no-redeclare:0, quotes:0 */
/*jshint ignore:start */
/*eslint-disable */
global.DEFINE_MODULE('path', (function () {
  'use strict'
  /*eslint-enable */

  const module = {};

  function assertPath (path) {
    if (typeof path !== 'string') {
      throw new TypeError(`Path must be a string. Received ${path}`);
    }
  }

  // Copyright Node.js contributors. All rights reserved.
  //
  // Permission is hereby granted, free of charge, to any person obtaining a copy
  // of this software and associated documentation files (the "Software"), to
  // deal in the Software without restriction, including without limitation the
  // rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  // sell copies of the Software, and to permit persons to whom the Software is
  // furnished to do so, subject to the following conditions:
  //
  // The above copyright notice and this permission notice shall be included in
  // all copies or substantial portions of the Software.
  //
  // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  // FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  // IN THE SOFTWARE.

  // resolves . and .. elements in a path array with directory names there
  // must be no slashes or device names (c:\) in the array
  // (so also no leading and trailing slashes - it does not distinguish
  // relative and absolute paths)
  function normalizeArray (parts, allowAboveRoot) {
    var res = [];
    for (var i = 0; i < parts.length; i++) {
      var p = parts[i];

      // ignore empty parts
      if (!p || p === '.')
        continue;

      if (p === '..') {
        if (res.length && res[res.length - 1] !== '..') {
          res.pop();
        } else if (allowAboveRoot) {
          res.push('..');
        }
      } else {
        res.push(p);
      }
    }

    return res;
  }

  // Returns an array with empty elements removed from either end of the input
  // array or the original array if no elements need to be removed
  function trimArray (arr) {
    var lastIndex = arr.length - 1;
    var start = 0;
    for (; start <= lastIndex; start++) {
      if (arr[start])
        break;
    }

    var end = lastIndex;
    for (; end >= 0; end--) {
      if (arr[end])
        break;
    }

    if (start === 0 && end === lastIndex)
      return arr;
    if (start > end)
      return [];
    return arr.slice(start, end + 1);
  }

  // Split a filename into [root, dir, basename, ext], unix version
  // 'root' is just a slash, or nothing.
  const splitPathRe =
  /^(\/?|)([\s\S]*?)((?:\.{1,2}|[^\/]+?|)(\.[^.\/]*|))(?:[\/]*)$/;
  var posix = {};

  function posixSplitPath (filename) {
    return splitPathRe.exec(filename).slice(1);
  }

  // path.resolve([from ...], to)
  // posix version
  posix.resolve = function () {
    var resolvedPath = '',
      resolvedAbsolute = false;

    for (var i = arguments.length - 1; i >= -1 && !resolvedAbsolute; i--) {
      var path = (i >= 0) ? arguments[i] : process.cwd();

      assertPath(path);

      // Skip empty entries
      if (path === '') {
        continue;
      }

      resolvedPath = path + '/' + resolvedPath;
      resolvedAbsolute = path[0] === '/';
    }

    // At this point the path should be resolved to a full absolute path, but
    // handle relative paths to be safe (might happen when process.cwd() fails)

    // Normalize the path
    resolvedPath = normalizeArray(resolvedPath.split('/'),
      !resolvedAbsolute).join('/');

    return ((resolvedAbsolute ? '/' : '') + resolvedPath) || '.';
  };

  // path.normalize(path)
  // posix version
  posix.normalize = function (path) {
    assertPath(path);

    var isAbsolute = posix.isAbsolute(path),
      trailingSlash = path && path[path.length - 1] === '/';

    // Normalize the path
    path = normalizeArray(path.split('/'), !isAbsolute).join('/');

    if (!path && !isAbsolute) {
      path = '.';
    }
    if (path && trailingSlash) {
      path += '/';
    }

    return (isAbsolute ? '/' : '') + path;
  };

  // posix version
  posix.isAbsolute = function (path) {
    assertPath(path);
    return !!path && path[0] === '/';
  };

  // posix version
  posix.join = function () {
    var path = '';
    for (var i = 0; i < arguments.length; i++) {
      var segment = arguments[i];
      assertPath(segment);
      if (segment) {
        if (!path) {
          path += segment;
        } else {
          path += '/' + segment;
        }
      }
    }
    return posix.normalize(path);
  };

  // path.relative(from, to)
  // posix version
  posix.relative = function (from, to) {
    assertPath(from);
    assertPath(to);

    from = posix.resolve(from).substr(1);
    to = posix.resolve(to).substr(1);

    var fromParts = trimArray(from.split('/'));
    var toParts = trimArray(to.split('/'));

    var length = Math.min(fromParts.length, toParts.length);
    var samePartsLength = length;
    for (var i = 0; i < length; i++) {
      if (fromParts[i] !== toParts[i]) {
        samePartsLength = i;
        break;
      }
    }

    var outputParts = [];
    for (var i = samePartsLength; i < fromParts.length; i++) {
      outputParts.push('..');
    }

    outputParts = outputParts.concat(toParts.slice(samePartsLength));

    return outputParts.join('/');
  };

  posix._makeLong = function (path) {
    return path;
  };

  posix.dirname = function (path) {
    var result = posixSplitPath(path),
      root = result[0],
      dir = result[1];

    if (!root && !dir) {
      // No dirname whatsoever
      return '.';
    }

    if (dir) {
      // It has a dirname, strip trailing slash
      dir = dir.substr(0, dir.length - 1);
    }

    return root + dir;
  };

  posix.basename = function (path, ext) {
    if (ext !== undefined && typeof ext !== 'string')
      throw new TypeError('ext must be a string');

    var f = posixSplitPath(path)[2];

    if (ext && f.substr(-1 * ext.length) === ext) {
      f = f.substr(0, f.length - ext.length);
    }
    return f;
  };

  posix.extname = function (path) {
    return posixSplitPath(path)[3];
  };

  posix.format = function (pathObject) {
    if (pathObject === null || typeof pathObject !== 'object') {
      throw new TypeError(
        "Parameter 'pathObject' must be an object, not " + typeof pathObject
      );
    }

    var root = pathObject.root || '';

    if (typeof root !== 'string') {
      throw new TypeError(
        "'pathObject.root' must be a string or undefined, not " +
        typeof pathObject.root
      );
    }

    var dir = pathObject.dir ? pathObject.dir + posix.sep : '';
    var base = pathObject.base || '';
    return dir + base;
  };

  posix.parse = function (pathString) {
    assertPath(pathString);

    var allParts = posixSplitPath(pathString);
    return {
      root: allParts[0],
      dir: allParts[0] + allParts[1].slice(0, -1),
      base: allParts[2],
      ext: allParts[3],
      name: allParts[2].slice(0, allParts[2].length - allParts[3].length)
    };
  };

  posix.sep = '/';
  posix.delimiter = ':';

  module.exports = posix;
  module.exports.posix = posix;

  return module.exports;
}()));
