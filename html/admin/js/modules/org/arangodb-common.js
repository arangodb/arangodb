module.define("org/arangodb-common", function(exports, module) {
/*jslint indent: 2, maxlen: 100, nomen: true, vars: true, white: true, plusplus: true */
/*global require, module, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript base module
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var fs = require("fs");

var mimetypes = require("org/arangodb/mimetypes").mimeTypes;

// -----------------------------------------------------------------------------
// --SECTION--                                                 module "arangodb"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoShell
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief errors
////////////////////////////////////////////////////////////////////////////////

(function () {
  'use strict';

  var name;

  for (name in internal.errors) {
    if (internal.errors.hasOwnProperty(name)) {
      exports[name] = internal.errors[name].code;
    }
  }
}());

exports.errors = internal.errors;

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
/// @brief defines a module
////////////////////////////////////////////////////////////////////////////////

exports.defineModule = function (path, file) {
  var content;
  var m;
  var mc;

  content = fs.read(file);

  mc = internal.db._collection("_modules");

  if (mc === null) {
    mc = internal.db._create("_modules", { isSystem: true });
  }

  path = module.normalize(path);
  m = mc.firstExample({ path: path });

  if (m === null) {
    mc.save({ path: path, content: content });
  }
  else {
    mc.replace(m, { path: path, content: content });
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief guessContentType
////////////////////////////////////////////////////////////////////////////////

exports.guessContentType = function (filename) {
  'use strict';

  var re = /\.([a-zA-Z0-9]+)$/;
  var match = re.exec(filename);

  if (match !== null) {
    var extension = match[1];
  
    if (mimetypes.hasOwnProperty(extension)) {
      var type = mimetypes[extension];
      
      if (type[1]) {
        // append charset
        return type[0] + "; charset=utf-8";
      }

      return type[0]; 
    }
    // fall-through intentional
  }

  // default mimetype
  return "text/plain; charset=utf-8";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizeURL
///
/// If @FA{path} starts with "." or "..", then it is a relative path.
/// Otherwise it is an absolute path. Normalizing will remove `//`,
/// `/./`, `/../` from the url - expect in the beginning, where it keeps
/// `../` and or at most one `./`.
///
/// If @FA{path} is empty, the url `./` will be returned.
////////////////////////////////////////////////////////////////////////////////

exports.normalizeURL = function (path) {
  'use strict';

  var i;
  var n;
  var p;
  var q;
  var r;
  var x;

  if (path === "") {
    return "./";
  }

  p = path.split('/');

  // relative path
  if (p[0] === "." || p[0] === "..") {
    r = p[0] + "/";
    p.shift();
    q = p;
  }

  // absolute path
  else if (p[0] === "") {
    r = "/";
    p.shift();
    q = p;
  }

  // assume that the path is relative
  else {
    r = "./";
    q = p;
  }

  // normalize path
  n = [];

  for (i = 0;  i < q.length;  ++i) {
    x = q[i];

    if (x === "..") {
      if (n.length === 0) {
        if (r === "../") {
          n.push(x);
        }
        else if (r === "./") {
          r = "../";
        }
        else {
          throw "cannot use '..' to escape top-level-directory";
        }
      }
      else if (n[n.length - 1] === "..") {
        n.push(x);
      }
      else {
        n.pop();
      }
    }
    else if (x !== "" && x !== ".") {
      n.push(x);
    }
  }

  return r + n.join('/');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief output
///
/// In order to allow "capture" output to work, we cannot assigne the
/// function here.
////////////////////////////////////////////////////////////////////////////////

exports.output = function () {
  'use strict';

  internal.output.apply(internal.output, arguments);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

exports.print = internal.print;

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

exports.printf = internal.printf;

////////////////////////////////////////////////////////////////////////////////
/// @brief printObject
////////////////////////////////////////////////////////////////////////////////

exports.printObject = internal.printObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief stringPadding
////////////////////////////////////////////////////////////////////////////////

exports.stringPadding = function (str, len, pad, dir) {
  'use strict';

  if (typeof(len) === "undefined") { len = 0; }
  if (typeof(pad) === "undefined") { pad = ' '; }
  if (typeof(dir) === "undefined") { dir = 'r'; }

  if (len + 1 >= str.length) {
    switch (dir){

      // LEFT
      case 'l':
        str = new Array(len + 1 - str.length).join(pad) + str;
        break;

      // BOTH
      case 'b':
        var padlen = len - str.length;
        var right = Math.ceil(padlen / 2);
        var left = padlen - right;
        str = new Array(left+1).join(pad) + str + new Array(right+1).join(pad);
        break;

      default:
         str = str + new Array(len + 1 - str.length).join(pad);
         break;
    }
  }

  return str;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:
});
