/*jslint indent: 2, maxlen: 120, vars: true, white: true, plusplus: true, regexp: true, nonpropdel: true, sloppy: true */
/*global require, FS_MAKE_DIRECTORY, FS_MOVE, FS_REMOVE, FS_REMOVE_DIRECTORY, FS_LIST,
  FS_REMOVE_RECURSIVE_DIRECTORY, FS_EXISTS, FS_IS_DIRECTORY, FS_IS_FILE, FS_FILESIZE, 
  FS_GET_TEMP_FILE, FS_GET_TEMP_PATH, FS_LIST_TREE, FS_UNZIP_FILE, FS_ZIP_FILE,
  SYS_READ, SYS_READ64, SYS_SAVE, PATH_SEPARATOR, HOME */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "js"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
///
/// Parts of the code are based on:
///
/// Copyright Joyent, Inc. and other Node contributors.
///
/// Permission is hereby granted, free of charge, to any person obtaining a
/// copy of this software and associated documentation files (the
/// "Software"), to deal in the Software without restriction, including
/// without limitation the rights to use, copy, modify, merge, publish,
/// distribute, sublicense, and/or sell copies of the Software, and to permit
/// persons to whom the Software is furnished to do so, subject to the
/// following conditions:
///
/// The above copyright notice and this permission notice shall be included
/// in all copies or substantial portions of the Software.
///
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
/// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
/// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
/// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
/// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
/// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
/// USE OR OTHER DEALINGS IN THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

(function () {
  // cannot use strict here as we are going to delete globals

  var exports = require("fs");
  var isWindows = require("internal").platform === 'win32';

// -----------------------------------------------------------------------------
// --SECTION--                                                       Module "fs"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief pathSeparator
////////////////////////////////////////////////////////////////////////////////

  exports.pathSeparator = "/";

  if (typeof PATH_SEPARATOR !== "undefined") {
    exports.pathSeparator = PATH_SEPARATOR;
    delete PATH_SEPARATOR;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes path parts
///
/// resolves . and .. elements in a path array with directory names there
/// must be no slashes, empty elements, or device names (c:\) in the array
/// (so also no leading and trailing slashes - it does not distinguish
/// relative and absolute paths)
////////////////////////////////////////////////////////////////////////////////

  function normalizeArray (parts, allowAboveRoot) {
    'use strict';

    var i;

    // if the path tries to go above the root, `up` ends up > 0
    var up = 0;

    for (i = parts.length - 1; i >= 0; i--) {
      var last = parts[i];

      if (last === '.') {
        parts.splice(i, 1);
      }
      else if (last === '..') {
        parts.splice(i, 1);
        up++;
      }
      else if (up) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a path string
////////////////////////////////////////////////////////////////////////////////

  var splitDeviceRe =
    /^([a-zA-Z]:|[\\\/]{2}[^\\\/]+[\\\/]+[^\\\/]+)?([\\\/])?([\s\S]*?)$/;

  function normalizeUNCRoot (device) {
    'use strict';

    return '\\\\' + device.replace(/^[\\\/]+/, '').replace(/[\\\/]+/g, '\\');
  }

  function normalizeWindows (path) {
    'use strict';

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
    tail = normalizeArray(tail.split(/[\\\/]+/).filter(function(p) {
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
    'use strict';

    var isAbsolute = path.charAt(0) === '/';
    var trailingSlash = path.substr(-1) === '/';

    // Normalize the path
    path = normalizeArray(path.split('/').filter(function(p) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief exists
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_EXISTS !== "undefined") {
    exports.exists = FS_EXISTS;
    delete FS_EXISTS;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief getTempFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_GET_TEMP_FILE !== "undefined") {
    exports.getTempFile = FS_GET_TEMP_FILE;
    delete FS_GET_TEMP_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief getTempPath
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_GET_TEMP_PATH !== "undefined") {
    exports.getTempPath = FS_GET_TEMP_PATH;
    delete FS_GET_TEMP_PATH;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief home
////////////////////////////////////////////////////////////////////////////////

  var homeDirectory = "";

  if (typeof HOME !== "undefined") {
    homeDirectory = HOME;
    delete HOME;
  }

  exports.home = function () {
    'use strict';

    return homeDirectory;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief isDirectory
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_IS_DIRECTORY !== "undefined") {
    exports.isDirectory = FS_IS_DIRECTORY;
    delete FS_IS_DIRECTORY;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief isFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_IS_FILE !== "undefined") {
    exports.isFile = FS_IS_FILE;
    delete FS_IS_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief join
////////////////////////////////////////////////////////////////////////////////

  if (isWindows) {

    exports.join = function () {
      'use strict';

      function f(p) {
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

  }
  else {

    exports.join = function () {
      'use strict';

      var paths = Array.prototype.slice.call(arguments, 0);

      return normalize(paths.filter(function(p, index) {
        if (typeof p !== 'string') {
          throw new TypeError('Arguments to path.join must be strings');
        }

        return p;
      }).join('/'));
    };

  }

////////////////////////////////////////////////////////////////////////////////
/// @brief safe-join
////////////////////////////////////////////////////////////////////////////////

  if (isWindows) {
    exports.safeJoin = function (base, relative) {
      'use strict';

      base = normalize(base + "/");
      var path = normalizeArray(relative.split(/[\\\/]+/), false);

      return base + path;
    };
  }
  else {
    exports.safeJoin = function (base, relative) {
      'use strict';

      base = normalize(base + "/");
      var path = normalizeArray(relative.split("/"), false);

      return base + path;
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief list
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_LIST !== "undefined") {
    exports.list = FS_LIST;
    delete FS_LIST;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief listTree
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_LIST_TREE !== "undefined") {
    exports.listTree = FS_LIST_TREE;
    delete FS_LIST_TREE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief makeDirectory
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_MAKE_DIRECTORY !== "undefined") {
    exports.makeDirectory = FS_MAKE_DIRECTORY;
    delete FS_MAKE_DIRECTORY;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief makeDirectoryRecursive
////////////////////////////////////////////////////////////////////////////////
  
  exports.makeDirectoryRecursive = function (path) {
    'use strict';

    var parts, subPart;

    parts = path.split(exports.pathSeparator);
    subPart = '';

    parts.forEach(function (s, i) {
      if (i > 0) {
        subPart += exports.pathSeparator;
      }
      subPart += s;

      try {
        // directory may already exist
        exports.makeDirectory(subPart);
      }
      catch (err) {
      }
    });
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief move
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_MOVE !== "undefined") {
    exports.move = FS_MOVE;
    delete FS_MOVE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief read
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_READ !== "undefined") {
    exports.read = SYS_READ;
    delete SYS_READ;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief read64
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_READ64 !== "undefined") {
    exports.read64 = SYS_READ64;
    delete SYS_READ64;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief remove
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_REMOVE !== "undefined") {
    exports.remove = FS_REMOVE;
    delete FS_REMOVE;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief removeDirectory
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_REMOVE_DIRECTORY !== "undefined") {
    exports.removeDirectory = FS_REMOVE_DIRECTORY;
    delete FS_REMOVE_DIRECTORY;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief removeDirectoryRecursive
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_REMOVE_RECURSIVE_DIRECTORY !== "undefined") {
    exports.removeDirectoryRecursive = FS_REMOVE_RECURSIVE_DIRECTORY;
    delete FS_REMOVE_RECURSIVE_DIRECTORY;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief size
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_FILESIZE !== "undefined") {
    exports.size = FS_FILESIZE;
    delete FS_FILESIZE;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief unzipFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_UNZIP_FILE !== "undefined") {
    exports.unzipFile = FS_UNZIP_FILE;
    delete FS_UNZIP_FILE;
  }
  
////////////////////////////////////////////////////////////////////////////////
/// @brief write
////////////////////////////////////////////////////////////////////////////////

  if (typeof SYS_SAVE !== "undefined") {
    exports.write = SYS_SAVE;
    delete SYS_SAVE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief zipFile
////////////////////////////////////////////////////////////////////////////////

  if (typeof FS_ZIP_FILE !== "undefined") {
    exports.zipFile = FS_ZIP_FILE;
    delete FS_ZIP_FILE;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
