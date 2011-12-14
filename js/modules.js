////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright by triAGENS GmbH - All rights reserved.
///
/// The Programs (which include both the software and documentation)
/// contain proprietary information of triAGENS GmbH; they are
/// provided under a license agreement containing restrictions on use and
/// disclosure and are also protected by copyright, patent and other
/// intellectual and industrial property laws. Reverse engineering,
/// disassembly or decompilation of the Programs, except to the extent
/// required to obtain interoperability with other independently created
/// software or as specified by law, is prohibited.
///
/// The Programs are not intended for use in any nuclear, aviation, mass
/// transit, medical, or other inherently dangerous applications. It shall
/// be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of such
/// applications if the Programs are used for such purposes, and triAGENS
/// GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of
/// triAGENS GmbH. You shall not disclose such confidential and
/// proprietary information and shall use it only in accordance with the
/// terms of the license agreement you entered into with triAGENS GmbH.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @page JSModules JavaScript Modules
///
/// The AvocadoDB uses a <a
/// href="http://wiki.commonjs.org/wiki/Modules">CommonJS</a> compatible module
/// concept. You can use the function @FN{require} in order to load a
/// module. @FN{require} returns the exported variables and functions of the
/// module. You can use the option @CO{startup.modules-path} to specify the
/// location of the JavaScript files.
///
/// @section Modules Modules
///
/// @copydetails JSF_require
///
/// @section InternalFunctions Internal Functions
///
/// The following functions are used internally to implement the module loader.
///
/// @copydetails JS_Execute
///
/// @copydetails JS_Load
///
/// @copydetails JS_Read
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            Module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8Module
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief module cache
////////////////////////////////////////////////////////////////////////////////

ModuleCache = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief module constructor
////////////////////////////////////////////////////////////////////////////////

function Module (id) {
  this.id = id;
  this.exports = {};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a file and creates a new module descriptor
////////////////////////////////////////////////////////////////////////////////

Module.prototype.require = function (path) {
  var content;
  var sandbox;
  var paths;
  var module;

  // first get rid of any ".." and "."
  path = this.normalise(path);

  // check if you already know the module, return the exports
  if (path in ModuleCache) {
    return ModuleCache[path].exports;
  }

  // try to load the file
  paths = MODULES_PATH.split(";");
  content = undefined;

  for (var i = 0;  i < paths.length;  ++i) {
    var p = paths[i];
    var n;

    if (p == "") {
      n = "." + path + ".js"
    }
    else {
      n = p + "/" + path + ".js";
    }

    if (FS_EXISTS(n)) {
      content = read(n);
      break;
    }
  }

  if (content == undefined) {
    throw "cannot find a module named '" + path + "' using the module path(s) '" + MODULES_PATH + "'";
  }

  // create a new sandbox and execute
  ModuleCache[path] = module = new Module(path);

  sandbox = {};
  sandbox.module = module;
  sandbox.exports = module.exports;
  sandbox.require = function(path) { return sandbox.module.require(path); }
  sandbox.print = print;

  execute(content, sandbox, path);

  module.exports = sandbox.exports;

  return module.exports;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief normalises a path
////////////////////////////////////////////////////////////////////////////////

Module.prototype.normalise = function (path) {
  var p;
  var q;
  var x;

  if (path == "") {
    return this.id;
  }

  p = path.split('/');

  // relative path
  if (p[0] == "." || p[0] == "..") {
    q = this.id.split('/');
    q.pop();
    q = q.concat(p);
  }

  // absolute path
  else {
    q = p;
  }

  // normalize path
  n = [];

  for (var i = 0;  i < q.length;  ++i) {
    x = q[i];

    if (x == "") {
    }
    else if (x == ".") {
    }
    else if (x == "..") {
      if (n.length == 0) {
        throw "cannot cross module top";
      }

      n.pop();
    }
    else {
      n.push(x);
    }
  }

  return "/" + n.join('/');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level model
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/"] = module = new Module("/");

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level require
///
/// @FUN{require(@FA{path})}
///
/// Require checks if the file specified by @FA{path} has already been loaded.
/// If not, the content of the file is executed in a new context. Within the
/// context you can use the global variable @CODE{exports} in order to export
/// variables and functions. This variable is returned by @FN{require}. 
///
/// Assume that your module file is @CODE{test1.js} and contains
///
/// @verbinclude modules1
///
/// Then you can use require to load the file and access the exports.
///
/// @verbinclude modules2
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  return module.require(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                            Module
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup V8ModuleFS
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief fs model
////////////////////////////////////////////////////////////////////////////////

ModuleCache["/fs"] = new Module("/fs");
ModuleCache["/fs"].exports.exists = FS_EXISTS;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\)"
// End:
