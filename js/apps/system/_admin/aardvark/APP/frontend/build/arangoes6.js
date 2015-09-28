module.define("org/arangodb-common", function(exports, module) {
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript base module
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
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
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
/// @brief errors
////////////////////////////////////////////////////////////////////////////////


Object.keys(internal.errors).forEach(function (key) {
  exports[key] = internal.errors[key].code;
});

exports.errors = internal.errors;

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

exports.ArangoError = internal.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

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

exports.guessContentType = function (filename, defaultValue) {
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
  if (defaultValue) {
    return defaultValue;
  }
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
/// @brief inspect
////////////////////////////////////////////////////////////////////////////////

exports.inspect = internal.inspect;

////////////////////////////////////////////////////////////////////////////////
/// @brief output
///
/// In order to allow "capture" output to work, we cannot assigne the
/// function here.
////////////////////////////////////////////////////////////////////////////////

exports.output = function () {
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
/// @brief sprintf
////////////////////////////////////////////////////////////////////////////////

exports.sprintf = internal.sprintf;

////////////////////////////////////////////////////////////////////////////////
/// @brief printObject
////////////////////////////////////////////////////////////////////////////////

exports.printObject = internal.printObject;

////////////////////////////////////////////////////////////////////////////////
/// @brief 2D ASCII table printing
////////////////////////////////////////////////////////////////////////////////

exports.printTable = function  (list, columns, options) {
  options = options || { };
  if (options.totalString === undefined) {
    options.totalString = "%s document(s)\n";
  }

  var pad = '...';
  var descriptions, matrix, col, what, j;

  if (columns === undefined) {
    what = list[0];
  }
  else if (Array.isArray(columns)) {
    what = { };

    columns.forEach(function (col) {
      what[col] = null;
    });
  }
  else {
    what = columns;
  }

  j = 0;
  descriptions = [ ];
  matrix = [ [ ] ];

  for (col in what) {
    if (what.hasOwnProperty(col)) {
      var fixedLength = null;

      if (columns && columns.hasOwnProperty(col) && columns[col] > 0) {
        fixedLength = columns[col] >= pad.length ? columns[col] : pad.length;
      }

      // header
      var name = col;

      // rename header?
      if (options.hasOwnProperty("rename")) {
        if (options.rename.hasOwnProperty(col)) {
          name = options.rename[col];
        }
      }

      descriptions.push({
        id: col,
        fixedLength: fixedLength,
        length: fixedLength || name.length
      });

      matrix[0][j++] = name;
    }
  }

  // determine values & max widths
  list.forEach(function (row, i) {
    matrix[i + 1] = [ ];
    descriptions.forEach(function (col) {

      if (row.hasOwnProperty(col.id)) {
        var value;
        if (options.prettyStrings && typeof row[col.id] === 'string') {
          value = row[col.id];
        }
        else {
          value = JSON.stringify(row[col.id]) || "";
        }

        matrix[i + 1].push(value);

        if (value.length > col.length && ! col.fixedLength) {
          col.length = Math.min(value.length, 100);
        }
      }
      else {
        // undefined
        matrix[i + 1].push('');
      }
    });
  });

  var divider = function () {
    var parts = [ ];
    descriptions.forEach(function (desc) {
      parts.push(exports.stringPadding('', desc.length, '-', 'r'));
    });

    if (options.framed) {
      return '+-' + parts.join('-+-') + '-+\n';
    }

    return parts.join('   ') + '\n';
  };

  var compose = function () {
    var result = '';

    if (options.framed) {
      result += divider();
    }
    matrix.forEach(function (row, i) {
      var parts = [ ];

      row.forEach(function (col, j) {

        var len = descriptions[j].length, value = row[j];
        if (value.length > len) {
          value = value.substr(0, len - pad.length) + pad;
        }
        parts.push(exports.stringPadding(value, len, ' ', 'r'));
      });

      if (options.framed) {
        result += '| ' + parts.join(' | ') + ' |\n';
      }
      else {
        result += parts.join('   ') + '\n';
      }

      if (i === 0) {
        result += divider();
      }
    });

    result += divider();

    if (! options.hideTotal) {
      result += internal.sprintf(options.totalString, String(list.length));
    }
    return result;
  };

  if (! Array.isArray(list)) {
    // not an array
    return;
  }

  if (list.length === 0) {
    exports.print(options.emptyString || "no document(s)");
  }
  else {
    exports.print(compose());
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stringPadding
////////////////////////////////////////////////////////////////////////////////

exports.stringPadding = function (str, len, pad, dir) {
  // yes, this is more code than new Array(length).join(chr), but it makes jslint happy
  function fill (length, chr) {
    var result = '', i;
    for (i = 0; i < length; ++i) {
      result += chr;
    }
    return result;
  }

  if (typeof(len) === "undefined") {
    len = 0;
  }
  if (typeof(pad) === "undefined") {
    pad = ' ';
  }

  if (len + 1 >= str.length) {
    switch (dir || "r"){

      // LEFT
      case 'l':
        str = fill(len + 1 - str.length, pad) + str;
        break;

      // BOTH
      case 'b':
        var padlen = len - str.length;
        var right = Math.ceil(padlen / 2);
        var left = padlen - right;
        str = fill(left + 1, pad) + str + fill(right + 1, pad);
        break;

      default:
         str = str + fill(len + 1 - str.length, pad);
         break;
    }
  }

  return str;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an error in case a download failed
////////////////////////////////////////////////////////////////////////////////

exports.throwDownloadError = function (msg) {
  throw new exports.ArangoError({
    errorNum: exports.errors.ERROR_APPLICATION_DOWNLOAD_FAILED.code,
    errorMessage: exports.errors.ERROR_APPLICATION_DOWNLOAD_FAILED.message + ': ' + String(msg)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an error in case of missing file
////////////////////////////////////////////////////////////////////////////////

exports.throwFileNotFound = function (msg) {
  throw new exports.ArangoError({
    errorNum: exports.errors.ERROR_FILE_NOT_FOUND.code,
    errorMessage: exports.errors.ERROR_FILE_NOT_FOUND.message + ': ' + String(msg)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief throws an error in case of a bad parameter
////////////////////////////////////////////////////////////////////////////////

exports.throwBadParameter = function (msg) {
  throw new exports.ArangoError({
    errorNum: exports.errors.ERROR_BAD_PARAMETER.code,
    errorMessage: exports.errors.ERROR_BAD_PARAMETER.message + ': ' + String(msg)
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks parameter, throws an error if missing
////////////////////////////////////////////////////////////////////////////////

exports.checkParameter = function (usage, descs, vars) {
  var i;

  for (i = 0;  i < descs.length;  ++i) {
    var desc = descs[i];

    if (typeof vars[i] === "undefined") {
      exports.throwBadParameter(desc[0] + " missing, usage: " + usage);
    }

    if (typeof vars[i] !== desc[1]) {
      exports.throwBadParameter(desc[0] + " should be a '" + desc[1] + "', "
                              + "not '" + (typeof vars[i]) + "'");
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief generate info message for newer version(s) available
////////////////////////////////////////////////////////////////////////////////

exports.checkAvailableVersions = function (version) {
  var console = require("console");
  var log;

  if (require("org/arangodb").isServer) {
    log = console.info;
  }
  else {
    log = internal.print;
  }

  if (version === undefined) {
    version = internal.version;
  }

  if (version.match(/beta|alpha|preview|devel/) !== null) {
    log("You are using an alpha/beta/preview version ('" + version + "') of ArangoDB");
    return;
  }

  try {
    var u = "https://www.arangodb.com/repositories/versions.php?version=";
    var d = internal.download(u + version, "", {timeout: 300});
    var v = JSON.parse(d.body);

    if (v.hasOwnProperty("bugfix")) {
      log("Please note that a new bugfix version '" + v.bugfix.version + "' is available");
    }

    if (v.hasOwnProperty("minor")) {
      log("Please note that a new minor version '" + v.minor.version + "' is available");
    }

    if (v.hasOwnProperty("major")) {
      log("Please note that a new major version '" + v.major.version + "' is available");
    }
  }
  catch (err) {
    if (console && console.debug) {
      console.debug("cannot check for newer version: ", err.stack);
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb", function(exports, module) {
'use strict';

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
var common = require("org/arangodb-common");

Object.keys(common).forEach(function (key) {
  exports[key] = common[key];
});

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief isServer
////////////////////////////////////////////////////////////////////////////////

exports.isServer = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief isClient
////////////////////////////////////////////////////////////////////////////////

exports.isClient = true;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoCollection"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoConnection"
////////////////////////////////////////////////////////////////////////////////

exports.ArangoConnection = internal.ArangoConnection;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoDatabase"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoDatabase = require("org/arangodb/arango-database").ArangoDatabase;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoStatement"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

////////////////////////////////////////////////////////////////////////////////
/// @brief class "ArangoQueryCursor"
////////////////////////////////////////////////////////////////////////////////

// cannot yet not use arangodb
exports.ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor;

////////////////////////////////////////////////////////////////////////////////
/// @brief the global "db" and "arango" object
////////////////////////////////////////////////////////////////////////////////

if (typeof internal.arango !== 'undefined') {
  try {
    exports.arango = internal.arango;
    exports.db = new exports.ArangoDatabase(internal.arango);
    internal.db = exports.db; // TODO remove
  }
  catch (err) {
    internal.print("cannot connect to server: " + String(err));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief the server version
////////////////////////////////////////////////////////////////////////////////

exports.plainServerVersion = function() {
  if (internal.arango) {
    let version = internal.arango.getVersion();
    let devel = version.match(/(.*)-(rc[0-9]*|devel)$/);

    if (devel !== null) {
      version = devel[1];
    }

    return version;
  }
  else {
    return undefined;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
});

module.define("org/arangodb/aql/explainer", function(exports, module) {
/*jshint strict: false, maxlen: 300 */

var db = require("org/arangodb").db,
  internal = require("internal"),
  systemColors = internal.COLORS, 
  print = internal.print,
  colors = { };

 
if (typeof internal.printBrowser === "function") {
  print = internal.printBrowser;
}

var stringBuilder = {

  output: "",

  appendLine: function(line) {
    if (!line) {
      this.output += "\n";
    }
    else {
      this.output += line + "\n";
    }
  },

  getOutput: function() {
    return this.output;
  },

  clearOutput: function() {
    this.output = "";
  }

};

/* set colors for output */
function setColors (useSystemColors) {
  'use strict';

  [ "COLOR_RESET", 
    "COLOR_CYAN", "COLOR_BLUE", "COLOR_GREEN", "COLOR_MAGENTA", "COLOR_YELLOW", "COLOR_RED", "COLOR_WHITE",
    "COLOR_BOLD_CYAN", "COLOR_BOLD_BLUE", "COLOR_BOLD_GREEN", "COLOR_BOLD_MAGENTA", "COLOR_BOLD_YELLOW", 
    "COLOR_BOLD_RED", "COLOR_BOLD_WHITE" ].forEach(function(c) {
    colors[c] = useSystemColors ? systemColors[c] : "";
  });
}

/* colorizer and output helper functions */ 

function attributeUncolored (v) {
  'use strict';
  return "`" + v + "`";
}
  
function keyword (v) {
  'use strict';
  return colors.COLOR_CYAN + v + colors.COLOR_RESET;
}

function annotation (v) {
  'use strict';
  return colors.COLOR_BLUE + v + colors.COLOR_RESET;
}

function value (v) {
  'use strict';
  return colors.COLOR_GREEN + v + colors.COLOR_RESET;
}
  
function variable (v) {
  'use strict';
  if (v[0] === "#") {
    return colors.COLOR_MAGENTA + v + colors.COLOR_RESET;
  }
  return colors.COLOR_YELLOW + v + colors.COLOR_RESET;
}

function func (v) {
  'use strict';
  return colors.COLOR_GREEN + v + colors.COLOR_RESET;
}
  
function collection (v) {
  'use strict';
  return colors.COLOR_RED + v + colors.COLOR_RESET;
}

function attribute (v) {
  'use strict';
  return "`" + colors.COLOR_YELLOW + v + colors.COLOR_RESET + "`";
}

function header (v) {
  'use strict';
  return colors.COLOR_MAGENTA + v + colors.COLOR_RESET;
}

function section (v) {
  'use strict';
  return colors.COLOR_BOLD_BLUE + v + colors.COLOR_RESET;
}

function pad (n) {
  'use strict';
  if (n < 0) {
    // value seems invalid...
    n = 0;
  }
  return new Array(n).join(" ");
}

function wrap (str, width) { 
  'use strict';
  var re = ".{1," + width + "}(\\s|$)|\\S+?(\\s|$)";
  return str.match(new RegExp(re, "g")).join("\n");
}

 
/* print functions */

/* print query string */  
function printQuery (query) {
  'use strict';
  stringBuilder.appendLine(section("Query string:"));
  stringBuilder.appendLine(" " + value(wrap(query, 100).replace(/\n+/g, "\n ", query)));
  stringBuilder.appendLine(); 
}

/* print write query modification flags */
function printModificationFlags (flags) {
  'use strict';
  if (flags === undefined) {
    return;
  }
  stringBuilder.appendLine(section("Write query options:"));
  var keys = Object.keys(flags), maxLen = "Option".length;
  keys.forEach(function(k) {
    if (k.length > maxLen) {
      maxLen = k.length;
    }
  });
  stringBuilder.appendLine(" " + header("Option") + pad(1 + maxLen - "Option".length) + "   " + header("Value"));
  keys.forEach(function(k) {
    stringBuilder.appendLine(" " + keyword(k) + pad(1 + maxLen - k.length) + "   " + value(JSON.stringify(flags[k]))); 
  });
  stringBuilder.appendLine();
}

/* print optimizer rules */
function printRules (rules) {
  'use strict';
  stringBuilder.appendLine(section("Optimization rules applied:"));
  if (rules.length === 0) {
    stringBuilder.appendLine(" " + value("none"));
  }
  else {
    var maxIdLen = String("Id").length;
    stringBuilder.appendLine(" " + pad(1 + maxIdLen - String("Id").length) + header("Id") + "   " + header("RuleName"));
    for (var i = 0; i < rules.length; ++i) {
      stringBuilder.appendLine(" " + pad(1 + maxIdLen - String(i + 1).length) + variable(String(i + 1)) + "   " + keyword(rules[i]));
    }
  }
  stringBuilder.appendLine(); 
}

/* print warnings */
function printWarnings (warnings) {
  'use strict';
  if (! Array.isArray(warnings) || warnings.length === 0) {
    return;
  }

  stringBuilder.appendLine(section("Warnings:"));
  var maxIdLen = String("Code").length;
  stringBuilder.appendLine(" " + pad(1 + maxIdLen - String("Code").length) + header("Code") + "   " + header("Message"));
  for (var i = 0; i < warnings.length; ++i) {
    stringBuilder.appendLine(" " + pad(1 + maxIdLen - String(warnings[i].code).length) + variable(warnings[i].code) + "   " + keyword(warnings[i].message));
  }
  stringBuilder.appendLine(); 
}

/* print indexes used */
function printIndexes (indexes) {
  'use strict';

  stringBuilder.appendLine(section("Indexes used:"));

  if (indexes.length === 0) {
    stringBuilder.appendLine(" " + value("none"));
  }
  else {
    var maxIdLen = String("Id").length;
    var maxCollectionLen = String("Collection").length;
    var maxUniqueLen = String("Unique").length;
    var maxSparseLen = String("Sparse").length;
    var maxTypeLen = String("Type").length;
    var maxSelectivityLen = String("Selectivity Est.").length;
    var maxFieldsLen = String("Fields").length;
    indexes.forEach(function(index) {
      var l = String(index.node).length;
      if (l > maxIdLen) {
        maxIdLen = l;
      }
      l = index.type.length;
      if (l > maxTypeLen) {
        maxTypeLen = l;
      }
      l = index.fields.map(attributeUncolored).join(", ").length;
      if (l > maxFieldsLen) {
        maxFieldsLen = l;
      }
      l = index.collection.length;
      if (l > maxCollectionLen) {
        maxCollectionLen = l;
      }
    });
    var line = " " + pad(1 + maxIdLen - String("Id").length) + header("Id") + "   " +
               header("Type") + pad(1 + maxTypeLen - "Type".length) + "   " +
               header("Collection") + pad(1 + maxCollectionLen - "Collection".length) + "   " +
               header("Unique") + pad(1 + maxUniqueLen - "Unique".length) + "   " +
               header("Sparse") + pad(1 + maxSparseLen - "Sparse".length) + "   " +
               header("Selectivity Est.") + "   " +
               header("Fields") + pad(1 + maxFieldsLen - "Fields".length) + "   " +
               header("Ranges");

    stringBuilder.appendLine(line);

    for (var i = 0; i < indexes.length; ++i) {
      var uniqueness = (indexes[i].unique ? "true" : "false");
      var sparsity = (indexes[i].hasOwnProperty("sparse") ? (indexes[i].sparse ? "true" : "false") : "n/a");
      var fields = indexes[i].fields.map(attribute).join(", ");
      var fieldsLen = indexes[i].fields.map(attributeUncolored).join(", ").length;
      var ranges = "[ " + indexes[i].ranges + " ]";
      var selectivity = (indexes[i].hasOwnProperty("selectivityEstimate") ?
        (indexes[i].selectivityEstimate * 100).toFixed(2) + " %" :
        "n/a"
      );
      line = " " + 
        pad(1 + maxIdLen - String(indexes[i].node).length) + variable(String(indexes[i].node)) + "   " + 
        keyword(indexes[i].type) + pad(1 + maxTypeLen - indexes[i].type.length) + "   " +
        collection(indexes[i].collection) + pad(1 + maxCollectionLen - indexes[i].collection.length) + "   " +
        value(uniqueness) + pad(1 + maxUniqueLen - uniqueness.length) + "   " +
        value(sparsity) + pad(1 + maxSparseLen - sparsity.length) + "   " +
        pad(1 + maxSelectivityLen - selectivity.length) + value(selectivity) + "   " +
        fields + pad(1 + maxFieldsLen - fieldsLen) + "   " + 
        ranges;

      stringBuilder.appendLine(line);
    }
  }
}

/* analzye and print execution plan */
function processQuery (query, explain) {
  'use strict';
  var nodes = { }, 
    parents = { }, 
    rootNode = null,
    maxTypeLen = 0,
    maxIdLen = String("Id").length,
    maxEstimateLen = String("Est.").length,
    plan = explain.plan;

  var recursiveWalk = function (n, level) {
    n.forEach(function(node) {
      nodes[node.id] = node;
      if (level === 0 && node.dependencies.length === 0) {
        rootNode = node.id;
      }
      if (node.type === "SubqueryNode") {
        recursiveWalk(node.subquery.nodes, level + 1);
      }
      node.dependencies.forEach(function(d) {
        if (! parents.hasOwnProperty(d)) {
          parents[d] = [ ];
        }
        parents[d].push(node.id);
      });

      if (String(node.id).length > maxIdLen) {
        maxIdLen = String(node.id).length;
      }
      if (String(node.type).length > maxTypeLen) {
        maxTypeLen = String(node.type).length;
      }
      if (String(node.estimatedNrItems).length > maxEstimateLen) {
        maxEstimateLen = String(node.estimatedNrItems).length;
      }
    });
  };
  recursiveWalk(plan.nodes, 0);

  var references = { }, 
    collectionVariables = { }, 
    usedVariables = { },
    indexes = [ ], 
    modificationFlags,
    isConst = true;

  var variableName = function (node) {
    if (/^[0-9_]/.test(node.name)) {
      return variable("#" + node.name);
    }
   
    if (collectionVariables.hasOwnProperty(node.id)) {
      usedVariables[node.name] = collectionVariables[node.id];
    }
    return variable(node.name); 
  };

  var buildExpression = function (node) {
    isConst = isConst && ([ "value", "object", "object element", "array" ].indexOf(node.type) !== -1);

    switch (node.type) {
      case "reference": 
        if (references.hasOwnProperty(node.name)) {
          var ref = references[node.name];
          delete references[node.name];
          if (Array.isArray(ref)) {
            var out = buildExpression(ref[1]) + "[" + (new Array(ref[0] + 1).join('*'));
            if (ref[2].type !== "no-op") {
              out += " " + keyword("FILTER") + " " + buildExpression(ref[2]);
            }
            if (ref[3].type !== "no-op") {
              out += " " + keyword("LIMIT ") + " " + buildExpression(ref[3]);
            }
            if (ref[4].type !== "no-op") {
              out += " " + keyword("RETURN ") + " " + buildExpression(ref[4]);
            }
            out += "]";
            return out;
          }
          return buildExpression(ref) + "[*]";
        }
        return variableName(node);
      case "collection": 
        return collection(node.name) + "   " + annotation("/* all collection documents */");
      case "value":
        return value(JSON.stringify(node.value));
      case "object":
        if (node.hasOwnProperty("subNodes")) {
          return "{ " + node.subNodes.map(buildExpression).join(", ") + " }";
        }
        return "{ }";
      case "object element":
        return value(JSON.stringify(node.name)) + " : " + buildExpression(node.subNodes[0]);
      case "calculated object element":
        return "[ " + buildExpression(node.subNodes[0]) + " ] : " + buildExpression(node.subNodes[1]);
      case "array":
        if (node.hasOwnProperty("subNodes")) {
          return "[ " + node.subNodes.map(buildExpression).join(", ") + " ]";
        }
        return "[ ]";
      case "unary not":
        return "! " + buildExpression(node.subNodes[0]);
      case "unary plus":
        return "+ " + buildExpression(node.subNodes[0]);
      case "unary minus":
        return "- " + buildExpression(node.subNodes[0]);
      case "array limit":
        return buildExpression(node.subNodes[0]) + ", " + buildExpression(node.subNodes[1]);
      case "attribute access":
        return buildExpression(node.subNodes[0]) + "." + attribute(node.name);
      case "indexed access":
        return buildExpression(node.subNodes[0]) + "[" + buildExpression(node.subNodes[1]) + "]";
      case "range":
        return buildExpression(node.subNodes[0]) + " .. " + buildExpression(node.subNodes[1]) + "   " + annotation("/* range */");
      case "expand":
      case "expansion":
        if (node.subNodes.length > 2) {
          // [FILTER ...]
          references[node.subNodes[0].subNodes[0].name] = [ node.levels, node.subNodes[0].subNodes[1], node.subNodes[2], node.subNodes[3], node.subNodes[4] ]; 
        }
        else {
          // [*]
          references[node.subNodes[0].subNodes[0].name] = node.subNodes[0].subNodes[1];
        }
        return buildExpression(node.subNodes[1]);
      case "verticalizer":
        return buildExpression(node.subNodes[0]);
      case "user function call":
        return func(node.name) + "(" + ((node.subNodes && node.subNodes[0].subNodes) || [ ]).map(buildExpression).join(", ") + ")" + "   " + annotation("/* user-defined function */");
      case "function call":
        return func(node.name) + "(" + ((node.subNodes && node.subNodes[0].subNodes) || [ ]).map(buildExpression).join(", ") + ")";
      case "plus":
        return buildExpression(node.subNodes[0]) + " + " + buildExpression(node.subNodes[1]);
      case "minus":
        return buildExpression(node.subNodes[0]) + " - " + buildExpression(node.subNodes[1]);
      case "times":
        return buildExpression(node.subNodes[0]) + " * " + buildExpression(node.subNodes[1]);
      case "division":
        return buildExpression(node.subNodes[0]) + " / " + buildExpression(node.subNodes[1]);
      case "modulus":
        return buildExpression(node.subNodes[0]) + " % " + buildExpression(node.subNodes[1]);
      case "compare not in":
        return buildExpression(node.subNodes[0]) + " not in " + buildExpression(node.subNodes[1]);
      case "compare in":
        return buildExpression(node.subNodes[0]) + " in " + buildExpression(node.subNodes[1]);
      case "compare ==":
        return buildExpression(node.subNodes[0]) + " == " + buildExpression(node.subNodes[1]);
      case "compare !=":
        return buildExpression(node.subNodes[0]) + " != " + buildExpression(node.subNodes[1]);
      case "compare >":
        return buildExpression(node.subNodes[0]) + " > " + buildExpression(node.subNodes[1]);
      case "compare >=":
        return buildExpression(node.subNodes[0]) + " >= " + buildExpression(node.subNodes[1]);
      case "compare <":
        return buildExpression(node.subNodes[0]) + " < " + buildExpression(node.subNodes[1]);
      case "compare <=":
        return buildExpression(node.subNodes[0]) + " <= " + buildExpression(node.subNodes[1]);
      case "logical or":
        return buildExpression(node.subNodes[0]) + " || " + buildExpression(node.subNodes[1]);
      case "logical and":
        return buildExpression(node.subNodes[0]) + " && " + buildExpression(node.subNodes[1]);
      case "ternary":
        return buildExpression(node.subNodes[0]) + " ? " + buildExpression(node.subNodes[1]) + " : " + buildExpression(node.subNodes[2]);
      default: 
        return "unhandled node type (" + node.type + ")";
    }
  };

  var buildBound = function (attr, operators, bound) {
    var boundValue = bound.isConstant ? value(JSON.stringify(bound.bound)) : buildExpression(bound.bound);
    return attribute(attr) + " " + operators[bound.include ? 1 : 0] + " " + boundValue;
  };

  var buildRanges = function (ranges) {
    var results = [ ];
    ranges.forEach(function(range) {
      var attr = range.attr;

      if (range.lowConst.hasOwnProperty("bound") && range.highConst.hasOwnProperty("bound") &&
          JSON.stringify(range.lowConst.bound) === JSON.stringify(range.highConst.bound)) {
        range.equality = true;
      }

      if (range.equality) {
        if (range.lowConst.hasOwnProperty("bound")) {
          results.push(buildBound(attr, [ "==", "==" ], range.lowConst));
        }
        else if (range.hasOwnProperty("lows")) {
          range.lows.forEach(function(bound) {
            results.push(buildBound(attr, [ "==", "==" ], bound));
          });
        }
      }
      else {
        if (range.lowConst.hasOwnProperty("bound")) {
          results.push(buildBound(attr, [ ">", ">=" ], range.lowConst));
        }
        if (range.highConst.hasOwnProperty("bound")) {
          results.push(buildBound(attr, [ "<", "<=" ], range.highConst));
        }
        if (range.hasOwnProperty("lows")) {
          range.lows.forEach(function(bound) {
            results.push(buildBound(attr, [ ">", ">=" ], bound));
          });
        }
        if (range.hasOwnProperty("highs")) {
          range.highs.forEach(function(bound) {
            results.push(buildBound(attr, [ "<", "<=" ], bound));
          });
        }
      }
    });
    if (results.length > 1) { 
      return "(" + results.join(" && ") + ")"; 
    }
    return results[0]; 
  };


  var label = function (node) { 
    switch (node.type) {
      case "SingletonNode":
        return keyword("ROOT");
      case "NoResultsNode":
        return keyword("EMPTY") + "   " + annotation("/* empty result set */");
      case "EnumerateCollectionNode":
        collectionVariables[node.outVariable.id] = node.collection;
        return keyword("FOR") + " " + variableName(node.outVariable) + " " + keyword("IN") + " " + collection(node.collection) + "   " + annotation("/* full collection scan" + (node.random ? ", random order" : "") + " */");
      case "EnumerateListNode":
        return keyword("FOR") + " " + variableName(node.outVariable) + " " + keyword("IN") + " " + variableName(node.inVariable) + "   " + annotation("/* list iteration */");
      case "IndexRangeNode":
        collectionVariables[node.outVariable.id] = node.collection;
        var index = node.index;
        index.ranges = node.ranges.map(buildRanges).join(" || ");
        index.collection = node.collection;
        index.node = node.id;
        indexes.push(index);
        return keyword("FOR") + " " + variableName(node.outVariable) + " " + keyword("IN") + " " + collection(node.collection) + "   " + annotation("/* " + (node.reverse ? "reverse " : "") + node.index.type + " index scan */");
      case "CalculationNode":
        return keyword("LET") + " " + variableName(node.outVariable) + " = " + buildExpression(node.expression) + "   " + annotation("/* " + node.expressionType + " expression */");
      case "FilterNode":
        return keyword("FILTER") + " " + variableName(node.inVariable);
      case "AggregateNode":
        return keyword("COLLECT") + " " + node.aggregates.map(function(node) {
          return variableName(node.outVariable) + " = " + variableName(node.inVariable);
        }).join(", ") + 
                 (node.count ? " " + keyword("WITH COUNT") : "") + 
                 (node.outVariable ? " " + keyword("INTO") + " " + variableName(node.outVariable) : "") +
                 (node.keepVariables ? " " + keyword("KEEP") + " " + node.keepVariables.map(function(variable) { return variableName(variable); }).join(", ") : "") + 
                 "   " + annotation("/* " + node.aggregationOptions.method + "*/");
      case "SortNode":
        return keyword("SORT") + " " + node.elements.map(function(node) {
          return variableName(node.inVariable) + " " + keyword(node.ascending ? "ASC" : "DESC"); 
        }).join(", ");
      case "LimitNode":
        return keyword("LIMIT") + " " + value(JSON.stringify(node.offset)) + ", " + value(JSON.stringify(node.limit)); 
      case "ReturnNode":
        return keyword("RETURN") + " " + variableName(node.inVariable);
      case "SubqueryNode":
        return keyword("LET") + " " + variableName(node.outVariable) + " = ...   " + annotation("/* subquery */");
      case "InsertNode":
        modificationFlags = node.modificationFlags;
        return keyword("INSERT") + " " + variableName(node.inVariable) + " " + keyword("IN") + " " + collection(node.collection);
      case "UpdateNode":
        modificationFlags = node.modificationFlags;
        if (node.hasOwnProperty("inKeyVariable")) {
          return keyword("UPDATE") + " " + variableName(node.inKeyVariable) + " " + keyword("WITH") + " " + variableName(node.inDocVariable) + " " + keyword("IN") + " " + collection(node.collection);
        }
        return keyword("UPDATE") + " " + variableName(node.inDocVariable) + " " + keyword("IN") + " " + collection(node.collection);
      case "ReplaceNode":
        modificationFlags = node.modificationFlags;
        if (node.hasOwnProperty("inKeyVariable")) {
          return keyword("REPLACE") + " " + variableName(node.inKeyVariable) + " " + keyword("WITH") + " " + variableName(node.inDocVariable) + " " + keyword("IN") + " " + collection(node.collection);
        }
        return keyword("REPLACE") + " " + variableName(node.inDocVariable) + " " + keyword("IN") + " " + collection(node.collection);
      case "UpsertNode":
        modificationFlags = node.modificationFlags;
        return keyword("UPSERT") + " " + variableName(node.inDocVariable) + " " + keyword("INSERT") + " " + variableName(node.insertVariable) + " " + keyword(node.isReplace ? "REPLACE" : "UPDATE") + " " + variableName(node.updateVariable) + " " + keyword("IN") + " " + collection(node.collection);
      case "RemoveNode":
        modificationFlags = node.modificationFlags;
        return keyword("REMOVE") + " " + variableName(node.inVariable) + " " + keyword("IN") + " " + collection(node.collection);
      case "RemoteNode":
        return keyword("REMOTE");
      case "DistributeNode":
        return keyword("DISTRIBUTE");
      case "ScatterNode":
        return keyword("SCATTER");
      case "GatherNode":
        return keyword("GATHER");
    }

    return "unhandled node type (" + node.type + ")";
  };
 
  var level = 0, subqueries = [ ];
  var indent = function (level, isRoot) {
    return pad(1 + level + level) + (isRoot ? "* " : "- ");
  };

  var preHandle = function (node) {
    usedVariables = { };
    isConst = true;
    if (node.type === "SubqueryNode") {
      subqueries.push(level);
    }
  };

  var postHandle = function (node) {
    if ([ "EnumerateCollectionNode",
          "EnumerateListNode",
          "IndexRangeNode",
          "SubqueryNode" ].indexOf(node.type) !== -1) {
      level++;
    }
    else if (node.type === "ReturnNode" && subqueries.length > 0) {
      level = subqueries.pop();
    }
    else if (node.type === "SingletonNode") {
      level++;
    }
  };

  var constNess = function () {
    if (isConst) {
      return "   " + annotation("/* const assignment */");
    }
    return "";
  };

  var variablesUsed = function () {
    var used = [ ];
    for (var a in usedVariables) { 
      if (usedVariables.hasOwnProperty(a)) {
        used.push(variable(a) + " : " + collection(usedVariables[a]));
      }
    }
    if (used.length > 0) {
      return "   " + annotation("/* collections used:") + " " + used.join(", ") + " " + annotation("*/");
    }
    return "";
  };
    
  var printNode = function (node) {
    preHandle(node);
    var line = " " +  
      pad(1 + maxIdLen - String(node.id).length) + variable(node.id) + "   " +
      keyword(node.type) + pad(1 + maxTypeLen - String(node.type).length) + "   " + 
      pad(1 + maxEstimateLen - String(node.estimatedNrItems).length) + value(node.estimatedNrItems) + "   " +
      indent(level, node.type === "SingletonNode") + label(node);

    if (node.type === "CalculationNode") {
      line += variablesUsed() + constNess();
    }
    stringBuilder.appendLine(line);
    postHandle(node);
  };

  printQuery(query);
  stringBuilder.appendLine(section("Execution plan:"));

  var line = " " + 
    pad(1 + maxIdLen - String("Id").length) + header("Id") + "   " +
    header("NodeType") + pad(1 + maxTypeLen - String("NodeType").length) + "   " +   
    pad(1 + maxEstimateLen - String("Est.").length) + header("Est.") + "   " +
    header("Comment");

  stringBuilder.appendLine(line);

  var walk = [ rootNode ];
  while (walk.length > 0) {
    var id = walk.pop();
    var node = nodes[id];
    printNode(node);
    if (parents.hasOwnProperty(id)) {
      walk = walk.concat(parents[id]);
    }
    if (node.type === "SubqueryNode") {
      walk = walk.concat([ node.subquery.nodes[0].id ]);
    }
  }

  stringBuilder.appendLine();
  printIndexes(indexes);
  stringBuilder.appendLine();
  printRules(plan.rules);
  printModificationFlags(modificationFlags);
  printWarnings(explain.warnings);
}

/* the exposed function */
function explain (data, options, shouldPrint) {
  'use strict';
  if (typeof data === "string") {
    data = { query: data };
  }
  if (! (data instanceof Object)) {
    throw "ArangoStatement needs initial data";
  }

  options = options || { };
  setColors(options.colors === undefined ? true : options.colors);

  var stmt = db._createStatement(data);
  var result = stmt.explain(options);

  stringBuilder.clearOutput();
  processQuery(data.query, result, true);

  if (shouldPrint === undefined || shouldPrint) {
    print(stringBuilder.getOutput());
  }
  else {
    return stringBuilder.getOutput();
  }
}

exports.explain = explain;

});

module.define("org/arangodb/aql/functions", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief AQL user functions management
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangodb = require("org/arangodb");

var db = arangodb.db;
var ArangoError = arangodb.ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                               module "org/arangodb/aql/functions"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the _aqlfunctions collection
////////////////////////////////////////////////////////////////////////////////

var getStorage = function () {
  'use strict';

  var functions = db._collection("_aqlfunctions");

  if (functions === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code;
    err.errorMessage = "collection '_aqlfunctions' not found";

    throw err;
  }

  return functions;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief apply a prefix filter on the functions
////////////////////////////////////////////////////////////////////////////////

var getFiltered = function (group) {
  'use strict';

  var result = [ ];

  if (group !== null && group !== undefined && group.length > 0) {
    var prefix = group.toUpperCase();

    if (group.length > 1 && group.substr(group.length - 2, 2) !== '::') {
      prefix += '::';
    }

    getStorage().toArray().forEach(function (f) {
      if (f.name.toUpperCase().substr(0, prefix.length) === prefix) {
        result.push(f);
      }
    });
  }
  else {
    result = getStorage().toArray();
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate a function name
////////////////////////////////////////////////////////////////////////////////

var validateName = function (name) {
  'use strict';

  if (typeof name !== 'string' ||
      ! name.match(/^[a-zA-Z0-9_]+(::[a-zA-Z0-9_]+)+$/) ||
      name.substr(0, 1) === "_") {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_NAME.message;

    throw err;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief validate user function code
////////////////////////////////////////////////////////////////////////////////

var stringifyFunction = function (code, name) {
  'use strict';

  if (typeof code === 'function') {
    code = String(code) + "\n";
  }

  if (typeof code === 'string') {
    code = "(" + code + "\n)";

    if (! internal.parse) {
      // no parsing possible. assume always valid
      return code;
    }

    try {
      if (internal.parse(code, name)) {
        // parsing successful
        return code;
      }
    }
    catch (e) {
    }
  }

  // fall-through intentional

  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
  err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message;

  throw err;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock aqlFunctionsUnregister
/// @brief delete an existing AQL user function
/// `aqlfunctions.unregister(name)`
///
/// Unregisters an existing AQL user function, identified by the fully qualified
/// function name.
///
/// Trying to unregister a function that does not exist will result in an
/// exception.
///
/// @EXAMPLES
///
/// ```js
///   require("org/arangodb/aql/functions").unregister("myfunctions::temperature::celsiustofahrenheit");
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var unregisterFunction = function (name) {
  'use strict';

  var func = null;

  validateName(name);

  try {
    func = getStorage().document(name.toUpperCase());
  }
  catch (err1) {
  }

  if (func === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.code;
    err.errorMessage = internal.sprintf(arangodb.errors.ERROR_QUERY_FUNCTION_NOT_FOUND.message, name);

    throw err;
  }

  getStorage().remove(func._id);
  internal.reloadAqlFunctions();

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock aqlFunctionsUnregisterGroup
/// @brief delete a group of AQL user functions
/// `aqlfunctions.unregisterGroup(prefix)`
///
/// Unregisters a group of AQL user function, identified by a common function
/// group prefix.
///
/// This will return the number of functions unregistered.
///
/// @EXAMPLES
///
/// ```js
///   require("org/arangodb/aql/functions").unregisterGroup("myfunctions::temperature");
///
///   require("org/arangodb/aql/functions").unregisterGroup("myfunctions");
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var unregisterFunctionsGroup = function (group) {
  'use strict';

  if (group.length === 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message;

    throw err;
  }

  var deleted = 0;

  getFiltered(group).forEach(function (f) {
    getStorage().remove(f._id);
    deleted++;
  });

  if (deleted > 0) {
    internal.reloadAqlFunctions();
  }

  return deleted;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock aqlFunctionsRegister
/// @brief register an AQL user function
/// `aqlfunctions.register(name, code, isDeterministic)`
///
/// Registers an AQL user function, identified by a fully qualified function
/// name. The function code in *code* must be specified as a JavaScript
/// function or a string representation of a JavaScript function.
/// If the function code in *code* is passed as a string, it is required that
/// the string evaluates to a JavaScript function definition.
///
/// If a function identified by *name* already exists, the previous function
/// definition will be updated. Please also make sure that the function code
/// does not violate the [Conventions](../AqlExtending/Conventions.md) for AQL 
/// functions.
///
/// The *isDeterministic* attribute can be used to specify whether the
/// function results are fully deterministic (i.e. depend solely on the input
/// and are the same for repeated calls with the same input values). It is not
/// used at the moment but may be used for optimizations later.
///
/// The registered function is stored in the selected database's system 
/// collection *_aqlfunctions*.
///
/// @EXAMPLES
///
/// ```js
///   require("org/arangodb/aql/functions").register("myfunctions::temperature::celsiustofahrenheit",
///   function (celsius) {
///     return celsius * 1.8 + 32;
///   });
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var registerFunction = function (name, code, isDeterministic) {
  // validate input
  validateName(name);

  code = stringifyFunction(code, name);

  var testCode = "(function() { var callback = " + code + "; return callback; })()";
  var err;

  try {
    if (internal && internal.hasOwnProperty("executeScript")) {
      var evalResult = internal.executeScript(testCode, undefined, "(user function " + name + ")");
      if (typeof evalResult !== "function") {
        err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
        err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message + 
                           ": code must be contained in function";
        throw err;
      }
    }
  }
  catch (err1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.code;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FUNCTION_INVALID_CODE.message;
    throw err;
  }

  var result = db._executeTransaction({
    collections: {
      write: getStorage().name()
    },
    action: function (params) {
      var exists = false;
      var collection = require("internal").db._collection(params.collection);
      var name = params.name;

      try {
        var doc = collection.document(name.toUpperCase());
        if (doc !== null) {
          collection.remove(doc._key);
          exists = true;
        }
      }
      catch (err2) {
      }

      var data = {
        _key: name.toUpperCase(),
        name: name,
        code: params.code,
        isDeterministic: params.isDeterministic || false
      };

      collection.save(data);
      return exists;
    },
    params: {
      name: name,
      code: code,
      isDeterministic: isDeterministic,
      collection: getStorage().name()
    }
  });

  internal.reloadAqlFunctions();

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock aqlFunctionsToArray
/// @brief list all AQL user functions
/// `aqlfunctions.toArray()`
///
/// Returns all previously registered AQL user functions, with their fully
/// qualified names and function code.
///
/// The result may optionally be restricted to a specified group of functions
/// by specifying a group prefix:
///
/// `aqlfunctions.toArray(prefix)`
///
/// @EXAMPLES
///
/// To list all available user functions:
///
/// ```js
///   require("org/arangodb/aql/functions").toArray();
/// ```
///
/// To list all available user functions in the *myfunctions* namespace:
///
/// ```js
///   require("org/arangodb/aql/functions").toArray("myfunctions");
/// ```
///
/// To list all available user functions in the *myfunctions::temperature* namespace:
///
/// ```js
///   require("org/arangodb/aql/functions").toArray("myfunctions::temperature");
/// ```
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var toArrayFunctions = function (group) {
  'use strict';

  var result = [ ];

  getFiltered(group).forEach(function (f) {
    result.push({ name: f.name, code: f.code.substr(1, f.code.length - 2).trim() });
  });

  return result;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

exports.unregister      = unregisterFunction;
exports.unregisterGroup = unregisterFunctionsGroup;
exports.register        = registerFunction;
exports.toArray         = toArrayFunctions;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:

});

module.define("org/arangodb/aql/queries", function(exports, module) {
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief AQL query management
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

// -----------------------------------------------------------------------------
// --SECTION--                                 module "org/arangodb/aql/queries"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clears the slow query log
////////////////////////////////////////////////////////////////////////////////

exports.clearSlow = function () {
  var db = internal.db;

  var requestResult = db._connection.DELETE("/_api/query/slow", "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the slow queries
////////////////////////////////////////////////////////////////////////////////

exports.slow = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/query/slow", "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the current queries
////////////////////////////////////////////////////////////////////////////////

exports.current = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/query/current", "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the query tracking properties
////////////////////////////////////////////////////////////////////////////////

exports.properties = function (config) {
  var db = internal.db;

  var requestResult;
  if (config === undefined) {
    requestResult = db._connection.GET("/_api/query/properties");
  }
  else {
    requestResult = db._connection.PUT("/_api/query/properties",
      JSON.stringify(config));
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief kills a query
////////////////////////////////////////////////////////////////////////////////

exports.kill = function (id) {
  if (typeof id === 'object' && 
      id.hasOwnProperty('id')) {
    id = id.id;
  }

  var db = internal.db;

  var requestResult = db._connection.DELETE("/_api/query/" + encodeURIComponent(id), "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:

});

module.define("org/arangodb/arango-collection-common", function(exports, module) {
/*jshint strict: false, unused: false, maxlen: 200 */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoCollection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;

var arangodb = require("org/arangodb");

var ArangoError = arangodb.ArangoError;
var sprintf = arangodb.sprintf;
var db = arangodb.db;

var simple = require("org/arangodb/simple-query");

var SimpleQueryAll = simple.SimpleQueryAll;
var SimpleQueryByExample = simple.SimpleQueryByExample;
var SimpleQueryByCondition = simple.SimpleQueryByCondition;
var SimpleQueryRange = simple.SimpleQueryRange;
var SimpleQueryGeo = simple.SimpleQueryGeo;
var SimpleQueryNear = simple.SimpleQueryNear;
var SimpleQueryWithin = simple.SimpleQueryWithin;
var SimpleQueryWithinRectangle = simple.SimpleQueryWithinRectangle;
var SimpleQueryFulltext = simple.SimpleQueryFulltext;

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                         constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is corrupted
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_CORRUPTED = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is new born
/// @deprecated
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_NEW_BORN = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is unloaded
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_UNLOADED = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is loaded
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_LOADED = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is unloading
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_UNLOADING = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is deleted
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_DELETED = 5;

////////////////////////////////////////////////////////////////////////////////
/// @brief collection is currently loading
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.STATUS_LOADING = 6;

////////////////////////////////////////////////////////////////////////////////
/// @brief document collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_DOCUMENT = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.TYPE_EDGE = 3;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._PRINT = function (context) {
  var status = "unknown";
  var type = "unknown";
  var name = this.name();

  switch (this.status()) {
    case ArangoCollection.STATUS_NEW_BORN: status = "new born"; break;
    case ArangoCollection.STATUS_UNLOADED: status = "unloaded"; break;
    case ArangoCollection.STATUS_UNLOADING: status = "unloading"; break;
    case ArangoCollection.STATUS_LOADED: status = "loaded"; break;
    case ArangoCollection.STATUS_CORRUPTED: status = "corrupted"; break;
    case ArangoCollection.STATUS_DELETED: status = "deleted"; break;
  }

  switch (this.type()) {
    case ArangoCollection.TYPE_DOCUMENT: type = "document"; break;
    case ArangoCollection.TYPE_EDGE:     type = "edge"; break;
  }

  var colors = require("internal").COLORS;
  var useColor = context.useColor;

  context.output += "[ArangoCollection ";
  if (useColor) { context.output += colors.COLOR_NUMBER; }
  context.output += this._id;
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += ", \"";
  if (useColor) { context.output += colors.COLOR_STRING; }
  context.output += name || "unknown";
  if (useColor) { context.output += colors.COLOR_RESET; }
  context.output += "\" (type " + type + ", status " + status + ")]";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into a string
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toString = function () {
  return "[ArangoCollection: " + this._id + "]";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an all query for a collection
/// @startDocuBlock collectionAll
/// `collection.all()`
///
/// Fetches all documents from a collection and returns a cursor. You can use
/// *toArray*, *next*, or *hasNext* to access the result. The result
/// can be limited using the *skip* and *limit* operator.
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{001_collectionAll}
/// ~ db._create("five");
///   db.five.save({ name : "one" });
///   db.five.save({ name : "two" });
///   db.five.save({ name : "three" });
///   db.five.save({ name : "four" });
///   db.five.save({ name : "five" });
///   db.five.all().toArray();
/// ~ db._drop("five");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use *limit* to restrict the documents:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{002_collectionAllNext}
/// ~ db._create("five");
///   db.five.save({ name : "one" });
///   db.five.save({ name : "two" });
///   db.five.save({ name : "three" });
///   db.five.save({ name : "four" });
///   db.five.save({ name : "five" });
///   db.five.all().limit(2).toArray();
/// ~ db._drop("five");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.all = function () {
  return new SimpleQueryAll(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
/// @startDocuBlock collectionByExample
/// `collection.byExample(example)`
///
/// Fetches all documents from a collection that match the specified
/// example and returns a cursor.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use
///
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }, b : 1 }*
///
/// will match, but the document
///
/// *{ a : { c : 1, b : 1 } }*
///
/// will not.
///
/// However, if you use
///
/// *{ a.c : 1 }*,
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute *c* of value *1*. Both the following documents
///
/// *{ a : { c : 1 }, b : 1 }* and
///
/// *{ a : { c : 1, b : 1 } }*
///
/// will match.
///
/// `collection.byExample(path1, value1, ...)`
///
/// As alternative you can supply an array of paths and values.
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{003_collectionByExample}
/// ~ db._create("users");
///   db.users.save({ name: "Gerhard" });
///   db.users.save({ name: "Helmut" });
///   db.users.save({ name: "Angela" });
///   db.users.all().toArray();
///   db.users.byExample({ "_id" : "users/20" }).toArray();
///   db.users.byExample({ "name" : "Gerhard" }).toArray();
///   db.users.byExample({ "name" : "Helmut", "_id" : "users/15" }).toArray();
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Use *next* to loop over all documents:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{004_collectionByExampleNext}
/// ~ db._create("users");
///   db.users.save({ name: "Gerhard" });
///   db.users.save({ name: "Helmut" });
///   db.users.save({ name: "Angela" });
///   var a = db.users.byExample( {"name" : "Angela" } );
///   while (a.hasNext()) print(a.next());
/// ~ db._drop("users");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExample = function (example) {
  var e;
  var i;

  // example is given as only argument
  if (arguments.length === 1) {
    e = example;
  }

  // example is given as list
  else {
    e = {};

    // create a REAL array, otherwise JSON.stringify will fail
    for (i = 0;  i < arguments.length;  i += 2) {
      e[arguments[i]] = arguments[i + 1];
    }
  }

  return new SimpleQueryByExample(this, e);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example using a hash index
/// @startDocuBLock collectionByExampleHash
/// `collection.byExampleHash(index, example)`
///
/// Selects all documents from the specified hash index that match the
/// specified example and returns a cursor.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use
///
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }, b : 1 }*
///
/// will match, but the document
///
/// *{ a : { c : 1, b : 1 } }*
///
/// will not.
///
/// However, if you use
///
/// *{ a.c : 1 }*,
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute @LIT{c} of value *1*. Both the following documents
///
/// *{ a : { c : 1 }, b : 1 }* and
///
/// *{ a : { c : 1, b : 1 } }*
///
/// will match.
///
/// `collection.byExampleHash(index-id, path1, value1, ...)`
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExampleHash = function (index, example) {
  var sq = this.byExample(example);
  sq._index = index;
  sq._type = "hash";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example using a skiplist index
/// @startDocuBlock collectionByExampleSkiplist
/// `collection.byExampleSkiplist(index, example)`
///
/// Selects all documents from the specified skiplist index that match the
/// specified example and returns a cursor.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute. If you use
///
/// *{ a : { c : 1 } }*
///
/// as example, then you will find all documents, such that the attribute
/// *a* contains a document of the form *{c : 1 }*. For example the document
///
/// *{ a : { c : 1 }, b : 1 }*
///
/// will match, but the document
///
/// *{ a : { c : 1, b : 1 } }*
///
/// will not.
///
/// However, if you use
///
/// *{ a.c : 1 }*,
///
/// then you will find all documents, which contain a sub-document in *a*
/// that has an attribute @LIT{c} of value *1*. Both the following documents
///
/// *{ a : { c : 1 }, b : 1 }*and
///
/// *{ a : { c : 1, b : 1 } }*
///
/// will match.
///
/// `collection.byExampleSkiplist(index, path1, value1, ...)`
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byExampleSkiplist = function (index, example) {
  var sq = this.byExample(example);
  sq._index = index;
  sq._type = "skiplist";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-condition using a skiplist index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.byConditionSkiplist = function (index, condition) {
  var sq = new SimpleQueryByCondition(this, condition);
  sq._index = index;
  sq._type = "skiplist";

  return sq;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a range query for a collection
/// @startDocuBlock collectionRange
/// `collection.range(attribute, left, right)`
///
/// Returns all documents from a collection such that the *attribute* is
/// greater or equal than *left* and strictly less than *right*.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute.
///
/// For range queries it is required that a skiplist index is present for the
/// queried attribute. If no skiplist index is present on the attribute, an
/// error will be thrown.
///
/// Note: the *range* simple query function is **deprecated** as of ArangoDB 2.6. 
/// The function may be removed in future versions of ArangoDB. The preferred
/// way for retrieving documents from a collection within a specific range
/// is to use an AQL query as follows: 
///
///     FOR doc IN @@collection 
///       FILTER doc.value >= @left && doc.value < @right 
///       LIMIT @skip, @limit 
///       RETURN doc
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{005_collectionRange}
/// ~ db._create("old");
///   db.old.ensureSkiplist("age");
///   db.old.save({ age: 15 });
///   db.old.save({ age: 25 });
///   db.old.save({ age: 30 });
///   db.old.range("age", 10, 30).toArray();
/// ~ db._drop("old")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.range = function (name, left, right) {
  return new SimpleQueryRange(this, name, left, right, 0);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a closed range query for a collection
/// @startDocuBlock collectionClosedRange
/// `collection.closedRange(attribute, left, right)`
///
/// Returns all documents of a collection such that the *attribute* is
/// greater or equal than *left* and less or equal than *right*.
///
/// You can use *toArray*, *next*, or *hasNext* to access the
/// result. The result can be limited using the *skip* and *limit*
/// operator.
///
/// An attribute name of the form *a.b* is interpreted as attribute path,
/// not as attribute.
///
/// Note: the *closedRange* simple query function is **deprecated** as of ArangoDB 2.6. 
/// The function may be removed in future versions of ArangoDB. The preferred
/// way for retrieving documents from a collection within a specific range
/// is to use an AQL query as follows: 
///
///     FOR doc IN @@collection 
///       FILTER doc.value >= @left && doc.value <= @right 
///       LIMIT @skip, @limit 
///       RETURN doc
///
/// @EXAMPLES
///
/// Use *toArray* to get all documents at once:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{006_collectionClosedRange}
/// ~ db._create("old");
///   db.old.ensureSkiplist("age");
///   db.old.save({ age: 15 });
///   db.old.save({ age: 25 });
///   db.old.save({ age: 30 });
///   db.old.closedRange("age", 10, 30).toArray();
/// ~ db._drop("old")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.closedRange = function (name, left, right) {
  return new SimpleQueryRange(this, name, left, right, 1);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a geo index selection
/// @startDocuBlock collectionGeo
/// `collection.geo(location-attribute)`
///
/// Looks up a geo index defined on attribute *location_attribute*.
///
/// Returns a geo index object if an index was found. The `near` or
/// `within` operators can then be used to execute a geo-spatial query on
/// this particular index.
///
/// This is useful for collections with multiple defined geo indexes.
///
/// `collection.geo(location_attribute, true)`
///
/// Looks up a geo index on a compound attribute *location_attribute*.
///
/// Returns a geo index object if an index was found. The `near` or
/// `within` operators can then be used to execute a geo-spatial query on
/// this particular index.
///
/// `collection.geo(latitude_attribute, longitude_attribute)`
///
/// Looks up a geo index defined on the two attributes *latitude_attribute*
/// and *longitude-attribute*.
///
/// Returns a geo index object if an index was found. The `near` or
/// `within` operators can then be used to execute a geo-spatial query on
/// this particular index.
///
/// Note: the *geo* simple query helper function is **deprecated** as of ArangoDB 
/// 2.6. The function may be removed in future versions of ArangoDB. The preferred
/// way for running geo queries is to use their AQL equivalents.
///
/// @EXAMPLES
///
/// Assume you have a location stored as list in the attribute *home*
/// and a destination stored in the attribute *work*. Then you can use the
/// `geo` operator to select which geo-spatial attributes (and thus which
/// index) to use in a `near` query.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{geoIndexSimpleQuery}
/// ~db._create("complex")
/// |for (i = -90;  i <= 90;  i += 10) {
/// |  for (j = -180;  j <= 180;  j += 10) {
/// |    db.complex.save({ name : "Name/" + i + "/" + j,
/// |                      home : [ i, j ],
/// |                      work : [ -i, -j ] });
/// |  }
/// |}
///
///  db.complex.near(0, 170).limit(5); // xpError(ERROR_QUERY_GEO_INDEX_MISSING)
///  db.complex.ensureGeoIndex("home");
///  db.complex.near(0, 170).limit(5).toArray();
///  db.complex.geo("work").near(0, 170).limit(5); // xpError(ERROR_QUERY_GEO_INDEX_MISSING)
///  db.complex.ensureGeoIndex("work");
///  db.complex.geo("work").near(0, 170).limit(5).toArray();
/// ~ db._drop("complex");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// 
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.geo = function(loc, order) {
  var idx;

  var locateGeoIndex1 = function(collection, loc, order) {
    var inds = collection.getIndexes();
    var i;

    for (i = 0;  i < inds.length;  ++i) {
      var index = inds[i];

      if (index.type === "geo1") {
        if (index.fields[0] === loc && index.geoJson === order) {
          return index;
        }
      }
    }

    return null;
  };

  var locateGeoIndex2 = function(collection, lat, lon) {
    var inds = collection.getIndexes();
    var i;

    for (i = 0;  i < inds.length;  ++i) {
      var index = inds[i];

      if (index.type === "geo2") {
        if (index.fields[0] === lat && index.fields[1] === lon) {
          return index;
        }
      }
    }

    return null;
  };

  if (order === undefined) {
    if (typeof loc === "object") {
      idx = this.index(loc);
    }
    else {
      idx = locateGeoIndex1(this, loc, false);
    }
  }
  else if (typeof order === "boolean") {
    idx = locateGeoIndex1(this, loc, order);
  }
  else {
    idx = locateGeoIndex2(this, loc, order);
  }

  if (idx === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.code;
    err.errorMessage = require("internal").sprintf(arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message, this.name());
    throw err;
  }

  return new SimpleQueryGeo(this, idx.id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a near query for a collection
/// @startDocuBlock collectionNear
/// `collection.near(latitude, longitude)`
///
/// The returned list is sorted according to the distance, with the nearest
/// document to the coordinate (*latitude*, *longitude*) coming first.
/// If there are near documents of equal distance, documents are chosen randomly
/// from this set until the limit is reached. It is possible to change the limit
/// using the *limit* operator.
///
/// In order to use the *near* operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the *geo* operator to select a particular index.
///
/// *Note*: `near` does not support negative skips.
//     However, you can still use `limit` followed to skip.
///
/// `collection.near(latitude, longitude).limit(limit)`
///
/// Limits the result to limit documents instead of the default 100.
///
/// *Note*: Unlike with multiple explicit limits, `limit` will raise
/// the implicit default limit imposed by `within`.
///
/// `collection.near(latitude, longitude).distance()`
///
/// This will add an attribute `distance` to all documents returned, which
/// contains the distance between the given point and the document in meters.
///
/// `collection.near(latitude, longitude).distance(name)`
///
/// This will add an attribute *name* to all documents returned, which
/// contains the distance between the given point and the document in meters.
///
/// Note: the *near* simple query function is **deprecated** as of ArangoDB 2.6. 
/// The function may be removed in future versions of ArangoDB. The preferred
/// way for retrieving documents from a collection using the near operator is
/// to use the AQL *NEAR* function in an [AQL query](../Aql/GeoFunctions.md) as follows: 
///
///     FOR doc IN NEAR(@@collection, @latitude, @longitude, @limit) 
///       RETURN doc
///
/// @EXAMPLES
///
/// To get the nearest two locations:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{007_collectionNear}
/// ~ db._create("geo");
///   db.geo.ensureGeoIndex("loc");
///   |for (var i = -90;  i <= 90;  i += 10) { 
///   |   for (var j = -180; j <= 180; j += 10) {
///   |     db.geo.save({
///   |        name : "Name/" + i + "/" + j,
///   |        loc: [ i, j ] });
///   } }
///   db.geo.near(0, 0).limit(2).toArray();
/// ~ db._drop("geo");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// If you need the distance as well, then you can use the `distance`
/// operator:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{008_collectionNearDistance}
/// ~ db._create("geo");
///   db.geo.ensureGeoIndex("loc");
///   |for (var i = -90;  i <= 90;  i += 10) {
///   |  for (var j = -180; j <= 180; j += 10) {
///   |     db.geo.save({
///   |         name : "Name/" + i + "/" + j,
///   |         loc: [ i, j ] });
///   } }
///   db.geo.near(0, 0).distance().limit(2).toArray();
/// ~ db._drop("geo");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this, lat, lon);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for a collection
/// @startDocuBlock collectionWithin
/// `collection.within(latitude, longitude, radius)`
///
/// This will find all documents within a given radius around the coordinate
/// (*latitude*, *longitude*). The returned array is sorted by distance,
/// beginning with the nearest document.
///
/// In order to use the *within* operator, a geo index must be defined for the
/// collection. This index also defines which attribute holds the coordinates
/// for the document.  If you have more then one geo-spatial index, you can use
/// the `geo` operator to select a particular index.
///
///
/// `collection.within(latitude, longitude, radius).distance()`
///
/// This will add an attribute `_distance` to all documents returned, which
/// contains the distance between the given point and the document in meters.
///
/// `collection.within(latitude, longitude, radius).distance(name)`
///
/// This will add an attribute *name* to all documents returned, which
/// contains the distance between the given point and the document in meters.
///
/// Note: the *within* simple query function is **deprecated** as of ArangoDB 2.6. 
/// The function may be removed in future versions of ArangoDB. The preferred
/// way for retrieving documents from a collection using the within operator  is
/// to use the AQL *WITHIN* function in an [AQL query](../Aql/GeoFunctions.md) as follows: 
///
///     FOR doc IN WITHIN(@@collection, @latitude, @longitude, @radius, @distanceAttributeName)
///       RETURN doc
///
/// @EXAMPLES
///
/// To find all documents within a radius of 2000 km use:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{009_collectionWithin}
/// ~ db._create("geo");
/// ~ db.geo.ensureGeoIndex("loc");
/// |~ for (var i = -90;  i <= 90;  i += 10) {
/// |  for (var j = -180; j <= 180; j += 10) {
///       db.geo.save({ name : "Name/" + i + "/" + j, loc: [ i, j ] }); } }
///   db.geo.within(0, 0, 2000 * 1000).distance().toArray();
/// ~ db._drop("geo");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this, lat, lon, radius);
};

ArangoCollection.prototype.withinRectangle = function (lat1, lon1, lat2, lon2) {
  return new SimpleQueryWithinRectangle(this, lat1, lon1, lat2, lon2);
};

ArangoCollection.prototype.fulltext = function (attribute, query, iid) {
  return new SimpleQueryFulltext(this, attribute, query, iid);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock collectionIterate
/// @brief iterates over some elements of a collection
/// `collection.iterate(iterator, options)`
///
/// Iterates over some elements of the collection and apply the function
/// *iterator* to the elements. The function will be called with the
/// document as first argument and the current number (starting with 0)
/// as second argument.
///
/// *options* must be an object with the following attributes:
///
/// - *limit* (optional, default none): use at most *limit* documents.
///
/// - *probability* (optional, default all): a number between *0* and
///   *1*. Documents are chosen with this probability.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{accessViaGeoIndex}
/// ~db._create("example")
/// |for (i = -90;  i <= 90;  i += 10) {
/// |  for (j = -180;  j <= 180;  j += 10) {
/// |    db.example.save({ name : "Name/" + i + "/" + j,
/// |                      home : [ i, j ],
/// |                      work : [ -i, -j ] });
/// |  }
/// |}
///
///  db.example.ensureGeoIndex("home");
///  |items = db.example.getIndexes().map(function(x) { return x.id; });
///  db.example.index(items[1]);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.iterate = function (iterator, options) {
  var probability = 1.0;
  var limit = null;
  var stmt;
  var cursor;
  var pos;

  // TODO: this is not optimal for the client, there should be an HTTP call handling
  // everything on the server

  if (options !== undefined) {
    if (options.hasOwnProperty("probability")) {
      probability = options.probability;
    }

    if (options.hasOwnProperty("limit")) {
      limit = options.limit;
    }
  }

  if (limit === null) {
    if (probability >= 1.0) {
      cursor = this.all();
    }
    else {
      stmt = sprintf("FOR d IN %s FILTER rand() >= @prob RETURN d", this.name());
      stmt = db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind("prob", probability);
      }

      cursor = stmt.execute();
    }
  }
  else {
    if (typeof limit !== "number") {
      var error = new ArangoError();
      error.errorNum = arangodb.errors.ERROR_ILLEGAL_NUMBER.code;
      error.errorMessage = "expecting a number, got " + String(limit);

      throw error;
    }

    if (probability >= 1.0) {
      cursor = this.all().limit(limit);
    }
    else {
      stmt = sprintf("FOR d IN %s FILTER rand() >= @prob LIMIT %d RETURN d",
                     this.name(), limit);
      stmt = db._createStatement({ query: stmt });

      if (probability < 1.0) {
        stmt.bind("prob", probability);
      }

      cursor = stmt.execute();
    }
  }

  pos = 0;

  while (cursor.hasNext()) {
    var document = cursor.next();

    iterator(document, pos);

    pos++;
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  document methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
/// @startDocuBlock documentsCollectionRemoveByExample
/// `collection.removeByExample(example)`
///
/// Removes all documents matching an example.
///
/// `collection.removeByExample(document, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document deletion operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.removeByExample(document, waitForSync, limit)`
///
/// The optional *limit* parameter can be used to restrict the number of
/// removals to the specified value. If *limit* is specified but less than the
/// number of documents in the collection, it is undefined which documents are
/// removed.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{010_documentsCollectionRemoveByExample}
/// ~ db._create("example");
/// ~ db.example.save({ Hello : "world" });
///   db.example.removeByExample( {Hello : "world"} );
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example, waitForSync, limit) {
  throw "cannot call abstract removeByExample function";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces documents matching an example
/// @startDocuBlock documentsCollectionReplaceByExample
/// `collection.replaceByExample(example, newValue)`
///
/// Replaces all documents matching an example with a new document body.
/// The entire document body of each document matching the *example* will be
/// replaced with *newValue*. The document meta-attributes such as *_id*,
/// *_key*, *_from*, *_to* will not be replaced.
///
/// `collection.replaceByExample(document, newValue, waitForSync)`
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document replacement operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.replaceByExample(document, newValue, waitForSync, limit)`
///
/// The optional *limit* parameter can be used to restrict the number of
/// replacements to the specified value. If *limit* is specified but less than
/// the number of documents in the collection, it is undefined which documents are
/// replaced.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{011_documentsCollectionReplaceByExample}
/// ~ db._create("example");
///   db.example.save({ Hello : "world" });
///   db.example.replaceByExample({ Hello: "world" }, {Hello: "mars"}, false, 5);
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example, newValue, waitForSync, limit) {
  throw "cannot call abstract replaceByExample function";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief partially updates documents matching an example
/// @startDocuBlock documentsCollectionUpdateByExample
/// `collection.updateByExample(example, newValue)`
///
/// Partially updates all documents matching an example with a new document body.
/// Specific attributes in the document body of each document matching the
/// *example* will be updated with the values from *newValue*.
/// The document meta-attributes such as *_id*, *_key*, *_from*,
/// *_to* cannot be updated.
///
/// Partial update could also be used to append new fields,
/// if there were no old field with same name.
///
/// `collection.updateByExample(document, newValue, keepNull, waitForSync)`
///
/// The optional *keepNull* parameter can be used to modify the behavior when
/// handling *null* values. Normally, *null* values are stored in the
/// database. By setting the *keepNull* parameter to *false*, this behavior
/// can be changed so that all attributes in *data* with *null* values will
/// be removed from the target document.
///
/// The optional *waitForSync* parameter can be used to force synchronization
/// of the document replacement operation to disk even in case that the
/// *waitForSync* flag had been disabled for the entire collection.  Thus,
/// the *waitForSync* parameter can be used to force synchronization of just
/// specific operations. To use this, set the *waitForSync* parameter to
/// *true*. If the *waitForSync* parameter is not specified or set to
/// *false*, then the collection's default *waitForSync* behavior is
/// applied. The *waitForSync* parameter cannot be used to disable
/// synchronization for collections that have a default *waitForSync* value
/// of *true*.
///
/// `collection.updateByExample(document, newValue, keepNull, waitForSync, limit)`
///
/// The optional *limit* parameter can be used to restrict the number of
/// updates to the specified value. If *limit* is specified but less than
/// the number of documents in the collection, it is undefined which documents are
/// updated.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{012_documentsCollectionUpdateByExample}
/// ~ db._create("example");
///   db.example.save({ Hello : "world", foo : "bar" });
///   db.example.updateByExample({ Hello: "world" }, { Hello: "foo", World: "bar" }, false);
///   db.example.byExample({ Hello: "foo" }).toArray()
/// ~ db._drop("example");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example, newValue, keepNull, waitForSync, limit) {
  throw "cannot call abstract updateExample function";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb/arango-collection", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoCollection
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief add options from arguments to index specification
////////////////////////////////////////////////////////////////////////////////

function addIndexOptions (body, parameters) {
  body.fields = [ ];

  var setOption = function(k) {
    if (! body.hasOwnProperty(k)) {
      body[k] = parameters[i][k];
    }
  };

  var i;
  for (i = 0; i < parameters.length; ++i) {
    if (typeof parameters[i] === "string") {
      // set fields
      body.fields.push(parameters[i]);
    }
    else if (typeof parameters[i] === "object" && 
             ! Array.isArray(parameters[i]) &&
             parameters[i] !== null) {
      // set arbitrary options
      Object.keys(parameters[i]).forEach(setOption);
      break;
    }
  }

  return body;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  ArangoCollection
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function ArangoCollection (database, data) {
  this._database = database;
  this._dbName = database._name();

  if (typeof data === "string") {
    this._id = null;
    this._name = data;
    this._status = null;
    this._type = null;
  }
  else if (data !== undefined) {
    this._id = data.id;
    this._name = data.name;
    this._status = data.status;
    this._type = data.type;
  }
  else {
    this._id = null;
    this._name = null;
    this._status = null;
    this._type = null;
  }
}

exports.ArangoCollection = ArangoCollection;

// must be called after exporting ArangoCollection
require("org/arangodb/arango-collection-common");

var ArangoError = require("org/arangodb").ArangoError;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief append the waitForSync parameter to a URL
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._appendSyncParameter = function (url, waitForSync) {
  if (waitForSync) {
    if (url.indexOf('?') === -1) {
      url += '?';
    }
    else {
      url += '&';
    }
    url += 'waitForSync=true';
  }
  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prefix a URL with the database name of the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._prefixurl = function (url) {
  if (url.substr(0, 5) === '/_db/') {
    return url;
  }

  if (url[0] === '/') {
    return '/_db/' + encodeURIComponent(this._dbName) + url;
  }
  return '/_db/' + encodeURIComponent(this._dbName) + '/' + url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for collection usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._baseurl = function (suffix) {
  var url = this._database._collectionurl(this.name());

  if (suffix) {
    url += "/" + suffix;
  }

  return this._prefixurl(url);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for document usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._documenturl = function (id) {
  var s = id.split("/"), url;
  if (s.length === 1) {
    url = this._database._documenturl(this.name() + "/" + id, this.name());
  }
  else {
    url = this._database._documenturl(id, this.name());
  }

  return this._prefixurl(url);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for collection index usage
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._indexurl = function () {
  return this._prefixurl("/_api/index?collection=" + encodeURIComponent(this.name()));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an edge query
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype._edgesQuery = function (vertex, direction) {
  // if vertex is a list, iterator and concat
  if (vertex instanceof Array) {
    var edges = [];
    var i;

    for (i = 0;  i < vertex.length;  ++i) {
      var e = this._edgesQuery(vertex[i], direction);

      edges.push.apply(edges, e);
    }

    return edges;
  }

  if (vertex.hasOwnProperty("_id")) {
    vertex = vertex._id;
  }

  // get the edges
  var url = "/_api/edges/" + encodeURIComponent(this.name())
            + "?vertex=" + encodeURIComponent(vertex)
            + (direction ? "&direction=" + direction : "");

  var requestResult = this._database._connection.GET(this._prefixurl(url));

  arangosh.checkRequestResult(requestResult);

  return requestResult.edges;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into an array
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.toArray = function () {
  return this.all().toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for ArangoCollection
////////////////////////////////////////////////////////////////////////////////

var helpArangoCollection = arangosh.createHelpHeadline("ArangoCollection help") +
  'ArangoCollection constructor:                                             ' + "\n" +
  ' > col = db.mycoll;                                                       ' + "\n" +
  ' > col = db._create("mycoll");                                            ' + "\n" +
  '                                                                          ' + "\n" +
  'Administration Functions:                                                 ' + "\n" +
  '  name()                                collection name                   ' + "\n" +
  '  status()                              status of the collection          ' + "\n" +
  '  type()                                type of the collection            ' + "\n" +
  '  truncate()                            delete all documents              ' + "\n" +
  '  properties()                          show collection properties        ' + "\n" +
  '  drop()                                delete a collection               ' + "\n" +
  '  load()                                load a collection into memory     ' + "\n" +
  '  unload()                              unload a collection from memory   ' + "\n" +
  '  rename(<new-name>)                    renames a collection              ' + "\n" +
  '  getIndexes()                          return defined indexes            ' + "\n" +
  '  refresh()                             refreshes the status and name     ' + "\n" +
  '  _help()                               this help                         ' + "\n" +
  '                                                                          ' + "\n" +
  'Document Functions:                                                       ' + "\n" +
  '  count()                               return number of documents        ' + "\n" +
  '  save(<data>)                          create document and return handle ' + "\n" +
  '  document(<id>)                        get document by handle (_id or _key)' + "\n" +
  '  replace(<id>, <data>, <overwrite>)    overwrite document                ' + "\n" +
  '  update(<id>, <data>, <overwrite>,     partially update document         ' + "\n" +
  '         <keepNull>)                                                      ' + "\n" +
  '  remove(<id>)                          delete document                   ' + "\n" +
  '  exists(<id>)                          checks whether a document exists  ' + "\n" +
  '  first()                               first inserted/updated document   ' + "\n" +
  '  last()                                last inserted/updated document    ' + "\n" +
  '                                                                          ' + "\n" +
  'Attributes:                                                               ' + "\n" +
  '  _database                             database object                   ' + "\n" +
  '  _id                                   collection identifier             ';

ArangoCollection.prototype._help = function () {
  internal.print(helpArangoCollection);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the name of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.name = function () {
  if (this._name === null) {
    this.refresh();
  }

  return this._name;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the status of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.status = function () {
  var result;

  if (this._status === null) {
    this.refresh();
  }

  // save original status
  result = this._status;

  if (this._status === ArangoCollection.STATUS_UNLOADING) {
    // if collection is currently unloading, we must not cache this info
    this._status = null;
  }

  // return the correct result
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the type of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.type = function () {
  if (this._type === null) {
    this.refresh();
  }

  return this._type;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets or sets the properties of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.properties = function (properties) {
  var attributes = {
    "doCompact": true,
    "journalSize": true,
    "isSystem": false,
    "isVolatile": false,
    "waitForSync": true,
    "shardKeys": false,
    "numberOfShards": false,
    "keyOptions": false,
    "indexBuckets": true
  };
  var a;

  var requestResult;
  if (properties === undefined) {
    requestResult = this._database._connection.GET(this._baseurl("properties"));

    arangosh.checkRequestResult(requestResult);
  }
  else {
    var body = {};

    for (a in attributes) {
      if (attributes.hasOwnProperty(a) &&
          attributes[a] &&
          properties.hasOwnProperty(a)) {
        body[a] = properties[a];
      }
    }

    requestResult = this._database._connection.PUT(this._baseurl("properties"),
      JSON.stringify(body));

    arangosh.checkRequestResult(requestResult);
  }

  var result = { };
  for (a in attributes) {
    if (attributes.hasOwnProperty(a) &&
        requestResult.hasOwnProperty(a) &&
        requestResult[a] !== undefined) {
      result[a] = requestResult[a];
    }
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rotate the journal of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.rotate = function () {
  var requestResult = this._database._connection.PUT(this._baseurl("rotate"), "");

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the figures of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.figures = function () {
  var requestResult = this._database._connection.GET(this._baseurl("figures"));

  arangosh.checkRequestResult(requestResult);

  return requestResult.figures;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the checksum of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.checksum = function (withRevisions, withData) {
  var append = '';
  if (withRevisions) {
    append += '?withRevisions=true';
  }
  if (withData) {
    append += (append === '' ? '?' : '&') + 'withData=true';
  }
  var requestResult = this._database._connection.GET(this._baseurl("checksum") + append);

  arangosh.checkRequestResult(requestResult);

  return {
    checksum: requestResult.checksum,
    revision: requestResult.revision
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the revision id of a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.revision = function () {
  var requestResult = this._database._connection.GET(this._baseurl("revision"));

  arangosh.checkRequestResult(requestResult);

  return requestResult.revision;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.drop = function () {
  var requestResult = this._database._connection.DELETE(this._baseurl());

  arangosh.checkRequestResult(requestResult);

  this._status = ArangoCollection.STATUS_DELETED;

  var database = this._database;
  var name;

  for (name in database) {
    if (database.hasOwnProperty(name)) {
      var collection = database[name];

      if (collection instanceof ArangoCollection) {
        if (collection.name() === this.name()) {
          delete database[name];
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.truncate = function () {
  var requestResult = this._database._connection.PUT(this._baseurl("truncate"), "");

  arangosh.checkRequestResult(requestResult);

  this._status = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.load = function (count) {
  var data = { count: true };

  // return the number of documents? this might slow down loading
  if (count !== undefined) {
    data.count = count;
  }

  var requestResult = this._database._connection.PUT(this._baseurl("load"), JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  this._status = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.unload = function () {
  var requestResult = this._database._connection.PUT(this._baseurl("unload"), "");

  arangosh.checkRequestResult(requestResult);

  this._status = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief renames a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.rename = function (name) {
  var body = { name : name };
  var requestResult = this._database._connection.PUT(this._baseurl("rename"), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  delete this._database[this._name];
  this._database[name] = this;

  this._status = null;
  this._name = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief refreshes a collection status and name
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.refresh = function () {
  var requestResult = this._database._connection.GET(
    this._database._collectionurl(this._id) + "?useId=true");

  arangosh.checkRequestResult(requestResult);

  this._name = requestResult.name;
  this._status = requestResult.status;
  this._type = requestResult.type;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   index functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets all indexes
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.getIndexes = function (withStats) {
  var requestResult = this._database._connection.GET(this._indexurl()+
          "&withStats="+(withStats || false));

  arangosh.checkRequestResult(requestResult);

  return requestResult.indexes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets one index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.index = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._database._connection.GET(this._database._indexurl(id, this.name()));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes an index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.dropIndex = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._database._connection.DELETE(this._database._indexurl(id, this.name()));

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum
      === internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a cap constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureCapConstraint = function (size, byteSize) {
  var body = {
    type : "cap",
    size : size || undefined,
    byteSize: byteSize || undefined
  };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a unique skip-list index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueSkiplist = function () {
  var body = addIndexOptions({
    type : "skiplist",
    unique : true
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a skip-list index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureSkiplist = function () {
  var body = addIndexOptions({
    type : "skiplist",
    unique : false
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a fulltext index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureFulltextIndex = function (field, minLength) {
  var body = {
    type: "fulltext",
    minLength: minLength || undefined,
    fields: [ field ]
  };

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a unique constraint
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureUniqueConstraint = function () {
  var body = addIndexOptions({
    type : "hash",
    unique : true
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a hash index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureHashIndex = function () {
  var body = addIndexOptions({
    type : "hash",
    unique : false
  }, arguments);

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a geo index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoIndex = function (lat, lon) {
  var body;

  if (typeof lat !== "string") {
    throw "usage: ensureGeoIndex(<lat>, <lon>) or ensureGeoIndex(<loc>[, <geoJson>])";
  }

  if (typeof lon === "boolean") {
    body = {
      type : "geo",
      fields : [ lat ],
      geoJson : lon
    };
  }
  else if (lon === undefined) {
    body = {
      type : "geo",
      fields : [ lat ],
      geoJson : false
    };
  }
  else {
    body = {
      type : "geo",
      fields : [ lat, lon ],
      geoJson: false
    };
  }

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures a geo constraint
/// since ArangoDB 2.5, this is just a redirection to ensureGeoIndex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureGeoConstraint = function (lat, lon) {
  return this.ensureGeoIndex(lat, lon);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ensures an index
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.ensureIndex = function (data) {
  if (typeof data !== "object" || Array.isArray(data)) {
    throw "usage: ensureIndex(<description>)";
  }

  var requestResult = this._database._connection.POST(this._indexurl(), JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the number of documents
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.count = function () {
  var requestResult = this._database._connection.GET(this._baseurl("count"));

  arangosh.checkRequestResult(requestResult);

  return requestResult.count;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a single document from the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev === null) {
    requestResult = this._database._connection.GET(this._documenturl(id));
  }
  else {
    requestResult = this._database._connection.GET(this._documenturl(id),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a specific document exists
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.exists = function (id) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev === null) {
    requestResult = this._database._connection.HEAD(this._documenturl(id));
  }
  else {
    requestResult = this._database._connection.HEAD(this._documenturl(id),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null &&
      requestResult.error === true &&
      (requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code ||
       requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code ||
       requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code)) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a random element from the collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.any = function () {
  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/any"),
    JSON.stringify({ collection: this._name }));

  arangosh.checkRequestResult(requestResult);

  return requestResult.document;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a query-by-example for a collection
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.firstExample = function (example) {
  var e;
  var i;

  // example is given as only argument
  if (arguments.length === 1) {
    e = example;
  }

  // example is given as list
  else {
    e = {};

    for (i = 0;  i < arguments.length;  i += 2) {
      e[arguments[i]] = arguments[i + 1];
    }
  }

  var data = {
    collection: this.name(),
    example: e
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/first-example"),
    JSON.stringify(data)
  );

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code) {
    return null;
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.document;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the first document(s) from the collection
/// If an argument is supplied, the result will be a list of the first n
/// documents. When no argument is supplied, the result is the first document
/// from the collection, or null if the collection is empty.
/// The document order is determined by the insertion/update order.
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.first = function (count) {
  var body = {
    collection: this.name(),
    count: count
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/first"),
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the last document(s) from the collection
/// If an argument is supplied, the result will be a list of the last n
/// documents. When no argument is supplied, the result is the last document
/// from the collection, or null if the collection is empty.
/// The document order is determined by the insertion/update order.
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.last = function (count) {
  var body = {
    collection: this.name(),
    count: count
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/last"),
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a document in the collection
/// note: this method is used to save documents and edges, but save() has a
/// different signature for both. For document collections, the signature is
/// save(<data>, <waitForSync>), whereas for edge collections, the signature is
/// save(<from>, <to>, <data>, <waitForSync>)
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.save =
ArangoCollection.prototype.insert = function (from, to, data, waitForSync) {
  var type = this.type(), url;

  if (type === undefined) {
    type = ArangoCollection.TYPE_DOCUMENT;
  }

  if (type === ArangoCollection.TYPE_DOCUMENT) {
    data = from;
    waitForSync = to;
    url = "/_api/document?collection=" + encodeURIComponent(this.name());
  }
  else if (type === ArangoCollection.TYPE_EDGE) {
    if (typeof from === 'object' && from.hasOwnProperty("_id")) {
      from = from._id;
    }

    if (typeof to === 'object' && to.hasOwnProperty("_id")) {
      to = to._id;
    }

    url = "/_api/edge?collection=" + encodeURIComponent(this.name())
        + "&from=" + encodeURIComponent(from)
        + "&to=" + encodeURIComponent(to);
  }

  url = this._appendSyncParameter(url, waitForSync);

  if (data === undefined || typeof data !== 'object') {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_TYPE_INVALID.message
    });
  }

  var requestResult = this._database._connection.POST(
    this._prefixurl(url),
    JSON.stringify(data)
  );

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a document in the collection
/// @param id the id of the document
/// @param overwrite (optional) a boolean value or a json object
/// @param waitForSync (optional) a boolean value .
/// @example remove("example/996280832675")
/// @example remove("example/996280832675", true)
/// @example remove("example/996280832675", false)
/// @example remove("example/996280832675", {waitForSync: false, overwrite: true})
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.remove = function (id, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";

  if (typeof overwrite === "object") {
    // we assume the caller uses new signature (id, data, options)
    if (typeof waitForSync !== "undefined") {
      throw "too many arguments";
    }
    var options = overwrite;
    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "?policy=last";
    }
    if (options.hasOwnProperty("waitForSync") ) {
     waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      params += "?policy=last";
    }
  }

  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._database._connection.DELETE(url);
  }
  else {
    requestResult = this._database._connection.DELETE(url,
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (overwrite) {
      if (requestResult.errorNum === internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces a document in the collection
/// @param id the id of the document
/// @param overwrite (optional) a boolean value or a json object
/// @param waitForSync (optional) a boolean value .
/// @example replace("example/996280832675", { a : 1, c : 2} )
/// @example replace("example/996280832675", { a : 1, c : 2}, true)
/// @example replace("example/996280832675", { a : 1, c : 2}, false)
/// @example replace("example/996280832675", { a : 1, c : 2}, {waitForSync: false, overwrite: true})
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replace = function (id, data, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";
  if (typeof overwrite === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "?policy=last";
    }
    if (options.hasOwnProperty("waitForSync") ) {
     waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      params += "?policy=last";
    }
  }
  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);
  if (rev === null) {
    requestResult = this._database._connection.PUT(url, JSON.stringify(data));
  }
  else {
    requestResult = this._database._connection.PUT(url, JSON.stringify(data),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection
/// @param id the id of the document
/// @param overwrite (optional) a boolean value or a json object
/// @param keepNull (optional) determines if null values should saved or not
/// @param mergeObjects (optional) whether or not object values should be merged
/// @param waitForSync (optional) a boolean value .
/// @example update("example/996280832675", { a : 1, c : 2} )
/// @example update("example/996280832675", { a : 1, c : 2, x: null}, true, true, true)
/// @example update("example/996280832675", { a : 1, c : 2, x: null},
//                 {keepNull: true, waitForSync: false, overwrite: true})
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.update = function (id, data, overwrite, keepNull, waitForSync) {
  var rev = null;
  var requestResult;

  if (id === undefined || id === null) {
    throw new ArangoError({
      errorNum : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage : internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";
  if (typeof overwrite === "object") {
    if (typeof keepNull !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (! options.hasOwnProperty("keepNull")) {
      options.keepNull = true;
    }
    params = "?keepNull=" + options.keepNull;

    if (! options.hasOwnProperty("mergeObjects")) {
      options.mergeObjects = true;
    }
    params += "&mergeObjects=" + options.mergeObjects;

    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "&policy=last";
    }

  } else {

    // set default value for keepNull
    var keepNullValue = ((typeof keepNull === "undefined") ? true : keepNull);
    params = "?keepNull=" + (keepNullValue ? "true" : "false");

    if (overwrite) {
      params += "&policy=last";
    }

  }

  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._database._connection.PATCH(url, JSON.stringify(data));
  }
  else {
    requestResult = this._database._connection.PATCH(url, JSON.stringify(data),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting or ending in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.edges = function (vertex) {
  return this._edgesQuery(vertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges ending in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.inEdges = function (vertex) {
  return this._edgesQuery(vertex, "in");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the edges starting in a vertex
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.outEdges = function (vertex) {
  return this._edgesQuery(vertex, "out");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByExample = function (example,
                                                       waitForSync,
                                                       limit) {
  var data = {
    collection: this._name,
    example: example,
    waitForSync: waitForSync,
    limit: limit
  };

  if (typeof waitForSync === "object") {
    if (typeof limit !== "undefined") {
      throw "too many parameters";
    }
    data = {
      collection: this._name,
      example: example,
      options: waitForSync
    };
  }

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/remove-by-example"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult.deleted;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replaces documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.replaceByExample = function (example,
                                                        newValue,
                                                        waitForSync,
                                                        limit) {
  var data = {
    collection: this._name,
    example: example,
    newValue: newValue,
    waitForSync: waitForSync,
    limit: limit
  };

  if (typeof waitForSync === "object") {
    if (typeof limit !== "undefined") {
      throw "too many parameters";
    }
    data = {
      collection: this._name,
      example: example,
      newValue: newValue,
      options: waitForSync
    };
  }
  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/replace-by-example"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult.replaced;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief updates documents matching an example
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.updateByExample = function (example,
                                                       newValue,
                                                       keepNull,
                                                       waitForSync,
                                                       limit) {
  var data = {
    collection: this._name,
    example: example,
    newValue: newValue,
    keepNull: keepNull,
    waitForSync: waitForSync,
    limit: limit
  };
  if (typeof keepNull === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many parameters";
    }
    var options = keepNull;
    data = {
      collection: this._name,
      example: example,
      newValue: newValue,
      options: options
    };
  }
  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/update-by-example"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return requestResult.updated;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up documents by keys
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.documents = function (keys) {
  var data = {
    collection: this._name,
    keys: keys || [ ]
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/lookup-by-keys"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return {
    documents: requestResult.documents
  };
};

// .lookupByKeys is now an alias for .documents
ArangoCollection.prototype.lookupByKeys = ArangoCollection.prototype.documents;

////////////////////////////////////////////////////////////////////////////////
/// @brief removes documents by keys
////////////////////////////////////////////////////////////////////////////////

ArangoCollection.prototype.removeByKeys = function (keys) {
  var data = {
    collection: this._name,
    keys: keys || [ ]
  };

  var requestResult = this._database._connection.PUT(
    this._prefixurl("/_api/simple/remove-by-keys"),
    JSON.stringify(data));

  arangosh.checkRequestResult(requestResult);

  return {
    removed: requestResult.removed,
    ignored: requestResult.ignored
  };
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb/arango-database", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDatabase
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

// -----------------------------------------------------------------------------
// --SECTION--                                                    ArangoDatabase
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

var ArangoCollection;

function ArangoDatabase (connection) {
  this._connection = connection;
  this._collectionConstructor = ArangoCollection;
  this._properties = null;

  this._registerCollection = function (name, obj) {
    // store the collection in our own list
    this[name] = obj;
  };
}

exports.ArangoDatabase = ArangoDatabase;

// load after exporting ArangoDatabase
ArangoCollection = require("org/arangodb/arango-collection").ArangoCollection;
var ArangoError = require("org/arangodb").ArangoError;
var ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief index id regex
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.indexRegex = /^([a-zA-Z0-9\-_]+)\/([0-9]+)$/;

////////////////////////////////////////////////////////////////////////////////
/// @brief key regex
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.keyRegex = /^([a-zA-Z0-9_:\-@\.\(\)\+,=;\$!\*'%])+$/;

////////////////////////////////////////////////////////////////////////////////
/// @brief append the waitForSync parameter to a URL
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._appendSyncParameter = function (url, waitForSync) {
  if (waitForSync) {
    if (url.indexOf('?') === -1) {
      url += '?';
    }
    else {
      url += '&';
    }
    url += 'waitForSync=true';
  }
  return url;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for collection usage
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collectionurl = function (id) {
  if (id === undefined) {
    return "/_api/collection";
  }

  return "/_api/collection/" + encodeURIComponent(id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for document usage
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._documenturl = function (id, expectedName) {
  var s = id.split("/");

  if (s.length !== 2) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }
  else if (expectedName !== undefined && expectedName !== "" && s[0] !== expectedName) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code,
      errorMessage: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.message
    });
  }

  if (ArangoDatabase.keyRegex.exec(s[1]) === null) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message
    });
  }

  return "/_api/document/" + encodeURIComponent(s[0]) + "/" + encodeURIComponent(s[1]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the base url for index usage
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._indexurl = function (id, expectedName) {
  if (typeof id === "string") {
    var pa = ArangoDatabase.indexRegex.exec(id);

    if (pa === null && expectedName !== undefined) {
      id = expectedName + "/" + id;
    }
  }
  else if (typeof id === "number" && expectedName !== undefined) {
    // stringify a numeric id
    id = expectedName + "/" + id;
  }

  var s = id.split("/");

  if (s.length !== 2) {
    // invalid index handle
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.code,
      errorMessage: internal.errors.ERROR_ARANGO_INDEX_HANDLE_BAD.message
    });
  }
  else if (expectedName !== undefined && expectedName !== "" && s[0] !== expectedName) {
    // index handle does not match collection name
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.code,
      errorMessage: internal.errors.ERROR_ARANGO_CROSS_COLLECTION_REQUEST.message
    });
  }

  return "/_api/index/" + encodeURIComponent(s[0]) + "/" + encodeURIComponent(s[1]);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the help for ArangoDatabase
////////////////////////////////////////////////////////////////////////////////

var helpArangoDatabase = arangosh.createHelpHeadline("ArangoDatabase (db) help") +
  'Administration Functions:                                                 ' + "\n" +
  '  _help()                               this help                         ' + "\n" +
  '  _flushCache()                         flush and refill collection cache ' + "\n" +
  '                                                                          ' + "\n" +
  'Collection Functions:                                                     ' + "\n" +
  '  _collections()                        list all collections              ' + "\n" +
  '  _collection(<name>)                   get collection by identifier/name ' + "\n" +
  '  _create(<name>, <properties>)         creates a new collection          ' + "\n" +
  '  _createEdgeCollection(<name>)         creates a new edge collection     ' + "\n" +
  '  _drop(<name>)                         delete a collection               ' + "\n" +
  '                                                                          ' + "\n" +
  'Document Functions:                                                       ' + "\n" +
  '  _document(<id>)                       get document by handle (_id)      ' + "\n" +
  '  _replace(<id>, <data>, <overwrite>)   overwrite document                ' + "\n" +
  '  _update(<id>, <data>, <overwrite>,    partially update document         ' + "\n" +
  '          <keepNull>)                                                     ' + "\n" +
  '  _remove(<id>)                         delete document                   ' + "\n" +
  '  _exists(<id>)                         checks whether a document exists  ' + "\n" +
  '  _truncate()                           delete all documents              ' + "\n" +
  '                                                                          ' + "\n" +
  'Database Management Functions:                                            ' + "\n" +
  '  _createDatabase(<name>)               creates a new database            ' + "\n" +
  '  _dropDatabase(<name>)                 drops an existing database        ' + "\n" +
  '  _useDatabase(<name>)                  switches into an existing database' + "\n" +
  '  _drop(<name>)                         delete a collection               ' + "\n" +
  '  _name()                               name of the current database      ' + "\n" +
  '                                                                          ' + "\n" +
  'Query / Transaction Functions:                                            ' + "\n" +
  '  _executeTransaction(<transaction>)    execute transaction               ' + "\n" +
  '  _query(<query>)                       execute AQL query                 ' + "\n" +
  '  _createStatement(<data>)              create and return AQL query       ';

ArangoDatabase.prototype._help = function () {
  internal.print(helpArangoDatabase);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the database object
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype.toString = function () {
  return "[object ArangoDatabase \"" + this._name() + "\"]";
};

// -----------------------------------------------------------------------------
// --SECTION--                                              collection functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return all collections from the database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collections = function () {
  var requestResult = this._connection.GET(this._collectionurl());

  arangosh.checkRequestResult(requestResult);

  if (requestResult.collections !== undefined) {
    var collections = requestResult.collections;
    var result = [];
    var i;

    // add all collentions to object
    for (i = 0;  i < collections.length;  ++i) {
      var collection = new this._collectionConstructor(this, collections[i]);
      this._registerCollection(collection._name, collection);
      result.push(collection);
    }

    return result;
  }

  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single collection, identified by its id or name
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._collection = function (id) {
  var url;

  if (typeof id === "number") {
    url = this._collectionurl(id) + "?useId=true";
  }
  else {
    url = this._collectionurl(id);
  }

  var requestResult = this._connection.GET(url);

  // return null in case of not found
  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
    return null;
  }

  // check all other errors and throw them
  arangosh.checkRequestResult(requestResult);

  var name = requestResult.name;

  if (name !== undefined) {
    this._registerCollection(name, new this._collectionConstructor(this, requestResult));
    return this[name];
  }

  return null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new collection
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._create = function (name, properties, type) {
  var body = {
    "name" : name,
    "type" : ArangoCollection.TYPE_DOCUMENT
  };

  if (properties !== undefined) {
    [ "waitForSync", "journalSize", "isSystem", "isVolatile",
      "doCompact", "keyOptions", "shardKeys", "numberOfShards",
      "distributeShardsLike", "indexBuckets" ].forEach(function(p) {
      if (properties.hasOwnProperty(p)) {
        body[p] = properties[p];
      }
    });
  }

  if (type !== undefined) {
    body.type = type;
  }

  var requestResult = this._connection.POST(this._collectionurl(),
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  var nname = requestResult.name;

  if (nname !== undefined) {
    this._registerCollection(nname, new this._collectionConstructor(this, requestResult));
    return this[nname];
  }

  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new document collection
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createDocumentCollection = function (name, properties) {
  return this._create(name, properties, ArangoCollection.TYPE_DOCUMENT);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new edges collection
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createEdgeCollection = function (name, properties) {
  return this._create(name, properties, ArangoCollection.TYPE_EDGE);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief truncates a collection
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._truncate = function (id) {
  var name;

  if (typeof id !== "string") {
    id = id._id;
  }

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof this._collectionConstructor) {
        if (collection._id === id || collection._name === id) {
          return collection.truncate();
        }
      }
    }
  }

  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops a collection
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._drop = function (id) {
  var name;

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof this._collectionConstructor) {
        if (collection._id === id || collection._name === id) {
          return collection.drop();
        }
      }
    }
  }

  var c = this._collection(id);
  if (c) {
    return c.drop();
  }
  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief flush the local cache
/// this is called by connection.reconnect()
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._flushCache = function () {
  var name;

  for (name in this) {
    if (this.hasOwnProperty(name)) {
      var collection = this[name];

      if (collection instanceof this._collectionConstructor) {
        // reset the collection status
        collection._status = null;
        this[name] = undefined;
      }
    }
  }

  try {
    // repopulate cache
    this._collections();
  }
  catch (err) {
  }

  this._properties = null;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief query the database properties
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._queryProperties = function (force) {
  if (force || this._properties === null) {
    var url = "/_api/database/current";
    var requestResult = this._connection.GET(url);

    arangosh.checkRequestResult(requestResult);
    this._properties = requestResult.result;
  }

  return this._properties;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the database id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._id = function () {
  return this._queryProperties().id;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether or not the current database is the system database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._isSystem = function () {
  return this._queryProperties().isSystem;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the name of the current database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._name = function () {
  return this._queryProperties().name;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the path of the current database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._path = function () {
  return this._queryProperties().path;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns one index
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._index = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._connection.GET(this._indexurl(id));

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes one index
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._dropIndex = function (id) {
  if (id.hasOwnProperty("id")) {
    id = id.id;
  }

  var requestResult = this._connection.DELETE(this._indexurl(id));

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_ARANGO_INDEX_NOT_FOUND.code) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the database version
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._version = function () {
  var requestResult = this._connection.GET("/_api/version");

  arangosh.checkRequestResult(requestResult);

  return requestResult.version;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                document functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a single document from the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._document = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev === null) {
    requestResult = this._connection.GET(this._documenturl(id));
  }
  else {
    requestResult = this._connection.GET(this._documenturl(id),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null
      && requestResult.error === true
      && requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether a document exists, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._exists = function (id) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  if (rev === null) {
    requestResult = this._connection.HEAD(this._documenturl(id));
  }
  else {
    requestResult = this._connection.HEAD(this._documenturl(id),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null &&
      requestResult.error === true &&
      (requestResult.errorNum === internal.errors.ERROR_ARANGO_COLLECTION_NOT_FOUND.code ||
       requestResult.errorNum === internal.errors.ERROR_HTTP_NOT_FOUND.code ||
       requestResult.errorNum === internal.errors.ERROR_HTTP_PRECONDITION_FAILED.code)) {
    return false;
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._remove = function (id, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";

  if (typeof overwrite === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "?policy=last";
    }
    if (options.hasOwnProperty("waitForSync") ) {
      waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      params += "?policy=last";
    }
  }

  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._connection.DELETE(url);
  }
  else {
    requestResult = this._connection.DELETE(url,
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    if (overwrite) {
      if (requestResult.errorNum === internal.errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
        return false;
      }
    }

    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._replace = function (id, data, overwrite, waitForSync) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";

  if (typeof overwrite === "object") {
    if (typeof waitForSync !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "?policy=last";
    }
    if (options.hasOwnProperty("waitForSync") ) {
     waitForSync = options.waitForSync;
    }
  } else {
    if (overwrite) {
      params += "?policy=last";
    }
  }
  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._connection.PUT(url, JSON.stringify(data));
  }
  else {
    requestResult = this._connection.PUT(url, JSON.stringify(data),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief update a document in the collection, identified by its id
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._update = function (id, data, overwrite, keepNull, waitForSync) {
  var rev = null;
  var requestResult;

  if (id.hasOwnProperty("_id")) {
    if (id.hasOwnProperty("_rev")) {
      rev = id._rev;
    }

    id = id._id;
  }

  var params = "";
  if (typeof overwrite === "object") {
    if (typeof keepNull !== "undefined") {
      throw "too many arguments";
    }
    // we assume the caller uses new signature (id, data, options)
    var options = overwrite;
    if (! options.hasOwnProperty("keepNull")) {
      options.keepNull = true;
    }
    params = "?keepNull=" + options.keepNull;
    if (! options.hasOwnProperty("mergeObjects")) {
      options.mergeObjects = true;
    }
    params += "&mergeObjects=" + options.mergeObjects;

    if (options.hasOwnProperty("overwrite") && options.overwrite) {
      params += "&policy=last";
    }
  } else {
    // set default value for keepNull
    var keepNullValue = ((typeof keepNull === "undefined") ? true : keepNull);
    params = "?keepNull=" + (keepNullValue ? "true" : "false");

    if (overwrite) {
      params += "&policy=last";
    }
  }
  var url = this._documenturl(id) + params;
  url = this._appendSyncParameter(url, waitForSync);

  if (rev === null) {
    requestResult = this._connection.PATCH(url, JSON.stringify(data));
  }
  else {
    requestResult = this._connection.PATCH(url, JSON.stringify(data),
      {'if-match' : JSON.stringify(rev) });
  }

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create a new statement
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createStatement = function (data) {
  return new ArangoStatement(this, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief factory method to create and execute a new statement
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._query = function (query, bindVars, cursorOptions, options) {
  if (typeof query === "object" && query !== null && arguments.length === 1) {
    return new ArangoStatement(this, query).execute();
  }

  var data = {
    query: query,
    bindVars: bindVars || undefined,
    count: (cursorOptions && cursorOptions.count) || false,
    batchSize: (cursorOptions && cursorOptions.batchSize) || undefined,
    options: options || undefined,
    cache: (options && options.cache) || undefined
  };

  return new ArangoStatement(this, data).execute();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                database functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._createDatabase = function (name, options, users) {
  var data = {
    name: name,
    options: options || { },
    users: users || [ ]
  };

  var requestResult = this._connection.POST("/_api/database", JSON.stringify(data));

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drop an existing database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._dropDatabase = function (name) {
  var requestResult = this._connection.DELETE("/_api/database/" + encodeURIComponent(name));

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief list all existing databases
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._listDatabases = function () {
  var requestResult = this._connection.GET("/_api/database");

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief uses a database
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._useDatabase = function (name) {
  if (internal.printBrowser) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_NOT_IMPLEMENTED.code,
      errorNum: internal.errors.ERROR_NOT_IMPLEMENTED.code,
      errorMessage: "_useDatabase() is not supported in the web interface"
    });
  }

  var old = this._connection.getDatabaseName();

  // no change
  if (name === old) {
    return true;
  }

  this._connection.setDatabaseName(name);

  try {
    // re-query properties
    this._queryProperties(true);
    this._flushCache();
  }
  catch (err) {
    this._connection.setDatabaseName(old);

    if (err.hasOwnProperty("errorNum")) {
      throw err;
    }

    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: "cannot use database '" + name + "'"
    });
  }

  return true;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                         endpoints
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lists all endpoints
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._listEndpoints = function () {
  var requestResult = this._connection.GET("/_api/endpoint");

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      transactions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a transaction
////////////////////////////////////////////////////////////////////////////////

ArangoDatabase.prototype._executeTransaction = function (data) {
  if (! data || typeof(data) !== 'object') {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: "usage: _executeTransaction(<object>)"
    });
  }

  if (! data.collections || typeof(data.collections) !== 'object') {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: "missing/invalid collections definition for transaction"
    });
  }

  if (! data.action ||
      (typeof(data.action) !== 'string' && typeof(data.action) !== 'function')) {
    throw new ArangoError({
      error: true,
      code: internal.errors.ERROR_HTTP_BAD_PARAMETER.code,
      errorNum: internal.errors.ERROR_BAD_PARAMETER.code,
      errorMessage: "missing/invalid action definition for transaction"
    });
  }

  if (typeof(data.action) === 'function') {
    data.action = String(data.action);
  }

  var requestResult = this._connection.POST("/_api/transaction",
    JSON.stringify(data));

  if (requestResult !== null && requestResult.error === true) {
    throw new ArangoError(requestResult);
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult.result;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb/arango-query-cursor", function(exports, module) {
/*jshint strict: false */
/*global more:true */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoQueryCursor
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

// -----------------------------------------------------------------------------
// --SECTION--                                                 ArangoQueryCursor
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function ArangoQueryCursor (database, data) {
  this._database = database;
  this._dbName = database._name();
  this.data = data;
  this._hasNext = false;
  this._hasMore = false;
  this._pos = 0;
  this._count = 0;
  this._total = 0;

  if (data.result !== undefined) {
    this._count = data.result.length;

    if (this._pos < this._count) {
      this._hasNext = true;
    }

    if (data.hasMore !== undefined && data.hasMore) {
      this._hasMore = true;
    }
  }
}

exports.ArangoQueryCursor = ArangoQueryCursor;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the cursor
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.toString = function () {
  var isCaptureModeActive = internal.isCaptureMode();
  var rows = [ ], i = 0;
  while (++i <= 10 && this.hasNext()) {
    rows.push(this.next());
  }

  var result = "[object ArangoQueryCursor";

  if (this.data.id) {
    result += " " + this.data.id;
  }

  if (this._count !== null && this._count !== undefined) {
    result += ", count: " + this._count;
  }

  result += ", hasMore: " + (this.hasNext() ? "true" : "false");

  result += "]";

  if (!isCaptureModeActive) {
    internal.print(result);
    result = "";
  }
  if (rows.length > 0) {
    if (!isCaptureModeActive) {
      var old = internal.startCaptureMode();
      internal.print(rows);
      result += "\n\n" + internal.stopCaptureMode(old);
    }
    else {
      internal.print(rows);
    }

    if (this.hasNext()) {
      result += "\ntype 'more' to show more documents\n";
      more = this; // assign cursor to global variable more!
    }
  }

  if (!isCaptureModeActive) {
    internal.print(result);
    result = "";
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return all remaining result documents from the cursor
///
/// If no more results are available locally but more results are available on
/// the server, this function will make one or multiple roundtrips to the
/// server. Calling this function will also fully exhaust the cursor.
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.toArray = function () {
  var result = [];

  while (this.hasNext()) {
    result.push(this.next());
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print the help for the cursor
////////////////////////////////////////////////////////////////////////////////

var helpArangoQueryCursor = arangosh.createHelpHeadline("ArangoQueryCursor help") +
  'ArangoQueryCursor constructor:                                      ' + "\n" +
  ' > cursor = stmt.execute()                                          ' + "\n" +
  'Functions:                                                          ' + "\n" +
  '  hasNext()                             returns true if there are   ' + "\n" +
  '                                        more results to fetch       ' + "\n" +
  '  next()                                returns the next document   ' + "\n" +
  '  toArray()                             returns all data from the cursor' + "\n" +
  '  _help()                               this help                   ' + "\n" +
  'Attributes:                                                         ' + "\n" +
  '  _database                             database object             ' + "\n" +
  'Example:                                                            ' + "\n" +
  ' > stmt = db._createStatement({ "query": "FOR c IN coll RETURN c" })' + "\n" +
  ' > cursor = stmt.execute()                                          ' + "\n" +
  ' > documents = cursor.toArray()                                     ' + "\n" +
  ' > cursor = stmt.execute()                                          ' + "\n" +
  ' > while (cursor.hasNext()) { print(cursor.next())  }               ';

ArangoQueryCursor.prototype._help = function () {
  internal.print(helpArangoQueryCursor);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether there are more results available in the cursor
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.hasNext = function () {
  return this._hasNext;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the next result document from the cursor
///
/// If no more results are available locally but more results are available on
/// the server, this function will make a roundtrip to the server
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.next = function () {
  if (! this._hasNext) {
    throw "No more results";
  }

  var result = this.data.result[this._pos];
  this._pos++;

  // reached last result
  if (this._pos === this._count) {
    this._hasNext = false;
    this._pos = 0;

    if (this._hasMore && this.data.id) {
      this._hasMore = false;

      // load more results
      var requestResult = this._database._connection.PUT(this._baseurl(), "");

      arangosh.checkRequestResult(requestResult);

      this.data = requestResult;
      this._count = requestResult.result.length;

      if (this._pos < this._count) {
        this._hasNext = true;
      }

      if (requestResult.hasMore !== undefined && requestResult.hasMore) {
        this._hasMore = true;
      }
    }
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief explicitly dispose the cursor
///
/// Calling this function will mark the cursor as deleted on the server. It will
/// therefore make a roundtrip to the server. Using a cursor after it has been
/// disposed is considered a user error
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.dispose = function () {
  if (! this.data.id) {
    // client side only cursor
    return;
  }

  var requestResult = this._database._connection.DELETE(this._baseurl(), "");

  arangosh.checkRequestResult(requestResult);

  this.data.id = undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the total number of documents in the cursor
///
/// The number will remain the same regardless how much result documents have
/// already been fetched from the cursor.
///
/// This function will return the number only if the cursor was constructed
/// with the "doCount" attribute. Otherwise it will return undefined.
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.count = function () {
  return this.data.count;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return extra data stored for the cursor (if any)
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype.getExtra = function () {
  return this.data.extra || { };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return baseurl for query cursor
////////////////////////////////////////////////////////////////////////////////

ArangoQueryCursor.prototype._baseurl = function () {
  return "/_db/" + encodeURIComponent(this._dbName) +
         "/_api/cursor/" + encodeURIComponent(this.data.id);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint\\)"
// End:
});

module.define("org/arangodb/arango-statement-common", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Arango statements
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoStatement
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

function ArangoStatement (database, data) {
  this._database = database;
  this._doCount = false;
  this._batchSize = null;
  this._bindVars = {};
  this._options = undefined;
  this._cache = undefined;

  if (! data) {
    throw "ArangoStatement needs initial data";
  }

  if (typeof data === "string") {
    data = { query: data };
  }
  else if (typeof data === 'object' && typeof data.toAQL === 'function') {
    data = { query: data.toAQL() };
  }

  if (! (data instanceof Object)) {
    throw "ArangoStatement needs initial data";
  }

  if (data.query === undefined || data.query === "") {
    throw "ArangoStatement needs a valid query attribute";
  }
  this.setQuery(data.query);

  if (data.bindVars instanceof Object) {
    this.bind(data.bindVars);
  }
  if (data.options instanceof Object) {
    this.setOptions(data.options);
  }
  if (data.count !== undefined) {
    this.setCount(data.count);
  }
  if (data.batchSize !== undefined) {
    this.setBatchSize(data.batchSize);
  }
  if (data.cache !== undefined) {
    this.setCache(data.cache);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief binds a parameter to the statement
///
/// This function can be called multiple times, once for each bind parameter.
/// All bind parameters will be transferred to the server in one go when
/// execute() is called.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.bind = function (key, value) {
  if (key instanceof Object) {
    if (value !== undefined) {
      throw "invalid bind parameter declaration";
    }

    this._bindVars = key;
  }
  else if (typeof(key) === "string") {
    this._bindVars[key] = value;
  }
  else if (typeof(key) === "number") {
    var strKey = String(parseInt(key, 10));

    if (strKey !== String(key)) {
      throw "invalid bind parameter declaration";
    }

    this._bindVars[strKey] = value;
  }
  else {
    throw "invalid bind parameter declaration";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the bind variables already set
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getBindVariables = function () {
  return this._bindVars;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the cache flag for the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getCache = function () {
  return this._cache;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the count flag for the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getCount = function () {
  return this._doCount;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the maximum number of results documents the cursor will return
/// in a single server roundtrip.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getBatchSize = function () {
  return this._batchSize;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets the user options
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getOptions = function () {
  return this._options;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets query string
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.getQuery = function () {
  return this._query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the cache flag for the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setCache = function (bool) {
  this._cache = bool ? true : false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the count flag for the statement
///
/// Setting the count flag will make the statement's result cursor return the
/// total number of result documents. The count flag is not set by default.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setCount = function (bool) {
  this._doCount = bool ? true : false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the maximum number of results documents the cursor will return
/// in a single server roundtrip.
/// The higher this number is, the less server roundtrips will be made when
/// iterating over the result documents of a cursor.
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setBatchSize = function (value) {
  var batch = parseInt(value, 10);

  if (batch > 0) {
    this._batchSize = batch;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the user options
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setOptions = function (value) {
  this._options = value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the query string
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.setQuery = function (query) {
  this._query = (query && typeof query.toAQL === 'function') ? query.toAQL() : query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief parses a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  throw "cannot call abstract method parse()";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief explains a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.explain = function () {
  throw "cannot call abstract method explain()";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be printed
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  throw "cannot call abstract method execute()";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.ArangoStatement = ArangoStatement;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb/arango-statement", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoStatement
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

var ArangoStatement = require("org/arangodb/arango-statement-common").ArangoStatement;
var ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor;

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoStatement
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a string representation of the statement
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.toString = function () {
  return arangosh.getIdString(this, "ArangoStatement");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the help for ArangoStatement
////////////////////////////////////////////////////////////////////////////////

var helpArangoStatement = arangosh.createHelpHeadline("ArangoStatement help") +
  'Create an AQL query:                                                    ' + "\n" +
  ' > stmt = new ArangoStatement(db, { "query": "FOR..." })                ' + "\n" +
  ' > stmt = db._createStatement({ "query": "FOR..." })                    ' + "\n" +
  'Set query options:                                                      ' + "\n" +
  ' > stmt.setBatchSize(<value>)           set the max. number of results  ' + "\n" +
  '                                        to be transferred per roundtrip ' + "\n" +
  ' > stmt.setCount(<value>)               set count flag (return number of' + "\n" +
  '                                        results in "count" attribute)   ' + "\n" +
  'Get query options:                                                      ' + "\n" +
  ' > stmt.setBatchSize()                  return the max. number of results' + "\n" +
  '                                        to be transferred per roundtrip ' + "\n" +
  ' > stmt.getCount()                      return count flag (return number' + "\n" +
  '                                        of results in "count" attribute)' + "\n" +
  ' > stmt.getQuery()                      return query string             ' + "\n" +
  '                                        results in "count" attribute)   ' + "\n" +
  'Bind parameters to a query:                                             ' + "\n" +
  ' > stmt.bind(<key>, <value>)            bind single variable            ' + "\n" +
  ' > stmt.bind(<values>)                  bind multiple variables         ' + "\n" +
  'Execute query:                                                          ' + "\n" +
  ' > cursor = stmt.execute()              returns a cursor                ' + "\n" +
  'Get all results in an array:                                            ' + "\n" +
  ' > docs = cursor.toArray()                                              ' + "\n" +
  'Or loop over the result set:                                            ' + "\n" +
  ' > while (cursor.hasNext()) { print(cursor.next()) }                    ';

ArangoStatement.prototype._help = function () {
  internal.print(helpArangoStatement);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.parse = function () {
  var body = {
    query: this._query
  };

  var requestResult = this._database._connection.POST(
    "/_api/query",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  var result = { 
    bindVars: requestResult.bindVars, 
    collections: requestResult.collections,
    ast: requestResult.ast 
  };
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief explain a query and return the results
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.explain = function (options) {
  var opts = this._options || { };
  if (typeof opts === 'object' && typeof options === 'object') {
    Object.keys(options).forEach(function(o) {
      // copy options
      opts[o] = options[o];
    });
  }

  var body = {
    query: this._query,
    bindVars: this._bindVars,
    options: opts
  };

  var requestResult = this._database._connection.POST(
    "/_api/explain",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  if (opts && opts.allPlans) {
    return {
      plans: requestResult.plans,
      warnings: requestResult.warnings,
      stats: requestResult.stats
    };
  }
  else {
    return {
      plan: requestResult.plan,
      warnings: requestResult.warnings,
      stats: requestResult.stats
    };
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the query
///
/// Invoking execute() will transfer the query and all bind parameters to the
/// server. It will return a cursor with the query results in case of success.
/// In case of an error, the error will be printed
////////////////////////////////////////////////////////////////////////////////

ArangoStatement.prototype.execute = function () {
  var body = {
    query: this._query,
    count: this._doCount,
    bindVars: this._bindVars
  };

  if (this._batchSize) {
    body.batchSize = this._batchSize;
  }

  if (this._options) {
    body.options = this._options;
  }

  if (this._cache !== undefined) {
    body.cache = this._cache;
  }

  var requestResult = this._database._connection.POST(
    "/_api/cursor",
    JSON.stringify(body));

  arangosh.checkRequestResult(requestResult);

  return new ArangoQueryCursor(this._database, requestResult);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.ArangoStatement = ArangoStatement;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb/arangosh", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "arangosh"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return a formatted type string for object
///
/// If the object has an id, it will be included in the string.
////////////////////////////////////////////////////////////////////////////////

exports.getIdString = function (object, typeName) {
  var result = "[object " + typeName;

  if (object._id) {
    result += ":" + object._id;
  }
  else if (object.data && object.data._id) {
    result += ":" + object.data._id;
  }

  result += "]";

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a formatted headline text
////////////////////////////////////////////////////////////////////////////////

exports.createHelpHeadline = function (text) {
  var i;
  var p = "";
  var x = Math.abs(78 - text.length) / 2;

  for (i = 0; i < x; ++i) {
    p += "-";
  }

  return "\n" + p + " " + text + " " + p + "\n";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief handles error results
///
/// throws an exception in case of an an error
////////////////////////////////////////////////////////////////////////////////

// must came after the export of createHelpHeadline
var arangodb = require("org/arangodb");
var ArangoError = arangodb.ArangoError;

exports.checkRequestResult = function (requestResult) {
  if (requestResult === undefined) {
    throw new ArangoError({
      "error" : true,
      "code"  : 500,
      "errorNum" : arangodb.ERROR_INTERNAL,
      "errorMessage" : "Unknown error. Request result is empty"
    });
  }

  if (requestResult.hasOwnProperty('error')) {
    if (requestResult.error) {
      if (requestResult.errorNum === arangodb.ERROR_TYPE_ERROR) {
        throw new TypeError(requestResult.errorMessage);
      }

      throw new ArangoError(requestResult);
    }

    // remove the property from the original object
    delete requestResult.error;
  }

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief general help
////////////////////////////////////////////////////////////////////////////////

exports.HELP = exports.createHelpHeadline("Help") +
  'Predefined objects:                                                ' + "\n" +
  '  arango:                               ArangoConnection           ' + "\n" +
  '  db:                                   ArangoDatabase             ' + "\n" +
  '  fm:                                   FoxxManager                ' + "\n" +
  'Examples:                                                          ' + "\n" +
  ' > db._collections()                    list all collections       ' + "\n" +
  ' > db._create(<name>)                   create a new collection    ' + "\n" +
  ' > db._drop(<name>)                     drop a collection          ' + "\n" +
  ' > db.<name>.toArray()                  list all documents         ' + "\n" +
  ' > id = db.<name>.save({ ... })         save a document            ' + "\n" +
  ' > db.<name>.remove(<_id>)              delete a document          ' + "\n" +
  ' > db.<name>.document(<_id>)            retrieve a document        ' + "\n" +
  ' > db.<name>.replace(<_id>, {...})      overwrite a document       ' + "\n" +
  ' > db.<name>.update(<_id>, {...})       partially update a document' + "\n" +
  ' > db.<name>.exists(<_id>)              check if document exists   ' + "\n" +
  ' > db._query(<query>).toArray()         execute an AQL query       ' + "\n" +
  ' > db._useDatabase(<name>)              switch database            ' + "\n" +
  ' > db._createDatabase(<name>)           create a new database      ' + "\n" +
  ' > db._listDatabases()                  list existing databases    ' + "\n" +
  ' > help                                 show help pages            ' + "\n" +
  ' > exit                                                            ' + "\n" +
  'Note: collection names and statuses may be cached in arangosh.     ' + "\n" +
  'To refresh the list of collections and their statuses, issue:      ' + "\n" +
  ' > db._collections();                                              ' + "\n" +
  '                                                                   ' + "\n" +
  (internal.printBrowser ?
  'To cancel the current prompt, press CTRL + z.                      ' + "\n" +
  '                                                                   ' + "\n" +
  'Please note that all variables defined with the var keyword will   ' + "\n" +
  'disappear when the command is finished. To introduce variables that' + "\n" +
  'are persisting until the next command, omit the var keyword.       ' + "\n\n" +
  'Type \'tutorial\' for a tutorial or \'help\' to see common examples' :
  'To cancel the current prompt, press CTRL + d.                      ' + "\n");

////////////////////////////////////////////////////////////////////////////////
/// @brief query help
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief extended help
////////////////////////////////////////////////////////////////////////////////

exports.helpExtended = exports.createHelpHeadline("More help") +
  'Pager:                                                              ' + "\n" +
  ' > stop_pager()                         stop the pager output       ' + "\n" +
  ' > start_pager()                        start the pager             ' + "\n" +
  'Pretty printing:                                                    ' + "\n" +
  ' > stop_pretty_print()                  stop pretty printing        ' + "\n" +
  ' > start_pretty_print()                 start pretty printing       ' + "\n" +
  'Color output:                                                       ' + "\n" +
  ' > stop_color_print()                   stop color printing         ' + "\n" +
  ' > start_color_print()                  start color printing        ' + "\n" +
  'Print function:                                                     ' + "\n" +
  ' > print(x)                             std. print function         ' + "\n" +
  ' > print_plain(x)                       print without prettifying   ' + "\n" +
  '                                        and without colors          ' + "\n" +
  ' > clear()                              clear screen                ' ;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
});

module.define("org/arangodb/general-graph", function(exports, module) {
/*jshint strict: false */
/*global ArangoClusterComm */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Florian Bartels, Michael Hackstein, Guido Schwab
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var arangodb = require("org/arangodb"),
  internal = require("internal"),
  ArangoCollection = arangodb.ArangoCollection,
  ArangoError = arangodb.ArangoError,
  db = arangodb.db,
  errors = arangodb.errors,
  _ = require("underscore");


// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief transform a string into an array.
////////////////////////////////////////////////////////////////////////////////


var stringToArray = function (x) {
  if (typeof x === "string") {
    return [x];
  }
  return _.clone(x);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a parameter is not defined, an empty string or an empty
//  array
////////////////////////////////////////////////////////////////////////////////


var isValidCollectionsParameter = function (x) {
  if (!x) {
    return false;
  }
  if (Array.isArray(x) && x.length === 0) {
    return false;
  }
  if (typeof x !== "string" && !Array.isArray(x)) {
    return false;
  }
  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionByName = function (name, type, noCreate) {
  var col = db._collection(name),
    res = false;
  if (col === null && ! noCreate) {
    if (type === ArangoCollection.TYPE_DOCUMENT) {
      col = db._create(name);
    } else {
      col = db._createEdgeCollection(name);
    }
    res = true;
  } 
  else if (! (col instanceof ArangoCollection)) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION.code;
    err.errorMessage = name + arangodb.errors.ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION.message;
    throw err;
  }
  return res;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief find or create a collection by name
////////////////////////////////////////////////////////////////////////////////

var findOrCreateCollectionsByEdgeDefinitions = function (edgeDefinitions, noCreate) {
  var vertexCollections = {},
  edgeCollections = {};
  edgeDefinitions.forEach(function (e) {
    if (! e.hasOwnProperty('collection') || 
        ! e.hasOwnProperty('from') ||
        ! e.hasOwnProperty('to') ||
        ! Array.isArray(e.from) ||
        ! Array.isArray(e.to)) {
      var err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.message;
      throw err;
    }

    e.from.concat(e.to).forEach(function (v) {
      findOrCreateCollectionByName(v, ArangoCollection.TYPE_DOCUMENT, noCreate);
      vertexCollections[v] = db[v];
    });
    findOrCreateCollectionByName(e.collection, ArangoCollection.TYPE_EDGE, noCreate);
    edgeCollections[e.collection] = db[e.collection];
  });
  return [
    vertexCollections,
    edgeCollections
  ];
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to get graphs collection
////////////////////////////////////////////////////////////////////////////////

var getGraphCollection = function() {
  var gCol = db._graphs;
  if (gCol === null || gCol === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NO_GRAPH_COLLECTION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NO_GRAPH_COLLECTION.message;
    throw err;
  }
  return gCol;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to print edge definitions in _PRINT
////////////////////////////////////////////////////////////////////////////////

var printEdgeDefinitions = function(defs) {
  return _.map(defs, function(d) {
    var out = d.collection;
    out += ": [";
    out += d.from.join(", ");
    out += "] -> [";
    out += d.to.join(", ");
    out += "]";
    return out;
  });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to wrap arango collections for overwrite
////////////////////////////////////////////////////////////////////////////////

var wrapCollection = function(col) {
  var wrapper = {};
  _.each(_.functions(col), function(func) {
    wrapper[func] = function() {
      return col[func].apply(col, arguments);
    };
  });
  return wrapper;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_example_description
///
/// For many of the following functions *examples* can be passed in as a parameter.
/// *Examples* are used to filter the result set for objects that match the conditions.
/// These *examples* can have the following values:
///
/// * *null*, there is no matching executed all found results are valid.
/// * A *string*, only results are returned, which *_id* equal the value of the string
/// * An example *object*, defining a set of attributes.
///     Only results having these attributes are matched.
/// * A *list* containing example *objects* and/or *strings*.
///     All results matching at least one of the elements in the list are returned.
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var transformExample = function(example) {
  if (example === undefined) {
    return {};
  }
  if (typeof example === "string") {
    return {_id: example};
  }
  if (typeof example === "object") {
    if (Array.isArray(example)) {
      return _.map(example, function(e) {
        if (typeof e === "string") {
          return {_id: e};
        }
        return e;
      });
    }
    return example;
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING.message;
  throw err;
};

var checkAllowsRestriction = function(list, rest, msg) {
  var unknown = [];
  var colList = _.map(list, function(item) {
    return item.name();
  });
  _.each(rest, function(r) {
    if (!_.contains(colList, r)) {
      unknown.push(r);
    }
  });
  if (unknown.length > 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = msg + ": "
    + unknown.join(" and ")
    + " are not known to the graph";
    throw err;
  }
  return true;
};


// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/general-graph"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                             Fluent AQL Interface
// -----------------------------------------------------------------------------

var AQLStatement = function(query, type) {
  this.query = query;
  if (type) {
    this.type = type;
  }
};

AQLStatement.prototype.printQuery = function() {
  return this.query;
};

AQLStatement.prototype.isPathQuery = function() {
  return this.type === "path";
};

AQLStatement.prototype.isPathVerticesQuery = function() {
  return this.type === "pathVertices";
};

AQLStatement.prototype.isPathEdgesQuery = function() {
  return this.type === "pathEdges";
};

AQLStatement.prototype.isEdgeQuery = function() {
  return this.type === "edge";
};

AQLStatement.prototype.isVertexQuery = function() {
  return this.type === "vertex";
};

AQLStatement.prototype.isNeighborQuery = function() {
  return this.type === "neighbor";
};

AQLStatement.prototype.allowsRestrict = function() {
  return this.isEdgeQuery()
    || this.isVertexQuery()
    || this.isNeighborQuery();
};

// -----------------------------------------------------------------------------
// --SECTION--                                AQL Generator for fluent interface
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Starting point of the fluent interface.
///
/// Only for internal use.
////////////////////////////////////////////////////////////////////////////////

var AQLGenerator = function(graph) {
  this.stack = [];
  this.callStack = [];
  this.bindVars = {
    "graphName": graph.__name
  };
  this.graph = graph;
  this.cursor = null;
  this.lastVar = "";
  this._path = [];
  this._pathVertices = [];
  this._pathEdges = [];
  this._getPath = false;
};

AQLGenerator.prototype._addToPrint = function(name) {
  var args = Array.prototype.slice.call(arguments);
  args.shift(); // The Name
  var stackEntry = {};
  stackEntry.name = name;
  if (args.length > 0 && args[0] !== undefined) {
    stackEntry.params = args;
  } else {
    stackEntry.params = [];
  }
  this.callStack.push(stackEntry);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Print the call stack of this query
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._PRINT = function(context) {
  context.output = "[ GraphAQL ";
  context.output += this.graph.__name;
  _.each(this.callStack, function(call) {
    if(context.prettyPrint) {
      context.output += "\n";
    }
    context.output += ".";
    context.output += call.name;
    context.output += "(";
    var i = 0;
    for(i = 0; i < call.params.length; ++i) {
      if (i > 0) {
        context.output += ", ";
      }
      internal.printRecursive(call.params[i], context);
    }
    context.output += ")";
  });
  context.output += " ] ";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Dispose and reset the current cursor of the query
///
/// Only for internal use.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._clearCursor = function() {
  if (this.cursor) {
    this.cursor.dispose();
    this.cursor = null;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute the query and keep the cursor
///
/// Only for internal use.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._createCursor = function() {
  if (!this.cursor) {
    this.cursor = this.execute();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief General edge query, takes direction as parameter
///
/// This will create the general AQL statement to load edges
/// connected to the vertices selected in the step before.
/// Will also bind the options into bindVars.
///
/// Only for internal use, user gets different functions for directions
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._edges = function(edgeExample, options) {
  this._clearCursor();
  this.options = options || {};
  var ex = transformExample(edgeExample);
  var edgeName = "edges_" + this.stack.length;
  var query = "FOR " + edgeName
    + ' IN GRAPH_EDGES(@graphName';
  if (!this.getLastVar()) {
    query += ',{}';
  } else {
    query += ',' + this.getLastVar();
  }
  query += ',@options_'
    + this.stack.length + ')';
  if (!Array.isArray(ex)) {
    ex = [ex];
  }
  this.options.edgeExamples = ex;
  this.options.includeData = true;
  this.bindVars["options_" + this.stack.length] = this.options;
  var stmt = new AQLStatement(query, "edge");
  this.stack.push(stmt);
  this.lastVar = edgeName;
  this._path.push(edgeName);
  this._pathEdges.push(edgeName);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_edges
/// @brief Select all edges for the vertices selected before.
///
/// `graph_query.edges(examples)`
///
/// Creates an AQL statement to select all edges for each of the vertices selected
/// in the step before.
/// This will include *inbound* as well as *outbound* edges.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
/// parameter vertexExamplex, *m* the average amount of edges of a vertex and *x* the maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// To request unfiltered edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.edges().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered edges by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.edges({type: "married"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered edges by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLEdgesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.edges([{type: "married"}, {type: "friend"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.edges = function(example) {
  this._addToPrint("edges", example);
  return this._edges(example, {direction: "any"});
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_outEdges
/// @brief Select all outbound edges for the vertices selected before.
///
/// `graph_query.outEdges(examples)`
///
/// Creates an AQL statement to select all *outbound* edges for each of the vertices selected
/// in the step before.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// To request unfiltered outbound edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.outEdges().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered outbound edges by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.outEdges({type: "married"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered outbound edges by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLOutEdgesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.outEdges([{type: "married"}, {type: "friend"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.outEdges = function(example) {
  this._addToPrint("outEdges", example);
  return this._edges(example, {direction: "outbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_inEdges
/// @brief Select all inbound edges for the vertices selected before.
///
/// `graph_query.inEdges(examples)`
///
///
/// Creates an AQL statement to select all *inbound* edges for each of the vertices selected
/// in the step before.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// To request unfiltered inbound edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.inEdges().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered inbound edges by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.inEdges({type: "married"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered inbound edges by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLInEdgesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices([{name: "Alice"}, {name: "Bob"}]);
///   query.inEdges([{type: "married"}, {type: "friend"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.inEdges = function(example) {
  this._addToPrint("inEdges", example);
  return this._edges(example, {direction: "inbound"});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief General vertex query, takes direction as parameter
///
/// This will create the general AQL statement to load vertices
/// connected to the edges selected in the step before.
/// Will also bind the options into bindVars.
///
/// Only for internal use, user gets different functions for directions
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._vertices = function(example, options, mergeWith) {
  this._clearCursor();
  this.options = options || {};
  var ex = transformExample(example);
  var vertexName = "vertices_" + this.stack.length;
  var query = "FOR " + vertexName
    + " IN GRAPH_VERTICES(@graphName,";
  if (mergeWith !== undefined) {
    if (Array.isArray(mergeWith)) {
      var i;
      query += "[";
      for (i = 0; i < mergeWith.length; ++i) {
        if (i > 0) {
          query += ",";
        }
        query += "MERGE(@vertexExample_" + this.stack.length 
          + "," + mergeWith[i] + ")";
      }
      query += "]";
    } else {
      query += "MERGE(@vertexExample_" + this.stack.length 
        + "," + mergeWith + ")";
    }
  } else {
    query += "@vertexExample_" + this.stack.length;
  }
  query += ',@options_' + this.stack.length + ')';
  this.bindVars["vertexExample_" + this.stack.length] = ex;
  this.bindVars["options_" + this.stack.length] = this.options;
  var stmt = new AQLStatement(query, "vertex");
  this.stack.push(stmt);
  this.lastVar = vertexName;
  this._path.push(vertexName);
  this._pathVertices.push(vertexName);
  return this;

};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_vertices
/// @brief Select all vertices connected to the edges selected before.
///
/// `graph_query.vertices(examples)`
///
/// Creates an AQL statement to select all vertices for each of the edges selected
/// in the step before.
/// This includes all vertices contained in *_from* as well as *_to* attribute of the edges.
/// The resulting set of vertices can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// To request unfiltered vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.vertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered vertices by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.vertices({name: "Alice"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered vertices by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLVerticesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.vertices([{name: "Alice"}, {name: "Charly"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.vertices = function(example) {
  this._addToPrint("vertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  return this._vertices(example, undefined,
    ["{'_id': " + edgeVar + "._from}", "{'_id': " + edgeVar + "._to}"]);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_fromVertices
/// @brief Select all source vertices of the edges selected before.
///
/// `graph_query.fromVertices(examples)`
///
/// Creates an AQL statement to select the set of vertices where the edges selected
/// in the step before start at.
/// This includes all vertices contained in *_from* attribute of the edges.
/// The resulting set of vertices can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// To request unfiltered source vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.fromVertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered source vertices by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.fromVertices({name: "Alice"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered source vertices by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFromVerticesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.fromVertices([{name: "Alice"}, {name: "Charly"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.fromVertices = function(example) {
  this._addToPrint("fromVertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  return this._vertices(example, undefined, "{'_id': " + edgeVar + "._from}");
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_toVertices
/// @brief Select all vertices targeted by the edges selected before.
///
/// `graph_query.toVertices(examples)`
///
/// Creates an AQL statement to select the set of vertices where the edges selected
/// in the step before end in.
/// This includes all vertices contained in *_to* attribute of the edges.
/// The resulting set of vertices can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// To request unfiltered target vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered target vertices by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices({name: "Bob"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered target vertices by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToVerticesFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices([{name: "Bob"}, {name: "Diana"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.toVertices = function(example) {
  this._addToPrint("toVertices", example);
  if (!this.getLastVar()) {
    return this._vertices(example);
  }
  var edgeVar = this.getLastVar();
  return this._vertices(example, undefined, "{'_id': " + edgeVar + "._to}");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the variable holding the last result
///
/// Only for internal use.
/// The return statement of the AQL query has to return
/// this value.
/// Also chaining has to use this variable to restrict
/// queries in the next step to only values from this set.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.getLastVar = function() {
  if (this.lastVar === "") {
    return false;
  }
  return this.lastVar;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_path
/// @brief The result of the query is the path to all elements.
///
/// `graph_query.path()`
///
/// By defaut the result of the generated AQL query is the set of elements passing the last matches.
/// So having a `vertices()` query as the last step the result will be set of vertices.
/// Using `path()` as the last action before requesting the result
/// will modify the result such that the path required to find the set vertices is returned.
///
/// @EXAMPLES
///
/// Request the iteratively explored path using vertices and edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLPathSimple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.outEdges().toVertices().path().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// When requesting neighbors the path to these neighbors is expanded:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLPathNeighbors}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.neighbors().path().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.path = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "path");
  this.stack.push(statement);
  return this;
};

AQLGenerator.prototype.pathVertices = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "pathVertices");
  this.stack.push(statement);
  return this;
};

AQLGenerator.prototype.pathEdges = function() {
  this._clearCursor();
  var statement = new AQLStatement("", "pathEdges");
  this.stack.push(statement);
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_neighbors
/// @brief Select all neighbors of the vertices selected in the step before.
///
/// `graph_query.neighbors(examples, options)`
///
/// Creates an AQL statement to select all neighbors for each of the vertices selected
/// in the step before.
/// The resulting set of vertices can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
///   An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition_of_examples)
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *vertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///   * *minDepth*: Defines the minimal number of intermediate steps to neighbors (default is 1).
///   * *maxDepth*: Defines the maximal number of intermediate steps to neighbors (default is 1).
///
/// @EXAMPLES
///
/// To request unfiltered neighbors:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.neighbors().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered neighbors by a single example:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredSingle}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.neighbors({name: "Bob"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered neighbors by multiple examples:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNeighborsFilteredMultiple}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.vertices([{name: "Bob"}, {name: "Charly"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.neighbors = function(vertexExample, options) {
  this._addToPrint("neighbors", vertexExample, options);
  var ex = transformExample(vertexExample);
  var resultName = "neighbors_" + this.stack.length;
  var query = "FOR " + resultName
    + " IN GRAPH_NEIGHBORS(@graphName,"
    + this.getLastVar()
    + ',@options_'
    + this.stack.length + ')';
  var opts;
  if (options) {
    opts = _.clone(options);
  } else {
    opts = {};
  }
  opts.neighborExamples = ex;
  opts.includeData = true;
  this.bindVars["options_" + this.stack.length] = opts;
  var stmt = new AQLStatement(query, "neighbor");
  this.stack.push(stmt);

  this.lastVar = resultName;
  this._path.push(resultName);
  this._pathVertices.push(resultName);

  /*
  this.lastVar = resultName + ".vertex";
  this._path.push(resultName + ".path");
  this._pathVertices.push("SLICE(" + resultName + ".path.vertices, 1)");
  this._pathEdges.push(resultName + ".path.edges");
  */
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the last statement that can be restricted to collections
///
/// Only for internal use.
/// This returnes the last statement that can be restricted to
/// specific collections.
/// Required to allow a chaining of `restrict` after `filter` for instance.
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype._getLastRestrictableStatementInfo = function() {
  var i = this.stack.length - 1;
  while (!this.stack[i].allowsRestrict()) {
    i--;
  }
  return {
    statement: this.stack[i],
    options: this.bindVars["options_" + i]
  };
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_restrict
/// @brief Restricts the last statement in the chain to return
/// only elements of a specified set of collections
///
/// `graph_query.restrict(restrictions)`
///
/// By default all collections in the graph are searched for matching elements
/// whenever vertices and edges are requested.
/// Using *restrict* after such a statement allows to restrict the search
/// to a specific set of collections within the graph.
/// Restriction is only applied to this one part of the query.
/// It does not effect earlier or later statements.
///
/// @PARAMS
///
/// @PARAM{restrictions, array, optional}
/// Define either one or a list of collections in the graph.
/// Only elements from these collections are taken into account for the result.
///
/// @EXAMPLES
///
/// Request all directly connected vertices unrestricted:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnrestricted}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.edges().vertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Apply a restriction to the directly connected vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestricted}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.edges().vertices().restrict("female").toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Restriction of a query is only valid for collections known to the graph:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLRestrictedUnknown}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices({name: "Alice"});
///   query.edges().vertices().restrict(["female", "male", "products"]).toArray(); // xpError(ERROR_BAD_PARAMETER);
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.restrict = function(restrictions) {
  var rest = stringToArray(restrictions);
  if (rest.length === 0) {
    return this;
  }
  this._addToPrint("restrict", restrictions);
  this._clearCursor();
  var lastQueryInfo = this._getLastRestrictableStatementInfo();
  var lastQuery = lastQueryInfo.statement;
  var opts = lastQueryInfo.options;
  var restricts;
  if (lastQuery.isEdgeQuery()) {
    checkAllowsRestriction(
      this.graph._edgeCollections(),
      rest,
      "edge collections"
    );
    restricts = opts.edgeCollectionRestriction || [];
    opts.edgeCollectionRestriction = restricts.concat(restrictions);
  } else if (lastQuery.isVertexQuery() || lastQuery.isNeighborQuery()) {
    checkAllowsRestriction(
      this.graph._vertexCollections(),
      rest,
      "vertex collections"
    );
    restricts = opts.vertexCollectionRestriction || [];
    opts.vertexCollectionRestriction = restricts.concat(restrictions);
  }
  return this;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_filter
/// @brief Filter the result of the query
///
/// `graph_query.filter(examples)`
///
/// This can be used to further specfiy the expected result of the query.
/// The result set is reduced to the set of elements that matches the given *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// Request vertices unfiltered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnfilteredVertices}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Request vertices filtered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFilteredVertices}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().filter({name: "Alice"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Request edges unfiltered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLUnfilteredEdges}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().outEdges().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Request edges filtered:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLFilteredEdges}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._edges({type: "married"});
///   query.toVertices().outEdges().filter({type: "married"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.filter = function(example) {
  this._addToPrint("filter", example);
  this._clearCursor();
  var ex = [];
  if (Object.prototype.toString.call(example) !== "[object Array]") {
    if (Object.prototype.toString.call(example) !== "[object Object]") {
      var err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT.code;
      err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT.message;
      throw err;
    }
    ex = [example];
  } else {
    ex = example;
  }
  var query = "FILTER MATCHES(" + this.getLastVar() + "," + JSON.stringify(ex) + ")";
  this.stack.push(new AQLStatement(query));
  return this;
};

AQLGenerator.prototype.printQuery = function() {
  return this.stack.map(function(stmt) {
    return stmt.printQuery();
  }).join(" ");
};

AQLGenerator.prototype.execute = function() {
  this._clearCursor();
  var query = this.printQuery();
  var bindVars = this.bindVars;
  if (this.stack[this.stack.length-1].isPathQuery()) {
    query += " RETURN [" + this._path + "]";
  } else if (this.stack[this.stack.length-1].isPathVerticesQuery()) {
    query += " RETURN FLATTEN([" + this._pathVertices + "])";
  } else if (this.stack[this.stack.length-1].isPathEdgesQuery()) {
    query += " RETURN FLATTEN([" + this._pathEdges + "])";
  } else {
    query += " RETURN " + this.getLastVar();
  }
  return db._query(query, bindVars, {count: true});
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_toArray
/// @brief Returns an array containing the complete result.
///
/// `graph_query.toArray()`
///
/// This function executes the generated query and returns the
/// entire result as one array.
/// ToArray does not return the generated query anymore and
/// hence can only be the endpoint of a query.
/// However keeping a reference to the query before
/// executing allows to chain further statements to it.
///
/// @EXAMPLES
///
/// To collect the entire result of a query toArray can be used:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLToArray}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
///   query.toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.toArray = function() {
  this._createCursor();

  return this.cursor.toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_count
/// @brief Returns the number of returned elements if the query is executed.
///
/// `graph_query.count()`
///
/// This function determines the amount of elements to be expected within the result of the query.
/// It can be used at the beginning of execution of the query
/// before using *next()* or in between *next()* calls.
/// The query object maintains a cursor of the query for you.
/// *count()* does not change the cursor position.
///
/// @EXAMPLES
///
/// To count the number of matched elements:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLCount}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
///   query.count();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.count = function() {
  this._createCursor();
  return this.cursor.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_hasNext
/// @brief Checks if the query has further results.
///
/// `graph_query.hasNext()`
///
/// The generated statement maintains a cursor for you.
/// If this cursor is already present *hasNext()* will
/// use this cursors position to determine if there are
/// further results available.
/// If the query has not yet been executed *hasNext()*
/// will execute it and create the cursor for you.
///
/// @EXAMPLES
///
/// Start query execution with hasNext:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLHasNext}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
///   query.hasNext();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Iterate over the result as long as it has more elements:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLHasNextIteration}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
/// | while (query.hasNext()) {
/// |   var entry = query.next();
/// |   // Do something with the entry
///   }
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.hasNext = function() {
  this._createCursor();
  return this.cursor.hasNext();
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fluent_aql_next
/// @brief Request the next element in the result.
///
/// `graph_query.next()`
///
/// The generated statement maintains a cursor for you.
/// If this cursor is already present *next()* will
/// use this cursors position to deliver the next result.
/// Also the cursor position will be moved by one.
/// If the query has not yet been executed *next()*
/// will execute it and create the cursor for you.
/// It will throw an error of your query has no further results.
///
/// @EXAMPLES
///
/// Request some elements with next:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNext}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
///   query.next();
///   query.next();
///   query.next();
///   query.next();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// The cursor is recreated if the query is changed:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphFluentAQLNextRecreate}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   var query = graph._vertices();
///   query.next();
///   query.edges();
///   query.next();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

AQLGenerator.prototype.next = function() {
  this._createCursor();
  return this.cursor.next();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_undirectedRelation
/// @brief Define an undirected relation.
///
/// `graph_module._undirectedRelation(relationName, vertexCollections)`
///
/// Defines an undirected relation with the name *relationName* using the
/// list of *vertexCollections*. This relation allows the user to store
/// edges in any direction between any pair of vertices within the
/// *vertexCollections*.
///
/// @PARAMS
///
/// @PARAM{relationName, string, required}
///   The name of the edge collection where the edges should be stored.
///   Will be created if it does not yet exist.
///
/// @PARAM{vertexCollections, array, required}
///   One or a list of collection names for which connections are allowed.
///   Will be created if they do not exist.
///
/// @EXAMPLES
///
/// To define simple relation with only one vertex collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition1}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._undirectedRelation("friend", "user");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To define a relation between several vertex collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphUndirectedRelationDefinition2}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._undirectedRelation("marriage", ["female", "male"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
var _undirectedRelation = function (relationName, vertexCollections) {
  var err;
  if (arguments.length < 2) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.message + "2";
    throw err;
  }

  if (typeof relationName !== "string" || relationName === "") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + " arg1 must not be empty";
    throw err;
  }

  if (!isValidCollectionsParameter(vertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + " arg2 must not be empty";
    throw err;
  }

  return {
    collection: relationName,
    from: stringToArray(vertexCollections),
    to: stringToArray(vertexCollections)
  };
};
////////////////////////////////////////////////////////////////////////////////
/// Deprecated block
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_directedRelation
/// @brief Define a directed relation.
///
/// `graph_module._directedRelation(relationName, fromVertexCollections, toVertexCollections)`
///
/// The *relationName* defines the name of this relation and references to the underlying edge collection.
/// The *fromVertexCollections* is an Array of document collections holding the start vertices.
/// The *toVertexCollections* is an Array of document collections holding the target vertices.
/// Relations are only allowed in the direction from any collection in *fromVertexCollections*
/// to any collection in *toVertexCollections*.
///
/// @PARAMS
///
/// @PARAM{relationName, string, required}
///   The name of the edge collection where the edges should be stored.
///   Will be created if it does not yet exist.
///
/// @PARAM{fromVertexCollections, array, required}
///   One or a list of collection names. Source vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @PARAM{toVertexCollections, array, required}
///   One or a list of collection names. Target vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDirectedRelationDefinition}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._directedRelation("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_relation
/// @brief Define a directed relation.
///
/// `graph_module._relation(relationName, fromVertexCollections, toVertexCollections)`
///
/// The *relationName* defines the name of this relation and references to the underlying edge collection.
/// The *fromVertexCollections* is an Array of document collections holding the start vertices.
/// The *toVertexCollections* is an Array of document collections holding the target vertices.
/// Relations are only allowed in the direction from any collection in *fromVertexCollections*
/// to any collection in *toVertexCollections*.
///
/// @PARAMS
///
/// @PARAM{relationName, string, required}
///   The name of the edge collection where the edges should be stored.
///   Will be created if it does not yet exist.
///
/// @PARAM{fromVertexCollections, array, required}
///   One or a list of collection names. Source vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @PARAM{toVertexCollections, array, required}
///   One or a list of collection names. Target vertices for the edges
///   have to be stored in these collections. Collections will be created if they do not exist.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRelationDefinition}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._relation("has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphRelationDefinitionSingle}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._relation("has_bought", "Customer", "Product");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _relation = function (
  relationName, fromVertexCollections, toVertexCollections) {
  var err;
  if (arguments.length < 3) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS.message + "3";
    throw err;
  }

  if (typeof relationName !== "string" || relationName === "") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message + " arg1 must be non empty string";
    throw err;
  }

  if (!isValidCollectionsParameter(fromVertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message
      + " arg2 must be non empty string or array";
    throw err;
  }

  if (!isValidCollectionsParameter(toVertexCollections)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_PARAMETER.message
      + " arg3 must be non empty string or array";
    throw err;
  }

  return {
    collection: relationName,
    from: stringToArray(fromVertexCollections),
    to: stringToArray(toVertexCollections)
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_list
/// @brief List all graphs.
///
/// `graph_module._list()`
///
/// Lists all graph names stored in this database.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphList}
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._list();
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var _list = function() {
  var gdb = getGraphCollection();
  return _.pluck(gdb.toArray(), "_key");
};


var _listObjects = function() {
  return getGraphCollection().toArray();
};




////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_definitions
/// @brief Create a list of edge definitions to construct a graph.
///
/// `graph_module._edgeDefinitions(relation1, relation2, ..., relationN)`
///
/// The list of edge definitions of a graph can be managed by the graph module itself.
/// This function is the entry point for the management and will return the correct list.
///
/// @PARAMS
///
/// @PARAM{relationX, object, optional}
/// An object representing a definition of one relation in the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitions}
///   var graph_module = require("org/arangodb/general-graph");
///   directed_relation = graph_module._relation("lives_in", "user", "city");
///   undirected_relation = graph_module._relation("knows", "user", "user");
///   edgedefinitions = graph_module._edgeDefinitions(directed_relation, undirected_relation);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////


var _edgeDefinitions = function () {

  var res = [], args = arguments;
  Object.keys(args).forEach(function (x) {
   res.push(args[x]);
  });

  return res;

};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_extend_edge_definitions
/// @brief Extend the list of edge definitions to construct a graph.
///
/// `graph_module._extendEdgeDefinitions(edgeDefinitions, relation1, relation2, ..., relationN)`
///
/// In order to add more edge definitions to the graph before creating
/// this function can be used to add more definitions to the initial list.
///
/// @PARAMS
///
/// @PARAM{edgeDefinitions, array, required}
/// A list of relation definition objects.
///
/// @PARAM{relationX, object, required}
/// An object representing a definition of one relation in the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeDefinitionsExtend}
///   var graph_module = require("org/arangodb/general-graph");
///   directed_relation = graph_module._relation("lives_in", "user", "city");
///   undirected_relation = graph_module._relation("knows", "user", "user");
///   edgedefinitions = graph_module._edgeDefinitions(directed_relation);
///   edgedefinitions = graph_module._extendEdgeDefinitions(undirected_relation);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

var _extendEdgeDefinitions = function (edgeDefinition) {
  var args = arguments, i = 0;

  Object.keys(args).forEach(
    function (x) {
      i++;
      if (i === 1) {
        return;
      }
      edgeDefinition.push(args[x]);
    }
  );
};
////////////////////////////////////////////////////////////////////////////////
/// internal helper to sort a graph's edge definitions
////////////////////////////////////////////////////////////////////////////////
var sortEdgeDefinition = function(edgeDefinition) {
  edgeDefinition.from = edgeDefinition.from.sort();
  edgeDefinition.to = edgeDefinition.to.sort();
  return edgeDefinition;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new graph
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_how_to_create
///
/// * Create a graph
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo1}
///   var graph_module = require("org/arangodb/general-graph");
///   var graph = graph_module._create("myGraph");
///   graph;
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// * Add some vertex collections
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo2}
/// ~ var graph_module = require("org/arangodb/general-graph");
/// ~ var graph = graph_module._create("myGraph");
///   graph._addVertexCollection("shop");
///   graph._addVertexCollection("customer");
///   graph._addVertexCollection("pet");
///   graph;
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// * Define relations on the
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphHowTo3}
/// ~ var graph_module = require("org/arangodb/general-graph");
/// ~ var graph = graph_module._create("myGraph");
///   var rel = graph_module._relation("isCustomer", ["shop"], ["customer"]);
///   graph._extendEdgeDefinitions(rel);
///   graph;
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create
/// @brief Create a graph
///
/// `graph_module._create(graphName, edgeDefinitions, orphanCollections)`
///
/// The creation of a graph requires the name of the graph and a definition of its edges.
///
/// For every type of edge definition a convenience method exists that can be used to create a graph.
/// Optionally a list of vertex collections can be added, which are not used in any edge definition.
/// These collections are referred to as orphan collections within this chapter.
/// All collections used within the creation process are created if they do not exist.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @PARAM{edgeDefinitions, array, optional}
/// List of relation definition objects
///
/// @PARAM{orphanCollections, array, optional}
/// List of additional vertex collection names
///
/// @EXAMPLES
///
/// Create an empty graph, edge definitions can be added at runtime:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph}
///   var graph_module = require("org/arangodb/general-graph");
///   graph = graph_module._create("myGraph");
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Create a graph using an edge collection `edges` and a single vertex collection `vertices` 
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraphSingle}
/// ~ db._drop("edges");
/// ~ db._drop("vertices");
///   var graph_module = require("org/arangodb/general-graph");
///   var edgeDefinitions = [ { collection: "edges", "from": [ "vertices" ], "to" : [ "vertices" ] } ];
///   graph = graph_module._create("myGraph", edgeDefinitions);
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Create a graph with edge definitions and orphan collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphCreateGraph2}
///   var graph_module = require("org/arangodb/general-graph");
/// | graph = graph_module._create("myGraph",
///   [graph_module._relation("myRelation", ["male", "female"], ["male", "female"])], ["sessions"]);
/// ~ graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _create = function (graphName, edgeDefinitions, orphanCollections, options) {

  if (! Array.isArray(orphanCollections) ) {
    orphanCollections = [];
  }
  var gdb = getGraphCollection(),
    err,
    graphAlreadyExists = true,
    collections,
    result;
  if (!graphName) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MISSING_NAME.message;
    throw err;
  }
  edgeDefinitions = edgeDefinitions || [];
  if (!Array.isArray(edgeDefinitions)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION.message;
    throw err;
  }
  //check, if a collection is already used in a different edgeDefinition
  var tmpCollections = [];
  var tmpEdgeDefinitions = {};
  edgeDefinitions.forEach(
    function(edgeDefinition) {
      var col = edgeDefinition.collection;
      if (tmpCollections.indexOf(col) !== -1) {
        err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code;
        err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message;
        throw err;
      }
      tmpCollections.push(col);
      tmpEdgeDefinitions[col] = edgeDefinition;
    }
  );
  gdb.toArray().forEach(
    function(singleGraph) {
      var sGEDs = singleGraph.edgeDefinitions;
      sGEDs.forEach(
        function(sGED) {
          var col = sGED.collection;
          if (tmpCollections.indexOf(col) !== -1) {
            if (JSON.stringify(sGED) !== JSON.stringify(tmpEdgeDefinitions[col])) {
              err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col + " "
                + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
              throw err;
            }
          }
        }
      );
    }
  );

  try {
    gdb.document(graphName);
  } catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    graphAlreadyExists = false;
  }

  if (graphAlreadyExists) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_DUPLICATE.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_DUPLICATE.message;
    throw err;
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(edgeDefinitions, false);
  orphanCollections.forEach(
    function(oC) {
      findOrCreateCollectionByName(oC, ArangoCollection.TYPE_DOCUMENT);
    }
  );

  edgeDefinitions.forEach(
    function(eD, index) {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );
  orphanCollections = orphanCollections.sort();

  var data =  gdb.save({
    'orphanCollections' : orphanCollections,
    'edgeDefinitions' : edgeDefinitions,
    '_key' : graphName
  }, options);

  result = new Graph(graphName, edgeDefinitions, collections[0], collections[1],
    orphanCollections, data._rev , data._id);
  return result;

};

var createHiddenProperty = function(obj, name, value) {
  Object.defineProperty(obj, name, {
    enumerable: false,
    writable: true
  });
  obj[name] = value;
};
////////////////////////////////////////////////////////////////////////////////
/// @brief helper for updating binded collections
////////////////////////////////////////////////////////////////////////////////
var removeEdge = function (graphs, edgeCollection, edgeId, self) {
  self.__idsToRemove[edgeId] = 1;
  graphs.forEach(
    function(graph) {
      var edgeDefinitions = graph.edgeDefinitions;
      if (graph.edgeDefinitions) {
        edgeDefinitions.forEach(
          function(edgeDefinition) {
            var from = edgeDefinition.from;
            var to = edgeDefinition.to;
            var collection = edgeDefinition.collection;
            // if collection of edge to be deleted is in from or to
            if (from.indexOf(edgeCollection) !== -1 || to.indexOf(edgeCollection) !== -1) {
              //search all edges of the graph
              var edges = db._collection(collection).edges(edgeId);
              edges.forEach(function(edge) {
                // if from is
                if(! self.__idsToRemove.hasOwnProperty(edge._id)) {
                  self.__collectionsToLock[collection] = 1;
                  removeEdge(graphs, collection, edge._id, self);
                }
              });
            }
          }
        );
      }
    }
  );
};

var bindEdgeCollections = function(self, edgeCollections) {
  _.each(edgeCollections, function(key) {
    var obj = db._collection(key);
    var wrap = wrapCollection(obj);
    // save
    var old_save = wrap.save;
    wrap.save = function(from, to, data) {
      if (typeof from !== 'string' || 
          from.indexOf('/') === -1 ||
          typeof to !== 'string' ||
          to.indexOf('/') === -1) {
        // invalid from or to value
        var err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
        err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
        throw err;
      }

      //check, if edge is allowed
      self.__edgeDefinitions.forEach(
        function(edgeDefinition) {
          if (edgeDefinition.collection === key) {
            var fromCollection = from.split("/")[0];
            var toCollection = to.split("/")[0];
            if (! _.contains(edgeDefinition.from, fromCollection)
              || ! _.contains(edgeDefinition.to, toCollection)) {
              var err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_EDGE.code;
              err.errorMessage =
                arangodb.errors.ERROR_GRAPH_INVALID_EDGE.message + " between " + from + " and " + to + ".";
              throw err;
            }
          }
        }
      );
      return old_save(from, to, data);
    };

    // remove
    wrap.remove = function(edgeId, options) {
      //if _key make _id (only on 1st call)
      if (edgeId.indexOf("/") === -1) {
        edgeId = key + "/" + edgeId;
      }
      var graphs = getGraphCollection().toArray();
      var edgeCollection = edgeId.split("/")[0];
      self.__collectionsToLock[edgeCollection] = 1;
      removeEdge(graphs, edgeCollection, edgeId, self);

      try {
        db._executeTransaction({
          collections: {
            write: Object.keys(self.__collectionsToLock)
          },
          embed: true,
          action: function (params) {
            var db = require("internal").db;
            params.ids.forEach(
              function(edgeId) {
                if (params.options) {
                  db._remove(edgeId, params.options);
                } else {
                  db._remove(edgeId);
                }
              }
            );
          },
          params: {
            ids: Object.keys(self.__idsToRemove),
            options: options
          }
        });
      } catch (e) {
        self.__idsToRemove = {};
        self.__collectionsToLock = {};
        throw e;
      }
      self.__idsToRemove = {};
      self.__collectionsToLock = {};

      return true;
    };

    self[key] = wrap;
  });
};

var bindVertexCollections = function(self, vertexCollections) {
  _.each(vertexCollections, function(key) {
    var obj = db._collection(key);
    var wrap = wrapCollection(obj);
    wrap.remove = function(vertexId, options) {
      //delete all edges using the vertex in all graphs
      var graphs = getGraphCollection().toArray();
      var vertexCollectionName = key;
      if (vertexId.indexOf("/") === -1) {
        vertexId = key + "/" + vertexId;
      }
      self.__collectionsToLock[vertexCollectionName] = 1;
      graphs.forEach(
        function(graph) {
          var edgeDefinitions = graph.edgeDefinitions;
          if (graph.edgeDefinitions) {
            edgeDefinitions.forEach(
              function(edgeDefinition) {
                var from = edgeDefinition.from;
                var to = edgeDefinition.to;
                var collection = edgeDefinition.collection;
                if (from.indexOf(vertexCollectionName) !== -1
                  || to.indexOf(vertexCollectionName) !== -1
                  ) {
                  var edges = db._collection(collection).edges(vertexId);
                  if (edges.length > 0) {
                    self.__collectionsToLock[collection] = 1;
                    edges.forEach(function(edge) {
                      removeEdge(graphs, collection, edge._id, self);
                    });
                  }
                }
              }
            );
          }
        }
      );

      try {
        db._executeTransaction({
          collections: {
            write: Object.keys(self.__collectionsToLock)
          },
          embed: true,
          action: function (params) {
            var db = require("internal").db;
            params.ids.forEach(
              function(edgeId) {
                if (params.options) {
                  db._remove(edgeId, params.options);
                } else {
                  db._remove(edgeId);
                }
              }
            );
            if (params.options) {
              db._remove(params.vertexId, params.options);
            } else {
              db._remove(params.vertexId);
            }
          },
          params: {
            ids: Object.keys(self.__idsToRemove),
            options: options,
            vertexId: vertexId
          }
        });
      } catch (e) {
        self.__idsToRemove = {};
        self.__collectionsToLock = {};
        throw e;
      }
      self.__idsToRemove = {};
      self.__collectionsToLock = {};

      return true;
    };
    self[key] = wrap;
  });

};
var updateBindCollections = function(graph) {
  //remove all binded collections
  Object.keys(graph).forEach(
    function(key) {
      if(key.substring(0,1) !== "_") {
        delete graph[key];
      }
    }
  );
  graph.__edgeDefinitions.forEach(
    function(edgeDef) {
      bindEdgeCollections(graph, [edgeDef.collection]);
      bindVertexCollections(graph, edgeDef.from);
      bindVertexCollections(graph, edgeDef.to);
    }
  );
  bindVertexCollections(graph, graph.__orphanCollections);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_save
/// @brief Create a new vertex in vertexCollectionName
///
/// `graph.vertexCollectionName.save(data)`
///
/// @PARAMS
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionSave}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({name: "Floyd", _key: "floyd"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_replace
/// @brief Replaces the data of a vertex in collection vertexCollectionName
///
/// `graph.vertexCollectionName.replace(vertexId, data, options)`
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionReplace}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({neym: "Jon", _key: "john"});
///   graph.male.replace("male/john", {name: "John"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_update
/// @brief Updates the data of a vertex in collection vertexCollectionName
///
/// `graph.vertexCollectionName.update(vertexId, data, options)`
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{data, object, required}
/// JSON data of vertex.
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionUpdate}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.female.save({name: "Lynda", _key: "linda"});
///   graph.female.update("female/linda", {name: "Linda", _key: "linda"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertex_collection_remove
/// @brief Removes a vertex in collection *vertexCollectionName*
///
/// `graph.vertexCollectionName.remove(vertexId, options)`
///
/// Additionally removes all ingoing and outgoing edges of the vertex recursively
/// (see [edge remove](#edge.remove)).
///
/// @PARAMS
///
/// @PARAM{vertexId, string, required}
/// *_id* attribute of the vertex
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVertexCollectionRemove}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.male.save({name: "Kermit", _key: "kermit"});
///   db._exists("male/kermit")
///   graph.male.remove("male/kermit")
///   db._exists("male/kermit")
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_save
/// @brief Creates an edge from vertex *from* to vertex *to* in collection edgeCollectionName
///
/// `graph.edgeCollectionName.save(from, to, data, options)`
///
/// @PARAMS
///
/// @PARAM{from, string, required}
/// *_id* attribute of the source vertex
///
/// @PARAM{to, string, required}
/// *_id* attribute of the target vertex
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Edges/README.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("male/bob", "female/alice", {type: "married", _key: "bobAndAlice"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// If the collections of *from* and *to* are not defined in an edge definition of the graph,
/// the edge will not be stored.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionSave2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   | graph.relation.save(
///   |  "relation/aliceAndBob",
///   |   "female/alice",
///      {type: "married", _key: "bobAndAlice"}); // xpError(ERROR_GRAPH_INVALID_EDGE)
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_replace
/// @brief Replaces the data of an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.replace(edgeId, data, options)`
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionReplace}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {typo: "nose", _key: "aliceAndDiana"});
///   graph.relation.replace("relation/aliceAndDiana", {type: "knows"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_update
/// @brief Updates the data of an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.update(edgeId, data, options)`
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{data, object, required}
/// JSON data of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionUpdate}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {type: "knows", _key: "aliceAndDiana"});
///   graph.relation.update("relation/aliceAndDiana", {type: "quarrelled", _key: "aliceAndDiana"});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edge_collection_remove
/// @brief Removes an edge in collection edgeCollectionName
///
/// `graph.edgeCollectionName.remove(edgeId, options)`
///
/// If this edge is used as a vertex by another edge, the other edge will be removed (recursively).
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @PARAM{options, object, optional}
/// See [collection documentation](../Documents/DocumentMethods.md)
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgeCollectionRemove}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph.relation.save("female/alice", "female/diana", {_key: "aliceAndDiana"});
///   db._exists("relation/aliceAndDiana")
///   graph.relation.remove("relation/aliceAndDiana")
///   db._exists("relation/aliceAndDiana")
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
var Graph = function(graphName, edgeDefinitions, vertexCollections, edgeCollections,
                     orphanCollections, revision, id) {
  edgeDefinitions.forEach(
    function(eD, index) {
      var tmp = sortEdgeDefinition(eD);
      edgeDefinitions[index] = tmp;
    }
  );

  if (!orphanCollections) {
    orphanCollections = [];
  }

  // we can call the "fast" version of some edge functions if we are
  // running server-side and are not a coordinator
  var useBuiltIn = (typeof ArangoClusterComm === "object");
  if (useBuiltIn && require("org/arangodb/cluster").isCoordinator()) {
    useBuiltIn = false;
  }

  var self = this;
  // Create Hidden Properties
  createHiddenProperty(this, "__useBuiltIn", useBuiltIn);
  createHiddenProperty(this, "__name", graphName);
  createHiddenProperty(this, "__vertexCollections", vertexCollections);
  createHiddenProperty(this, "__edgeCollections", edgeCollections);
  createHiddenProperty(this, "__edgeDefinitions", edgeDefinitions);
  createHiddenProperty(this, "__idsToRemove", {});
  createHiddenProperty(this, "__collectionsToLock", {});
  createHiddenProperty(this, "__id", id);
  createHiddenProperty(this, "__rev", revision);
  createHiddenProperty(this, "__orphanCollections", orphanCollections);
  updateBindCollections(self);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_graph
/// @brief Get a graph
///
/// `graph_module._graph(graphName)`
///
/// A graph can be get by its name.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @EXAMPLES
///
/// Get a graph:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphLoadGraph}
/// ~ var examples = require("org/arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("org/arangodb/general-graph");
///   graph = graph_module._graph("social");
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _graph = function(graphName) {

  var gdb = getGraphCollection(),
    g, collections, orphanCollections;

  try {
    g = gdb.document(graphName);
  }
  catch (e) {
    if (e.errorNum !== errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code) {
      throw e;
    }
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message;
    throw err;
  }

  collections = findOrCreateCollectionsByEdgeDefinitions(g.edgeDefinitions, true);
  orphanCollections = g.orphanCollections;
  if (!orphanCollections) {
    orphanCollections = [];
  }

  return new Graph(graphName, g.edgeDefinitions, collections[0], collections[1], orphanCollections,
    g._rev , g._id);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief check if a graph exists.
////////////////////////////////////////////////////////////////////////////////

var _exists = function(graphId) {
  var gCol = getGraphCollection();
  return gCol.exists(graphId);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper for dropping collections of a graph.
////////////////////////////////////////////////////////////////////////////////

var checkIfMayBeDropped = function(colName, graphName, graphs) {
  var result = true;
  graphs.forEach(
    function(graph) {
      if (graph._key === graphName) {
        return;
      }
      var edgeDefinitions = graph.edgeDefinitions;
      if (edgeDefinitions) {
        edgeDefinitions.forEach(
          function(edgeDefinition) {
            var from = edgeDefinition.from;
            var to = edgeDefinition.to;
            var collection = edgeDefinition.collection;
            if (collection === colName
              || from.indexOf(colName) !== -1
              || to.indexOf(colName) !== -1
              ) {
              result = false;
            }
          }
        );
      }

      var orphanCollections = graph.orphanCollections;
      if (orphanCollections) {
        if (orphanCollections.indexOf(colName) !== -1) {
          result = false;
        }
      }
    }
  );

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_drop
/// @brief Remove a graph
///
/// `graph_module._drop(graphName, dropCollections)`
///
/// A graph can be dropped by its name.
/// This will automatically drop all collections contained in the graph as
/// long as they are not used within other graphs.
/// To drop the collections, the optional parameter *drop-collections* can be set to *true*.
///
/// @PARAMS
///
/// @PARAM{graphName, string, required}
/// Unique identifier of the graph
///
/// @PARAM{dropCollections, boolean, optional}
/// Define if collections should be dropped (default: false)
///
/// @EXAMPLES
///
/// Drop a graph and keep collections:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphKeep}
/// ~ var examples = require("org/arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._drop("social");
///   db._collection("female");
///   db._collection("male");
///   db._collection("relation");
/// ~ db._drop("female");
/// ~ db._drop("male");
/// ~ db._drop("relation");
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphDropGraphDropCollections}
/// ~ var examples = require("org/arangodb/graph-examples/example-graph.js");
/// ~ var g1 = examples.loadGraph("social");
///   var graph_module = require("org/arangodb/general-graph");
///   graph_module._drop("social", true);
///   db._collection("female");
///   db._collection("male");
///   db._collection("relation");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

var _drop = function(graphId, dropCollections) {

  var gdb = getGraphCollection(),
    graphs;

  if (!gdb.exists(graphId)) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message;
    throw err;
  }

  if (dropCollections === true) {
    var graph = gdb.document(graphId);
    var edgeDefinitions = graph.edgeDefinitions;
    edgeDefinitions.forEach(
      function(edgeDefinition) {
        var from = edgeDefinition.from;
        var to = edgeDefinition.to;
        var collection = edgeDefinition.collection;
        graphs = getGraphCollection().toArray();
        if (checkIfMayBeDropped(collection, graph._key, graphs)) {
          db._drop(collection);
        }
        from.forEach(
          function(col) {
            if (checkIfMayBeDropped(col, graph._key, graphs)) {
              db._drop(col);
            }
          }
        );
        to.forEach(
          function(col) {
            if (checkIfMayBeDropped(col, graph._key, graphs)) {
              db._drop(col);
            }
          }
        );
      }
    );
    //drop orphans
    graphs = getGraphCollection().toArray();
    if (!graph.orphanCollections) {
      graph.orphanCollections = [];
    }
    graph.orphanCollections.forEach(
      function(oC) {
        if (checkIfMayBeDropped(oC, graph._key, graphs)) {
          try {
            db._drop(oC);
          } catch (ignore) {}
        }
      }
    );
  }

  gdb.remove(graphId);
  return true;
};


////////////////////////////////////////////////////////////////////////////////
/// @brief return all edge collections of the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edgeCollections = function() {
  return _.values(this.__edgeCollections);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return all vertex collections of the graph.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertexCollections = function() {
  var orphans = [];
  _.each(this.__orphanCollections, function(o) {
    orphans.push(db[o]);
  });
  return _.union(_.values(this.__vertexCollections), orphans);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief _EDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

// might be needed from AQL itself
Graph.prototype._EDGES = function(vertexId) {
  var err;
  if (vertexId.indexOf("/") === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ": " + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].EDGES(vertexId));
      }
      else {
        result = result.concat(this.__edgeCollections[c].edges(vertexId));
      }
    }
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief INEDGES(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._INEDGES = function(vertexId) {
  var err;
  if (vertexId.indexOf("/") === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ": " + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].INEDGES(vertexId));
      }
      else {
        result = result.concat(this.__edgeCollections[c].inEdges(vertexId));
      }
    }
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outEdges(vertexId).
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._OUTEDGES = function(vertexId) {
  var err;
  if (vertexId.indexOf("/") === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_FOUND.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_FOUND.message + ": " + vertexId;
    throw err;
  }

  var result = [], c;
  for (c in this.__edgeCollections) {
    if (this.__edgeCollections.hasOwnProperty(c)) {
      if (this.__useBuiltIn) {
        result = result.concat(this.__edgeCollections[c].OUTEDGES(vertexId));
      }
      else {
        result = result.concat(this.__edgeCollections[c].outEdges(vertexId));
      }
    }
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_edges
/// @brief Select some edges from the graph.
///
/// `graph._edges(examples)`
///
/// Creates an AQL statement to select a subset of the edges stored in the graph.
/// This is one of the entry points for the fluent AQL interface.
/// It will return a mutable AQL statement which can be further refined, using the
/// functions described below.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// In the examples the *toArray* function is used to print the result.
/// The description of this function can be found below.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._edges().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered edges:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphEdgesFiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._edges({type: "married"}).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._edges = function(edgeExample) {
  var AQLStmt = new AQLGenerator(this);
  // If no direction is specified all edges are duplicated.
  // => For initial requests a direction has to be set
  return AQLStmt.outEdges(edgeExample);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_vertices
/// @brief Select some vertices from the graph.
///
/// `graph._vertices(examples)`
///
/// Creates an AQL statement to select a subset of the vertices stored in the graph.
/// This is one of the entry points for the fluent AQL interface.
/// It will return a mutable AQL statement which can be further refined, using the
/// functions described below.
/// The resulting set of edges can be filtered by defining one or more *examples*.
///
/// @PARAMS
///
/// @PARAM{examples, object, optional}
/// See [Definition of examples](#definition_of_examples)
///
/// @EXAMPLES
///
/// In the examples the *toArray* function is used to print the result.
/// The description of this function can be found below.
///
/// To request unfiltered vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVerticesUnfiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._vertices().toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// To request filtered vertices:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphVerticesFiltered}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._vertices([{name: "Alice"}, {name: "Bob"}]).toArray();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._vertices = function(example) {
  var AQLStmt = new AQLGenerator(this);
  return AQLStmt.vertices(example);
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_fromVertex
/// @brief Get the source vertex of an edge
///
/// `graph._fromVertex(edgeId)`
///
/// Returns the vertex defined with the attribute *_from* of the edge with *edgeId* as its *_id*.
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphGetFromVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._fromVertex("relation/aliceAndBob")
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._fromVertex = function(edgeId) {
  if (typeof edgeId !== 'string' ||
      edgeId.indexOf('/') === -1) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
    throw err;
  }
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._from;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_toVertex
/// @brief Get the target vertex of an edge
///
/// `graph._toVertex(edgeId)`
///
/// Returns the vertex defined with the attribute *_to* of the edge with *edgeId* as its *_id*.
///
/// @PARAMS
///
/// @PARAM{edgeId, string, required}
/// *_id* attribute of the edge
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphGetToVertex}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("social");
///   graph._toVertex("relation/aliceAndBob")
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._toVertex = function(edgeId) {
  if (typeof edgeId !== 'string' ||
      edgeId.indexOf('/') === -1) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.code;
    err.errorMessage = arangodb.errors.ERROR_ARANGO_DOCUMENT_HANDLE_BAD.message;
    throw err;
  }
  var edgeCollection = this._getEdgeCollectionByName(edgeId.split("/")[0]);
  var document = edgeCollection.document(edgeId);
  if (document) {
    var vertexId = document._to;
    var vertexCollection = this._getVertexCollectionByName(vertexId.split("/")[0]);
    return vertexCollection.document(vertexId);
  }
};


////////////////////////////////////////////////////////////////////////////////
/// @brief get edge collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getEdgeCollectionByName = function(name) {
  if (this.__edgeCollections[name]) {
    return this.__edgeCollections[name];
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST.message + ": " + name;
  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get vertex collection by name.
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getVertexCollectionByName = function(name) {
  if (this.__vertexCollections[name]) {
    return this.__vertexCollections[name];
  }
  var err = new ArangoError();
  err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
  err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message + ": " + name;
  throw err;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_neighbors
/// @brief Get all neighbors of the vertices defined by the example
///
/// `graph._neighbors(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertexExample.
/// The complexity of this method is **O(n\*m^x)** with *n* being the vertices defined by the
/// parameter vertexExamplex, *m* the average amount of neighbors and *x* the maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// See [Definition of examples](#definition_of_examples)
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeExamples*: Filter the edges, see [Definition of examples](#definition_of_examples)
///   * *neighborExamples*: Filter the neighbor vertices, see [Definition of examples](#definition_of_examples)
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *vertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///   * *minDepth*: Defines the minimal number of intermediate steps to neighbors (default is 1).
///   * *maxDepth*: Defines the maximal number of intermediate steps to neighbors (default is 1).
///
/// @EXAMPLES
///
/// A route planner example, all neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleNeighbors1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._neighbors({isCapital : true});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all outbound neighbors of Hamburg.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleNeighbors2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._neighbors('germanCity/Hamburg', {direction : 'outbound', maxDepth : 2});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._neighbors = function(vertexExample, options) {
  var AQLStmt = new AQLGenerator(this);
  // If no direction is specified all edges are duplicated.
  // => For initial requests a direction has to be set
  if (!options) {
    options = {};
  }
  return AQLStmt.vertices(vertexExample).neighbors(options.neighborExamples, options)
    .toArray();
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_common_neighbors
/// @brief Get all common neighbors of the vertices defined by the examples.
///
/// `graph._commonNeighbors(vertex1Example, vertex2Examples, optionsVertex1, optionsVertex2)`
///
/// This function returns the intersection of *graph_module._neighbors(vertex1Example, optionsVertex1)*
/// and *graph_module._neighbors(vertex2Example, optionsVertex2)*.
/// For parameter documentation see [_neighbors](#_neighbors).
///
/// The complexity of this method is **O(n\*m^x)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples, *m* the average amount of neighbors and *x* the
/// maximal depths.
/// Hence the default call would have a complexity of **O(n\*m)**;
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighbors1}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._commonNeighbors({isCapital : true}, {isCapital : true});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighbors2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._commonNeighbors(
/// |   'germanCity/Hamburg',
/// |   {},
/// |   {direction : 'outbound', maxDepth : 2},
///     {direction : 'outbound', maxDepth : 2});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonNeighbors = function(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {

  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_NEIGHBORS(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options1'
    + ',@options2'
    + ') RETURN e';
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  var bindVars = {
    "graphName": this.__name,
    "options1": optionsVertex1,
    "options2": optionsVertex2,
    "ex1": ex1,
    "ex2": ex2
  };
  return db._query(query, bindVars, {count: true}).toArray();
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_count_common_neighbors
/// @brief Get the amount of common neighbors of the vertices defined by the examples.
///
/// `graph._countCommonNeighbors(vertex1Example, vertex2Examples, optionsVertex1, optionsVertex2)`
///
/// Similar to [_commonNeighbors](#_commonNeighbors) but returns count instead of the elements.
///
/// @EXAMPLES
///
/// A route planner example, all common neighbors of capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighborsAmount1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   var example = { isCapital: true };
///   var options = { includeData: true };
///   graph._countCommonNeighbors(example, example, options, options);
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all common outbound neighbors of Hamburg with any other location
/// which have a maximal depth of 2 :
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCommonNeighborsAmount2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   var options = { direction: 'outbound', maxDepth: 2, includeData: true };
///   graph._countCommonNeighbors('germanCity/Hamburg', {}, options, options);
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonNeighbors = function(vertex1Example, vertex2Example, optionsVertex1, optionsVertex2) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_NEIGHBORS(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options1'
    + ',@options2'
    + ') RETURN [e.left, e.right, LENGTH(e.neighbors)]';
  optionsVertex1 = optionsVertex1 || {};
  optionsVertex2 = optionsVertex2 || {};
  var bindVars = {
    "graphName": this.__name,
    "options1": optionsVertex1,
    "options2": optionsVertex2,
    "ex1": ex1,
    "ex2": ex2
  };

  var result = db._query(query, bindVars, {count: true}).toArray(),
    tmp = {}, tmp2={}, returnHash = [];
  result.forEach(function (r) {
    if (!tmp[r[0]]) {
      tmp[r[0]] = [];
    }
    tmp2 = {};
    tmp2[r[1]] = r[2];
    tmp[r[0]].push(tmp2);
  });
  Object.keys(tmp).forEach(function(w) {
    tmp2 = {};
    tmp2[w] = tmp[w];
    returnHash.push(tmp2);
  });
  return returnHash;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_common_properties
/// @brief Get the vertices of the graph that share common properties.
///
/// `graph._commonProperties(vertex1Example, vertex2Examples, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertex1Example and vertex2Example.
///
/// The complexity of this method is **O(n)** with *n* being the maximal amount of vertices
/// defined by the parameters vertexExamples.
///
/// @PARAMS
///
/// @PARAM{vertex1Examples, object, optional}
/// Filter the set of source vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{vertex2Examples, object, optional}
/// Filter the set of vertices compared to, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *vertex1CollectionRestriction* : One or a list of vertex-collection names that should be
///       searched for source vertices.
///   * *vertex2CollectionRestriction* : One or a list of vertex-collection names that should be
///       searched for compare vertices.
///   * *ignoreProperties* : One or a list of attribute names of a document that should be ignored.
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleProperties1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._commonProperties({}, {});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleProperties2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._commonProperties({}, {}, {ignoreProperties: 'population'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._commonProperties = function(vertex1Example, vertex2Example, options) {

  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_PROPERTIES(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ')  SORT  ATTRIBUTES(e)[0] RETURN e';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  return db._query(query, bindVars, {count: true}).toArray();

};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_count_common_properties
/// @brief Get the amount of vertices of the graph that share common properties.
///
/// `graph._countCommonProperties(vertex1Example, vertex2Examples, options)`
///
/// Similar to [_commonProperties](#_commonProperties) but returns count instead of
/// the objects.
///
/// @EXAMPLES
///
/// A route planner example, all locations with the same properties:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAmountProperties1}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._countCommonProperties({}, {});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, all German cities which share same properties except for population.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAmountProperties2}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// | graph._countCommonProperties({}, {}, {vertex1CollectionRestriction : 'germanCity',
///   vertex2CollectionRestriction : 'germanCity' ,ignoreProperties: 'population'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._countCommonProperties = function(vertex1Example, vertex2Example, options) {
  var ex1 = transformExample(vertex1Example);
  var ex2 = transformExample(vertex2Example);
  var query = "FOR e"
    + " IN GRAPH_COMMON_PROPERTIES(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ') FOR a in ATTRIBUTES(e)  SORT  ATTRIBUTES(e)[0] RETURN [ ATTRIBUTES(e)[0], LENGTH(e[a]) ]';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(query, bindVars, {count: true}).toArray(), returnHash = [];
  result.forEach(function (r) {
    var tmp = {};
    tmp[r[0]] = r[1];
    returnHash.push(tmp);
  });
  return returnHash;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_paths
/// @brief The _paths function returns all paths of a graph.
///
/// `graph._paths(options)`
///
/// This function determines all available paths in a graph.
///
/// The complexity of this method is **O(n\*n\*m)** with *n* being the amount of vertices in
/// the graph and *m* the average amount of connected edges;
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object containing options, see below:
///   * *direction*        : The direction of the edges. Possible values are *any*,
///     *inbound* and *outbound* (default).
///   * *followCycles* (optional) : If set to *true* the query follows cycles in the graph,
///     default is false.
///   * *minLength* (optional)     : Defines the minimal length a path must
///     have to be returned (default is 0).
///   * *maxLength* (optional)     : Defines the maximal length a path must
///      have to be returned (default is 10).
///
/// @EXAMPLES
///
/// Return all paths of the graph "social":
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModulePaths}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   g._paths();
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Return all inbound paths of the graph "social" with a maximal
/// length of 1 and a minimal length of 2:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModulePaths2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("social");
///   g._paths({direction : 'inbound', minLength : 1, maxLength :  2});
/// ~ examples.dropGraph("social");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._paths = function(options) {
  var query = "RETURN"
    + " GRAPH_PATHS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_shortest_path
/// @brief The _shortestPath function returns all shortest paths of a graph.
///
/// `graph._shortestPath(startVertexExample, endVertexExample, options)`
///
/// This function determines all shortest paths in a graph.
/// The function accepts an id, an example, a list of examples
/// or even an empty example as parameter for
/// start and end vertex. If one wants to call this function to receive nearly all
/// shortest paths for a graph the option *algorithm* should be set to
/// [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
/// to increase performance.
/// If no algorithm is provided in the options the function chooses the appropriate
/// one (either [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm)
/// or [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)) according to its parameters.
/// The length of a path is by default the amount of edges from one start vertex to
/// an end vertex. The option weight allows the user to define an edge attribute
/// representing the length.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{startVertexExample, object, optional}
/// An example for the desired start Vertices
/// (see [Definition of examples](#definition_of_examples)).
///
/// @PARAM{endVertexExample, object, optional}
/// An example for the desired
/// end Vertices (see [Definition of examples](#definition_of_examples)).
///
/// @PARAM{options, object, optional}
/// An object containing options, see below:
///   * *direction*                        : The direction of the edges as a string.
///   Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction*        : One or multiple edge
///   collection names. Only edges from these collections will be considered for the path.
///   * *startVertexCollectionRestriction* : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   start vertex of a path.
///   * *endVertexCollectionRestriction*   : One or multiple vertex
///   collection names. Only vertices from these collections will be considered as
///   end vertex of a path.
///   * *edgeExamples*                     : A filter example for the
///   edges in the shortest paths
///   (see [example](#short_explaination_of_the_vertex_example_parameter)).
///   * *algorithm*                        : The algorithm to calculate
///   the shortest paths. If both start and end vertex examples are empty
///   [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) is
///   used, otherwise the default is [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm)
///   * *weight*                           : The name of the attribute of
///   the edges containing the length as a string.
///   * *defaultWeight*                    : Only used with the option *weight*.
///   If an edge does not have the attribute named as defined in option *weight* this default
///   is used as length.
///   If no default is supplied the default would be positive Infinity so the path could
///   not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, shortest path from all german to all french cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleShortestPaths1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._shortestPath({}, {}, {weight : 'distance', endVertexCollectionRestriction : 'frenchCity',
///   startVertexCollectionRestriction : 'germanCity'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, shortest path from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleShortestPaths2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._shortestPath([{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], 'frenchCity/Lyon',
///   {weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._shortestPath = function(startVertexExample, endVertexExample, options) {
  var ex1 = transformExample(startVertexExample);
  var ex2 = transformExample(endVertexExample);
  var query = "RETURN"
    + " GRAPH_SHORTEST_PATH(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(query, bindVars).toArray();
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_distance_to
/// @brief The _distanceTo function returns all paths and there distance within a graph.
///
/// `graph._distanceTo(startVertexExample, endVertexExample, options)`
///
/// This function is a wrapper of [graph._shortestPath](#_shortestpath).
/// It does not return the actual path but only the distance between two vertices.
///
/// @EXAMPLES
///
/// A route planner example, shortest distance from all german to all french cities:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDistanceTo1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._distanceTo({}, {}, {weight : 'distance', endVertexCollectionRestriction : 'frenchCity',
///   startVertexCollectionRestriction : 'germanCity'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, shortest distance from Hamburg and Cologne to Lyon:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDistanceTo2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var g = examples.loadGraph("routeplanner");
/// | g._distanceTo([{_id: 'germanCity/Cologne'},{_id: 'germanCity/Munich'}], 'frenchCity/Lyon',
///   {weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._distanceTo = function(startVertexExample, endVertexExample, options) {
  var ex1 = transformExample(startVertexExample);
  var ex2 = transformExample(endVertexExample);
  var query = "RETURN"
    + " GRAPH_DISTANCE_TO(@graphName"
    + ',@ex1'
    + ',@ex2'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1,
    "ex2": ex2
  };
  var result = db._query(query, bindVars).toArray();
  return result[0];
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_eccentricity
/// @brief Get the
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the vertices defined by the examples.
///
/// `graph._absoluteEccentricity(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertexExample.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// Filter the vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *startVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for source vertices.
///   * *endVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for target vertices.
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition_of_examples)
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the eccentricity can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsEccentricity1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | db._query("RETURN GRAPH_ABSOLUTE_ECCENTRICITY("
/// |   + "'routeplanner', {})"
///   ).toArray();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsEccentricity2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteEccentricity({}, {weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute eccentricity of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsEccentricity3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._absoluteEccentricity({}, {startVertexCollectionRestriction : 'germanCity',
///   direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteEccentricity = function(vertexExample, options) {
  var ex1 = transformExample(vertexExample);
  var query = "RETURN"
    + " GRAPH_ABSOLUTE_ECCENTRICITY(@graphName"
    + ',@ex1'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_eccentricity
/// @brief Get the normalized
/// [eccentricity](http://en.wikipedia.org/wiki/Distance_%28graph_theory%29)
/// of the vertices defined by the examples.
///
/// `graph._eccentricity(vertexExample, options)`
///
/// Similar to [_absoluteEccentricity](#_absoluteeccentricity) but returns a normalized result.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @EXAMPLES
///
/// A route planner example, the eccentricity of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleEccentricity2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._eccentricity();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the weighted eccentricity.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleEccentricity3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._eccentricity({weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._eccentricity = function(options) {
  var query = "RETURN"
    + " GRAPH_ECCENTRICITY(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_closeness
/// @brief Get the
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of the vertices defined by the examples.
///
/// `graph._absoluteCloseness(vertexExample, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for *vertexExample*.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// Filter the vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *startVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for source vertices.
///   * *endVertexCollectionRestriction* : One or a list of vertex-collection names that should be
///       considered for target vertices.
///   * *edgeExamples*: Filter the edges to be followed, see [Definition of examples](#definition_of_examples)
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the closeness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteCloseness({});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteCloseness({}, {weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute closeness of all German Cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsCloseness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
/// | graph._absoluteCloseness({}, {startVertexCollectionRestriction : 'germanCity',
///   direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteCloseness = function(vertexExample, options) {
  var ex1 = transformExample(vertexExample);
  var query = "RETURN"
    + " GRAPH_ABSOLUTE_CLOSENESS(@graphName"
    + ',@ex1'
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options,
    "ex1": ex1
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_closeness
/// @brief Get the normalized
/// [closeness](http://en.wikipedia.org/wiki/Centrality#Closeness_centrality)
/// of graphs vertices.
///
/// `graph._closeness(options)`
///
/// Similar to [_absoluteCloseness](#_absolutecloseness) but returns a normalized value.
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @EXAMPLES
///
/// A route planner example, the normalized closeness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness1}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._closeness();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the closeness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness2}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._closeness({weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the closeness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleCloseness3}
/// var examples = require("org/arangodb/graph-examples/example-graph.js");
/// var graph = examples.loadGraph("routeplanner");
/// graph._closeness({direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._closeness = function(options) {
  var query = "RETURN"
    + " GRAPH_CLOSENESS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_absolute_betweenness
/// @brief Get the
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of all vertices in the graph.
///
/// `graph._absoluteBetweenness(vertexExample, options)`
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{vertexExample, object, optional}
/// Filter the vertices, see [Definition of examples](#definition_of_examples)
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the betweeness can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the absolute betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteBetweenness({});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteBetweenness({weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the absolute betweenness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleAbsBetweenness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._absoluteBetweenness({direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._absoluteBetweenness = function(example, options) {

  var query = "RETURN"
    + " GRAPH_ABSOLUTE_BETWEENNESS(@graphName"
    + ",@example"
    + ",@options"
    + ")";
  options = options || {};
  var bindVars = {
    "example": example,
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_betweenness
/// @brief Get the normalized
/// [betweenness](http://en.wikipedia.org/wiki/Betweenness_centrality)
/// of graphs vertices.
///
/// `graph_module._betweenness(options)`
///
/// Similar to [_absoluteBetweeness](#_absolutebetweeness) but returns normalized values.
///
/// @EXAMPLES
///
/// A route planner example, the betweenness of all locations.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._betweenness();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness of all locations.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._betweenness({weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the betweenness of all cities regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleBetweenness3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._betweenness({direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._betweenness = function(options) {

  var query = "RETURN"
    + " GRAPH_BETWEENNESS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_radius
/// @brief Get the
/// [radius](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
/// of a graph.
///
/// `graph._radius(options)`
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the radius can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the radius of the graph.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleRadius1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._radius();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleRadius2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._radius({weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the radius of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleRadius3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._radius({direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._radius = function(options) {

  var query = "RETURN"
    + " GRAPH_RADIUS(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};



////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_diameter
/// @brief Get the
/// [diameter](http://en.wikipedia.org/wiki/Eccentricity_%28graph_theory%29)
/// of a graph.
///
/// `graph._diameter(graphName, options)`
///
/// The complexity of the function is described
/// [here](../Aql/GraphOperations.html#the_complexity_of_the_shortest_path_algorithms).
///
/// @PARAMS
///
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *direction*: The direction of the edges. Possible values are *outbound*, *inbound* and *any* (default).
///   * *algorithm*: The algorithm to calculate the shortest paths, possible values are
///       [Floyd-Warshall](http://en.wikipedia.org/wiki/Floyd%E2%80%93Warshall_algorithm) and
///       [Dijkstra](http://en.wikipedia.org/wiki/Dijkstra's_algorithm).
///   * *weight*: The name of the attribute of the edges containing the weight.
///   * *defaultWeight*: Only used with the option *weight*.
///       If an edge does not have the attribute named as defined in option *weight* this default
///       is used as weight.
///       If no default is supplied the default would be positive infinity so the path and
///       hence the radius can not be calculated.
///
/// @EXAMPLES
///
/// A route planner example, the diameter of the graph.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._diameter();
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph.
/// This considers the actual distances.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter2}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._diameter({weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// A route planner example, the diameter of the graph regarding only
/// outbound paths.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleDiameter3}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._diameter({direction : 'outbound', weight : 'distance'});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////
Graph.prototype._diameter = function(options) {

  var query = "RETURN"
    + " GRAPH_DIAMETER(@graphName"
    + ',@options'
    + ')';
  options = options || {};
  var bindVars = {
    "graphName": this.__name,
    "options": options
  };
  var result = db._query(query, bindVars).toArray();
  if (result.length === 1) {
    return result[0];
  }
  return result;
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__extendEdgeDefinitions
/// @brief Add another edge definition to the graph
///
/// `graph._extendEdgeDefinitions(edgeDefinition)`
///
/// Extends the edge definitions of a graph. If an orphan collection is used in this
/// edge definition, it will be removed from the orphanage. If the edge collection of
/// the edge definition to add is already used in the graph or used in a different
/// graph with different *from* and/or *to* collections an error is thrown.
///
/// @PARAMS
///
/// @PARAM{edgeDefinition, object, required}
/// The relation definition to extend the graph
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__extendEdgeDefinitions}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._extendEdgeDefinitions(ed2);
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._extendEdgeDefinitions = function(edgeDefinition) {
  edgeDefinition = sortEdgeDefinition(edgeDefinition);
  var self = this;
  var err;
  //check if edgeCollection not already used
  var eC = edgeDefinition.collection;
  // ... in same graph
  if (this.__edgeCollections[eC] !== undefined) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_MULTI_USE.message;
    throw err;
  }
  //in different graph
  db._graphs.toArray().forEach(
    function(singleGraph) {
      var sGEDs = singleGraph.edgeDefinitions;
      sGEDs.forEach(
        function(sGED) {
          var col = sGED.collection;
          if (col === eC) {
            if (JSON.stringify(sGED) !== JSON.stringify(edgeDefinition)) {
              err = new ArangoError();
              err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.code;
              err.errorMessage = col
                + " " + arangodb.errors.ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS.message;
              throw err;
            }
          }
        }
      );
    }
  );

  findOrCreateCollectionsByEdgeDefinitions([edgeDefinition]);

  this.__edgeDefinitions.push(edgeDefinition);
  db._graphs.update(this.__name, {edgeDefinitions: this.__edgeDefinitions});
  this.__edgeCollections[edgeDefinition.collection] = db[edgeDefinition.collection];
  edgeDefinition.from.forEach(
    function(vc) {
      self[vc] = db[vc];
      //remove from __orphanCollections
      var orphanIndex = self.__orphanCollections.indexOf(vc);
      if (orphanIndex !== -1) {
        self.__orphanCollections.splice(orphanIndex, 1);
      }
      //push into __vertexCollections
      if (self.__vertexCollections[vc] === undefined) {
        self.__vertexCollections[vc] = db[vc];
      }
    }
  );
  edgeDefinition.to.forEach(
    function(vc) {
      self[vc] = db[vc];
      //remove from __orphanCollections
      var orphanIndex = self.__orphanCollections.indexOf(vc);
      if (orphanIndex !== -1) {
        self.__orphanCollections.splice(orphanIndex, 1);
      }
      //push into __vertexCollections
      if (self.__vertexCollections[vc] === undefined) {
        self.__vertexCollections[vc] = db[vc];
      }
    }
  );
  updateBindCollections(this);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function for editing edge definitions
////////////////////////////////////////////////////////////////////////////////

var changeEdgeDefinitionsForGraph = function(graph, edgeDefinition, newCollections, possibleOrphans, self) {

  var graphCollections = [];
  var graphObj = _graph(graph._key);
  var eDs = graph.edgeDefinitions;
  var gotAHit = false;

  //replace edgeDefintion
  eDs.forEach(
    function(eD, id) {
      if(eD.collection === edgeDefinition.collection) {
        gotAHit = true;
        eDs[id].from = edgeDefinition.from;
        eDs[id].to = edgeDefinition.to;
        db._graphs.update(graph._key, {edgeDefinitions: eDs});
        if (graph._key === self.__name) {
          self.__edgeDefinitions[id].from = edgeDefinition.from;
          self.__edgeDefinitions[id].to = edgeDefinition.to;
        }
      } else {
        //collect all used collections
        graphCollections = _.union(graphCollections, eD.from);
        graphCollections = _.union(graphCollections, eD.to);
      }
    }
  );
  if (!gotAHit) {
    return;
  }

  //remove used collection from orphanage
  if (graph._key === self.__name) {
    newCollections.forEach(
      function(nc) {
        if (self.__vertexCollections[nc] === undefined) {
          self.__vertexCollections[nc] = db[nc];
        }
        try {
          self._removeVertexCollection(nc, false);
        } catch (ignore) { }
      }
    );
    possibleOrphans.forEach(
      function(po) {
        if (graphCollections.indexOf(po) === -1) {
          delete self.__vertexCollections[po];
          self._addVertexCollection(po);
        }
      }
    );
  } else {
    newCollections.forEach(
      function(nc) {
        try {
          graphObj._removeVertexCollection(nc, false);
        } catch (ignore) { }
      }
    );
    possibleOrphans.forEach(
      function(po) {
        if (graphCollections.indexOf(po) === -1) {
          delete graphObj.__vertexCollections[po];
          graphObj._addVertexCollection(po);
        }
      }
    );

  }

  //move unused collections to orphanage
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__editEdgeDefinition
/// @brief Modify an relation definition
///
/// `graph_module._editEdgeDefinition(edgeDefinition)`
///
/// Edits one relation definition of a graph. The edge definition used as argument will
/// replace the existing edge definition of the graph which has the same collection.
/// Vertex Collections of the replaced edge definition that are not used in the new
/// definition will transform to an orphan. Orphans that are used in this new edge
/// definition will be deleted from the list of orphans. Other graphs with the same edge
/// definition will be modified, too.
///
/// @PARAMS
///
/// @PARAM{edgeDefinition, object, required}
/// The edge definition to replace the existing edge
/// definition with the same attribute *collection*.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__editEdgeDefinition}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var original = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var modified = graph_module._relation("myEC1", ["myVC2"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [original]);
///   graph._editEdgeDefinitions(modified);
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._editEdgeDefinitions = function(edgeDefinition) {
  edgeDefinition = sortEdgeDefinition(edgeDefinition);
  var self = this;

  //check, if in graphs edge definition
  if (this.__edgeCollections[edgeDefinition.collection] === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
    throw err;
  }

  findOrCreateCollectionsByEdgeDefinitions([edgeDefinition]);

  //evaluate collections to add to orphanage
  var possibleOrphans = [];
  var currentEdgeDefinition;
  this.__edgeDefinitions.forEach(
    function(ed) {
      if (edgeDefinition.collection === ed.collection) {
        currentEdgeDefinition = ed;
      }
    }
  );

  var currentCollections = _.union(currentEdgeDefinition.from, currentEdgeDefinition.to);
  var newCollections = _.union(edgeDefinition.from, edgeDefinition.to);
  currentCollections.forEach(
    function(colName) {
      if (newCollections.indexOf(colName) === -1) {
        possibleOrphans.push(colName);
      }
    }
  );
  //change definition for ALL graphs
  var graphs = getGraphCollection().toArray();
  graphs.forEach(
    function(graph) {
      changeEdgeDefinitionsForGraph(graph, edgeDefinition, newCollections, possibleOrphans, self);
    }
  );
  updateBindCollections(this);
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__deleteEdgeDefinition
/// @brief Delete one relation definition
///
/// `graph_module._deleteEdgeDefinition(edgeCollectionName, dropCollection)`
///
/// Deletes a relation definition defined by the edge collection of a graph. If the
/// collections defined in the edge definition (collection, from, to) are not used
/// in another edge definition of the graph, they will be moved to the orphanage.
///
/// @PARAMS
///
/// @PARAM{edgeCollectionName, string, required}
/// Name of edge collection in the relation definition.
/// @PARAM{dropCollection, boolean, optional}
/// Define if the edge collection should be dropped. Default false.
///
/// @EXAMPLES
///
/// Remove an edge definition but keep the edge collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinition}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [ed1, ed2]);
///   graph._deleteEdgeDefinition("myEC1");
///   db._collection("myEC1");
/// ~ db._drop("myEC1");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Remove an edge definition and drop the edge collection:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__deleteEdgeDefinitionWithDrop}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var ed2 = graph_module._relation("myEC2", ["myVC1"], ["myVC3"]);
///   var graph = graph_module._create("myGraph", [ed1, ed2]);
///   graph._deleteEdgeDefinition("myEC1", true);
///   db._collection("myEC1");
/// ~ db._drop("myEC1");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._deleteEdgeDefinition = function(edgeCollection, dropCollection) {

  //check, if in graphs edge definition
  if (this.__edgeCollections[edgeCollection] === undefined) {
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_EDGE_COLLECTION_NOT_USED.message;
    throw err;
  }

  var edgeDefinitions = this.__edgeDefinitions,
    self = this,
    usedVertexCollections = [],
    possibleOrphans = [],
    index;

  edgeDefinitions.forEach(
    function(edgeDefinition, idx) {
      if (edgeDefinition.collection === edgeCollection) {
        index = idx;
        possibleOrphans = edgeDefinition.from;
        possibleOrphans = _.union(possibleOrphans, edgeDefinition.to);
      } else {
        usedVertexCollections = _.union(usedVertexCollections, edgeDefinition.from);
        usedVertexCollections = _.union(usedVertexCollections, edgeDefinition.to);
      }
    }
  );
  this.__edgeDefinitions.splice(index, 1);
  possibleOrphans.forEach(
    function(po) {
      if (usedVertexCollections.indexOf(po) === -1) {
        self.__orphanCollections.push(po);
      }
    }
  );

  updateBindCollections(this);
  db._graphs.update(
    this.__name,
    {
      orphanCollections: this.__orphanCollections,
      edgeDefinitions: this.__edgeDefinitions
    }
  );

  if (dropCollection) {
    db._drop(edgeCollection);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__addVertexCollection
/// @brief Add a vertex collection to the graph
///
/// `graph._addVertexCollection(vertexCollectionName, createCollection)`
///
/// Adds a vertex collection to the set of orphan collections of the graph. If the
/// collection does not exist, it will be created. If it is already used by any edge
/// definition of the graph, an error will be thrown.
///
/// @PARAMS
///
/// @PARAM{vertexCollectionName, string, required}
/// Name of vertex collection.
///
/// @PARAM{createCollection, boolean, optional}
/// If true the collection will be created if it does not exist. Default: true.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__addVertexCollection}
///   var graph_module = require("org/arangodb/general-graph");
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
/// ~ db._drop("myVC3");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._addVertexCollection = function(vertexCollectionName, createCollection) {
  //check edgeCollection
  var ec = db._collection(vertexCollectionName);
  var err;
  if (ec === null) {
    if (createCollection !== false) {
      db._create(vertexCollectionName);
    } else {
      err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
      err.errorMessage = vertexCollectionName + arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message;
      throw err;
    }
  } else if (ec.type() !== 2) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX.message;
    throw err;
  }
  if (this.__vertexCollections[vertexCollectionName] !== undefined) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF.message;
    throw err;
  }
  if (_.contains(this.__orphanCollections, vertexCollectionName)) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS.message;
    throw err;
  }
  this.__orphanCollections.push(vertexCollectionName);
  updateBindCollections(this);
  db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});

};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__orphanCollections
/// @brief Get all orphan collections
///
/// `graph._orphanCollections()`
///
/// Returns all vertex collections of the graph that are not used in any edge definition.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__orphanCollections}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
///   graph._orphanCollections();
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._orphanCollections = function() {
  return this.__orphanCollections;
};

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph__removeVertexCollection
/// @brief Remove a vertex collection from the graph
///
/// `graph._removeVertexCollection(vertexCollectionName, dropCollection)`
///
/// Removes a vertex collection from the graph.
/// Only collections not used in any relation definition can be removed.
/// Optionally the collection can be deleted, if it is not used in any other graph.
///
/// @PARAMS
///
/// @PARAM{vertexCollectionName, string, required}
/// Name of vertex collection.
///
/// @PARAM{dropCollection, boolean, optional}
/// If true the collection will be dropped if it is
/// not used in any other graph. Default: false.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph__removeVertexCollections}
///   var graph_module = require("org/arangodb/general-graph")
/// ~ if (graph_module._exists("myGraph")){var blub = graph_module._drop("myGraph", true);}
///   var ed1 = graph_module._relation("myEC1", ["myVC1"], ["myVC2"]);
///   var graph = graph_module._create("myGraph", [ed1]);
///   graph._addVertexCollection("myVC3", true);
///   graph._addVertexCollection("myVC4", true);
///   graph._orphanCollections();
///   graph._removeVertexCollection("myVC3");
///   graph._orphanCollections();
/// ~ db._drop("myVC3");
/// ~ var blub = graph_module._drop("myGraph", true);
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._removeVertexCollection = function(vertexCollectionName, dropCollection) {
  var err;
  if (db._collection(vertexCollectionName) === null) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST.message;
    throw err;
  }
  var index = this.__orphanCollections.indexOf(vertexCollectionName);
  if (index === -1) {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION.message;
    throw err;
  }
  this.__orphanCollections.splice(index, 1);
  delete this[vertexCollectionName];
  db._graphs.update(this.__name, {orphanCollections: this.__orphanCollections});

  if (dropCollection === true) {
    var graphs = getGraphCollection().toArray();
    if (checkIfMayBeDropped(vertexCollectionName, null, graphs)) {
      db._drop(vertexCollectionName);
    }
  }
  updateBindCollections(this);
};


////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_connectingEdges
/// @brief Get all connecting edges between 2 groups of vertices defined by the examples
///
/// `graph._connectingEdges(vertexExample, vertexExample2, options)`
///
/// The function accepts an id, an example, a list of examples or even an empty
/// example as parameter for vertexExample.
///
/// @PARAMS
///
/// @PARAM{vertexExample1, object, optional}
/// See [Definition of examples](#definition_of_examples)
/// @PARAM{vertexExample2, object, optional}
/// See [Definition of examples](#definition_of_examples)
/// @PARAM{options, object, optional}
/// An object defining further options. Can have the following values:
///   * *edgeExamples*: Filter the edges, see [Definition of examples](#definition_of_examples)
///   * *edgeCollectionRestriction* : One or a list of edge-collection names that should be
///       considered to be on the path.
///   * *vertex1CollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///   * *vertex2CollectionRestriction* : One or a list of vertex-collection names that should be
///       considered on the intermediate vertex steps.
///
/// @EXAMPLES
///
/// A route planner example, all connecting edges between capitals.
///
/// @EXAMPLE_ARANGOSH_OUTPUT{generalGraphModuleConnectingEdges1}
///   var examples = require("org/arangodb/graph-examples/example-graph.js");
///   var graph = examples.loadGraph("routeplanner");
///   graph._getConnectingEdges({isCapital : true}, {isCapital : true});
/// ~ examples.dropGraph("routeplanner");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
///
/// @endDocuBlock
//
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._getConnectingEdges = function(vertexExample1, vertexExample2, options) {

  options = options || {};

  var opts = {
    includeData: true
  };

  if (options.vertex1CollectionRestriction) {
    opts.startVertexCollectionRestriction = options.vertex1CollectionRestriction;
  }

  if (options.vertex2CollectionRestriction) {
    opts.endVertexCollectionRestriction = options.vertex2CollectionRestriction;
  }

  if (options.edgeCollectionRestriction) {
    opts.edgeCollectionRestriction = options.edgeCollectionRestriction;
  }

  if (options.edgeExamples) {
    opts.edgeExamples = options.edgeExamples;
  }

  if (vertexExample2) {
    opts.neighborExamples = vertexExample2;
  }

  var query = "RETURN"
    + " GRAPH_EDGES(@graphName"
    + ',@vertexExample'
    + ',@options'
    + ')';
  var bindVars = {
    "graphName": this.__name,
    "vertexExample": vertexExample1,
    "options": opts
  };
  var result = db._query(query, bindVars).toArray();
  return result[0];
};




////////////////////////////////////////////////////////////////////////////////
/// @brief print basic information for the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function(context) {
  var name = this.__name;
  var edgeDefs = printEdgeDefinitions(this.__edgeDefinitions);
  context.output += "[ Graph ";
  context.output += name;
  context.output += " EdgeDefinitions: ";
  internal.printRecursive(edgeDefs, context);
  context.output += " VertexCollections: ";
  internal.printRecursive(this.__orphanCollections, context);
  context.output += " ]";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

/// Deprecated function (announced 2.3)
exports._undirectedRelation = _undirectedRelation;
/// Deprecated function (announced 2.3)
exports._directedRelation = function () {
  return _relation.apply(this, arguments);
};
exports._relation = _relation;
exports._graph = _graph;
exports._edgeDefinitions = _edgeDefinitions;
exports._extendEdgeDefinitions = _extendEdgeDefinitions;
exports._create = _create;
exports._drop = _drop;
exports._exists = _exists;
exports._list = _list;
exports._listObjects = _listObjects;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:


////////////////////////////////////////////////////////////////////////////////
/// some more documentation
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create_graph_example1
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example1}
///   var graph_module = require("org/arangodb/general-graph");
///   var edgeDefinitions = graph_module._edgeDefinitions();
///   graph_module._extendEdgeDefinitions(edgeDefinitions, graph_module._relation("friend_of", "Customer", "Customer"));
/// | graph_module._extendEdgeDefinitions(
/// | edgeDefinitions, graph_module._relation(
///   "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
///   graph_module._create("myStore", edgeDefinitions);
/// ~ graph_module._drop("myStore");
/// ~ db._drop("Electronics");
/// ~ db._drop("Customer");
/// ~ db._drop("Groceries");
/// ~ db._drop("Company");
/// ~ db._drop("has_bought");
/// ~ db._drop("friend_of");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @startDocuBlock JSF_general_graph_create_graph_example2
/// @EXAMPLE_ARANGOSH_OUTPUT{general_graph_create_graph_example2}
///   var graph_module = require("org/arangodb/general-graph");
/// |  var edgeDefinitions = graph_module._edgeDefinitions(
/// |  graph_module._relation("friend_of", ["Customer"], ["Customer"]), graph_module._relation(
///    "has_bought", ["Customer", "Company"], ["Groceries", "Electronics"]));
///   graph_module._create("myStore", edgeDefinitions);
/// ~ graph_module._drop("myStore");
/// ~ db._drop("Electronics");
/// ~ db._drop("Customer");
/// ~ db._drop("Groceries");
/// ~ db._drop("Company");
/// ~ db._drop("has_bought");
/// ~ db._drop("friend_of");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
///
////////////////////////////////////////////////////////////////////////////////
});

module.define("org/arangodb/graph-blueprint", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var arangodb = require("org/arangodb"),
  is = require("org/arangodb/is"),
  common = require("org/arangodb/graph-common"),
  Edge = common.Edge,
  Graph = common.Graph,
  Vertex = common.Vertex,
  GraphArray = common.GraphArray,
  Iterator = common.Iterator,
  GraphAPI = require ("org/arangodb/api/graph").GraphAPI;


// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of an edge
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.setProperty = function (name, value) {
  var results,
    update = this._properties;

  update[name] = value;
  this._graph.emptyCachedPredecessors();

  results = GraphAPI.putEdge(this._graph._properties._key, this._properties._key, update);

  this._properties = results.edge;

  return name;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vertex
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound and outbound edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.edges = function (direction, labels) {
  var edge,
    edges = new GraphArray(),
    cursor;

  cursor = GraphAPI.postEdges(this._graph._vertices._database, this._graph._properties._key, this, {
    filter : { direction : direction, labels: labels }
  });

  while (cursor.hasNext()) {
    edge = new Edge(this._graph, cursor.next());
    edges.push(edge);
  }

  return edges;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges with given label
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getInEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  return this.edges("in", labels);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges with given label
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getOutEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  return this.edges("out", labels);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief in- or outbound edges with given label
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getEdges = function () {
  var labels = Array.prototype.slice.call(arguments);
  return this.edges("any", labels);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inbound edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inbound = function () {
  return this.getInEdges();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief outbound edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outbound = function () {
  return this.getOutEdges();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief changes a property of a vertex
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.setProperty = function (name, value) {
  var results,
    update = this._properties;

  update[name] = value;

  results = GraphAPI.putVertex(this._graph._properties._key, this._properties._key, update);

  this._properties = results.vertex;

  return name;
};

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new graph object
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.initialize = function (name, vertices, edges) {
  var results;

  if (is.notExisty(vertices) && is.notExisty(edges)) {
    results = GraphAPI.getGraph(name);
  } else {
    if (typeof vertices === 'object' && typeof vertices.name === 'function') {
      vertices = vertices.name();
    }
    if (typeof edges === 'object' && typeof edges.name === 'function') {
      edges = edges.name();
    }

    results = GraphAPI.postGraph({
      _key: name,
      vertices: vertices,
      edges: edges
    });
  }

  this._properties = results.graph;
  this._vertices = arangodb.db._collection(this._properties.edgeDefinitions[0].from[0]);
  this._edges = arangodb.db._collection(this._properties.edgeDefinitions[0].collection);

  // and dictionary for vertices and edges
  this._verticesCache = {};
  this._edgesCache = {};

  // and store the cashes
  this.predecessors = {};
  this.distances = {};

  return this;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return all graphs
////////////////////////////////////////////////////////////////////////////////

Graph.getAll = function () {
  return GraphAPI.getAllGraphs();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief static delete method
////////////////////////////////////////////////////////////////////////////////

Graph.drop = function (name) {
  GraphAPI.deleteGraph(name);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the graph, the vertices, and the edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.drop = function () {
  GraphAPI.deleteGraph(this._properties._key);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves an edge to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._saveEdge = function(id, out_vertex_id, in_vertex_id, params) {
  var results;

  this.emptyCachedPredecessors();

  params._key = id;
  params._from = out_vertex_id;
  params._to = in_vertex_id;

  results = GraphAPI.postEdge(this._properties._key, params);
  return new Edge(this, results.edge);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief saves a vertex to the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._saveVertex = function (id, params) {
  var results;

  if (is.existy(id)) {
    params._key = id;
  }

  results = GraphAPI.postVertex(this._properties._key, params);
  return new Vertex(this, results.vertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replace a vertex in the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._replaceVertex = function (id, data) {
  GraphAPI.putVertex(this._properties._key, id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief replace an edge in the graph
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._replaceEdge = function (id, data) {
  GraphAPI.putEdge(this._properties._key, id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a vertex given its id
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertex = function (id) {
  var results = GraphAPI.getVertex(this._properties._key, id);

  if (is.notExisty(results)) {
    return null;
  }

  return new Vertex(this, results.vertex);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all vertices
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getVertices = function () {
  var cursor = GraphAPI.getVertices(this._vertices._database, this._properties._key, {}),
    graph = this,
    wrapper = function(object) {
      return new Vertex(graph, object);
    };

  return new Iterator(wrapper, cursor, "[vertex iterator]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an edge given its id
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdge = function (id) {
  var results = GraphAPI.getEdge(this._properties._key, id);

  if (is.notExisty(results)) {
    return null;
  }

  return new Edge(this, results.edge);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns an iterator for all edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getEdges = function () {
  var cursor = GraphAPI.getEdges(this._vertices._database, this._properties._key, {}),
    graph = this,
    wrapper = function(object) {
      return new Edge(graph, object);
    };
  return new Iterator(wrapper, cursor, "[edge iterator]");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a vertex and all in- or out-bound edges
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeVertex = function (vertex) {
  this.emptyCachedPredecessors();
  GraphAPI.deleteVertex(this._properties._key, vertex._properties._key);
  vertex._properties = undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.removeEdge = function (edge) {
  this.emptyCachedPredecessors();
  GraphAPI.deleteEdge(this._properties._key, edge._properties._key);
  this._edgesCache[edge._properties._id] = undefined;
  edge._properties = undefined;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;
exports.GraphArray = GraphArray;

require("org/arangodb/graph/algorithms-common");

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
});

module.define("org/arangodb/graph-common", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var is = require("org/arangodb/is"),
  Edge,
  Graph,
  Vertex,
  GraphArray,
  Iterator;

// -----------------------------------------------------------------------------
// --SECTION--                             module "org/arangodb/graph-blueprint"
// -----------------------------------------------------------------------------

Iterator = function (wrapper, cursor, stringRepresentation) {
  this.next = function next() {
    if (cursor.hasNext()) {
      return wrapper(cursor.next());
    }

    return undefined;
  };

  this.hasNext = function hasNext() {
    return cursor.hasNext();
  };

  this._PRINT = function (context) {
    context.output += stringRepresentation;
  };
};


// -----------------------------------------------------------------------------
// --SECTION--                                                        GraphArray
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a graph arrays
////////////////////////////////////////////////////////////////////////////////

GraphArray = function (len) {
  if (len !== undefined) {
    this.length = len;
  }
};

GraphArray.prototype = new Array(0);

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief map
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.map = function (fun, thisp) {
  var len = this.length;
  var i;

  if (typeof fun !== "function") {
    throw new TypeError();
  }

  var res = new GraphArray(len);

  for (i = 0;  i < len;  i++) {
    if (this.hasOwnProperty(i)) {
      res[i] = fun.call(thisp, this[i], i, this);
    }
  }

  return res;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the in vertices
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getInVertex = function () {
  return this.map(function(a) {return a.getInVertex();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the out vertices
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getOutVertex = function () {
  return this.map(function(a) {return a.getOutVertex();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the peer vertices
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getPeerVertex = function (vertex) {
  return this.map(function(a) {return a.getPeerVertex(vertex);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the property
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.setProperty = function (name, value) {
  return this.map(function(a) {return a.setProperty(name, value);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.edges = function () {
  return this.map(function(a) {return a.edges();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get outbound edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.outbound = function () {
  return this.map(function(a) {return a.outbound();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get inbound edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.inbound = function () {
  return this.map(function(a) {return a.inbound();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the in edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getInEdges = function () {
  var args = arguments;
  return this.map(function(a) {return a.getInEdges.apply(a, args);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the out edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getOutEdges = function () {
  var args = arguments;
  return this.map(function(a) {return a.getOutEdges.apply(a, args);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief get the edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.getEdges = function () {
  var args = arguments;
  return this.map(function(a) {return a.getEdges.apply(a, args);});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.degree = function () {
  return this.map(function(a) {return a.degree();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of in-edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.inDegree = function () {
  return this.map(function(a) {return a.inDegree();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of out-edges
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.inDegree = function () {
  return this.map(function(a) {return a.outDegree();});
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the properties
////////////////////////////////////////////////////////////////////////////////

GraphArray.prototype.properties = function () {
  return this.map(function(a) {return a.properties();});
};

// -----------------------------------------------------------------------------
// --SECTION--                                                              Edge
// -----------------------------------------------------------------------------

Edge = function (graph, properties) {
  this._graph = graph;
  this._id = properties._key;
  this._properties = properties;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getId = function () {
  return this._properties._key;
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getLabel = function () {
  return this._properties.$label;
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.properties = function () {
  return this._properties._shallowCopy;
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getInVertex = function () {
  return this._graph.getVertex(this._properties._to);
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/EdgeMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getOutVertex = function () {
  return this._graph.getVertex(this._properties._from);
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @start Docu Block edgeGetPeerVertex
///
/// `edge*.getPeerVertex(vertex)`
///
/// Returns the peer vertex of the *edge* and the *vertex*.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{edgeGetPeerVertex}
/// ~ db._drop("v");
/// ~ db._drop("e");
///   Graph = require("org/arangodb/graph-blueprint").Graph;
///   g = new Graph("example", "v", "e");
///   v1 = g.addVertex("1");
///   v2 = g.addVertex("2");
///   e = g.addEdge(v1, v2, "1-to-2", "knows");
///   e.getPeerVertex(v1);
/// ~ Graph.drop("example");
/// ~ db._drop("v");
/// ~ db._drop("e");
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Edge.prototype.getPeerVertex = function (vertex) {
  if (vertex._properties._id === this._properties._to) {
    return this._graph.getVertex(this._properties._from);
  }

  if (vertex._properties._id === this._properties._from) {
    return this._graph.getVertex(this._properties._to);
  }

  return null;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief edge printing
////////////////////////////////////////////////////////////////////////////////

Edge.prototype._PRINT = function (context) {
  if (!this._properties._id) {
    context.output += "[deleted Edge]";
  }
  else if (this._properties._key !== undefined) {
    if (typeof this._properties._key === "string") {
      context.output += "Edge(\"" + this._properties._key + "\")";
    }
    else {
      context.output += "Edge(" + this._properties._key + ")";
    }
  }
  else {
    context.output += "Edge(<" + this._id + ">)";
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                            Vertex
// -----------------------------------------------------------------------------

Vertex = function (graph, properties) {
  this._graph = graph;
  this._id = properties._key;
  this._properties = properties;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/VertexMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addInEdge = function (out, id, label, data) {
  return this._graph.addEdge(out, this, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/VertexMethods.mdpp
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.addOutEdge = function (ine, id, label, data) {
  return this._graph.addEdge(this, ine, id, label, data);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.degree = function () {
  return this.getEdges().length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of in-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.inDegree = function () {
  return this.getInEdges().length;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of out-edges
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.outDegree = function () {
  return this.getOutEdges().length;
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @start Docu Block peerGetId
///
/// `peer.getId()`
///
/// Returns the identifier of the *peer*. If the vertex was deleted, then
/// *undefined* is returned.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-id
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getId = function () {
  return this._properties._key;
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @start Docu Block peerGetProperty
///
/// `peer.getProperty(edge)`
///
/// Returns the property *edge* a *peer*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getProperty = function (name) {
  return this._properties[name];
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @start Docu Block peerGetPropertyKeys
///
/// `peer.getPropertyKeys()`
///
/// Returns all propety names a *peer*.
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-get-property-keys
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.getPropertyKeys = function () {
  return this._properties.propertyKeys;
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @start Docu Block peerproperties
///
/// `peer.properties()`
///
/// Returns all properties and their values of a *peer*
///
/// @EXAMPLES
///
/// @verbinclude graph-vertex-properties
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype.properties = function () {
  return this._properties._shallowCopy;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex representation
////////////////////////////////////////////////////////////////////////////////

Vertex.prototype._PRINT = function (context) {
  if (! this._properties._id) {
    context.output += "[deleted Vertex]";
  }
  else if (this._properties._key !== undefined) {
    if (typeof this._properties._key === "string") {
      context.output += "Vertex(\"" + this._properties._key + "\")";
    }
    else {
      context.output += "Vertex(" + this._properties._key + ")";
    }
  }
  else {
    context.output += "Vertex(<" + this._id + ">)";
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                             Graph
// -----------------------------------------------------------------------------

Graph = function (name, vertices, edges, waitForSync) {
  this.initialize(name, vertices, edges, waitForSync);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

Graph.prototype._prepareEdgeData = function (data, label) {
  var edgeData;

  if (is.notExisty(data) && is.object(label)) {
    data = label;
    label = null;
  }

  if (is.notExisty(label) && is.existy(data) && is.existy(data.$label)) {
    label = data.$label;
  }

  if (is.notExisty(data) || is.noObject(data)) {
    edgeData = {};
  } else {
    edgeData = data._shallowCopy || {};
  }

  edgeData.$label = label;

  return edgeData;
};

Graph.prototype._prepareVertexData = function (data) {
  var vertexData;

  if (is.notExisty(data) || is.noObject(data)) {
    vertexData = {};
  } else {
    vertexData = data._shallowCopy || {};
  }

  return vertexData;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief get a vertex from the graph, create it if it doesn't exist
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getOrAddVertex = function (id) {
  var v = this.getVertex(id);

  if (v === null) {
    v = this.addVertex(id);
  }

  return v;
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/GraphConstructor.mdpp
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addEdge = function (out_vertex, in_vertex, id, label, data, waitForSync) {
  var out_vertex_id, in_vertex_id;

  if (is.string(out_vertex)) {
    out_vertex_id = out_vertex;
  } else {
    out_vertex_id = out_vertex._properties._id;
  }

  if (is.string(in_vertex)) {
    in_vertex_id = in_vertex;
  } else {
    in_vertex_id = in_vertex._properties._id;
  }

  return this._saveEdge(id,
                        out_vertex_id,
                        in_vertex_id,
                        this._prepareEdgeData(data, label),
                        waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// the real docu is in Users/ModuleGraph/GraphConstructor.mdpp
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.addVertex = function (id, data, waitForSync) {
  return this._saveVertex(id, this._prepareVertexData(data), waitForSync);
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @brief replaces an existing vertex by ID
///
/// @FUN{@FA{graph}.replaceVertex(*peer*, *peer*)}
///
/// Replaces an existing vertex by ID
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.replaceVertex = function (id, data) {
  this._replaceVertex(id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @start DocuBlock graphReplaceEdge
///
/// `graph.replaceEdge(peer, peer)`
///
/// Replaces an existing edge by ID
/// @end Docu Block
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.replaceEdge = function (id, data) {
  this._replaceEdge(id, data);
};

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @brief returns the number of vertices
///
/// @FUN{@FA{graph}.order()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.order = function () {
  return this._vertices.count();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of edges
///
/// @FUN{@FA{graph}.size()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.size = function () {
  return this._edges.count();
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// this is no documentation.
/// @brief empties the internal cache for Predecessors
///
/// @FUN{@FA{graph}.emptyCachedPredecessors()}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.emptyCachedPredecessors = function () {
  this.predecessors = {};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief gets Predecessors for a pair from the internal cache
///
/// @FUN{@FA{graph}.getCachedPredecessors(@FA{target}), @FA{source})}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.getCachedPredecessors = function (target, source) {
  var predecessors;

  if (this.predecessors[target.getId()]) {
    predecessors = this.predecessors[target.getId()][source.getId()];
  }

  return predecessors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets Predecessors for a pair in the internal cache
///
/// @FUN{@FA{graph}.setCachedPredecessors(@FA{target}), @FA{source}, @FA{value})}
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.setCachedPredecessors = function (target, source, value) {
  if (!this.predecessors[target.getId()]) {
    this.predecessors[target.getId()] = {};
  }

  this.predecessors[target.getId()][source.getId()] = value;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a vertex
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructVertex = function (data) {
  var id, rev;

  if (typeof data === "string") {
    id = data;
  } else {
    id = data._id;
    rev = data._rev;
  }

  var vertex = this._verticesCache[id];

  if (vertex === undefined || vertex._rev !== rev) {
    var properties = this._vertices.document(id);

    if (! properties) {
      throw "accessing a deleted vertex";
    }

    this._verticesCache[id] = vertex = new Vertex(this, properties);
  }

  return vertex;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs an edge
////////////////////////////////////////////////////////////////////////////////

Graph.prototype.constructEdge = function (data) {
  var id, rev, edge, properties;

  if (typeof data === "string") {
    id = data;
  } else {
    id = data._id;
    rev = data._rev;
  }

  edge = this._edgesCache[id];

  if (edge === undefined || edge._rev !== rev) {
    properties = this._edges.document(id);

    if (!properties) {
      throw "accessing a deleted edge";
    }

    this._edgesCache[id] = edge = new Edge(this, properties);
  }

  return edge;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief graph printing
////////////////////////////////////////////////////////////////////////////////

Graph.prototype._PRINT = function (context) {
  context.output += "Graph(\"" + this._properties._key + "\")";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.Edge = Edge;
exports.Graph = Graph;
exports.Vertex = Vertex;
exports.GraphArray = GraphArray;
exports.Iterator = Iterator;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
});

module.define("org/arangodb/graph", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Graph functionality
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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
/// @author Florian Bartels
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


var gp = require("org/arangodb/graph-blueprint");

// The warning will be activated soon.
/*
 * require("console").warn('module "graph" is deprecated, please use ' +
 *                      'module "general-graph" instead');
 */

Object.keys(gp).forEach(function (m) {
  exports[m] = gp[m];
});



// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
});

module.define("org/arangodb/graph/traversal", function(exports, module) {
/*jshint strict: false, unused: false */
/*global ArangoClusterComm, AQL_QUERY_IS_KILLED */

////////////////////////////////////////////////////////////////////////////////
/// @brief Traversal "classes"
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2013 triagens GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var graph = require("org/arangodb/graph-blueprint");
var generalGraph = require("org/arangodb/general-graph");
var arangodb = require("org/arangodb");
var BinaryHeap = require("org/arangodb/heap").BinaryHeap;
var ArangoError = arangodb.ArangoError;
var ShapedJson = require("internal").ShapedJson; // this may be undefined/null on the client

var db = arangodb.db;

var ArangoTraverser;

// -----------------------------------------------------------------------------
// --SECTION--                                                  helper functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the query was aborted
/// use the AQL_QUERY_IS_KILLED function on the server side, and a dummy 
/// function otherwise (ArangoShell etc.)
////////////////////////////////////////////////////////////////////////////////

var throwIfAborted = function () {
};

try {
  if (typeof AQL_QUERY_IS_KILLED === "function") {
    throwIfAborted = function () { 
      if (AQL_QUERY_IS_KILLED()) {
        var err = new ArangoError();
        err.errorNum = arangodb.errors.ERROR_QUERY_KILLED.code;
        err.errorMessage = arangodb.errors.ERROR_QUERY_KILLED.message;
        throw err;
      }
    };
  }
}
catch (err) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone any object
////////////////////////////////////////////////////////////////////////////////

function clone (obj) {
  if (obj === null || typeof obj !== "object") {
    return obj;
  }

  var copy;
  if (Array.isArray(obj)) {
    copy = [ ];
    obj.forEach(function (i) {
      copy.push(clone(i));
    });
  }
  else if (obj instanceof Object) {
    if (ShapedJson && obj instanceof ShapedJson) {
      return obj;
    }
    copy = { };
    Object.keys(obj).forEach(function(k) {
      copy[k] = clone(obj[k]);
    });
  }

  return copy;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test if object is empty
////////////////////////////////////////////////////////////////////////////////

function isEmpty(obj) {
  for(var key in obj) {
    if (obj.hasOwnProperty(key)) {
      return false;
    }
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief traversal abortion exception
////////////////////////////////////////////////////////////////////////////////

var abortedException = function (message, options) {
  'use strict';
  this.message = message || "traversal intentionally aborted by user";
  this.options = options || { };
  this._intentionallyAborted = true;
};

abortedException.prototype = new Error();

// -----------------------------------------------------------------------------
// --SECTION--                                                       datasources
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief default ArangoCollection datasource
///
/// This is a factory function that creates a datasource that operates on the
/// specified edge collection. The vertices and edges are the documents in the
/// corresponding collections.
////////////////////////////////////////////////////////////////////////////////

function collectionDatasourceFactory (edgeCollection) {
  var c = edgeCollection;
  if (typeof c === 'string') {
    c = db._collection(c);
  }

  // we can call the "fast" version of some edge functions if we are
  // running server-side and are not a coordinator
  var useBuiltIn = (typeof ArangoClusterComm === "object");
  if (useBuiltIn && require("org/arangodb/cluster").isCoordinator()) {
    useBuiltIn = false;
  }

  return {
    edgeCollection: c,
    useBuiltIn: useBuiltIn,

    getVertexId: function (vertex) {
      return vertex._id;
    },

    getPeerVertex: function (edge, vertex) {
      if (edge._from === vertex._id) {
        return db._document(edge._to);
      }

      if (edge._to === vertex._id) {
        return db._document(edge._from);
      }

      return null;
    },

    getInVertex: function (edge) {
      return db._document(edge._to);
    },

    getOutVertex: function (edge) {
      return db._document(edge._from);
    },

    getEdgeId: function (edge) {
      return edge._id;
    },

    getEdgeFrom: function (edge) {
      return edge._from;
    },

    getEdgeTo: function (edge) {
      return edge._to;
    },

    getLabel: function (edge) {
      return edge.$label;
    },

    getAllEdges: function (vertex) {
      if (this.useBuiltIn) {
        return this.edgeCollection.EDGES(vertex._id);
      }
      return this.edgeCollection.edges(vertex._id);
    },

    getInEdges: function (vertex) {
      if (this.useBuiltIn) {
        return this.edgeCollection.INEDGES(vertex._id);
      }
      return this.edgeCollection.inEdges(vertex._id);
    },

    getOutEdges: function (vertex) {
      if (this.useBuiltIn) {
        return this.edgeCollection.OUTEDGES(vertex._id);
      }
      return this.edgeCollection.outEdges(vertex._id);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief general graph datasource
///
/// This is a factory function that creates a datasource that operates on the
/// specified general graph. The vertices and edges are delivered by the
/// the general-graph module.
////////////////////////////////////////////////////////////////////////////////

function generalGraphDatasourceFactory (graph) {
  var g = graph;
  if (typeof g === 'string') {
   g = generalGraph._graph(g);
  }

  return {
    graph: g,

    getVertexId: function (vertex) {
      return vertex._id;
    },

    getPeerVertex: function (edge, vertex) {
      if (edge._from === vertex._id) {
        return db._document(edge._to);
      }

      if (edge._to === vertex._id) {
        return db._document(edge._from);
      }

      return null;
    },

    getInVertex: function (edge) {
      return db._document(edge._to);
    },

    getOutVertex: function (edge) {
      return db._document(edge._from);
    },

    getEdgeId: function (edge) {
      return edge._id;
    },

    getEdgeFrom: function (edge) {
      return edge._from;
    },

    getEdgeTo: function (edge) {
      return edge._to;
    },

    getLabel: function (edge) {
      return edge.$label;
    },

    getAllEdges: function (vertex) {
      return this.graph._EDGES(vertex._id);
    },

    getInEdges: function (vertex) {
      return this.graph._INEDGES(vertex._id);
    },

    getOutEdges: function (vertex) {
      return this.graph._OUTEDGES(vertex._id);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default Graph datasource
///
/// This is a datasource that operates on the specified graph. The vertices
/// are from type Vertex, the edges from type Edge.
////////////////////////////////////////////////////////////////////////////////

function graphDatasourceFactory (name) {
  return {
    graph: new graph.Graph(name),

    getVertexId: function (vertex) {
      return vertex.getId();
    },

    getPeerVertex: function (edge, vertex) {
      return edge.getPeerVertex(vertex);
    },

    getInVertex: function (edge) {
      return edge.getInVertex();
    },

    getOutVertex: function (edge) {
      return edge.getOutVertex();
    },

    getEdgeId: function (edge) {
      return edge.getId();
    },

    getEdgeFrom: function (edge) {
      return edge._properties._from;
    },

    getEdgeTo: function (edge) {
      return edge._properties._to;
    },

    getLabel: function (edge) {
      return edge.getLabel();
    },

    getAllEdges: function (vertex) {
      return vertex.edges();
    },

    getInEdges: function (vertex) {
      return vertex.inbound();
    },

    getOutEdges: function (vertex) {
      return vertex.outbound();
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                         expanders
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief default outbound expander function
////////////////////////////////////////////////////////////////////////////////

function outboundExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [ ];
  var outEdges = datasource.getOutEdges(vertex);
  var edgeIterator;

  if (outEdges.length > 1 && config.sort) {
    outEdges.sort(config.sort);
  }

  if (config.buildVertices) {
    if (!config.expandFilter) {
      edgeIterator = function(edge) {
        try {
          var v = datasource.getInVertex(edge);
          connections.push({ edge: edge, vertex: v });
        }
        catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    } else {
      edgeIterator = function(edge) {
        try {
          var v = datasource.getInVertex(edge);
          if (config.expandFilter(config, v, edge, path)) {
            connections.push({ edge: edge, vertex: v });
          }
        }
        catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    }
  } else {
    if (!config.expandFilter) {
      edgeIterator = function(edge) {
        var id = datasource.getEdgeTo(edge);
        var v = { _id: id, _key: id.substr(id.indexOf("/") + 1)};
        connections.push({ edge: edge, vertex: v });
      };
    } else {
      edgeIterator = function(edge) {
        var id = datasource.getEdgeTo(edge);
        var v = { _id: id, _key: id.substr(id.indexOf("/") + 1)};
        if (config.expandFilter(config, v, edge, path)) {
          connections.push({ edge: edge, vertex: v });
        }
      };
    }
  }
  outEdges.forEach(edgeIterator);
  return connections;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default inbound expander function
////////////////////////////////////////////////////////////////////////////////

function inboundExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [ ];

  var inEdges = datasource.getInEdges(vertex);

  if (inEdges.length > 1 && config.sort) {
    inEdges.sort(config.sort);
  }
  var edgeIterator;

  if (config.buildVertices) {
    if (!config.expandFilter) {
      edgeIterator = function(edge) {
        try {
          var v = datasource.getOutVertex(edge);
          connections.push({ edge: edge, vertex: v });
        }
        catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    } else {
      edgeIterator = function(edge) {
        try {
          var v = datasource.getOutVertex(edge);
          if (config.expandFilter(config, v, edge, path)) {
            connections.push({ edge: edge, vertex: v });
          }
        }
        catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    }
  } else {
    if (!config.expandFilter) {
      edgeIterator = function(edge) {
        var id = datasource.getEdgeFrom(edge);
        var v = { _id: id, _key: id.substr(id.indexOf("/") + 1)};
        connections.push({ edge: edge, vertex: v });
      };
    } else {
      edgeIterator = function(edge) {
        var id = datasource.getEdgeFrom(edge);
        var v = { _id: id, _key: id.substr(id.indexOf("/") + 1)};
        if (config.expandFilter(config, v, edge, path)) {
          connections.push({ edge: edge, vertex: v });
        }
      };
    }
  }
  inEdges.forEach(edgeIterator);

  return connections;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief default "any" expander function
////////////////////////////////////////////////////////////////////////////////

function anyExpander (config, vertex, path) {
  var datasource = config.datasource;
  var connections = [ ];
  var edges = datasource.getAllEdges(vertex);

  if (edges.length > 1 && config.sort) {
    edges.sort(config.sort);
  }

  var edgeIterator;
  if (config.buildVertices) {
    if (!config.expandFilter) {
      edgeIterator = function(edge) {
        try {
          var v = datasource.getPeerVertex(edge, vertex);
          connections.push({ edge: edge, vertex: v });
        }
        catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    } else {
      edgeIterator = function(edge) {
        try {
          var v = datasource.getPeerVertex(edge, vertex);
          if (config.expandFilter(config, v, edge, path)) {
            connections.push({ edge: edge, vertex: v });
          }
        }
        catch (e) {
          // continue even in the face of non-existing documents
        }
      };
    }
  } else {
    if (!config.expandFilter) {
      edgeIterator = function(edge) {
        var id = datasource.getEdgeFrom(edge);
        if (id === vertex._id) {
          id = datasource.getEdgeTo(edge);
        }
        var v = { _id: id, _key: id.substr(id.indexOf("/") + 1)};
        connections.push({ edge: edge, vertex: v });
      };
    } else {
      edgeIterator = function(edge) {
        var id = datasource.getEdgeFrom(edge);
        if (id === vertex._id) {
          id = datasource.getEdgeTo(edge);
        }
        var v = { _id: id, _key: id.substr(id.indexOf("/") + 1)};
        if (config.expandFilter(config, v, edge, path)) {
          connections.push({ edge: edge, vertex: v });
        }
      };
    }
  }
  edges.forEach(edgeIterator);
  return connections;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief expands all outbound edges labeled with at least one label in config.labels
///////////////////////////////////////////////////////////////////////////////////////////

function expandOutEdgesWithLabels (config, vertex, path) {
  var datasource = config.datasource;
  var result = [ ];
  var i;

  if (! Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }

  var edgesList = datasource.getOutEdges(vertex);

  if (edgesList !== undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      var edge = edgesList[i];
      var label = datasource.getLabel(edge);

      if (config.labels.indexOf(label) >= 0) {
        result.push({ edge: edge, vertex: datasource.getInVertex(edge) });
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief expands all inbound edges labeled with at least one label in config.labels
///////////////////////////////////////////////////////////////////////////////////////////

function expandInEdgesWithLabels (config, vertex, path) {
  var datasource = config.datasource;
  var result = [ ];
  var i;

  if (! Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }

  var edgesList = config.datasource.getInEdges(vertex);

  if (edgesList !== undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      var edge = edgesList[i];
      var label = datasource.getLabel(edge);

      if (config.labels.indexOf(label) >= 0) {
        result.push({ edge: edge, vertex: datasource.getOutVertex(edge) });
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////////////////
/// @brief expands all edges labeled with at least one label in config.labels
///////////////////////////////////////////////////////////////////////////////////////////

function expandEdgesWithLabels (config, vertex, path) {
  var datasource = config.datasource;
  var result = [ ];
  var i;

  if (! Array.isArray(config.labels)) {
    config.labels = [config.labels];
  }

  var edgesList = config.datasource.getAllEdges(vertex);

  if (edgesList !== undefined) {
    for (i = 0; i < edgesList.length; ++i) {
      var edge = edgesList[i];
      var label = datasource.getLabel(edge);

      if (config.labels.indexOf(label) >= 0) {
        result.push({ edge: edge, vertex: datasource.getPeerVertex(edge, vertex) });
      }
    }
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                          visitors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief default visitor that just tracks every visit
////////////////////////////////////////////////////////////////////////////////

function trackingVisitor (config, result, vertex, path) {
  if (! result || ! result.visited) {
    return;
  }

  if (result.visited.vertices) {
    result.visited.vertices.push(clone(vertex));
  }

  if (result.visited.paths) {
    result.visited.paths.push(clone(path));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a visitor that counts the number of nodes visited
////////////////////////////////////////////////////////////////////////////////

function countingVisitor (config, result, vertex, path) {
  if (! result) {
    return;
  }

  if (result.hasOwnProperty('count')) {
    ++result.count;
  }
  else {
    result.count = 1;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief a visitor that does nothing - can be used to quickly traverse a
/// graph, e.g. for performance comparisons etc.
////////////////////////////////////////////////////////////////////////////////

function doNothingVisitor () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           filters
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief default filter to visit & expand all vertices
////////////////////////////////////////////////////////////////////////////////

function visitAllFilter () {
  return "";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief filter to visit & expand all vertices up to a given depth
////////////////////////////////////////////////////////////////////////////////

function maxDepthFilter (config, vertex, path) {
  if (path && path.vertices && path.vertices.length > config.maxDepth) {
    return ArangoTraverser.PRUNE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude all vertices up to a given depth
////////////////////////////////////////////////////////////////////////////////

function minDepthFilter (config, vertex, path) {
  if (path && path.vertices && path.vertices.length <= config.minDepth) {
    return ArangoTraverser.EXCLUDE;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief include all vertices matching one of the given attribute sets
////////////////////////////////////////////////////////////////////////////////

function includeMatchingAttributesFilter (config, vertex, path) {
  if (! Array.isArray(config.matchingAttributes)) {
    config.matchingAttributes = [config.matchingAttributes];
  }

  var include = false;

  config.matchingAttributes.forEach(function(example) {
    var count = 0;
    var keys = Object.keys(example);

    keys.forEach(function (key) {
      if (vertex[key] && vertex[key] === example[key]) {
        count++;
      }
    });

    if (count > 0 && count === keys.length) {
      include = true;
    }
  });

  var result;

  if (! include) {
    result = "exclude";
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief combine an array of filters
////////////////////////////////////////////////////////////////////////////////

function combineFilters (filters, config, vertex, path) {
  var result = [ ];

  filters.forEach(function (f) {
    var tmp = f(config, vertex, path);

    if (! Array.isArray(tmp)) {
      tmp = [ tmp ];
    }

    result = result.concat(tmp);
  });

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse a filter result
////////////////////////////////////////////////////////////////////////////////

function parseFilterResult (args) {
  var result = {
    visit: true,
    expand: true
  };

  function processArgument (arg) {
    if (arg === undefined || arg === null) {
      return;
    }

    var finish = false;

    if (typeof(arg) === 'string') {
      if (arg === ArangoTraverser.EXCLUDE) {
        result.visit = false;
        finish = true;
      }
      else if (arg === ArangoTraverser.PRUNE) {
        result.expand = false;
        finish = true;
      }
      else if (arg === '') {
        finish = true;
      }
    }
    else if (Array.isArray(arg)) {
      var i;
      for (i = 0; i < arg.length; ++i) {
        processArgument(arg[i]);
      }
      finish = true;
    }

    if (finish) {
      return;
    }

    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_GRAPH_INVALID_FILTER_RESULT.code;
    err.errorMessage = arangodb.errors.ERROR_GRAPH_INVALID_FILTER_RESULT.message;
    throw err;
  }

  processArgument(args);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief apply the uniqueness checks
////////////////////////////////////////////////////////////////////////////////

function checkUniqueness (config, visited, vertex, edge) {
  var uniqueness = config.uniqueness;
  var datasource = config.datasource;
  var id;

  if (uniqueness.vertices !== ArangoTraverser.UNIQUE_NONE) {
    id = datasource.getVertexId(vertex);

    if (visited.vertices[id] === true) {
      return false;
    }

    visited.vertices[id] = true;
  }

  if (edge !== null && uniqueness.edges !== ArangoTraverser.UNIQUE_NONE) {
    id = datasource.getEdgeId(edge);

    if (visited.edges[id] === true) {
      return false;
    }

    visited.edges[id] = true;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if we must process items in reverse order
////////////////////////////////////////////////////////////////////////////////

function checkReverse (config) {
  var result = false;

  if (config.order === ArangoTraverser.POST_ORDER) {
    // post order
    if (config.itemOrder === ArangoTraverser.FORWARD) {
      result = true;
    }
  }
  else if (config.order === ArangoTraverser.PRE_ORDER ||
           config.order === ArangoTraverser.PRE_ORDER_EXPANDER) {
    // pre order
    if (config.itemOrder === ArangoTraverser.BACKWARD &&
        config.strategy === ArangoTraverser.BREADTH_FIRST) {
      result = true;
    }
    else if (config.itemOrder === ArangoTraverser.FORWARD &&
             config.strategy === ArangoTraverser.DEPTH_FIRST) {
      result = true;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for breadth-first strategy
////////////////////////////////////////////////////////////////////////////////

function breadthFirstSearch () {
  return {
    requiresEndVertex: function () {
      return false;
    },

    getPathItems: function (id, items) {
      var visited = { };
      var ignore = items.length - 1;

      items.forEach(function (item, i) {
        if (i !== ignore) {
          visited[id(item)] = true;
        }
      });

      return visited;
    },

    createPath: function (items, idx) {
      var path = { edges: [ ], vertices: [ ] };
      var pathItem = items[idx];

      while (true) {
        if (pathItem.edge !== null) {
          path.edges.unshift(pathItem.edge);
        }
        path.vertices.unshift(pathItem.vertex);
        idx = pathItem.parentIndex;
        if (idx < 0) {
          break;
        }
        pathItem = items[idx];
      }

      return path;
    },

    run: function (config, result, startVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;
      var toVisit = [ { edge: null, vertex: startVertex, parentIndex: -1 } ];
      var visited = { edges: { }, vertices: { } };

      var index = 0;
      var step = 1;
      var reverse = checkReverse(config);

      while ((step === 1 && index < toVisit.length) ||
             (step === -1 && index >= 0)) {
        var current = toVisit[index];
        var vertex  = current.vertex;
        var edge    = current.edge;
        var path;

        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        throwIfAborted();

        if (current.visit === null || current.visit === undefined) {
          current.visit = false;

          path = this.createPath(toVisit, index);

          // first apply uniqueness check
          if (config.uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
            visited.vertices = this.getPathItems(config.datasource.getVertexId, path.vertices);
          }
          if (config.uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
            visited.edges = this.getPathItems(config.datasource.getEdgeId, path.edges);
          }

          if (! checkUniqueness(config, visited, vertex, edge)) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
            continue;
          }

          var filterResult = parseFilterResult(config.filter(config, vertex, path));
          if (config.order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder
            config.visitor(config, result, vertex, path);
          }
          else {
            // postorder
            current.visit = filterResult.visit || false;
          }

          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path), i;

            if (reverse) {
              connected.reverse();
            }

            if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) { 
              config.visitor(config, result, vertex, path, connected);
            }

            for (i = 0; i < connected.length; ++i) {
              connected[i].parentIndex = index;
              toVisit.push(connected[i]);
            }
          }
          else if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) {
            config.visitor(config, result, vertex, path, [ ]);
          } 

          if (config.order === ArangoTraverser.POST_ORDER) {
            if (index < toVisit.length - 1) {
              index += step;
            }
            else {
              step = -1;
            }
          }
        }
        else {
          if (config.order === ArangoTraverser.POST_ORDER && current.visit) {
            path = this.createPath(toVisit, index);
            config.visitor(config, result, vertex, path);
          }
          index += step;
        }
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for depth-first strategy
////////////////////////////////////////////////////////////////////////////////

function depthFirstSearch () {
  return {
    requiresEndVertex: function () {
      return false;
    },

    getPathItems: function (id, items) {
      var visited = { };
      items.forEach(function (item) {
        visited[id(item)] = true;
      });
      return visited;
    },

    run: function (config, result, startVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;
      var toVisit = [ { edge: null, vertex: startVertex, visit: null } ];
      var path    = { edges: [ ], vertices: [ ] };
      var visited = { edges: { }, vertices: { } };
      var reverse = checkReverse(config);
      var uniqueness = config.uniqueness;
      var haveUniqueness = ((uniqueness.vertices !== ArangoTraverser.UNIQUE_NONE) ||
                            (uniqueness.edges !== ArangoTraverser.UNIQUE_NONE));

      while (toVisit.length > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }
        
        throwIfAborted();

        // peek at the top of the stack
        var current = toVisit[toVisit.length - 1];
        var vertex  = current.vertex;
        var edge    = current.edge;

        // check if we visit the element for the first time
        if (current.visit === null || current.visit === undefined) {
          current.visit = false;

          if (haveUniqueness) {
            // first apply uniqueness check
            if (uniqueness.vertices === ArangoTraverser.UNIQUE_PATH) {
              visited.vertices = this.getPathItems(config.datasource.getVertexId, path.vertices);
            }

            if (uniqueness.edges === ArangoTraverser.UNIQUE_PATH) {
              visited.edges = this.getPathItems(config.datasource.getEdgeId, path.edges);
            }

            if (! checkUniqueness(config, visited, vertex, edge)) {
              // skip element if not unique
              toVisit.pop();
              continue;
            }
          }

          // push the current element onto the path stack
          if (edge !== null) {
            path.edges.push(edge);
          }

          path.vertices.push(vertex);

          var filterResult = parseFilterResult(config.filter(config, vertex, path));
          if (config.order === ArangoTraverser.PRE_ORDER && filterResult.visit) {
            // preorder visit
            config.visitor(config, result, vertex, path);
          }
          else {
            // postorder. mark the element visitation flag because we'll got to check it later
            current.visit = filterResult.visit || false;
          }

          // expand the element's children?
          if (filterResult.expand) {
            var connected = config.expander(config, vertex, path), i;
              
            if (reverse) {
              connected.reverse();
            }
          
            if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) { 
              config.visitor(config, result, vertex, path, connected);
            }

            for (i = 0; i < connected.length; ++i) {
              connected[i].visit = null;
              toVisit.push(connected[i]);
            }
          }
          else if (config.order === ArangoTraverser.PRE_ORDER_EXPANDER && filterResult.visit) {
            config.visitor(config, result, vertex, path, [ ]);
          } 
        }
        else {
          // we have already seen this element
          if (config.order === ArangoTraverser.POST_ORDER && current.visit) {
            // postorder visitation
            config.visitor(config, result, vertex, path);
          }

          // pop the element from the stack
          toVisit.pop();
          if (path.edges.length > 0) {
            path.edges.pop();
          }
          path.vertices.pop();
        }
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for dijkstra shortest path strategy
////////////////////////////////////////////////////////////////////////////////

function dijkstraSearch () {
  return {
    nodes: { },

    requiresEndVertex: function () {
      return true;
    },

    makeNode: function (vertex) {
      var id = vertex._id;
      if (! this.nodes.hasOwnProperty(id)) {
        this.nodes[id] = { vertex: vertex, dist: Infinity };
      }

      return this.nodes[id];
    },

    vertexList: function (vertex) {
      var result = [ ];
      while (vertex) {
        result.push(vertex);
        vertex = vertex.parent;
      }
      return result;
    },

    buildPath: function (vertex) {
      var path = { vertices: [ vertex.vertex ], edges: [ ] };
      var v = vertex;

      while (v.parent) {
        path.vertices.unshift(v.parent.vertex);
        path.edges.unshift(v.parentEdge);
        v = v.parent;
      }
      return path;
    },

    run: function (config, result, startVertex, endVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;

      var heap = new BinaryHeap(function (node) {
        return node.dist;
      });

      var startNode = this.makeNode(startVertex);
      startNode.dist = 0;
      heap.push(startNode);

      while (heap.size() > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }
        
        throwIfAborted();

        var currentNode = heap.pop();
        var i, n;

        if (currentNode.vertex._id === endVertex._id) {
          var vertices = this.vertexList(currentNode).reverse();

          n = vertices.length;
          for (i = 0; i < n; ++i) {
            if (! vertices[i].hide) {
              config.visitor(config, result, vertices[i].vertex, this.buildPath(vertices[i]));
            }
          }
          return;
        }

        if (currentNode.visited) {
          continue;
        }

        if (currentNode.dist === Infinity) {
          break;
        }

        currentNode.visited = true;

        var path = this.buildPath(currentNode);
        var filterResult = parseFilterResult(config.filter(config, currentNode.vertex, path));

        if (! filterResult.visit) {
          currentNode.hide = true;
        }

        if (! filterResult.expand) {
          continue;
        }

        var dist = currentNode.dist;
        var connected = config.expander(config, currentNode.vertex, path);
        n = connected.length;

        for (i = 0; i < n; ++i) {
          var neighbor = this.makeNode(connected[i].vertex);

          if (neighbor.visited) {
            continue;
          }

          var edge = connected[i].edge;
          var weight = 1;
          if (config.distance) {
            weight = config.distance(config, currentNode.vertex, neighbor.vertex, edge);
          }
          else if (config.weight) {
            if (typeof edge[config.weight] === "number") {
              weight = edge[config.weight];
            }
            else if (config.defaultWeight) {
              weight = config.defaultWeight;
            }
            else {
              weight = Infinity;
            }
          }

          var alt = dist + weight;
          if (alt < neighbor.dist) {
            neighbor.dist = alt;
            neighbor.parent = currentNode;
            neighbor.parentEdge = edge;
            heap.push(neighbor);
          }
        }
      }
    }
  };
}

function dijkstraSearchMulti () {
  return {
    nodes: { },

    requiresEndVertex: function () {
      return true;
    },

    makeNode: function (vertex) {
      var id = vertex._id;
      if (! this.nodes.hasOwnProperty(id)) {
        this.nodes[id] = { vertex: vertex, dist: Infinity };
      }

      return this.nodes[id];
    },

    vertexList: function (vertex) {
      var result = [ ];
      while (vertex) {
        result.push(vertex);
        vertex = vertex.parent;
      }
      return result;
    },

    buildPath: function (vertex) {
      var path = { vertices: [ vertex.vertex ], edges: [ ] };
      var v = vertex;

      while (v.parent) {
        path.vertices.unshift(v.parent.vertex);
        path.edges.unshift(v.parentEdge);
        v = v.parent;
      }
      return path;
    },

    run: function (config, result, startVertex, endVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;

      var heap = new BinaryHeap(function (node) {
        return node.dist;
      });

      var startNode = this.makeNode(startVertex);
      startNode.dist = 0;
      heap.push(startNode);

      while (heap.size() > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        var currentNode = heap.pop();
        var i, n;

        if (endVertex.hasOwnProperty(currentNode.vertex._id)) {
          delete endVertex[currentNode.vertex._id];
          config.visitor(config, result, currentNode, this.buildPath(currentNode));
          if (isEmpty(endVertex)) {
            return;
          }
        }

        if (currentNode.visited) {
          continue;
        }

        if (currentNode.dist === Infinity) {
          break;
        }

        currentNode.visited = true;

        var path = this.buildPath(currentNode);
        var filterResult = parseFilterResult(config.filter(config, currentNode.vertex, path));

        if (! filterResult.visit) {
          currentNode.hide = true;
        }

        if (! filterResult.expand) {
          continue;
        }

        var dist = currentNode.dist;
        var connected = config.expander(config, currentNode.vertex, path);
        n = connected.length;

        for (i = 0; i < n; ++i) {
          var neighbor = this.makeNode(connected[i].vertex);

          if (neighbor.visited) {
            continue;
          }

          var edge = connected[i].edge;
          var weight = 1;
          if (config.distance) {
            weight = config.distance(config, currentNode.vertex, neighbor.vertex, edge);
          }
          else if (config.weight) {
            if (typeof edge[config.weight] === "number") {
              weight = edge[config.weight];
            }
            else if (config.defaultWeight) {
              weight = config.defaultWeight;
            }
            else {
              weight = Infinity;
            }
          }

          var alt = dist + weight;
          if (alt < neighbor.dist) {
            neighbor.dist = alt;
            neighbor.parent = currentNode;
            neighbor.parentEdge = edge;
            heap.push(neighbor);
          }
        }
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief implementation details for a* shortest path strategy
////////////////////////////////////////////////////////////////////////////////

function astarSearch () {
  return {
    nodes: { },

    requiresEndVertex: function () {
      return true;
    },

    makeNode: function (vertex) {
      var id = vertex._id;
      if (! this.nodes.hasOwnProperty(id)) {
        this.nodes[id] = { vertex: vertex, f: 0, g: 0, h: 0 };
      }

      return this.nodes[id];
    },

    vertexList: function (vertex) {
      var result = [ ];
      while (vertex) {
        result.push(vertex);
        vertex = vertex.parent;
      }
      return result;
    },

    buildPath: function (vertex) {
      var path = { vertices: [ vertex.vertex ], edges: [ ] };
      var v = vertex;

      while (v.parent) {
        path.vertices.unshift(v.parent.vertex);
        path.edges.unshift(v.parentEdge);
        v = v.parent;
      }
      return path;
    },

    run: function (config, result, startVertex, endVertex) {
      var maxIterations = config.maxIterations, visitCounter = 0;

      var heap = new BinaryHeap(function (node) {
        return node.f;
      });

      heap.push(this.makeNode(startVertex));


      while (heap.size() > 0) {
        if (visitCounter++ > maxIterations) {
          var err = new ArangoError();
          err.errorNum = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.code;
          err.errorMessage = arangodb.errors.ERROR_GRAPH_TOO_MANY_ITERATIONS.message;
          throw err;
        }

        throwIfAborted();

        var currentNode = heap.pop();
        var i, n;

        if (currentNode.vertex._id === endVertex._id) {
          var vertices = this.vertexList(currentNode);
          if (config.order !== ArangoTraverser.PRE_ORDER) {
            vertices.reverse();
          }

          n = vertices.length;
          for (i = 0; i < n; ++i) {
            config.visitor(config, result, vertices[i].vertex, this.buildPath(vertices[i]));
          }
          return;
        }

        currentNode.closed = true;

        var path = this.buildPath(currentNode);
        var connected = config.expander(config, currentNode.vertex, path);
        n = connected.length;

        for (i = 0; i < n; ++i) {
          var neighbor = this.makeNode(connected[i].vertex);

          if (neighbor.closed) {
            continue;
          }

          var gScore = currentNode.g + 1;// + neighbor.cost;
          var beenVisited = neighbor.visited;

          if (! beenVisited || gScore < neighbor.g) {
            var edge = connected[i].edge;
            neighbor.visited = true;
            neighbor.parent = currentNode;
            neighbor.parentEdge = edge;
            neighbor.h = 1;
            if (config.distance && ! neighbor.h) {
              neighbor.h = config.distance(config, neighbor.vertex, endVertex, edge);
            }
            neighbor.g = gScore;
            neighbor.f = neighbor.g + neighbor.h;

            if (! beenVisited) {
              heap.push(neighbor);
            }
            else {
              heap.rescoreElement(neighbor);
            }
          }
        }
      }
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   ArangoTraverser
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief traversal constructor
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser = function (config) {
  var defaults = {
    order: ArangoTraverser.PRE_ORDER,
    itemOrder: ArangoTraverser.FORWARD,
    strategy: ArangoTraverser.DEPTH_FIRST,
    uniqueness: {
      vertices: ArangoTraverser.UNIQUE_NONE,
      edges: ArangoTraverser.UNIQUE_PATH
    },
    visitor: trackingVisitor,
    filter: null,
    expander: outboundExpander,
    datasource: null,
    maxIterations: 10000000,
    minDepth: 0,
    maxDepth: 256,
    buildVertices: true
  }, d;

  var err;

  if (typeof config !== "object") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message;
    throw err;
  }

  // apply defaults
  for (d in defaults) {
    if (defaults.hasOwnProperty(d)) {
      if (! config.hasOwnProperty(d) || config[d] === undefined) {
        config[d] = defaults[d];
      }
    }
  }

  function validate (value, map, param) {
    var m;

    if (value === null || value === undefined) {
      // use first key from map
      for (m in map) {
        if (map.hasOwnProperty(m)) {
          value = m;
          break;
        }
      }
    }
    if (typeof value === 'string') {
      value = value.toLowerCase().replace(/-/, "");
      if (map[value] !== null && map[value] !== undefined) {
        return map[value];
      }
    }
    for (m in map) {
      if (map.hasOwnProperty(m)) {
        if (map[m] === value) {
          return value;
        }
      }
    }

    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid value for " + param;
    throw err;
  }

  config.uniqueness = {
    vertices: validate(config.uniqueness && config.uniqueness.vertices, {
      none:   ArangoTraverser.UNIQUE_NONE,
      global: ArangoTraverser.UNIQUE_GLOBAL,
      path:   ArangoTraverser.UNIQUE_PATH
    }, "uniqueness.vertices"),
    edges: validate(config.uniqueness && config.uniqueness.edges, {
      path:   ArangoTraverser.UNIQUE_PATH,
      none:   ArangoTraverser.UNIQUE_NONE,
      global: ArangoTraverser.UNIQUE_GLOBAL
    }, "uniqueness.edges")
  };

  config.strategy = validate(config.strategy, {
    depthfirst: ArangoTraverser.DEPTH_FIRST,
    breadthfirst: ArangoTraverser.BREADTH_FIRST,
    astar: ArangoTraverser.ASTAR_SEARCH,
    dijkstra: ArangoTraverser.DIJKSTRA_SEARCH,
    dijkstramulti: ArangoTraverser.DIJKSTRA_SEARCH_MULTI
  }, "strategy");

  config.order = validate(config.order, {
    preorder: ArangoTraverser.PRE_ORDER,
    postorder: ArangoTraverser.POST_ORDER,
    preorderexpander: ArangoTraverser.PRE_ORDER_EXPANDER
  }, "order");

  config.itemOrder = validate(config.itemOrder, {
    forward: ArangoTraverser.FORWARD,
    backward: ArangoTraverser.BACKWARD
  }, "itemOrder");

  if (typeof config.visitor !== "function") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid visitor function";
    throw err;
  }

  // prepare an array of filters
  var filters = [ ];
  if (config.minDepth !== undefined &&
      config.minDepth !== null &&
      config.minDepth > 0) {
    filters.push(minDepthFilter);
  }
  if (config.maxDepth !== undefined &&
      config.maxDepth !== null &&
      config.maxDepth > 0) {
    filters.push(maxDepthFilter);
  }

  if (! Array.isArray(config.filter)) {
    if (typeof config.filter === "function") {
      config.filter = [ config.filter ];
    }
    else {
      config.filter = [ ];
    }
  }

  config.filter.forEach( function (f) {
    if (typeof f !== "function") {
      err = new ArangoError();
      err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
      err.errorMessage = "invalid filter function";
      throw err;
    }

    filters.push(f);
  });

  if (filters.length > 1) {
    // more than one filter. combine their results
    config.filter = function (config, vertex, path) {
      return combineFilters(filters, config, vertex, path);
    };
  }
  else if (filters.length === 1) {
    // exactly one filter
    config.filter = filters[0];
  }
  else {
    config.filter = visitAllFilter;
  }

  if (typeof config.expander !== "function") {
    config.expander = validate(config.expander, {
      outbound: outboundExpander,
      inbound: inboundExpander,
      any: anyExpander
    }, "expander");
  }

  if (typeof config.expander !== "function") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid expander function";
    throw err;
  }

  if (typeof config.datasource !== "object") {
    err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err.errorMessage = "invalid datasource";
    throw err;
  }

  this.config = config;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief execute the traversal
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.prototype.traverse = function (result, startVertex, endVertex) {
  // get the traversal strategy
  var strategy;

  if (this.config.strategy === ArangoTraverser.ASTAR_SEARCH) {
    strategy = astarSearch();
  }
  else if (this.config.strategy === ArangoTraverser.DIJKSTRA_SEARCH) {
    strategy = dijkstraSearch();
  }
  else if (this.config.strategy === ArangoTraverser.DIJKSTRA_SEARCH_MULTI) {
    strategy = dijkstraSearchMulti();
  }
  else if (this.config.strategy === ArangoTraverser.BREADTH_FIRST) {
    strategy = breadthFirstSearch();
  }
  else {
    strategy = depthFirstSearch();
  }

  // check the start vertex
  if (startVertex === undefined ||
      startVertex === null ||
      typeof startVertex !== 'object') {
    var err1 = new ArangoError();
    err1.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err1.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message +
                       ": invalid startVertex specified for traversal";
    throw err1;
  }

  if (strategy.requiresEndVertex() &&
      (endVertex === undefined ||
       endVertex === null ||
       typeof endVertex !== 'object')) {
    var err2 = new ArangoError();
    err2.errorNum = arangodb.errors.ERROR_BAD_PARAMETER.code;
    err2.errorMessage = arangodb.errors.ERROR_BAD_PARAMETER.message +
                       ": invalid endVertex specified for traversal";
    throw err2;
  }

  // run the traversal
  try {
    strategy.run(this.config, result, startVertex, endVertex);
  }
  catch (err3) {
    if (typeof err3 !== "object" || ! err3._intentionallyAborted) {
      throw err3;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief every element can be revisited
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_NONE          = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief element can only be revisited if not already in current path
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_PATH          = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief element can only be revisited if not already visited
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.UNIQUE_GLOBAL        = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief visitation strategy breadth first
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.BREADTH_FIRST        = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief visitation strategy depth first
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DEPTH_FIRST          = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief astar search
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.ASTAR_SEARCH         = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief dijkstra search
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DIJKSTRA_SEARCH      = 3;

////////////////////////////////////////////////////////////////////////////////
/// @brief dijkstra search with multiple end vertices
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.DIJKSTRA_SEARCH_MULTI = 4;

////////////////////////////////////////////////////////////////////////////////
/// @brief pre-order traversal, visitor called before expander
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRE_ORDER            = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief post-order traversal
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.POST_ORDER           = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief pre-order traversal, visitor called at expander
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRE_ORDER_EXPANDER   = 2;

////////////////////////////////////////////////////////////////////////////////
/// @brief forward item processing order
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.FORWARD              = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief backward item processing order
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.BACKWARD             = 1;

////////////////////////////////////////////////////////////////////////////////
/// @brief prune "constant"
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.PRUNE                = 'prune';

////////////////////////////////////////////////////////////////////////////////
/// @brief exclude "constant"
////////////////////////////////////////////////////////////////////////////////

ArangoTraverser.EXCLUDE              = 'exclude';

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.collectionDatasourceFactory     = collectionDatasourceFactory;
exports.generalGraphDatasourceFactory   = generalGraphDatasourceFactory;
exports.graphDatasourceFactory          = graphDatasourceFactory;

exports.outboundExpander                = outboundExpander;
exports.inboundExpander                 = inboundExpander;
exports.anyExpander                     = anyExpander;
exports.expandOutEdgesWithLabels        = expandOutEdgesWithLabels;
exports.expandInEdgesWithLabels         = expandInEdgesWithLabels;
exports.expandEdgesWithLabels           = expandEdgesWithLabels;

exports.trackingVisitor                 = trackingVisitor;
exports.countingVisitor                 = countingVisitor;
exports.doNothingVisitor                = doNothingVisitor;

exports.visitAllFilter                  = visitAllFilter;
exports.maxDepthFilter                  = maxDepthFilter;
exports.minDepthFilter                  = minDepthFilter;
exports.includeMatchingAttributesFilter = includeMatchingAttributesFilter;
exports.abortedException                = abortedException;

exports.Traverser                       = ArangoTraverser;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
});

module.define("org/arangodb/is", function(exports, module) {
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Check if something is something
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// Check if a value is not undefined or null
function existy (x) {
  return x !== null && x !== undefined;
}

// Check if a value is undefined or null
function notExisty (x) {
  return !existy(x);
}

// Check if a value is existy and not false
function truthy (x) {
  return (x !== false) && existy(x);
}

// Check if a value is not truthy
function falsy (x) {
  return !truthy(x);
}

// is.object, is.noObject, is.array, is.noArray...
[
  'Object',
  'Array',
  'Boolean',
  'Date',
  'Function',
  'Number',
  'String',
  'RegExp'
].forEach(function(type) {
  exports[type.toLowerCase()] = function (obj) {
    return Object.prototype.toString.call(obj) === '[object '+type+']';
  };

  exports["no" + type] = function (obj) {
    return Object.prototype.toString.call(obj) !== '[object '+type+']';
  };
});

exports.existy = existy;
exports.notExisty = notExisty;
exports.truthy = truthy;
exports.falsy = falsy;
});

module.define("org/arangodb/mimetypes", function(exports, module) {
/*jslint indent: 2,
         nomen: true,
         maxlen: 100,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from mimetypes.dat
////////////////////////////////////////////////////////////////////////////////

exports.mimeTypes = {
  "gif": [ "image/gif", false ], 
  "jpg": [ "image/jpg", false ], 
  "png": [ "image/png", false ], 
  "tiff": [ "image/tiff", false ], 
  "ico": [ "image/x-icon", false ], 
  "css": [ "text/css", true ], 
  "js": [ "text/javascript", true ], 
  "json": [ "application/json", true ], 
  "html": [ "text/html", true ], 
  "htm": [ "text/html", true ], 
  "pdf": [ "application/pdf", false ], 
  "ps": [ "application/postscript", false ], 
  "txt": [ "text/plain", true ], 
  "text": [ "text/plain", true ], 
  "xml": [ "application/xml", true ], 
  "dtd": [ "application/xml-dtd", true ], 
  "svg": [ "image/svg+xml", true ], 
  "ttf": [ "application/x-font-ttf", false ], 
  "otf": [ "application/x-font-opentype", false ], 
  "woff": [ "application/font-woff", false ], 
  "eot": [ "application/vnd.ms-fontobject", false ], 
  "bz2": [ "application/x-bzip2", false ], 
  "gz": [ "application/x-gzip", false ], 
  "tgz": [ "application/x-tar", false ], 
  "zip": [ "application/x-compressed-zip", false ], 
  "doc": [ "application/msword", false ], 
  "docx": [ "application/vnd.openxmlformats-officedocument.wordprocessingml.document", false ], 
  "dotx": [ "application/vnd.openxmlformats-officedocument.wordprocessingml.template", false ], 
  "potx": [ "application/vnd.openxmlformats-officedocument.presentationml.template", false ], 
  "ppsx": [ "application/vnd.openxmlformats-officedocument.presentationml.slideshow", false ], 
  "ppt": [ "application/vnd.ms-powerpoint", false ], 
  "pptx": [ "application/vnd.openxmlformats-officedocument.presentationml.presentation", false ], 
  "xls": [ "application/vnd.ms-excel", false ], 
  "xlsb": [ "application/vnd.ms-excel.sheet.binary.macroEnabled.12", false ], 
  "xlsx": [ "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", false ], 
  "xltx": [ "application/vnd.openxmlformats-officedocument.spreadsheetml.template", false ], 
  "swf": [ "application/x-shockwave-flash", false ]
};

exports.extensions = {
  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet": [ "xlsx" ], 
  "image/svg+xml": [ "svg" ], 
  "application/postscript": [ "ps" ], 
  "image/png": [ "png" ], 
  "application/x-font-ttf": [ "ttf" ], 
  "application/vnd.ms-excel.sheet.binary.macroEnabled.12": [ "xlsb" ], 
  "application/x-font-opentype": [ "otf" ], 
  "application/vnd.openxmlformats-officedocument.wordprocessingml.document": [ "docx" ], 
  "application/x-bzip2": [ "bz2" ], 
  "application/json": [ "json" ], 
  "application/pdf": [ "pdf" ], 
  "application/vnd.openxmlformats-officedocument.presentationml.presentation": [ "pptx" ], 
  "application/vnd.ms-fontobject": [ "eot" ], 
  "application/xml-dtd": [ "dtd" ], 
  "application/x-shockwave-flash": [ "swf" ], 
  "image/gif": [ "gif" ], 
  "image/jpg": [ "jpg" ], 
  "application/xml": [ "xml" ], 
  "application/vnd.ms-excel": [ "xls" ], 
  "image/tiff": [ "tiff" ], 
  "application/vnd.ms-powerpoint": [ "ppt" ], 
  "application/font-woff": [ "woff" ], 
  "application/vnd.openxmlformats-officedocument.presentationml.template": [ "potx" ], 
  "text/plain": [ "txt", "text" ], 
  "application/x-tar": [ "tgz" ], 
  "application/vnd.openxmlformats-officedocument.spreadsheetml.template": [ "xltx" ], 
  "application/x-gzip": [ "gz" ], 
  "text/javascript": [ "js" ], 
  "text/html": [ "html", "htm" ], 
  "application/vnd.openxmlformats-officedocument.wordprocessingml.template": [ "dotx" ], 
  "image/x-icon": [ "ico" ], 
  "application/x-compressed-zip": [ "zip" ], 
  "application/vnd.openxmlformats-officedocument.presentationml.slideshow": [ "ppsx" ], 
  "text/css": [ "css" ], 
  "application/msword": [ "doc" ]
};

});

module.define("org/arangodb/replication", function(exports, module) {
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Replication management
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
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");
var arangosh = require("org/arangodb/arangosh");

// -----------------------------------------------------------------------------
// --SECTION--                                 module "org/arangodb/replication"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

var logger  = { };
var applier = { };

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication logger state
////////////////////////////////////////////////////////////////////////////////

logger.state = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/logger-state");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the tick ranges that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

logger.tickRanges = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/logger-tick-ranges");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the first tick that can be provided by the replication logger
////////////////////////////////////////////////////////////////////////////////

logger.firstTick = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/logger-first-tick");
  arangosh.checkRequestResult(requestResult);

  return requestResult.firstTick;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.start = function (initialTick) {
  var db = internal.db;
  var append = "";

  if (initialTick !== undefined) {
    append = "?from=" + encodeURIComponent(initialTick);
  }

  var requestResult = db._connection.PUT("/_api/replication/applier-start" + append, "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.stop = applier.shutdown = function () {
  var db = internal.db;

  var requestResult = db._connection.PUT("/_api/replication/applier-stop", "");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the replication applier state
////////////////////////////////////////////////////////////////////////////////

applier.state = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/applier-state");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier state and "forget" all state
////////////////////////////////////////////////////////////////////////////////

applier.forget = function () {
  var db = internal.db;

  var requestResult = db._connection.DELETE("/_api/replication/applier-state");
  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief configures the replication applier
////////////////////////////////////////////////////////////////////////////////

applier.properties = function (config) {
  var db = internal.db;

  var requestResult;
  if (config === undefined) {
    requestResult = db._connection.GET("/_api/replication/applier-config");
  }
  else {
    requestResult = db._connection.PUT("/_api/replication/applier-config",
      JSON.stringify(config));
  }

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                   other functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a one-time synchronization with a remote endpoint
////////////////////////////////////////////////////////////////////////////////

var sync = function (config) {
  var db = internal.db;

  var body = JSON.stringify(config || { });
  var requestResult = db._connection.PUT("/_api/replication/sync", body);

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief performs a one-time synchronization with a remote endpoint, for
/// a single collection
////////////////////////////////////////////////////////////////////////////////

var syncCollection = function (collection, config) {
  var db = internal.db;

  config = config || { };
  config.restrictType = "include";
  config.restrictCollections = [ collection ];
  config.includeSystem = true;
  config.incremental = true;
  var body = JSON.stringify(config);

  var requestResult = db._connection.PUT("/_api/replication/sync", body);

  arangosh.checkRequestResult(requestResult);

  return requestResult;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches a server's id
////////////////////////////////////////////////////////////////////////////////

var serverId = function () {
  var db = internal.db;

  var requestResult = db._connection.GET("/_api/replication/server-id");

  arangosh.checkRequestResult(requestResult);

  return requestResult.serverId;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    module exports
// -----------------------------------------------------------------------------

exports.logger         = logger;
exports.applier        = applier;
exports.sync           = sync;
exports.syncCollection = syncCollection;
exports.serverId       = serverId;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\|/\\*jslint"
// End:

});

module.define("org/arangodb/simple-query-common", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Arango Simple Query Language
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

var arangodb = require("org/arangodb");

var ArangoError = arangodb.ArangoError;

// forward declaration
var SimpleQueryArray;
var SimpleQueryNear;
var SimpleQueryWithin;
var SimpleQueryWithinRectangle;

// -----------------------------------------------------------------------------
// --SECTION--                                              GENERAL ARRAY CURSOR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief array query
////////////////////////////////////////////////////////////////////////////////

function GeneralArrayCursor (documents, skip, limit, data) {
  this._documents = documents;
  this._countTotal = documents.length;
  this._skip = skip;
  this._limit = limit;
  this._cached = false;
  this._extra = { };
  
  var self = this;
  if (data !== null && data !== undefined && typeof data === 'object') {
    [ 'stats', 'warnings', 'profile' ].forEach(function(d) {
      if (data.hasOwnProperty(d)) {
        self._extra[d] = data[d];
      }
    });
    this._cached = data.cached || false;
  }

  this.execute();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an array query
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.execute = function () {
  if (this._skip === null) {
    this._skip = 0;
  }

  var len = this._documents.length;
  var s = 0;
  var e = len;

  // skip from the beginning
  if (0 < this._skip) {
    s = this._skip;

    if (e < s) {
      s = e;
    }
  }

  // skip from the end
  else if (this._skip < 0) {
    var skip = -this._skip;

    if (skip < e) {
      s = e - skip;
    }
  }

  // apply limit
  if (this._limit !== null) {
    if (s + this._limit < e) {
      e = s + this._limit;
    }
  }

  this._current = s;
  this._stop = e;

  this._countQuery = e - s;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype._PRINT = function (context) {
  var text;

  text = "GeneralArrayCursor([.. " + this._documents.length + " docs .., cached: " + String(this._cached) + "])";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all elements of the cursor
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.toArray =
GeneralArrayCursor.prototype.elements = function () {
  return this._documents;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return the count of the cursor
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.count = function () {
  return this._countTotal;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief return an extra value of the cursor
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.getExtra = function () {
  return this._extra || { };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.hasNext = function () {
  return this._current < this._stop;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.next = function() {
  if (this._current < this._stop) {
    return this._documents[this._current++];
  }

  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief drops the result
////////////////////////////////////////////////////////////////////////////////

GeneralArrayCursor.prototype.dispose = function() {
  this._documents = null;
  this._skip = null;
  this._limit = null;
  this._countTotal = null;
  this._countQuery = null;
  this._current = null;
  this._stop = null;
  this._extra = null;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                      SIMPLE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief simple query
////////////////////////////////////////////////////////////////////////////////

function SimpleQuery () {
  this._execution = null;
  this._skip = 0;
  this._limit = null;
  this._countQuery = null;
  this._countTotal = null;
  this._batchSize = null;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief join limits
////////////////////////////////////////////////////////////////////////////////

function joinLimits (query, limit) {
  // original limit is 0, keep it
  if (query._limit === 0) {
    query = query.clone();
  }

  // new limit is 0, use it
  else if (limit === 0) {
    query = query.clone();
    query._limit = 0;
  }

  // no old limit, use new limit
  else if (query._limit === null) {
    query = query.clone();
    query._limit = limit;
  }

  // use the smaller one
  else {
    query = query.clone();

    if (limit < query._limit) {
      query._limit = limit;
    }
  }

  return query;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.clone = function () {
  throw "cannot clone abstract query";
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query
/// @startDocuBlock queryExecute
/// `query.execute(batchSize)`
///
/// Executes a simple query. If the optional batchSize value is specified,
/// the server will return at most batchSize values in one roundtrip.
/// The batchSize cannot be adjusted after the query is first executed.
///
/// **Note**: There is no need to explicitly call the execute method if another
/// means of fetching the query results is chosen. The following two approaches
/// lead to the same result:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{executeQuery}
/// ~ db._create("users");
/// ~ db.users.save({ name: "Gerhard" });
/// ~ db.users.save({ name: "Helmut" });
/// ~ db.users.save({ name: "Angela" });
///   result = db.users.all().toArray();
///   q = db.users.all(); q.execute(); result = [ ]; while (q.hasNext()) { result.push(q.next()); }
/// ~ db._drop("users")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// The following two alternatives both use a batchSize and return the same
/// result:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{executeQueryBatchSize}
/// ~ db._create("users");
/// ~ db.users.save({ name: "Gerhard" });
/// ~ db.users.save({ name: "Helmut" });
/// ~ db.users.save({ name: "Angela" });
///   q = db.users.all(); q.setBatchSize(20); q.execute(); while (q.hasNext()) { print(q.next()); }
///   q = db.users.all(); q.execute(20); while (q.hasNext()) { print(q.next()); }
/// ~ db._drop("users")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.execute = function () {
  throw "cannot execute abstract query";
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief limit
/// @startDocuBlock queryLimit
/// `query.limit(number)`
///
/// Limits a result to the first *number* documents. Specifying a limit of
/// *0* will return no documents at all. If you do not need a limit, just do
/// not add the limit operator. The limit must be non-negative.
///
/// In general the input to *limit* should be sorted. Otherwise it will be
/// unclear which documents will be included in the result set.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{queryLimit}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().toArray();
///   db.five.all().limit(2).toArray();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.limit = function (limit) {
  if (this._execution !== null) {
    throw "query is already executing";
  }

  if (limit < 0) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_BAD_PARAMETER;
    err.errorMessage = "limit must be non-negative";
    throw err;
  }

  return joinLimits(this, limit);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief skip
/// @startDocuBlock querySkip
/// `query.skip(number)`
///
/// Skips the first *number* documents. If *number* is positive, then this
/// number of documents are skipped before returning the query results.
///
/// In general the input to *skip* should be sorted. Otherwise it will be
/// unclear which documents will be included in the result set.
///
/// Note: using negative *skip* values is **deprecated** as of ArangoDB 2.6 and 
/// will not be supported in future versions of ArangoDB.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{querySkip}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().toArray();
///   db.five.all().skip(3).toArray();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.skip = function (skip) {
  var query;
  var documents;

  if (skip === undefined || skip === null) {
    skip = 0;
  }

  if (this._execution !== null) {
    throw "query is already executing";
  }

  // no limit set, use or add skip
  if (this._limit === null) {
    query = this.clone();

    if (this._skip === null || this._skip === 0) {
      query._skip = skip;
    }
    else {
      query._skip += skip;
    }
  }

  // limit already set
  else {
    documents = this.clone().toArray();

    query = new SimpleQueryArray(documents);
    query._skip = skip;
    query._countTotal = documents._countTotal;
  }

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief converts into an array
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.toArray = function () {
  var result;

  this.execute();

  result = [];

  while (this.hasNext()) {
    result.push(this.next());
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the batch size
/// @startDocuBlock cursorGetBatchSize
/// `cursor.getBatchSize()`
///
/// Returns the batch size for queries. If the returned value is undefined, the
/// server will determine a sensible batch size for any following requests.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.getBatchSize = function () {
  return this._batchSize;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the batch size for any following requests
/// @startDocuBlock cursorSetBatchSize
/// `cursor.setBatchSize(number)`
///
/// Sets the batch size for queries. The batch size determines how many results
/// are at most transferred from the server to the client in one chunk.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.setBatchSize = function (value) {
  if (value >= 1) {
    this._batchSize = value;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief counts the number of documents
/// @startDocuBlock cursorCount
/// `cursor.count()`
///
/// The *count* operator counts the number of document in the result set and
/// returns that number. The *count* operator ignores any limits and returns
/// the total number of documents found.
///
/// **Note**: Not all simple queries support counting. In this case *null* is
/// returned.
///
/// `cursor.count(true)`
///
/// If the result set was limited by the *limit* operator or documents were
/// skiped using the *skip* operator, the *count* operator with argument
/// *true* will use the number of elements in the final result set - after
/// applying *limit* and *skip*.
///
/// **Note**: Not all simple queries support counting. In this case *null* is
/// returned.
///
/// @EXAMPLES
///
/// Ignore any limit:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorCount}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().limit(2).count();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// Counting any limit or skip:
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorCountLimit}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().limit(2).count(true);
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
///
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.count = function (applyPagination) {
  this.execute();

  if (applyPagination === undefined || ! applyPagination) {
    return this._countTotal;
  }

  return this._countQuery;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the cursor is exhausted
/// @startDocuBlock cursorHasNext
/// `cursor.hasNext()`
///
/// The *hasNext* operator returns *true*, then the cursor still has
/// documents. In this case the next document can be accessed using the
/// *next* operator, which will advance the cursor.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorHasNext}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   var a = db.five.all();
///   while (a.hasNext()) print(a.next());
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.hasNext = function () {
  this.execute();

  return this._execution.hasNext();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the next result document
/// @startDocuBlock cursorNext
/// `cursor.next()`
///
/// If the *hasNext* operator returns *true*, then the underlying
/// cursor of the simple query still has documents.  In this case the
/// next document can be accessed using the *next* operator, which
/// will advance the underlying cursor. If you use *next* on an
/// exhausted cursor, then *undefined* is returned.
///
/// @EXAMPLES
///
/// @EXAMPLE_ARANGOSH_OUTPUT{cursorNext}
/// ~ db._create("five");
/// ~ db.five.save({ name : "one" });
/// ~ db.five.save({ name : "two" });
/// ~ db.five.save({ name : "three" });
/// ~ db.five.save({ name : "four" });
/// ~ db.five.save({ name : "five" });
///   db.five.all().next();
/// ~ db._drop("five")
/// @END_EXAMPLE_ARANGOSH_OUTPUT
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.next = function () {
  this.execute();

  return this._execution.next();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief disposes the result
/// @startDocuBlock cursorDispose
/// `cursor.dispose()`
///
/// If you are no longer interested in any further results, you should call
/// *dispose* in order to free any resources associated with the cursor.
/// After calling *dispose* you can no longer access the cursor.
/// @endDocuBlock
////////////////////////////////////////////////////////////////////////////////

SimpleQuery.prototype.dispose = function() {
  if (this._execution !== null) {
    this._execution.dispose();
  }

  this._execution = null;
  this._countQuery = null;
  this._countTotal = null;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief all query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryAll (collection) {
  this._collection = collection;
}

SimpleQueryAll.prototype = new SimpleQuery();
SimpleQueryAll.prototype.constructor = SimpleQueryAll;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.clone = function () {
  var query;

  query = new SimpleQueryAll(this._collection);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryAll(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                SIMPLE QUERY ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief array query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray = function (documents) {
  this._documents = documents;
};

SimpleQueryArray.prototype = new SimpleQuery();
SimpleQueryArray.prototype.constructor = SimpleQueryArray;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.clone = function () {
  var query;

  query = new SimpleQueryArray(this._documents);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype.execute = function () {
  if (this._execution === null) {
    if (this._skip === null) {
      this._skip = 0;
    }

    this._execution = new GeneralArrayCursor(this._documents, this._skip, this._limit);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryArray.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryArray(documents)";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query-by-example
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByExample (collection, example) {
  this._collection = collection;
  this._example = example;
}

SimpleQueryByExample.prototype = new SimpleQuery();
SimpleQueryByExample.prototype.constructor = SimpleQueryByExample;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.clone = function () {
  var query;

  query = new SimpleQueryByExample(this._collection, this._example);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryByExample(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                QUERY BY CONDITION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief query-by-condition
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryByCondition (collection, condition) {
  this._collection = collection;
  this._condition = condition;
}

SimpleQueryByCondition.prototype = new SimpleQuery();
SimpleQueryByCondition.prototype.constructor = SimpleQueryByCondition;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a query-by-condition
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.clone = function () {
  var query;

  query = new SimpleQueryByCondition(this._collection, this._condition);
  query._skip = this._skip;
  query._limit = this._limit;
  query._type = this._type;
  query._index = this._index;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print a query-by-condition
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryByCondition(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       RANGE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief range query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryRange (collection, attribute, left, right, type) {
  this._collection = collection;
  this._attribute = attribute;
  this._left = left;
  this._right = right;
  this._type = type;
}

SimpleQueryRange.prototype = new SimpleQuery();
SimpleQueryRange.prototype.constructor = SimpleQueryRange;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.clone = function () {
  var query;

  query = new SimpleQueryRange(this._collection,
                               this._attribute,
                               this._left,
                               this._right,
                               this._type);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryRange(" + this._collection.name() + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY GEO
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryGeo (collection, index) {
  this._collection = collection;
  this._index = index;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a geo index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype._PRINT = function (context) {
  var text;

  text = "GeoIndex("
       + this._collection.name()
       + ", "
       + this._index
       + ")";

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a near query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.near = function (lat, lon) {
  return new SimpleQueryNear(this._collection, lat, lon, this._index);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.within = function (lat, lon, radius) {
  return new SimpleQueryWithin(this._collection, lat, lon, radius, this._index);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a within-rectangle query for an index
////////////////////////////////////////////////////////////////////////////////

SimpleQueryGeo.prototype.withinRectangle = function (lat1, lon1, lat2, lon2) {
  return new SimpleQueryWithinRectangle(this._collection, lat1, lon1, lat2, lon2, this._index);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear = function (collection, latitude, longitude, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._distance = null;

  if (iid === undefined) {
    idx = collection.getIndexes();

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === "geo1" || index.type === "geo2") {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = require("internal").sprintf(
      arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message,
      collection.name());
    throw err;
  }
};

SimpleQueryNear.prototype = new SimpleQuery();
SimpleQueryNear.prototype.constructor = SimpleQueryNear;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.clone = function () {
  var query;

  query = new SimpleQueryNear(this._collection, this._latitude, this._longitude, this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryNear("
       + this._collection.name()
       + ", "
       + this._latitude
       + ", "
       + this._longitude
       + ", "
       + this._index
       + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance attribute
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.distance = function (attribute) {
  var clone;

  clone = this.clone();

  if (attribute) {
    clone._distance = attribute;
  }
  else {
    clone._distance = "distance";
  }

  return clone;
};

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin = function (collection, latitude, longitude, radius, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude = latitude;
  this._longitude = longitude;
  this._index = (iid === undefined ? null : iid);
  this._radius = radius;
  this._distance = null;

  if (iid === undefined) {
    idx = collection.getIndexes();

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === "geo1" || index.type === "geo2") {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }
};

SimpleQueryWithin.prototype = new SimpleQuery();
SimpleQueryWithin.prototype.constructor = SimpleQueryWithin;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.clone = function () {
  var query;

  query = new SimpleQueryWithin(this._collection,
                                this._latitude,
                                this._longitude,
                                this._radius,
                                this._index);
  query._skip = this._skip;
  query._limit = this._limit;
  query._distance = this._distance;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryWithin("
       + this._collection.name()
       + ", "
       + this._latitude
       + ", "
       + this._longitude
       + ", "
       + this._radius
       + ", "
       + this._index
       + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief adds the distance attribute
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.distance = function (attribute) {
  var clone;

  clone = this.clone();

  if (attribute) {
    clone._distance = attribute;
  }
  else {
    clone._distance = "distance";
  }

  return clone;
};

// -----------------------------------------------------------------------------
// --SECTION--                                      SIMPLE QUERY WITHINRECTANGLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief within-rectangle query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle = function (collection, latitude1, longitude1, latitude2, longitude2, iid) {
  var idx;
  var i;

  this._collection = collection;
  this._latitude1 = latitude1;
  this._longitude1 = longitude1;
  this._latitude2 = latitude2;
  this._longitude2 = longitude2;
  this._index = (iid === undefined ? null : iid);

  if (iid === undefined) {
    idx = collection.getIndexes();

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === "geo1" || index.type === "geo2") {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.id < this._index) {
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_GEO_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_GEO_INDEX_MISSING.message;
    throw err;
  }
};

SimpleQueryWithinRectangle.prototype = new SimpleQuery();
SimpleQueryWithinRectangle.prototype.constructor = SimpleQueryWithinRectangle;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a within-rectangle query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype.clone = function () {
  var query;

  query = new SimpleQueryWithinRectangle(this._collection,
                                         this._latitude1,
                                         this._longitude1,
                                         this._latitude2,
                                         this._longitude2,
                                         this._index);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a within-rectangle query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryWithinRectangle("
       + this._collection.name()
       + ", "
       + this._latitude1
       + ", "
       + this._longitude1
       + ", "
       + this._latitude2
       + ", "
       + this._longitude2
       + ", "
       + this._index
       + ")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                             SIMPLE QUERY FULLTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief fulltext query
////////////////////////////////////////////////////////////////////////////////

function SimpleQueryFulltext (collection, attribute, query, iid) {
  this._collection = collection;
  this._attribute = attribute;
  this._query = query;
  this._index = (iid === undefined ? null : iid);

  if (iid === undefined) {
    var idx = collection.getIndexes();
    var i;

    for (i = 0;  i < idx.length;  ++i) {
      var index = idx[i];

      if (index.type === "fulltext" && index.fields[0] === attribute) {
        if (this._index === null) {
          this._index = index.id;
        }
        else if (index.indexSubstrings && ! this._index.indexSubstrings) {
          // prefer indexes that have substrings indexed
          this._index = index.id;
        }
      }
    }
  }

  if (this._index === null) {
    var err = new ArangoError();
    err.errorNum = arangodb.ERROR_QUERY_FULLTEXT_INDEX_MISSING;
    err.errorMessage = arangodb.errors.ERROR_QUERY_FULLTEXT_INDEX_MISSING.message;
    throw err;
  }
}

SimpleQueryFulltext.prototype = new SimpleQuery();
SimpleQueryFulltext.prototype.constructor = SimpleQueryFulltext;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.clone = function () {
  var query;

  query = new SimpleQueryFulltext(this._collection, this._attribute, this._query, this._index);
  query._skip = this._skip;
  query._limit = this._limit;

  return query;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype._PRINT = function (context) {
  var text;

  text = "SimpleQueryFulltext("
       + this._collection.name()
       + ", "
       + this._attribute
       + ", \""
       + this._query
       + "\")";

  if (this._skip !== null && this._skip !== 0) {
    text += ".skip(" + this._skip + ")";
  }

  if (this._limit !== null) {
    text += ".limit(" + this._limit + ")";
  }

  context.output += text;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryWithinRectangle = SimpleQueryWithinRectangle;
exports.SimpleQueryFulltext = SimpleQueryFulltext;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
});

module.define("org/arangodb/simple-query", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Arango Simple Query Language
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

var arangosh = require("org/arangodb/arangosh");

var ArangoQueryCursor = require("org/arangodb/arango-query-cursor").ArangoQueryCursor;

var sq = require("org/arangodb/simple-query-common");

var GeneralArrayCursor = sq.GeneralArrayCursor;
var SimpleQueryAll = sq.SimpleQueryAll;
var SimpleQueryArray = sq.SimpleQueryArray;
var SimpleQueryByExample = sq.SimpleQueryByExample;
var SimpleQueryByCondition = sq.SimpleQueryByCondition;
var SimpleQueryFulltext = sq.SimpleQueryFulltext;
var SimpleQueryGeo = sq.SimpleQueryGeo;
var SimpleQueryNear = sq.SimpleQueryNear;
var SimpleQueryRange = sq.SimpleQueryRange;
var SimpleQueryWithin = sq.SimpleQueryWithin;
var SimpleQueryWithinRectangle = sq.SimpleQueryWithinRectangle;

// -----------------------------------------------------------------------------
// --SECTION--                                                  SIMPLE QUERY ALL
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an all query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryAll.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name()
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/all", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  QUERY BY EXAMPLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-example
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByExample.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      example: this._example
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var method = "by-example";
    if (this.hasOwnProperty("_type")) {
      data.index = this._index;

      switch (this._type) {
        case "hash":
          method = "by-example-hash";
          break;
        case "skiplist":
          method = "by-example-skiplist";
          break;
      }
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/" + method, JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
      this._countTotal = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                QUERY BY CONDITION
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a query-by-condition
////////////////////////////////////////////////////////////////////////////////

SimpleQueryByCondition.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      condition: this._condition
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var method = "by-condition";
    if (this.hasOwnProperty("_type")) {
      data.index = this._index;

      switch (this._type) {
        case "skiplist":
          method = "by-condition-skiplist";
          break;
      }
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/" + method, JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
      this._countTotal = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       RANGE QUERY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a range query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryRange.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      attribute: this._attribute,
      right: this._right,
      left: this._left,
      closed: this._type === 1
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/range", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 SIMPLE QUERY NEAR
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a near query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryNear.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      latitude: this._latitude,
      longitude: this._longitude
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }

    if (this._distance !== null) {
      data.distance = this._distance;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/near", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                               SIMPLE QUERY WITHIN
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a within query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithin.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      latitude: this._latitude,
      longitude: this._longitude,
      radius: this._radius
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }

    if (this._distance !== null) {
      data.distance = this._distance;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/within", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                      SIMPLE QUERY WITHINRECTANGLE
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a withinRectangle query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryWithinRectangle.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      latitude1: this._latitude1,
      longitude1: this._longitude1,
      latitude2: this._latitude2,
      longitude2: this._longitude2
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._index !== null) {
      data.geo = this._index;
    }

    if (this._distance !== null) {
      data.distance = this._distance;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/within-rectangle", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                             SIMPLE QUERY FULLTEXT
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a fulltext query
////////////////////////////////////////////////////////////////////////////////

SimpleQueryFulltext.prototype.execute = function (batchSize) {
  if (this._execution === null) {
    if (batchSize !== undefined && batchSize > 0) {
      this._batchSize = batchSize;
    }

    var data = {
      collection: this._collection.name(),
      attribute: this._attribute,
      query: this._query
    };

    if (this._limit !== null) {
      data.limit = this._limit;
    }

    if (this._index !== null) {
      data.index = this._index;
    }

    if (this._skip !== null) {
      data.skip = this._skip;
    }

    if (this._batchSize !== null) {
      data.batchSize = this._batchSize;
    }

    var requestResult = this._collection._database._connection.PUT(
      "/_api/simple/fulltext", JSON.stringify(data));

    arangosh.checkRequestResult(requestResult);

    this._execution = new ArangoQueryCursor(this._collection._database, requestResult);

    if (requestResult.hasOwnProperty("count")) {
      this._countQuery = requestResult.count;
    }
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                    MODULE EXPORTS
// -----------------------------------------------------------------------------

exports.GeneralArrayCursor = GeneralArrayCursor;
exports.SimpleQueryAll = SimpleQueryAll;
exports.SimpleQueryArray = SimpleQueryArray;
exports.SimpleQueryByExample = SimpleQueryByExample;
exports.SimpleQueryByCondition = SimpleQueryByCondition;
exports.SimpleQueryFulltext = SimpleQueryFulltext;
exports.SimpleQueryGeo = SimpleQueryGeo;
exports.SimpleQueryNear = SimpleQueryNear;
exports.SimpleQueryRange = SimpleQueryRange;
exports.SimpleQueryWithin = SimpleQueryWithin;
exports.SimpleQueryWithinRectangle = SimpleQueryWithinRectangle;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
});

module.define("org/arangodb/tutorial", function(exports, module) {
/*jshint strict: false */

////////////////////////////////////////////////////////////////////////////////
/// @brief Shell tutorial
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2012-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                    module "org/arangodb/tutorial"
// -----------------------------------------------------------------------------

var index = 0;
var next = "Type 'tutorial' again to get to the next chapter.";

var lessons = [
  {
    title: "Welcome to the tutorial!",
    text:  "This is a user-interactive tutorial on ArangoDB and the ArangoDB shell.\n" +
           "It will give you a first look into ArangoDB and how it works."
  },
  {
    title: "JavaScript Shell",
    text:  "On this shell's prompt, you can issue arbitrary JavaScript commands.\n" +
           "So you are able to do things like...:\n\n" +
           "  number = 123;\n" +
           "  number = number * 10;"
  },
  {
    title: "Running Complex Instructions",
    text:  "You can also run more complex instructions, such as for loops:\n\n" +
           "  for (i = 0; i < 10; i++) { number = number + 1; }"
  },
  {
    title: "Printing Results",
    text:  "As you can see, the result of the last command executed is printed automatically. " +
           "To explicitly print a value at any other time, there is the print function:\n\n" +
           "  for (i = 0; i < 5; ++i) { print(\"I am a JavaScript shell\"); }"
  },
  {
    title: "Creating Collections",
    text:  "ArangoDB is a document database. This means that we store data as documents " +
           "(which are similar to JavaScript objects) in so-called 'collections'. " +
           "Let's create a collection named 'places' now:\n\n" +
           "  db._create('places');\n\n" +
           "Note: each collection is identified by a unique name. Trying to create a " +
           "collection that already exists will produce an error."
  },
  {
    title: "Displaying Collections",
    text:  "Now you can take a look at the collection(s) you just created:\n\n" +
           "  db._collections();\n\n" +
           "Please note that all collections will be returned, including ArangoDB's pre-defined " +
           "system collections."
  },
  {
    title: "Creating Documents",
    text:  "Now we have a collection, but it is still empty. So let's create some documents!\n\n" +
           "  db.places.save({ _key : \"foo\", city : \"foo-city\" });\n" +
           "  for (i = 0; i <= 10; i++) { db.places.save({ _key: \"example\" + i, zipcode: i }) };"
  },
  {
    title: "Displaying All Documents",
    text:  "You want to take a look at your docs? No problem:\n\n" +
           "  db.places.toArray();"
  },
  {
    title: "Counting Documents",
    text:  "To see how many documents there are in a collection, use the 'count' method:\n\n" +
           "  db.places.count();"
  },
  {
    title: "Retrieving Single Documents",
    text:  "As you can see, each document has some meta attributes '_id', '_key' and '_rev'.\n" +
           "The '_key' attribute can be used to quickly retrieve a single document from " +
           "a collection:\n\n" +
           "  db.places.document(\"foo\");\n" +
           "  db.places.document(\"example5\");"
  },
  {
    title: "Retrieving Single Documents",
    text:  "The '_id' attribute can also be used to retrieve documents using the 'db' object:\n\n" +
           "  db._document(\"places/foo\");\n" +
           "  db._document(\"places/example5\");"
  },
  {
    title: "Modifying Documents",
    text:  "You can modify existing documents. Try to add a new attribute to a document and " +
           "verify whether it has been added:\n\n" +
           "  db._update(\"places/foo\", { zipcode: 39535 });\n" +
           "  db._document(\"places/foo\");"
  },
  {
    title: "Document Revisions",
    text:  "Note that after updating the document, its '_rev' attribute changed automatically.\n" +
           "The '_rev' attribute contains a document revision number, and it can be used for " +
           "conditional modifications. Here's an example of how to avoid lost updates in case " +
           "multiple clients are accessing the documents in parallel:\n\n" +
           "  doc = db._document(\"places/example1\");\n" +
           "  db._update(\"places/example1\", { someValue: 23 });\n" +
           "  db._update(doc, { someValue: 42 });\n\n" +
           "Note that the first update will succeed because it was unconditional. The second " +
           "update however is conditional because we're also passing the document's revision " +
           "id in the first parameter to _update. As the revision id we're passing to update " +
           "does not match the document's current revision anymore, the update is rejected."
  },
  {
    title: "Removing Documents",
    text:  "Deleting single documents can be achieved by providing the document _id or _key:\n\n" +
           "  db._remove(\"places/example7\");\n" +
           "  db.places.remove(\"example8\");\n" +
           "  db.places.count();"
  },
  {
    title: "Searching Documents",
    text:  "Searching for documents with specific attributes can be done by using the " +
           "byExample method:\n\n" +
           "  db._create(\"users\");\n" +
           "  for (i = 0; i < 10; ++i) { " +
              "db.users.save({ name: \"username\" + i, active: (i % 3 == 0), age: 30 + i }); }\n" +
           "  db.users.byExample({ active: false }).toArray();\n" +
           "  db.users.byExample({ name: \"username3\", active: true }).toArray();\n"
  },
  {
    title: "Running AQL Queries",
    text:  "ArangoDB also provides a query language for more complex matching:\n\n" +
           "  db._query(\"FOR u IN users FILTER u.active == true && u.age >= 33 " +
              "RETURN { username: u.name, age: u.age }\").toArray();"
  },
  {
    title: "Using Databases",
    text:  "By default, the ArangoShell connects to the default database. The default database " +
           "is named '_system'. To create another database, use the '_createDatabase' method of the " +
           "'db' object. To switch into an existing database, use '_useDatabase'. To get rid of a " +
           "database and all of its collections, use '_dropDatabase':\n\n" +
           "  db._createDatabase(\"mydb\");\n" +
           "  db._useDatabase(\"mydb\");\n" +
           "  db._dropDatabase(\"mydb\");"
  }
];

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief print tutorial contents
////////////////////////////////////////////////////////////////////////////////

exports._PRINT = function (context) {
  var colors = require("internal").COLORS;

  /*jslint regexp: true */
  function process (text) {
    return text.replace(/\n {2}(.+?)(?=\n)/g,
                        "\n  " + colors.COLOR_MAGENTA + "$1" + colors.COLOR_RESET);
  }
  /*jslint regexp: false */

  var headline = colors.COLOR_BOLD_BLUE + (index + 1) + ". " + lessons[index].title + colors.COLOR_RESET;

  context.output += "\n\n" +
                    headline + "\n\n" +
                    process(lessons[index].text + "\n") + "\n";

  ++index;

  if (index >= lessons.length) {
    context.output += "Congratulations! You finished the tutorial.\n";
    index = 0;
  }
  else {
    context.output += next + "\n";
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
});

module.define("underscore", function(exports, module) {
//     Underscore.js 1.8.3
//     http://underscorejs.org
//     (c) 2009-2015 Jeremy Ashkenas, DocumentCloud and Investigative Reporters & Editors
//     Underscore may be freely distributed under the MIT license.

(function() {

  // Baseline setup
  // --------------

  // Establish the root object, `window` in the browser, or `exports` on the server.
  var root = this;

  // Save the previous value of the `_` variable.
  var previousUnderscore = root._;

  // Save bytes in the minified (but not gzipped) version:
  var ArrayProto = Array.prototype, ObjProto = Object.prototype, FuncProto = Function.prototype;

  // Create quick reference variables for speed access to core prototypes.
  var
    push             = ArrayProto.push,
    slice            = ArrayProto.slice,
    toString         = ObjProto.toString,
    hasOwnProperty   = ObjProto.hasOwnProperty;

  // All **ECMAScript 5** native function implementations that we hope to use
  // are declared here.
  var
    nativeIsArray      = Array.isArray,
    nativeKeys         = Object.keys,
    nativeBind         = FuncProto.bind,
    nativeCreate       = Object.create;

  // Naked function reference for surrogate-prototype-swapping.
  var Ctor = function(){};

  // Create a safe reference to the Underscore object for use below.
  var _ = function(obj) {
    if (obj instanceof _) return obj;
    if (!(this instanceof _)) return new _(obj);
    this._wrapped = obj;
  };

  // Export the Underscore object for **Node.js**, with
  // backwards-compatibility for the old `require()` API. If we're in
  // the browser, add `_` as a global object.
  if (typeof exports !== 'undefined') {
    if (typeof module !== 'undefined' && module.exports) {
      exports = module.exports = _;
    }
    exports._ = _;
  } else {
    root._ = _;
  }

  // Current version.
  _.VERSION = '1.8.3';

  // Internal function that returns an efficient (for current engines) version
  // of the passed-in callback, to be repeatedly applied in other Underscore
  // functions.
  var optimizeCb = function(func, context, argCount) {
    if (context === void 0) return func;
    switch (argCount == null ? 3 : argCount) {
      case 1: return function(value) {
        return func.call(context, value);
      };
      case 2: return function(value, other) {
        return func.call(context, value, other);
      };
      case 3: return function(value, index, collection) {
        return func.call(context, value, index, collection);
      };
      case 4: return function(accumulator, value, index, collection) {
        return func.call(context, accumulator, value, index, collection);
      };
    }
    return function() {
      return func.apply(context, arguments);
    };
  };

  // A mostly-internal function to generate callbacks that can be applied
  // to each element in a collection, returning the desired result — either
  // identity, an arbitrary callback, a property matcher, or a property accessor.
  var cb = function(value, context, argCount) {
    if (value == null) return _.identity;
    if (_.isFunction(value)) return optimizeCb(value, context, argCount);
    if (_.isObject(value)) return _.matcher(value);
    return _.property(value);
  };
  _.iteratee = function(value, context) {
    return cb(value, context, Infinity);
  };

  // An internal function for creating assigner functions.
  var createAssigner = function(keysFunc, undefinedOnly) {
    return function(obj) {
      var length = arguments.length;
      if (length < 2 || obj == null) return obj;
      for (var index = 1; index < length; index++) {
        var source = arguments[index],
            keys = keysFunc(source),
            l = keys.length;
        for (var i = 0; i < l; i++) {
          var key = keys[i];
          if (!undefinedOnly || obj[key] === void 0) obj[key] = source[key];
        }
      }
      return obj;
    };
  };

  // An internal function for creating a new object that inherits from another.
  var baseCreate = function(prototype) {
    if (!_.isObject(prototype)) return {};
    if (nativeCreate) return nativeCreate(prototype);
    Ctor.prototype = prototype;
    var result = new Ctor;
    Ctor.prototype = null;
    return result;
  };

  var property = function(key) {
    return function(obj) {
      return obj == null ? void 0 : obj[key];
    };
  };

  // Helper for collection methods to determine whether a collection
  // should be iterated as an array or as an object
  // Related: http://people.mozilla.org/~jorendorff/es6-draft.html#sec-tolength
  // Avoids a very nasty iOS 8 JIT bug on ARM-64. #2094
  var MAX_ARRAY_INDEX = Math.pow(2, 53) - 1;
  var getLength = property('length');
  var isArrayLike = function(collection) {
    var length = getLength(collection);
    return typeof length == 'number' && length >= 0 && length <= MAX_ARRAY_INDEX;
  };

  // Collection Functions
  // --------------------

  // The cornerstone, an `each` implementation, aka `forEach`.
  // Handles raw objects in addition to array-likes. Treats all
  // sparse array-likes as if they were dense.
  _.each = _.forEach = function(obj, iteratee, context) {
    iteratee = optimizeCb(iteratee, context);
    var i, length;
    if (isArrayLike(obj)) {
      for (i = 0, length = obj.length; i < length; i++) {
        iteratee(obj[i], i, obj);
      }
    } else {
      var keys = _.keys(obj);
      for (i = 0, length = keys.length; i < length; i++) {
        iteratee(obj[keys[i]], keys[i], obj);
      }
    }
    return obj;
  };

  // Return the results of applying the iteratee to each element.
  _.map = _.collect = function(obj, iteratee, context) {
    iteratee = cb(iteratee, context);
    var keys = !isArrayLike(obj) && _.keys(obj),
        length = (keys || obj).length,
        results = Array(length);
    for (var index = 0; index < length; index++) {
      var currentKey = keys ? keys[index] : index;
      results[index] = iteratee(obj[currentKey], currentKey, obj);
    }
    return results;
  };

  // Create a reducing function iterating left or right.
  function createReduce(dir) {
    // Optimized iterator function as using arguments.length
    // in the main function will deoptimize the, see #1991.
    function iterator(obj, iteratee, memo, keys, index, length) {
      for (; index >= 0 && index < length; index += dir) {
        var currentKey = keys ? keys[index] : index;
        memo = iteratee(memo, obj[currentKey], currentKey, obj);
      }
      return memo;
    }

    return function(obj, iteratee, memo, context) {
      iteratee = optimizeCb(iteratee, context, 4);
      var keys = !isArrayLike(obj) && _.keys(obj),
          length = (keys || obj).length,
          index = dir > 0 ? 0 : length - 1;
      // Determine the initial value if none is provided.
      if (arguments.length < 3) {
        memo = obj[keys ? keys[index] : index];
        index += dir;
      }
      return iterator(obj, iteratee, memo, keys, index, length);
    };
  }

  // **Reduce** builds up a single result from a list of values, aka `inject`,
  // or `foldl`.
  _.reduce = _.foldl = _.inject = createReduce(1);

  // The right-associative version of reduce, also known as `foldr`.
  _.reduceRight = _.foldr = createReduce(-1);

  // Return the first value which passes a truth test. Aliased as `detect`.
  _.find = _.detect = function(obj, predicate, context) {
    var key;
    if (isArrayLike(obj)) {
      key = _.findIndex(obj, predicate, context);
    } else {
      key = _.findKey(obj, predicate, context);
    }
    if (key !== void 0 && key !== -1) return obj[key];
  };

  // Return all the elements that pass a truth test.
  // Aliased as `select`.
  _.filter = _.select = function(obj, predicate, context) {
    var results = [];
    predicate = cb(predicate, context);
    _.each(obj, function(value, index, list) {
      if (predicate(value, index, list)) results.push(value);
    });
    return results;
  };

  // Return all the elements for which a truth test fails.
  _.reject = function(obj, predicate, context) {
    return _.filter(obj, _.negate(cb(predicate)), context);
  };

  // Determine whether all of the elements match a truth test.
  // Aliased as `all`.
  _.every = _.all = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var keys = !isArrayLike(obj) && _.keys(obj),
        length = (keys || obj).length;
    for (var index = 0; index < length; index++) {
      var currentKey = keys ? keys[index] : index;
      if (!predicate(obj[currentKey], currentKey, obj)) return false;
    }
    return true;
  };

  // Determine if at least one element in the object matches a truth test.
  // Aliased as `any`.
  _.some = _.any = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var keys = !isArrayLike(obj) && _.keys(obj),
        length = (keys || obj).length;
    for (var index = 0; index < length; index++) {
      var currentKey = keys ? keys[index] : index;
      if (predicate(obj[currentKey], currentKey, obj)) return true;
    }
    return false;
  };

  // Determine if the array or object contains a given item (using `===`).
  // Aliased as `includes` and `include`.
  _.contains = _.includes = _.include = function(obj, item, fromIndex, guard) {
    if (!isArrayLike(obj)) obj = _.values(obj);
    if (typeof fromIndex != 'number' || guard) fromIndex = 0;
    return _.indexOf(obj, item, fromIndex) >= 0;
  };

  // Invoke a method (with arguments) on every item in a collection.
  _.invoke = function(obj, method) {
    var args = slice.call(arguments, 2);
    var isFunc = _.isFunction(method);
    return _.map(obj, function(value) {
      var func = isFunc ? method : value[method];
      return func == null ? func : func.apply(value, args);
    });
  };

  // Convenience version of a common use case of `map`: fetching a property.
  _.pluck = function(obj, key) {
    return _.map(obj, _.property(key));
  };

  // Convenience version of a common use case of `filter`: selecting only objects
  // containing specific `key:value` pairs.
  _.where = function(obj, attrs) {
    return _.filter(obj, _.matcher(attrs));
  };

  // Convenience version of a common use case of `find`: getting the first object
  // containing specific `key:value` pairs.
  _.findWhere = function(obj, attrs) {
    return _.find(obj, _.matcher(attrs));
  };

  // Return the maximum element (or element-based computation).
  _.max = function(obj, iteratee, context) {
    var result = -Infinity, lastComputed = -Infinity,
        value, computed;
    if (iteratee == null && obj != null) {
      obj = isArrayLike(obj) ? obj : _.values(obj);
      for (var i = 0, length = obj.length; i < length; i++) {
        value = obj[i];
        if (value > result) {
          result = value;
        }
      }
    } else {
      iteratee = cb(iteratee, context);
      _.each(obj, function(value, index, list) {
        computed = iteratee(value, index, list);
        if (computed > lastComputed || computed === -Infinity && result === -Infinity) {
          result = value;
          lastComputed = computed;
        }
      });
    }
    return result;
  };

  // Return the minimum element (or element-based computation).
  _.min = function(obj, iteratee, context) {
    var result = Infinity, lastComputed = Infinity,
        value, computed;
    if (iteratee == null && obj != null) {
      obj = isArrayLike(obj) ? obj : _.values(obj);
      for (var i = 0, length = obj.length; i < length; i++) {
        value = obj[i];
        if (value < result) {
          result = value;
        }
      }
    } else {
      iteratee = cb(iteratee, context);
      _.each(obj, function(value, index, list) {
        computed = iteratee(value, index, list);
        if (computed < lastComputed || computed === Infinity && result === Infinity) {
          result = value;
          lastComputed = computed;
        }
      });
    }
    return result;
  };

  // Shuffle a collection, using the modern version of the
  // [Fisher-Yates shuffle](http://en.wikipedia.org/wiki/Fisher–Yates_shuffle).
  _.shuffle = function(obj) {
    var set = isArrayLike(obj) ? obj : _.values(obj);
    var length = set.length;
    var shuffled = Array(length);
    for (var index = 0, rand; index < length; index++) {
      rand = _.random(0, index);
      if (rand !== index) shuffled[index] = shuffled[rand];
      shuffled[rand] = set[index];
    }
    return shuffled;
  };

  // Sample **n** random values from a collection.
  // If **n** is not specified, returns a single random element.
  // The internal `guard` argument allows it to work with `map`.
  _.sample = function(obj, n, guard) {
    if (n == null || guard) {
      if (!isArrayLike(obj)) obj = _.values(obj);
      return obj[_.random(obj.length - 1)];
    }
    return _.shuffle(obj).slice(0, Math.max(0, n));
  };

  // Sort the object's values by a criterion produced by an iteratee.
  _.sortBy = function(obj, iteratee, context) {
    iteratee = cb(iteratee, context);
    return _.pluck(_.map(obj, function(value, index, list) {
      return {
        value: value,
        index: index,
        criteria: iteratee(value, index, list)
      };
    }).sort(function(left, right) {
      var a = left.criteria;
      var b = right.criteria;
      if (a !== b) {
        if (a > b || a === void 0) return 1;
        if (a < b || b === void 0) return -1;
      }
      return left.index - right.index;
    }), 'value');
  };

  // An internal function used for aggregate "group by" operations.
  var group = function(behavior) {
    return function(obj, iteratee, context) {
      var result = {};
      iteratee = cb(iteratee, context);
      _.each(obj, function(value, index) {
        var key = iteratee(value, index, obj);
        behavior(result, value, key);
      });
      return result;
    };
  };

  // Groups the object's values by a criterion. Pass either a string attribute
  // to group by, or a function that returns the criterion.
  _.groupBy = group(function(result, value, key) {
    if (_.has(result, key)) result[key].push(value); else result[key] = [value];
  });

  // Indexes the object's values by a criterion, similar to `groupBy`, but for
  // when you know that your index values will be unique.
  _.indexBy = group(function(result, value, key) {
    result[key] = value;
  });

  // Counts instances of an object that group by a certain criterion. Pass
  // either a string attribute to count by, or a function that returns the
  // criterion.
  _.countBy = group(function(result, value, key) {
    if (_.has(result, key)) result[key]++; else result[key] = 1;
  });

  // Safely create a real, live array from anything iterable.
  _.toArray = function(obj) {
    if (!obj) return [];
    if (_.isArray(obj)) return slice.call(obj);
    if (isArrayLike(obj)) return _.map(obj, _.identity);
    return _.values(obj);
  };

  // Return the number of elements in an object.
  _.size = function(obj) {
    if (obj == null) return 0;
    return isArrayLike(obj) ? obj.length : _.keys(obj).length;
  };

  // Split a collection into two arrays: one whose elements all satisfy the given
  // predicate, and one whose elements all do not satisfy the predicate.
  _.partition = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var pass = [], fail = [];
    _.each(obj, function(value, key, obj) {
      (predicate(value, key, obj) ? pass : fail).push(value);
    });
    return [pass, fail];
  };

  // Array Functions
  // ---------------

  // Get the first element of an array. Passing **n** will return the first N
  // values in the array. Aliased as `head` and `take`. The **guard** check
  // allows it to work with `_.map`.
  _.first = _.head = _.take = function(array, n, guard) {
    if (array == null) return void 0;
    if (n == null || guard) return array[0];
    return _.initial(array, array.length - n);
  };

  // Returns everything but the last entry of the array. Especially useful on
  // the arguments object. Passing **n** will return all the values in
  // the array, excluding the last N.
  _.initial = function(array, n, guard) {
    return slice.call(array, 0, Math.max(0, array.length - (n == null || guard ? 1 : n)));
  };

  // Get the last element of an array. Passing **n** will return the last N
  // values in the array.
  _.last = function(array, n, guard) {
    if (array == null) return void 0;
    if (n == null || guard) return array[array.length - 1];
    return _.rest(array, Math.max(0, array.length - n));
  };

  // Returns everything but the first entry of the array. Aliased as `tail` and `drop`.
  // Especially useful on the arguments object. Passing an **n** will return
  // the rest N values in the array.
  _.rest = _.tail = _.drop = function(array, n, guard) {
    return slice.call(array, n == null || guard ? 1 : n);
  };

  // Trim out all falsy values from an array.
  _.compact = function(array) {
    return _.filter(array, _.identity);
  };

  // Internal implementation of a recursive `flatten` function.
  var flatten = function(input, shallow, strict, startIndex) {
    var output = [], idx = 0;
    for (var i = startIndex || 0, length = getLength(input); i < length; i++) {
      var value = input[i];
      if (isArrayLike(value) && (_.isArray(value) || _.isArguments(value))) {
        //flatten current level of array or arguments object
        if (!shallow) value = flatten(value, shallow, strict);
        var j = 0, len = value.length;
        output.length += len;
        while (j < len) {
          output[idx++] = value[j++];
        }
      } else if (!strict) {
        output[idx++] = value;
      }
    }
    return output;
  };

  // Flatten out an array, either recursively (by default), or just one level.
  _.flatten = function(array, shallow) {
    return flatten(array, shallow, false);
  };

  // Return a version of the array that does not contain the specified value(s).
  _.without = function(array) {
    return _.difference(array, slice.call(arguments, 1));
  };

  // Produce a duplicate-free version of the array. If the array has already
  // been sorted, you have the option of using a faster algorithm.
  // Aliased as `unique`.
  _.uniq = _.unique = function(array, isSorted, iteratee, context) {
    if (!_.isBoolean(isSorted)) {
      context = iteratee;
      iteratee = isSorted;
      isSorted = false;
    }
    if (iteratee != null) iteratee = cb(iteratee, context);
    var result = [];
    var seen = [];
    for (var i = 0, length = getLength(array); i < length; i++) {
      var value = array[i],
          computed = iteratee ? iteratee(value, i, array) : value;
      if (isSorted) {
        if (!i || seen !== computed) result.push(value);
        seen = computed;
      } else if (iteratee) {
        if (!_.contains(seen, computed)) {
          seen.push(computed);
          result.push(value);
        }
      } else if (!_.contains(result, value)) {
        result.push(value);
      }
    }
    return result;
  };

  // Produce an array that contains the union: each distinct element from all of
  // the passed-in arrays.
  _.union = function() {
    return _.uniq(flatten(arguments, true, true));
  };

  // Produce an array that contains every item shared between all the
  // passed-in arrays.
  _.intersection = function(array) {
    var result = [];
    var argsLength = arguments.length;
    for (var i = 0, length = getLength(array); i < length; i++) {
      var item = array[i];
      if (_.contains(result, item)) continue;
      for (var j = 1; j < argsLength; j++) {
        if (!_.contains(arguments[j], item)) break;
      }
      if (j === argsLength) result.push(item);
    }
    return result;
  };

  // Take the difference between one array and a number of other arrays.
  // Only the elements present in just the first array will remain.
  _.difference = function(array) {
    var rest = flatten(arguments, true, true, 1);
    return _.filter(array, function(value){
      return !_.contains(rest, value);
    });
  };

  // Zip together multiple lists into a single array -- elements that share
  // an index go together.
  _.zip = function() {
    return _.unzip(arguments);
  };

  // Complement of _.zip. Unzip accepts an array of arrays and groups
  // each array's elements on shared indices
  _.unzip = function(array) {
    var length = array && _.max(array, getLength).length || 0;
    var result = Array(length);

    for (var index = 0; index < length; index++) {
      result[index] = _.pluck(array, index);
    }
    return result;
  };

  // Converts lists into objects. Pass either a single array of `[key, value]`
  // pairs, or two parallel arrays of the same length -- one of keys, and one of
  // the corresponding values.
  _.object = function(list, values) {
    var result = {};
    for (var i = 0, length = getLength(list); i < length; i++) {
      if (values) {
        result[list[i]] = values[i];
      } else {
        result[list[i][0]] = list[i][1];
      }
    }
    return result;
  };

  // Generator function to create the findIndex and findLastIndex functions
  function createPredicateIndexFinder(dir) {
    return function(array, predicate, context) {
      predicate = cb(predicate, context);
      var length = getLength(array);
      var index = dir > 0 ? 0 : length - 1;
      for (; index >= 0 && index < length; index += dir) {
        if (predicate(array[index], index, array)) return index;
      }
      return -1;
    };
  }

  // Returns the first index on an array-like that passes a predicate test
  _.findIndex = createPredicateIndexFinder(1);
  _.findLastIndex = createPredicateIndexFinder(-1);

  // Use a comparator function to figure out the smallest index at which
  // an object should be inserted so as to maintain order. Uses binary search.
  _.sortedIndex = function(array, obj, iteratee, context) {
    iteratee = cb(iteratee, context, 1);
    var value = iteratee(obj);
    var low = 0, high = getLength(array);
    while (low < high) {
      var mid = Math.floor((low + high) / 2);
      if (iteratee(array[mid]) < value) low = mid + 1; else high = mid;
    }
    return low;
  };

  // Generator function to create the indexOf and lastIndexOf functions
  function createIndexFinder(dir, predicateFind, sortedIndex) {
    return function(array, item, idx) {
      var i = 0, length = getLength(array);
      if (typeof idx == 'number') {
        if (dir > 0) {
            i = idx >= 0 ? idx : Math.max(idx + length, i);
        } else {
            length = idx >= 0 ? Math.min(idx + 1, length) : idx + length + 1;
        }
      } else if (sortedIndex && idx && length) {
        idx = sortedIndex(array, item);
        return array[idx] === item ? idx : -1;
      }
      if (item !== item) {
        idx = predicateFind(slice.call(array, i, length), _.isNaN);
        return idx >= 0 ? idx + i : -1;
      }
      for (idx = dir > 0 ? i : length - 1; idx >= 0 && idx < length; idx += dir) {
        if (array[idx] === item) return idx;
      }
      return -1;
    };
  }

  // Return the position of the first occurrence of an item in an array,
  // or -1 if the item is not included in the array.
  // If the array is large and already in sort order, pass `true`
  // for **isSorted** to use binary search.
  _.indexOf = createIndexFinder(1, _.findIndex, _.sortedIndex);
  _.lastIndexOf = createIndexFinder(-1, _.findLastIndex);

  // Generate an integer Array containing an arithmetic progression. A port of
  // the native Python `range()` function. See
  // [the Python documentation](http://docs.python.org/library/functions.html#range).
  _.range = function(start, stop, step) {
    if (stop == null) {
      stop = start || 0;
      start = 0;
    }
    step = step || 1;

    var length = Math.max(Math.ceil((stop - start) / step), 0);
    var range = Array(length);

    for (var idx = 0; idx < length; idx++, start += step) {
      range[idx] = start;
    }

    return range;
  };

  // Function (ahem) Functions
  // ------------------

  // Determines whether to execute a function as a constructor
  // or a normal function with the provided arguments
  var executeBound = function(sourceFunc, boundFunc, context, callingContext, args) {
    if (!(callingContext instanceof boundFunc)) return sourceFunc.apply(context, args);
    var self = baseCreate(sourceFunc.prototype);
    var result = sourceFunc.apply(self, args);
    if (_.isObject(result)) return result;
    return self;
  };

  // Create a function bound to a given object (assigning `this`, and arguments,
  // optionally). Delegates to **ECMAScript 5**'s native `Function.bind` if
  // available.
  _.bind = function(func, context) {
    if (nativeBind && func.bind === nativeBind) return nativeBind.apply(func, slice.call(arguments, 1));
    if (!_.isFunction(func)) throw new TypeError('Bind must be called on a function');
    var args = slice.call(arguments, 2);
    var bound = function() {
      return executeBound(func, bound, context, this, args.concat(slice.call(arguments)));
    };
    return bound;
  };

  // Partially apply a function by creating a version that has had some of its
  // arguments pre-filled, without changing its dynamic `this` context. _ acts
  // as a placeholder, allowing any combination of arguments to be pre-filled.
  _.partial = function(func) {
    var boundArgs = slice.call(arguments, 1);
    var bound = function() {
      var position = 0, length = boundArgs.length;
      var args = Array(length);
      for (var i = 0; i < length; i++) {
        args[i] = boundArgs[i] === _ ? arguments[position++] : boundArgs[i];
      }
      while (position < arguments.length) args.push(arguments[position++]);
      return executeBound(func, bound, this, this, args);
    };
    return bound;
  };

  // Bind a number of an object's methods to that object. Remaining arguments
  // are the method names to be bound. Useful for ensuring that all callbacks
  // defined on an object belong to it.
  _.bindAll = function(obj) {
    var i, length = arguments.length, key;
    if (length <= 1) throw new Error('bindAll must be passed function names');
    for (i = 1; i < length; i++) {
      key = arguments[i];
      obj[key] = _.bind(obj[key], obj);
    }
    return obj;
  };

  // Memoize an expensive function by storing its results.
  _.memoize = function(func, hasher) {
    var memoize = function(key) {
      var cache = memoize.cache;
      var address = '' + (hasher ? hasher.apply(this, arguments) : key);
      if (!_.has(cache, address)) cache[address] = func.apply(this, arguments);
      return cache[address];
    };
    memoize.cache = {};
    return memoize;
  };

  // Delays a function for the given number of milliseconds, and then calls
  // it with the arguments supplied.
  _.delay = function(func, wait) {
    var args = slice.call(arguments, 2);
    return setTimeout(function(){
      return func.apply(null, args);
    }, wait);
  };

  // Defers a function, scheduling it to run after the current call stack has
  // cleared.
  _.defer = _.partial(_.delay, _, 1);

  // Returns a function, that, when invoked, will only be triggered at most once
  // during a given window of time. Normally, the throttled function will run
  // as much as it can, without ever going more than once per `wait` duration;
  // but if you'd like to disable the execution on the leading edge, pass
  // `{leading: false}`. To disable execution on the trailing edge, ditto.
  _.throttle = function(func, wait, options) {
    var context, args, result;
    var timeout = null;
    var previous = 0;
    if (!options) options = {};
    var later = function() {
      previous = options.leading === false ? 0 : _.now();
      timeout = null;
      result = func.apply(context, args);
      if (!timeout) context = args = null;
    };
    return function() {
      var now = _.now();
      if (!previous && options.leading === false) previous = now;
      var remaining = wait - (now - previous);
      context = this;
      args = arguments;
      if (remaining <= 0 || remaining > wait) {
        if (timeout) {
          clearTimeout(timeout);
          timeout = null;
        }
        previous = now;
        result = func.apply(context, args);
        if (!timeout) context = args = null;
      } else if (!timeout && options.trailing !== false) {
        timeout = setTimeout(later, remaining);
      }
      return result;
    };
  };

  // Returns a function, that, as long as it continues to be invoked, will not
  // be triggered. The function will be called after it stops being called for
  // N milliseconds. If `immediate` is passed, trigger the function on the
  // leading edge, instead of the trailing.
  _.debounce = function(func, wait, immediate) {
    var timeout, args, context, timestamp, result;

    var later = function() {
      var last = _.now() - timestamp;

      if (last < wait && last >= 0) {
        timeout = setTimeout(later, wait - last);
      } else {
        timeout = null;
        if (!immediate) {
          result = func.apply(context, args);
          if (!timeout) context = args = null;
        }
      }
    };

    return function() {
      context = this;
      args = arguments;
      timestamp = _.now();
      var callNow = immediate && !timeout;
      if (!timeout) timeout = setTimeout(later, wait);
      if (callNow) {
        result = func.apply(context, args);
        context = args = null;
      }

      return result;
    };
  };

  // Returns the first function passed as an argument to the second,
  // allowing you to adjust arguments, run code before and after, and
  // conditionally execute the original function.
  _.wrap = function(func, wrapper) {
    return _.partial(wrapper, func);
  };

  // Returns a negated version of the passed-in predicate.
  _.negate = function(predicate) {
    return function() {
      return !predicate.apply(this, arguments);
    };
  };

  // Returns a function that is the composition of a list of functions, each
  // consuming the return value of the function that follows.
  _.compose = function() {
    var args = arguments;
    var start = args.length - 1;
    return function() {
      var i = start;
      var result = args[start].apply(this, arguments);
      while (i--) result = args[i].call(this, result);
      return result;
    };
  };

  // Returns a function that will only be executed on and after the Nth call.
  _.after = function(times, func) {
    return function() {
      if (--times < 1) {
        return func.apply(this, arguments);
      }
    };
  };

  // Returns a function that will only be executed up to (but not including) the Nth call.
  _.before = function(times, func) {
    var memo;
    return function() {
      if (--times > 0) {
        memo = func.apply(this, arguments);
      }
      if (times <= 1) func = null;
      return memo;
    };
  };

  // Returns a function that will be executed at most one time, no matter how
  // often you call it. Useful for lazy initialization.
  _.once = _.partial(_.before, 2);

  // Object Functions
  // ----------------

  // Keys in IE < 9 that won't be iterated by `for key in ...` and thus missed.
  var hasEnumBug = !{toString: null}.propertyIsEnumerable('toString');
  var nonEnumerableProps = ['valueOf', 'isPrototypeOf', 'toString',
                      'propertyIsEnumerable', 'hasOwnProperty', 'toLocaleString'];

  function collectNonEnumProps(obj, keys) {
    var nonEnumIdx = nonEnumerableProps.length;
    var constructor = obj.constructor;
    var proto = (_.isFunction(constructor) && constructor.prototype) || ObjProto;

    // Constructor is a special case.
    var prop = 'constructor';
    if (_.has(obj, prop) && !_.contains(keys, prop)) keys.push(prop);

    while (nonEnumIdx--) {
      prop = nonEnumerableProps[nonEnumIdx];
      if (prop in obj && obj[prop] !== proto[prop] && !_.contains(keys, prop)) {
        keys.push(prop);
      }
    }
  }

  // Retrieve the names of an object's own properties.
  // Delegates to **ECMAScript 5**'s native `Object.keys`
  _.keys = function(obj) {
    if (!_.isObject(obj)) return [];
    if (nativeKeys) return nativeKeys(obj);
    var keys = [];
    for (var key in obj) if (_.has(obj, key)) keys.push(key);
    // Ahem, IE < 9.
    if (hasEnumBug) collectNonEnumProps(obj, keys);
    return keys;
  };

  // Retrieve all the property names of an object.
  _.allKeys = function(obj) {
    if (!_.isObject(obj)) return [];
    var keys = [];
    for (var key in obj) keys.push(key);
    // Ahem, IE < 9.
    if (hasEnumBug) collectNonEnumProps(obj, keys);
    return keys;
  };

  // Retrieve the values of an object's properties.
  _.values = function(obj) {
    var keys = _.keys(obj);
    var length = keys.length;
    var values = Array(length);
    for (var i = 0; i < length; i++) {
      values[i] = obj[keys[i]];
    }
    return values;
  };

  // Returns the results of applying the iteratee to each element of the object
  // In contrast to _.map it returns an object
  _.mapObject = function(obj, iteratee, context) {
    iteratee = cb(iteratee, context);
    var keys =  _.keys(obj),
          length = keys.length,
          results = {},
          currentKey;
      for (var index = 0; index < length; index++) {
        currentKey = keys[index];
        results[currentKey] = iteratee(obj[currentKey], currentKey, obj);
      }
      return results;
  };

  // Convert an object into a list of `[key, value]` pairs.
  _.pairs = function(obj) {
    var keys = _.keys(obj);
    var length = keys.length;
    var pairs = Array(length);
    for (var i = 0; i < length; i++) {
      pairs[i] = [keys[i], obj[keys[i]]];
    }
    return pairs;
  };

  // Invert the keys and values of an object. The values must be serializable.
  _.invert = function(obj) {
    var result = {};
    var keys = _.keys(obj);
    for (var i = 0, length = keys.length; i < length; i++) {
      result[obj[keys[i]]] = keys[i];
    }
    return result;
  };

  // Return a sorted list of the function names available on the object.
  // Aliased as `methods`
  _.functions = _.methods = function(obj) {
    var names = [];
    for (var key in obj) {
      if (_.isFunction(obj[key])) names.push(key);
    }
    return names.sort();
  };

  // Extend a given object with all the properties in passed-in object(s).
  _.extend = createAssigner(_.allKeys);

  // Assigns a given object with all the own properties in the passed-in object(s)
  // (https://developer.mozilla.org/docs/Web/JavaScript/Reference/Global_Objects/Object/assign)
  _.extendOwn = _.assign = createAssigner(_.keys);

  // Returns the first key on an object that passes a predicate test
  _.findKey = function(obj, predicate, context) {
    predicate = cb(predicate, context);
    var keys = _.keys(obj), key;
    for (var i = 0, length = keys.length; i < length; i++) {
      key = keys[i];
      if (predicate(obj[key], key, obj)) return key;
    }
  };

  // Return a copy of the object only containing the whitelisted properties.
  _.pick = function(object, oiteratee, context) {
    var result = {}, obj = object, iteratee, keys;
    if (obj == null) return result;
    if (_.isFunction(oiteratee)) {
      keys = _.allKeys(obj);
      iteratee = optimizeCb(oiteratee, context);
    } else {
      keys = flatten(arguments, false, false, 1);
      iteratee = function(value, key, obj) { return key in obj; };
      obj = Object(obj);
    }
    for (var i = 0, length = keys.length; i < length; i++) {
      var key = keys[i];
      var value = obj[key];
      if (iteratee(value, key, obj)) result[key] = value;
    }
    return result;
  };

   // Return a copy of the object without the blacklisted properties.
  _.omit = function(obj, iteratee, context) {
    if (_.isFunction(iteratee)) {
      iteratee = _.negate(iteratee);
    } else {
      var keys = _.map(flatten(arguments, false, false, 1), String);
      iteratee = function(value, key) {
        return !_.contains(keys, key);
      };
    }
    return _.pick(obj, iteratee, context);
  };

  // Fill in a given object with default properties.
  _.defaults = createAssigner(_.allKeys, true);

  // Creates an object that inherits from the given prototype object.
  // If additional properties are provided then they will be added to the
  // created object.
  _.create = function(prototype, props) {
    var result = baseCreate(prototype);
    if (props) _.extendOwn(result, props);
    return result;
  };

  // Create a (shallow-cloned) duplicate of an object.
  _.clone = function(obj) {
    if (!_.isObject(obj)) return obj;
    return _.isArray(obj) ? obj.slice() : _.extend({}, obj);
  };

  // Invokes interceptor with the obj, and then returns obj.
  // The primary purpose of this method is to "tap into" a method chain, in
  // order to perform operations on intermediate results within the chain.
  _.tap = function(obj, interceptor) {
    interceptor(obj);
    return obj;
  };

  // Returns whether an object has a given set of `key:value` pairs.
  _.isMatch = function(object, attrs) {
    var keys = _.keys(attrs), length = keys.length;
    if (object == null) return !length;
    var obj = Object(object);
    for (var i = 0; i < length; i++) {
      var key = keys[i];
      if (attrs[key] !== obj[key] || !(key in obj)) return false;
    }
    return true;
  };


  // Internal recursive comparison function for `isEqual`.
  var eq = function(a, b, aStack, bStack) {
    // Identical objects are equal. `0 === -0`, but they aren't identical.
    // See the [Harmony `egal` proposal](http://wiki.ecmascript.org/doku.php?id=harmony:egal).
    if (a === b) return a !== 0 || 1 / a === 1 / b;
    // A strict comparison is necessary because `null == undefined`.
    if (a == null || b == null) return a === b;
    // Unwrap any wrapped objects.
    if (a instanceof _) a = a._wrapped;
    if (b instanceof _) b = b._wrapped;
    // Compare `[[Class]]` names.
    var className = toString.call(a);
    if (className !== toString.call(b)) return false;
    switch (className) {
      // Strings, numbers, regular expressions, dates, and booleans are compared by value.
      case '[object RegExp]':
      // RegExps are coerced to strings for comparison (Note: '' + /a/i === '/a/i')
      case '[object String]':
        // Primitives and their corresponding object wrappers are equivalent; thus, `"5"` is
        // equivalent to `new String("5")`.
        return '' + a === '' + b;
      case '[object Number]':
        // `NaN`s are equivalent, but non-reflexive.
        // Object(NaN) is equivalent to NaN
        if (+a !== +a) return +b !== +b;
        // An `egal` comparison is performed for other numeric values.
        return +a === 0 ? 1 / +a === 1 / b : +a === +b;
      case '[object Date]':
      case '[object Boolean]':
        // Coerce dates and booleans to numeric primitive values. Dates are compared by their
        // millisecond representations. Note that invalid dates with millisecond representations
        // of `NaN` are not equivalent.
        return +a === +b;
    }

    var areArrays = className === '[object Array]';
    if (!areArrays) {
      if (typeof a != 'object' || typeof b != 'object') return false;

      // Objects with different constructors are not equivalent, but `Object`s or `Array`s
      // from different frames are.
      var aCtor = a.constructor, bCtor = b.constructor;
      if (aCtor !== bCtor && !(_.isFunction(aCtor) && aCtor instanceof aCtor &&
                               _.isFunction(bCtor) && bCtor instanceof bCtor)
                          && ('constructor' in a && 'constructor' in b)) {
        return false;
      }
    }
    // Assume equality for cyclic structures. The algorithm for detecting cyclic
    // structures is adapted from ES 5.1 section 15.12.3, abstract operation `JO`.

    // Initializing stack of traversed objects.
    // It's done here since we only need them for objects and arrays comparison.
    aStack = aStack || [];
    bStack = bStack || [];
    var length = aStack.length;
    while (length--) {
      // Linear search. Performance is inversely proportional to the number of
      // unique nested structures.
      if (aStack[length] === a) return bStack[length] === b;
    }

    // Add the first object to the stack of traversed objects.
    aStack.push(a);
    bStack.push(b);

    // Recursively compare objects and arrays.
    if (areArrays) {
      // Compare array lengths to determine if a deep comparison is necessary.
      length = a.length;
      if (length !== b.length) return false;
      // Deep compare the contents, ignoring non-numeric properties.
      while (length--) {
        if (!eq(a[length], b[length], aStack, bStack)) return false;
      }
    } else {
      // Deep compare objects.
      var keys = _.keys(a), key;
      length = keys.length;
      // Ensure that both objects contain the same number of properties before comparing deep equality.
      if (_.keys(b).length !== length) return false;
      while (length--) {
        // Deep compare each member
        key = keys[length];
        if (!(_.has(b, key) && eq(a[key], b[key], aStack, bStack))) return false;
      }
    }
    // Remove the first object from the stack of traversed objects.
    aStack.pop();
    bStack.pop();
    return true;
  };

  // Perform a deep comparison to check if two objects are equal.
  _.isEqual = function(a, b) {
    return eq(a, b);
  };

  // Is a given array, string, or object empty?
  // An "empty" object has no enumerable own-properties.
  _.isEmpty = function(obj) {
    if (obj == null) return true;
    if (isArrayLike(obj) && (_.isArray(obj) || _.isString(obj) || _.isArguments(obj))) return obj.length === 0;
    return _.keys(obj).length === 0;
  };

  // Is a given value a DOM element?
  _.isElement = function(obj) {
    return !!(obj && obj.nodeType === 1);
  };

  // Is a given value an array?
  // Delegates to ECMA5's native Array.isArray
  _.isArray = nativeIsArray || function(obj) {
    return toString.call(obj) === '[object Array]';
  };

  // Is a given variable an object?
  _.isObject = function(obj) {
    var type = typeof obj;
    return type === 'function' || type === 'object' && !!obj;
  };

  // Add some isType methods: isArguments, isFunction, isString, isNumber, isDate, isRegExp, isError.
  _.each(['Arguments', 'Function', 'String', 'Number', 'Date', 'RegExp', 'Error'], function(name) {
    _['is' + name] = function(obj) {
      return toString.call(obj) === '[object ' + name + ']';
    };
  });

  // Define a fallback version of the method in browsers (ahem, IE < 9), where
  // there isn't any inspectable "Arguments" type.
  if (!_.isArguments(arguments)) {
    _.isArguments = function(obj) {
      return _.has(obj, 'callee');
    };
  }

  // Optimize `isFunction` if appropriate. Work around some typeof bugs in old v8,
  // IE 11 (#1621), and in Safari 8 (#1929).
  if (typeof /./ != 'function' && typeof Int8Array != 'object') {
    _.isFunction = function(obj) {
      return typeof obj == 'function' || false;
    };
  }

  // Is a given object a finite number?
  _.isFinite = function(obj) {
    return isFinite(obj) && !isNaN(parseFloat(obj));
  };

  // Is the given value `NaN`? (NaN is the only number which does not equal itself).
  _.isNaN = function(obj) {
    return _.isNumber(obj) && obj !== +obj;
  };

  // Is a given value a boolean?
  _.isBoolean = function(obj) {
    return obj === true || obj === false || toString.call(obj) === '[object Boolean]';
  };

  // Is a given value equal to null?
  _.isNull = function(obj) {
    return obj === null;
  };

  // Is a given variable undefined?
  _.isUndefined = function(obj) {
    return obj === void 0;
  };

  // Shortcut function for checking if an object has a given property directly
  // on itself (in other words, not on a prototype).
  _.has = function(obj, key) {
    return obj != null && hasOwnProperty.call(obj, key);
  };

  // Utility Functions
  // -----------------

  // Run Underscore.js in *noConflict* mode, returning the `_` variable to its
  // previous owner. Returns a reference to the Underscore object.
  _.noConflict = function() {
    root._ = previousUnderscore;
    return this;
  };

  // Keep the identity function around for default iteratees.
  _.identity = function(value) {
    return value;
  };

  // Predicate-generating functions. Often useful outside of Underscore.
  _.constant = function(value) {
    return function() {
      return value;
    };
  };

  _.noop = function(){};

  _.property = property;

  // Generates a function for a given object that returns a given property.
  _.propertyOf = function(obj) {
    return obj == null ? function(){} : function(key) {
      return obj[key];
    };
  };

  // Returns a predicate for checking whether an object has a given set of
  // `key:value` pairs.
  _.matcher = _.matches = function(attrs) {
    attrs = _.extendOwn({}, attrs);
    return function(obj) {
      return _.isMatch(obj, attrs);
    };
  };

  // Run a function **n** times.
  _.times = function(n, iteratee, context) {
    var accum = Array(Math.max(0, n));
    iteratee = optimizeCb(iteratee, context, 1);
    for (var i = 0; i < n; i++) accum[i] = iteratee(i);
    return accum;
  };

  // Return a random integer between min and max (inclusive).
  _.random = function(min, max) {
    if (max == null) {
      max = min;
      min = 0;
    }
    return min + Math.floor(Math.random() * (max - min + 1));
  };

  // A (possibly faster) way to get the current timestamp as an integer.
  _.now = Date.now || function() {
    return new Date().getTime();
  };

   // List of HTML entities for escaping.
  var escapeMap = {
    '&': '&amp;',
    '<': '&lt;',
    '>': '&gt;',
    '"': '&quot;',
    "'": '&#x27;',
    '`': '&#x60;'
  };
  var unescapeMap = _.invert(escapeMap);

  // Functions for escaping and unescaping strings to/from HTML interpolation.
  var createEscaper = function(map) {
    var escaper = function(match) {
      return map[match];
    };
    // Regexes for identifying a key that needs to be escaped
    var source = '(?:' + _.keys(map).join('|') + ')';
    var testRegexp = RegExp(source);
    var replaceRegexp = RegExp(source, 'g');
    return function(string) {
      string = string == null ? '' : '' + string;
      return testRegexp.test(string) ? string.replace(replaceRegexp, escaper) : string;
    };
  };
  _.escape = createEscaper(escapeMap);
  _.unescape = createEscaper(unescapeMap);

  // If the value of the named `property` is a function then invoke it with the
  // `object` as context; otherwise, return it.
  _.result = function(object, property, fallback) {
    var value = object == null ? void 0 : object[property];
    if (value === void 0) {
      value = fallback;
    }
    return _.isFunction(value) ? value.call(object) : value;
  };

  // Generate a unique integer id (unique within the entire client session).
  // Useful for temporary DOM ids.
  var idCounter = 0;
  _.uniqueId = function(prefix) {
    var id = ++idCounter + '';
    return prefix ? prefix + id : id;
  };

  // By default, Underscore uses ERB-style template delimiters, change the
  // following template settings to use alternative delimiters.
  _.templateSettings = {
    evaluate    : /<%([\s\S]+?)%>/g,
    interpolate : /<%=([\s\S]+?)%>/g,
    escape      : /<%-([\s\S]+?)%>/g
  };

  // When customizing `templateSettings`, if you don't want to define an
  // interpolation, evaluation or escaping regex, we need one that is
  // guaranteed not to match.
  var noMatch = /(.)^/;

  // Certain characters need to be escaped so that they can be put into a
  // string literal.
  var escapes = {
    "'":      "'",
    '\\':     '\\',
    '\r':     'r',
    '\n':     'n',
    '\u2028': 'u2028',
    '\u2029': 'u2029'
  };

  var escaper = /\\|'|\r|\n|\u2028|\u2029/g;

  var escapeChar = function(match) {
    return '\\' + escapes[match];
  };

  // JavaScript micro-templating, similar to John Resig's implementation.
  // Underscore templating handles arbitrary delimiters, preserves whitespace,
  // and correctly escapes quotes within interpolated code.
  // NB: `oldSettings` only exists for backwards compatibility.
  _.template = function(text, settings, oldSettings) {
    if (!settings && oldSettings) settings = oldSettings;
    settings = _.defaults({}, settings, _.templateSettings);

    // Combine delimiters into one regular expression via alternation.
    var matcher = RegExp([
      (settings.escape || noMatch).source,
      (settings.interpolate || noMatch).source,
      (settings.evaluate || noMatch).source
    ].join('|') + '|$', 'g');

    // Compile the template source, escaping string literals appropriately.
    var index = 0;
    var source = "__p+='";
    text.replace(matcher, function(match, escape, interpolate, evaluate, offset) {
      source += text.slice(index, offset).replace(escaper, escapeChar);
      index = offset + match.length;

      if (escape) {
        source += "'+\n((__t=(" + escape + "))==null?'':_.escape(__t))+\n'";
      } else if (interpolate) {
        source += "'+\n((__t=(" + interpolate + "))==null?'':__t)+\n'";
      } else if (evaluate) {
        source += "';\n" + evaluate + "\n__p+='";
      }

      // Adobe VMs need the match returned to produce the correct offest.
      return match;
    });
    source += "';\n";

    // If a variable is not specified, place data values in local scope.
    if (!settings.variable) source = 'with(obj||{}){\n' + source + '}\n';

    source = "var __t,__p='',__j=Array.prototype.join," +
      "print=function(){__p+=__j.call(arguments,'');};\n" +
      source + 'return __p;\n';

    try {
      var render = new Function(settings.variable || 'obj', '_', source);
    } catch (e) {
      e.source = source;
      throw e;
    }

    var template = function(data) {
      return render.call(this, data, _);
    };

    // Provide the compiled source as a convenience for precompilation.
    var argument = settings.variable || 'obj';
    template.source = 'function(' + argument + '){\n' + source + '}';

    return template;
  };

  // Add a "chain" function. Start chaining a wrapped Underscore object.
  _.chain = function(obj) {
    var instance = _(obj);
    instance._chain = true;
    return instance;
  };

  // OOP
  // ---------------
  // If Underscore is called as a function, it returns a wrapped object that
  // can be used OO-style. This wrapper holds altered versions of all the
  // underscore functions. Wrapped objects may be chained.

  // Helper function to continue chaining intermediate results.
  var result = function(instance, obj) {
    return instance._chain ? _(obj).chain() : obj;
  };

  // Add your own custom functions to the Underscore object.
  _.mixin = function(obj) {
    _.each(_.functions(obj), function(name) {
      var func = _[name] = obj[name];
      _.prototype[name] = function() {
        var args = [this._wrapped];
        push.apply(args, arguments);
        return result(this, func.apply(_, args));
      };
    });
  };

  // Add all of the Underscore functions to the wrapper object.
  _.mixin(_);

  // Add all mutator Array functions to the wrapper.
  _.each(['pop', 'push', 'reverse', 'shift', 'sort', 'splice', 'unshift'], function(name) {
    var method = ArrayProto[name];
    _.prototype[name] = function() {
      var obj = this._wrapped;
      method.apply(obj, arguments);
      if ((name === 'shift' || name === 'splice') && obj.length === 0) delete obj[0];
      return result(this, obj);
    };
  });

  // Add all accessor Array functions to the wrapper.
  _.each(['concat', 'join', 'slice'], function(name) {
    var method = ArrayProto[name];
    _.prototype[name] = function() {
      return result(this, method.apply(this._wrapped, arguments));
    };
  });

  // Extracts the result from a wrapped and chained object.
  _.prototype.value = function() {
    return this._wrapped;
  };

  // Provide unwrapping proxy for some methods used in engine operations
  // such as arithmetic and JSON stringification.
  _.prototype.valueOf = _.prototype.toJSON = _.prototype.value;

  _.prototype.toString = function() {
    return '' + this._wrapped;
  };

  // AMD registration happens at the end for compatibility with AMD loaders
  // that may not enforce next-turn semantics on modules. Even though general
  // practice for AMD registration is to be anonymous, underscore registers
  // as a named module because, like jQuery, it is a base library that is
  // popular enough to be bundled in a third party lib, but not be part of
  // an AMD load request. Those cases could generate an error when an
  // anonymous define() is called outside of a loader request.
  if (typeof define === 'function' && define.amd) {
    define('underscore', [], function() {
      return _;
    });
  }
}.call(this));
});

/*eslint camelcase:0 */
/*jshint esnext:true, -W051:true */
/*eslint-disable */
global.DEFINE_MODULE('internal', (function () {
'use strict';
/*eslint-enable */

const exports = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
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
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoError
////////////////////////////////////////////////////////////////////////////////

if (global.ArangoError) {
  exports.ArangoError = global.ArangoError;
  delete global.ArangoError;
}
else {
  exports.ArangoError = function (error) {
    if (error !== undefined) {
      this.error = error.error;
      this.code = error.code;
      this.errorNum = error.errorNum;
      this.errorMessage = error.errorMessage;
    }

    this.message = this.toString();
  };

  exports.ArangoError.prototype = new Error();
}

exports.ArangoError.prototype._PRINT = function (context) {
  context.output += this.toString();
};

exports.ArangoError.prototype.toString = function() {
  var errorNum = this.errorNum;
  var errorMessage = this.errorMessage || this.message;

  return '[ArangoError ' + errorNum + ': ' + errorMessage + ']';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief SleepAndRequeue
////////////////////////////////////////////////////////////////////////////////

if (global.SleepAndRequeue) {
  exports.SleepAndRequeue = global.SleepAndRequeue;
  delete global.SleepAndRequeue;

  exports.SleepAndRequeue.prototype._PRINT = function (context) {
    context.output += this.toString();
  };

  exports.SleepAndRequeue.prototype.toString = function() {
    return '[SleepAndRequeue sleep: ' + this.sleep + ']';
};

}
// -----------------------------------------------------------------------------
// --SECTION--                                                  public constants
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief threadNumber
////////////////////////////////////////////////////////////////////////////////

exports.threadNumber = 0;

if (global.THREAD_NUMBER) {
  exports.threadNumber = global.THREAD_NUMBER;
  delete global.THREAD_NUMBER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief developmentMode. this is only here for backwards compatibility
////////////////////////////////////////////////////////////////////////////////

exports.developmentMode = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief logfilePath
////////////////////////////////////////////////////////////////////////////////

if (global.LOGFILE_PATH) {
  exports.logfilePath = global.LOGFILE_PATH;
  delete global.LOGFILE_PATH;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quiet
////////////////////////////////////////////////////////////////////////////////

exports.quiet = false;

if (global.ARANGO_QUIET) {
  exports.quiet = global.ARANGO_QUIET;
  delete global.ARANGO_QUIET;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief valgrind
////////////////////////////////////////////////////////////////////////////////

exports.valgrind = false;

if (global.VALGRIND) {
  exports.valgrind = global.VALGRIND;
  delete global.VALGRIND;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief valgrind
////////////////////////////////////////////////////////////////////////////////

exports.coverage = false;

if (global.COVERAGE) {
  exports.coverage = global.COVERAGE;
  delete global.COVERAGE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief version
////////////////////////////////////////////////////////////////////////////////

exports.version = 'unknown';

if (global.VERSION) {
  exports.version = global.VERSION;
  delete global.VERSION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief platform
////////////////////////////////////////////////////////////////////////////////

exports.platform = 'unknown';

if (global.SYS_PLATFORM) {
  exports.platform = global.SYS_PLATFORM;
  delete global.SYS_PLATFORM;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesSentDistribution
////////////////////////////////////////////////////////////////////////////////

exports.bytesSentDistribution = [];

if (global.BYTES_SENT_DISTRIBUTION) {
  exports.bytesSentDistribution = global.BYTES_SENT_DISTRIBUTION;
  delete global.BYTES_SENT_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief bytesReceivedDistribution
////////////////////////////////////////////////////////////////////////////////

exports.bytesReceivedDistribution = [];

if (global.BYTES_RECEIVED_DISTRIBUTION) {
  exports.bytesReceivedDistribution = global.BYTES_RECEIVED_DISTRIBUTION;
  delete global.BYTES_RECEIVED_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief connectionTimeDistribution
////////////////////////////////////////////////////////////////////////////////

exports.connectionTimeDistribution = [];

if (global.CONNECTION_TIME_DISTRIBUTION) {
  exports.connectionTimeDistribution = global.CONNECTION_TIME_DISTRIBUTION;
  delete global.CONNECTION_TIME_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief requestTimeDistribution
////////////////////////////////////////////////////////////////////////////////

exports.requestTimeDistribution = [];

if (global.REQUEST_TIME_DISTRIBUTION) {
  exports.requestTimeDistribution = global.REQUEST_TIME_DISTRIBUTION;
  delete global.REQUEST_TIME_DISTRIBUTION;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startupPath
////////////////////////////////////////////////////////////////////////////////

exports.startupPath = '';

if (global.STARTUP_PATH) {
  exports.startupPath = global.STARTUP_PATH;
  delete global.STARTUP_PATH;
}

if (exports.startupPath === '') {
  exports.startupPath = '.';
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief configureEndpoint
////////////////////////////////////////////////////////////////////////////////

if (global.CONFIGURE_ENDPOINT) {
  exports.configureEndpoint = global.CONFIGURE_ENDPOINT;
  delete global.CONFIGURE_ENDPOINT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removeEndpoint
////////////////////////////////////////////////////////////////////////////////

if (global.REMOVE_ENDPOINT) {
  exports.removeEndpoint = global.REMOVE_ENDPOINT;
  delete global.REMOVE_ENDPOINT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief listEndpoints
////////////////////////////////////////////////////////////////////////////////

if (global.LIST_ENDPOINTS) {
  exports.listEndpoints = global.LIST_ENDPOINTS;
  delete global.LIST_ENDPOINTS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Decode
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_BASE64DECODE) {
  exports.base64Decode = global.SYS_BASE64DECODE;
  delete global.SYS_BASE64DECODE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief base64Encode
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_BASE64ENCODE) {
  exports.base64Encode = global.SYS_BASE64ENCODE;
  delete global.SYS_BASE64ENCODE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugSegfault
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_SEGFAULT) {
  exports.debugSegfault = global.SYS_DEBUG_SEGFAULT;
  delete global.SYS_DEBUG_SEGFAULT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugSetFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_SET_FAILAT) {
  exports.debugSetFailAt = global.SYS_DEBUG_SET_FAILAT;
  delete global.SYS_DEBUG_SET_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugRemoveFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_REMOVE_FAILAT) {
  exports.debugRemoveFailAt = global.SYS_DEBUG_REMOVE_FAILAT;
  delete global.SYS_DEBUG_REMOVE_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugClearFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_CLEAR_FAILAT) {
  exports.debugClearFailAt = global.SYS_DEBUG_CLEAR_FAILAT;
  delete global.SYS_DEBUG_CLEAR_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief debugCanUseFailAt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DEBUG_CAN_USE_FAILAT) {
  exports.debugCanUseFailAt = global.SYS_DEBUG_CAN_USE_FAILAT;
  delete global.SYS_DEBUG_CAN_USE_FAILAT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief download
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_DOWNLOAD) {
  exports.download = global.SYS_DOWNLOAD;
  delete global.SYS_DOWNLOAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executeScript
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_EXECUTE) {
  exports.executeScript = global.SYS_EXECUTE;
  delete global.SYS_EXECUTE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getCurrentRequest
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GET_CURRENT_REQUEST) {
  exports.getCurrentRequest = global.SYS_GET_CURRENT_REQUEST;
  delete global.SYS_GET_CURRENT_REQUEST;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getCurrentResponse
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GET_CURRENT_RESPONSE) {
  exports.getCurrentResponse = global.SYS_GET_CURRENT_RESPONSE;
  delete global.SYS_GET_CURRENT_RESPONSE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extend
////////////////////////////////////////////////////////////////////////////////

exports.extend = function (target, source) {

  Object.getOwnPropertyNames(source)
  .forEach(function(propName) {
    Object.defineProperty(
      target,
      propName,
      Object.getOwnPropertyDescriptor(source, propName)
    );
  });

  return target;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief load
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_LOAD) {
  exports.load = global.SYS_LOAD;
  delete global.SYS_LOAD;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief logLevel
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_LOG_LEVEL) {
  exports.logLevel = global.SYS_LOG_LEVEL;
  delete global.SYS_LOG_LEVEL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief md5
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_MD5) {
  exports.md5 = global.SYS_MD5;
  delete global.SYS_MD5;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomNumbers
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GEN_RANDOM_NUMBERS) {
  exports.genRandomNumbers = global.SYS_GEN_RANDOM_NUMBERS;
  delete global.SYS_GEN_RANDOM_NUMBERS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomAlphaNumbers
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GEN_RANDOM_ALPHA_NUMBERS) {
  exports.genRandomAlphaNumbers = global.SYS_GEN_RANDOM_ALPHA_NUMBERS;
  delete global.SYS_GEN_RANDOM_ALPHA_NUMBERS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief genRandomSalt
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GEN_RANDOM_SALT) {
  exports.genRandomSalt = global.SYS_GEN_RANDOM_SALT;
  delete global.SYS_GEN_RANDOM_SALT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief hmac
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_HMAC) {
  exports.hmac = global.SYS_HMAC;
  delete global.SYS_HMAC;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief pbkdf2-hmac
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PBKDF2) {
  exports.pbkdf2 = global.SYS_PBKDF2;
  delete global.SYS_PBKDF2;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief createNonce
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_CREATE_NONCE) {
  exports.createNonce = global.SYS_CREATE_NONCE;
  delete global.SYS_CREATE_NONCE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checkAndMarkNonce
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_CHECK_AND_MARK_NONCE) {
  exports.checkAndMarkNonce = global.SYS_CHECK_AND_MARK_NONCE;
  delete global.SYS_CHECK_AND_MARK_NONCE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief output
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_OUTPUT) {
  exports.stdOutput = global.SYS_OUTPUT;
  exports.output = exports.stdOutput;
  delete global.SYS_OUTPUT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PARSE) {
  exports.parse = global.SYS_PARSE;
  delete global.SYS_PARSE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parseFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PARSE_FILE) {
  exports.parseFile = global.SYS_PARSE_FILE;
  delete global.SYS_PARSE_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PROCESS_STATISTICS) {
  exports.processStatistics = global.SYS_PROCESS_STATISTICS;
  delete global.SYS_PROCESS_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief rand
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_RAND) {
  exports.rand = global.SYS_RAND;
  delete global.SYS_RAND;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha512
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA512) {
  exports.sha512 = global.SYS_SHA512;
  delete global.SYS_SHA512;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha384
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA384) {
  exports.sha384 = global.SYS_SHA384;
  delete global.SYS_SHA384;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha256
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA256) {
  exports.sha256 = global.SYS_SHA256;
  delete global.SYS_SHA256;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha224
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA224) {
  exports.sha224 = global.SYS_SHA224;
  delete global.SYS_SHA224;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sha1
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SHA1) {
  exports.sha1 = global.SYS_SHA1;
  delete global.SYS_SHA1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief serverStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SERVER_STATISTICS) {
  exports.serverStatistics = global.SYS_SERVER_STATISTICS;
  delete global.SYS_SERVER_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sleep
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SLEEP) {
  exports.sleep = global.SYS_SLEEP;
  delete global.SYS_SLEEP;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_TIME) {
  exports.time = global.SYS_TIME;
  delete global.SYS_TIME;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_WAIT) {
  exports.wait = global.SYS_WAIT;
  delete global.SYS_WAIT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief importCsvFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_IMPORT_CSV_FILE) {
  exports.importCsvFile = global.SYS_IMPORT_CSV_FILE;
  delete global.SYS_IMPORT_CSV_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief importJsonFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_IMPORT_JSON_FILE) {
  exports.importJsonFile = global.SYS_IMPORT_JSON_FILE;
  delete global.SYS_IMPORT_JSON_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processCsvFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PROCESS_CSV_FILE) {
  exports.processCsvFile = global.SYS_PROCESS_CSV_FILE;
  delete global.SYS_PROCESS_CSV_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief processJsonFile
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_PROCESS_JSON_FILE) {
  exports.processJsonFile = global.SYS_PROCESS_JSON_FILE;
  delete global.SYS_PROCESS_JSON_FILE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clientStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_CLIENT_STATISTICS) {
  exports.clientStatistics = global.SYS_CLIENT_STATISTICS;
  delete global.SYS_CLIENT_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief httpStatistics
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_HTTP_STATISTICS) {
  exports.httpStatistics = global.SYS_HTTP_STATISTICS;
  delete global.SYS_HTTP_STATISTICS;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executeExternal
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_EXECUTE_EXTERNAL) {
  exports.executeExternal = global.SYS_EXECUTE_EXTERNAL;
  delete global.SYS_EXECUTE_EXTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executeExternalAndWait - instantly waits for the exit, returns
/// joint result.
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_EXECUTE_EXTERNAL_AND_WAIT) {
  exports.executeExternalAndWait = global.SYS_EXECUTE_EXTERNAL_AND_WAIT;
  delete global.SYS_EXECUTE_EXTERNAL_AND_WAIT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief killExternal
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_KILL_EXTERNAL) {
  exports.killExternal = global.SYS_KILL_EXTERNAL;
  delete global.SYS_KILL_EXTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief statusExternal
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_STATUS_EXTERNAL) {
  exports.statusExternal = global.SYS_STATUS_EXTERNAL;
  delete global.SYS_STATUS_EXTERNAL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief registerTask
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_REGISTER_TASK) {
  exports.registerTask = global.SYS_REGISTER_TASK;
  delete global.SYS_REGISTER_TASK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unregisterTask
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_UNREGISTER_TASK) {
  exports.unregisterTask = global.SYS_UNREGISTER_TASK;
  delete global.SYS_UNREGISTER_TASK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getTasks
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GET_TASK) {
  exports.getTask = global.SYS_GET_TASK;
  delete global.SYS_GET_TASK;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief testPort
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_TEST_PORT) {
  exports.testPort = global.SYS_TEST_PORT;
  delete global.SYS_TEST_PORT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief isIP
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_IS_IP) {
  exports.isIP = global.SYS_IS_IP;
  delete global.SYS_IS_IP;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief unitTests
////////////////////////////////////////////////////////////////////////////////

exports.unitTests = function () {
  return global.SYS_UNIT_TESTS;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief setUnitTestsResult
////////////////////////////////////////////////////////////////////////////////

exports.setUnitTestsResult = function (value) {
  global.SYS_UNIT_TESTS_RESULT = value;
};

// -----------------------------------------------------------------------------
// --SECTION--                                     Commandline argument handling
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief structured to flat commandline arguments
/// @param longOptsEqual whether long-options are in the type --opt=value
///                      or --opt value
////////////////////////////////////////////////////////////////////////////////

exports.toArgv = function (structure, longOptsEqual) {
  if (typeof longOptsEqual === 'undefined') {
    longOptsEqual = false;
  }
  var vec = [];
  for (let key in structure) {
    if (structure.hasOwnProperty(key)) {
      if (key === 'commandSwitches') {
        let multivec = '';
        for (let i = 0; i < structure[key].length; i++) {
          if (structure[key][i].length > 1) {
            vec.push(structure[key][i]);
          }
          else {
            multivec += structure[key][i];
          }
        }
        if (multivec.length > 0) {
          vec.push(multivec);
        }
      }
      else if (key === 'flatCommands') {
        vec = vec.concat(structure[key]);
      }
      else {
        if (longOptsEqual) {
          vec.push('--' + key + '=' + structure[key]);
        }
        else {
          vec.push('--' + key);
          if (structure[key] !== false) {
            if (structure[key] !== true) {
              vec.push(structure[key]);
            }
            else {
              vec.push('true');
            }
          }
          else {
            vec.push('false');
          }
        }
      }
    }
  }
  return vec;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief argv to structured
////////////////////////////////////////////////////////////////////////////////

exports.parseArgv = function (argv, startOffset) {
  var i;
  function setOption(ret, option, value) {
    if (option.indexOf(':') > 0) {
      var n = option.indexOf(':');
      var topOption = option.slice(0, n);
      if (!ret.hasOwnProperty(topOption)) {
        ret[topOption] = {};
      }
      setOption(ret[topOption], option.slice(n + 1, option.length), value);
    }
    else if (argv[i + 1] === 'true') {
      ret[option] = true;
    }
    else if (argv[i + 1] === 'false') {
      ret[option] = false;
    }
    else if (!isNaN(argv[i + 1])) {
      ret[option] = parseInt(argv[i + 1]);
    }
    else {
      ret[option] = argv[i + 1];
    }
  }
  function setSwitch(ret, option) {
    if (!ret.hasOwnProperty('commandSwitches')) {
      ret.commandSwitches = [];
    }
    ret.commandSwitches.push(option);
  }

  function setSwitchVec(ret, option) {
    for (var i = 0; i < option.length; i++) {
      setSwitch(ret, option[i]);
    }
  }

  function setFlatCommand(ret, thisString) {
    if (!ret.hasOwnProperty('flatCommands')) {
      ret.flatCommands = [];
    }
    ret.flatCommands.push(thisString);
  }

  var inFlat = false;
  var ret = {};
  for (i = startOffset; i < argv.length; i++) {
    let thisString = argv[i];
    if (!inFlat) {
      if ((thisString.length > 2) &&
          (thisString.slice(0, 2) === '--')) {
        let option = thisString.slice(2, thisString.length);
        if ((argv.length > i) &&
            (argv[i + 1].slice(0, 1) !== '-')) {
          setOption(ret, option, argv[i + 1]);
          i++;
        }
        else {
          setSwitch(ret, option);
        }
      }
      else if (thisString === '--') {
        inFlat = true;
      }
      else if ((thisString.length > 1) &&
              (thisString.slice(0, 1) === '-')) {
        setSwitchVec(ret, thisString.slice(1, thisString.length));
      }
      else {
        setFlatCommand(ret, thisString);
      }
    }
    else {
      setFlatCommand(ret, thisString);
    }
  }
  return ret;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                          PRINTING
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                         public printing variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief COLORS
////////////////////////////////////////////////////////////////////////////////

exports.COLORS = {};

if (global.COLORS) {
  exports.COLORS = global.COLORS;
  delete global.COLORS;
}
else {
  [ 'COLOR_RED', 'COLOR_BOLD_RED', 'COLOR_GREEN', 'COLOR_BOLD_GREEN',
    'COLOR_BLUE', 'COLOR_BOLD_BLUE', 'COLOR_YELLOW', 'COLOR_BOLD_YELLOW',
    'COLOR_WHITE', 'COLOR_BOLD_WHITE', 'COLOR_CYAN', 'COLOR_BOLD_CYAN',
    'COLOR_MAGENTA', 'COLOR_BOLD_MAGENTA', 'COLOR_BLACK', 'COLOR_BOLD_BLACK',
    'COLOR_BLINK', 'COLOR_BRIGHT', 'COLOR_RESET' ].forEach(function(color) {
      exports.COLORS[color] = '';
    });
}

exports.COLORS.COLOR_PUNCTUATION = exports.COLORS.COLOR_RESET;
exports.COLORS.COLOR_STRING = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_NUMBER = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_INDEX = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_TRUE = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_FALSE = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_NULL = exports.COLORS.COLOR_BRIGHT;
exports.COLORS.COLOR_UNDEFINED = exports.COLORS.COLOR_BRIGHT;

// -----------------------------------------------------------------------------
// --SECTION--                                        private printing variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief quote cache
////////////////////////////////////////////////////////////////////////////////

var characterQuoteCache = {
  '\b': '\\b', // ASCII 8, Backspace
  '\t': '\\t', // ASCII 9, Tab
  '\n': '\\n', // ASCII 10, Newline
  '\f': '\\f', // ASCII 12, Formfeed
  '\r': '\\r', // ASCII 13, Carriage Return
  '\"': '\\"',
  '\\': '\\\\'
};

////////////////////////////////////////////////////////////////////////////////
/// @brief colors
////////////////////////////////////////////////////////////////////////////////

var colors = exports.COLORS;

////////////////////////////////////////////////////////////////////////////////
/// @brief useColor
////////////////////////////////////////////////////////////////////////////////

var useColor = false;

if (global.COLOR_OUTPUT) {
  useColor = global.COLOR_OUTPUT;
  delete global.COLOR_OUTPUT;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief usePrettyPrint
////////////////////////////////////////////////////////////////////////////////

var usePrettyPrint = false;

if (global.PRETTY_PRINT) {
  usePrettyPrint = global.PRETTY_PRINT;
  delete global.PRETTY_PRINT;
}

// -----------------------------------------------------------------------------
// --SECTION--                                        private printing functions
// -----------------------------------------------------------------------------

var printRecursive;

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a single character
////////////////////////////////////////////////////////////////////////////////

function quoteSingleJsonCharacter (c) {

  if (characterQuoteCache.hasOwnProperty(c)) {
    return characterQuoteCache[c];
  }

  var charCode = c.charCodeAt(0);
  var result;

  if (charCode < 16) {
    result = '\\u000';
  }
  else if (charCode < 256) {
    result = '\\u00';
  }
  else if (charCode < 4096) {
    result = '\\u0';
  }
  else {
    result = '\\u';
  }

  result += charCode.toString(16);
  characterQuoteCache[c] = result;

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quotes a string character
////////////////////////////////////////////////////////////////////////////////

var quotable = /[\\\"\x00-\x1f]/g;

function quoteJsonString (str) {

  return '"' + str.replace(quotable, quoteSingleJsonCharacter) + '"';
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the ident for pretty printing
////////////////////////////////////////////////////////////////////////////////

function printIndent (context) {

  var j;
  var indent = '';

  if (context.prettyPrint) {
    indent += '\n';

    for (j = 0; j < context.level; ++j) {
      indent += '  ';
    }
  }

  context.output += indent;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints the JSON representation of an array
////////////////////////////////////////////////////////////////////////////////

function printArray (object, context) {

  var useColor = context.useColor;

  if (object.length === 0) {
    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += '[ ]';

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }
  }
  else {
    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += '[';

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    var newLevel = context.level + 1;
    var sep = ' ';

    context.level = newLevel;

    for (let i = 0; i < object.length; i++) {
      if (useColor) {
        context.output += colors.COLOR_PUNCTUATION;
      }

      context.output += sep;

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }

      printIndent(context);

      var path = context.path;
      context.path += '[' + i + ']';

      printRecursive(object[i], context);

      if (context.emit && context.output.length >= context.emit) {
        exports.output(context.output);
        context.output = '';
      }

      context.path = path;
      sep = ', ';
    }

    context.level = newLevel - 1;
    context.output += ' ';

    printIndent(context);

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += ']';

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints an object
////////////////////////////////////////////////////////////////////////////////

function printObject (object, context) {

  var useColor = context.useColor;
  var sep = ' ';

  if (useColor) {
    context.output += colors.COLOR_PUNCTUATION;
  }

  context.output += '{';

  if (useColor) {
    context.output += colors.COLOR_RESET;
  }

  var newLevel = context.level + 1;

  context.level = newLevel;

  var keys;
  try {
    keys = Object.keys(object);
  }
  catch (err) {
    // ES6 proxy objects don't support key enumeration
    keys = [];
  }

  for (let i = 0, n = keys.length; i < n; ++i) {
    var k = keys[i];
    var val = object[k];

    if (useColor) {
      context.output += colors.COLOR_PUNCTUATION;
    }

    context.output += sep;

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    printIndent(context);

    if (useColor) {
      context.output += colors.COLOR_INDEX;
    }

    context.output += quoteJsonString(k);

    if (useColor) {
      context.output += colors.COLOR_RESET;
    }

    context.output += ' : ';

    var path = context.path;
    context.path += '[' + k + ']';

    printRecursive(val, context);

    context.path = path;
    sep = ', ';

    if (context.emit && context.output.length >= context.emit) {
      exports.output(context.output);
      context.output = '';
    }
  }

  context.level = newLevel - 1;
  context.output += ' ';

  printIndent(context);

  if (useColor) {
    context.output += colors.COLOR_PUNCTUATION;
  }

  context.output += '}';

  if (useColor) {
    context.output += colors.COLOR_RESET;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints objects to standard output without a new-line
////////////////////////////////////////////////////////////////////////////////

var funcRE = /function ([^\(]*)?\(\) \{ \[native code\] \}/;
var func2RE = /function ([^\(]*)?\((.*)\) \{/;

exports.printRecursive = printRecursive = function (value, context) {

  var useColor = context.useColor;
  var customInspect = context.customInspect;
  var useToString = context.useToString;
  var limitString = context.limitString;
  var showFunction = context.showFunction;

  if (typeof context.seen === 'undefined') {
    context.seen = [];
    context.names = [];
  }

  var p = context.seen.indexOf(value);

  if (p >= 0) {
    context.output += context.names[p];
  }
  else {
    if (value && (value instanceof Object || (typeof value === 'object' && Object.getPrototypeOf(value) === null))) {
      context.seen.push(value);
      context.names.push(context.path);
      if (customInspect && typeof value._PRINT === 'function') {
        value._PRINT(context);

        if (context.emit && context.output.length >= context.emit) {
          exports.output(context.output);
          context.output = '';
        }
      }
      else if (value instanceof Array) {
        printArray(value, context);
      }
      else if (
        value.toString === Object.prototype.toString
        || (typeof value === 'object' && Object.getPrototypeOf(value) === null)
      ) {
        var handled = false;
        try {
          if (value instanceof Set ||
              value instanceof Map ||
              value instanceof WeakSet ||
              value instanceof WeakMap ||
              typeof value[Symbol.iterator] === 'function') {
            // ES6 iterators
            context.output += value.toString();
            handled = true;
          }
        }
        catch (err) {
          // ignore any errors thrown above, and simply fall back to normal printing
        }

        if (!handled) {
          // all other objects
          printObject(value, context);
        }

        if (context.emit && context.output.length >= context.emit) {
          exports.output(context.output);
          context.output = '';
        }
      }
      else if (typeof value === 'function') {
        // it's possible that toString() throws, and this looks quite ugly
        try {
          var s = value.toString();

          if (context.level > 0 && !showFunction) {
            var a = s.split('\n');
            var f = a[0];

            var m = funcRE.exec(f);

            if (m !== null) {
              if (m[1] === undefined) {
                context.output += 'function { [native code] }';
              }
              else {
                context.output += 'function ' + m[1] + ' { [native code] }';
              }
            }
            else {
              m = func2RE.exec(f);

              if (m !== null) {
                if (m[1] === undefined) {
                  context.output += 'function ' + '(' + m[2] + ') { ... }';
                }
                else {
                  context.output += 'function ' + m[1] + ' (' + m[2] + ') { ... }';
                }
              }
              else {
                f = f.substr(8, f.length - 10).trim();
                context.output += '[Function "' + f + '" ...]';
              }
            }
          }
          else {
            context.output += s;
          }
        }
        catch (e1) {
          exports.stdOutput(String(e1));
          context.output += '[Function]';
        }
      }
      else if (useToString && typeof value.toString === 'function') {
        try {
          context.output += value.toString();
        }
        catch (e2) {
          context.output += '[Object ';
          printObject(value, context);
          context.output += ']';
        }
      }
      else {
        context.output += '[Object ';
        printObject(value, context);
        context.output += ']';
      }
    }
    else if (value === undefined) {
      if (useColor) {
        context.output += colors.COLOR_UNDEFINED;
      }

      context.output += 'undefined';

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (typeof value === 'string') {
      if (useColor) {
        context.output += colors.COLOR_STRING;
      }

      if (limitString) {
        if (limitString < value.length) {
          value = value.substr(0, limitString) + '...';
        }
      }

      context.output += quoteJsonString(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (typeof value === 'boolean') {
      if (useColor) {
        context.output += value ? colors.COLOR_TRUE : colors.COLOR_FALSE;
      }

      context.output += String(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (typeof value === 'number') {
      if (useColor) {
        context.output += colors.COLOR_NUMBER;
      }

      context.output += String(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else if (value === null) {
      if (useColor) {
        context.output += colors.COLOR_NULL;
      }

      context.output += String(value);

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    /* jshint notypeof: true */
    else if (typeof value === 'symbol') {
    /* jshint notypeof: false */
      // handle ES6 symbols
      if (useColor) {
        context.output += colors.COLOR_NULL;
      }

      context.output += value.toString();

      if (useColor) {
        context.output += colors.COLOR_RESET;
      }
    }
    else {
      context.output += String(value);
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief buffers output instead of printing it
////////////////////////////////////////////////////////////////////////////////

function bufferOutput () {

  for (let i = 0; i < arguments.length; ++i) {
    var value = arguments[i];
    var text;

    if (value === null) {
      text = 'null';
    }
    else if (value === undefined) {
      text = 'undefined';
    }
    else if (typeof value === 'object') {
      try {
        text = JSON.stringify(value);
      }
      catch (err) {
        text = String(value);
      }
    }
    else {
      text = String(value);
    }

    exports.outputBuffer += text;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief prints all arguments
///
/// @FUN{exports.printShell(@FA{arg1}, @FA{arg2}, @FA{arg3}, ...)}
///
/// Only available in shell mode.
///
/// Prints the arguments. If an argument is an object having a function
/// @FN{_PRINT}, then this function is called. A final newline is printed.
////////////////////////////////////////////////////////////////////////////////

function printShell () {

  var output = exports.output;

  for (let i = 0; i < arguments.length; ++i) {
    if (i > 0) {
      output(' ');
    }

    if (typeof arguments[i] === 'string') {
      output(arguments[i]);
    }
    else {
      var context = {
        customInspect: true,
        emit: 16384,
        level: 0,
        limitString: 80,
        names: [],
        output: '',
        path: '~',
        prettyPrint: usePrettyPrint,
        seen: [],
        showFunction: false,
        useColor: useColor,
        useToString: true
      };

      printRecursive(arguments[i], context);

      output(context.output);
    }
  }

  output('\n');
}

// -----------------------------------------------------------------------------
// --SECTION--                                         public printing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief flatten
////////////////////////////////////////////////////////////////////////////////

var hasOwnProperty = Function.prototype.call.bind(Object.prototype.hasOwnProperty);

exports.flatten = function (obj, seen) {

  if (!obj || (typeof obj !== 'object' && typeof obj !== 'function')) {
    return obj;
  }

  if (obj instanceof Date) {
    return obj.toJSON();
  }

  if (!seen) {
    seen = [];
  }

  var result = Object.create(null),
    src = obj,
    keys,
    key,
    val;

  if (typeof obj === 'function') {
    result.__exec = String(obj);
  }

  while (src) {
    if (
      seen.indexOf(src) !== -1
        || (obj.constructor && src === obj.constructor.prototype)
    ) {
      break;
    }
    seen.push(src);
    keys = Object.getOwnPropertyNames(src);
    for (let i = 0; i < keys.length; i++) {
      key = keys[i];
      if (typeof src !== 'function' || (
        key !== 'arguments' && key !== 'caller' && key !== 'callee'
      )) {
        if (key.charAt(0) !== '_' && !hasOwnProperty(result, key)) {
          val = obj[key];
          if (seen.indexOf(val) !== -1 && (
            typeof val === 'object' || typeof val === 'function'
          )) {
            result[key] = '[Circular]';
          } else {
            result[key] = exports.flatten(val, seen);
          }
        }
      }
    }
    src = Object.getPrototypeOf(src);
  }

  if (obj.constructor && obj.constructor.name) {
    if (obj instanceof Error && obj.name === Error.name) {
      result.name = obj.constructor.name;
    } else if (!hasOwnProperty(result, 'constructor')) {
      result.constructor = {name: obj.constructor.name};
    }
  }

  return result;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief inspect
////////////////////////////////////////////////////////////////////////////////

exports.inspect = function (object, options) {

  var context = {
    customInspect: options && options.customInspect,
    emit: false,
    level: 0,
    limitString: false,
    names: [],
    output: '',
    prettyPrint: true,
    path: '~',
    seen: [],
    showFunction: true,
    useColor: false,
    useToString: false
  };

  if (options && options.hasOwnProperty('prettyPrint')) {
    context.prettyPrint = options.prettyPrint;
  }

  printRecursive(object, context);

  return context.output;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sprintf
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_SPRINTF) {
  exports.sprintf = global.SYS_SPRINTF;
  delete global.SYS_SPRINTF;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

var sprintf = exports.sprintf;

exports.printf = function () {
  exports.output(sprintf.apply(sprintf, arguments));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

if (typeof exports.printBrowser === 'function') {
  exports.printShell = printShell;
  exports.print = exports.printBrowser;
}
else {
  exports.print = printShell;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief printObject
////////////////////////////////////////////////////////////////////////////////

exports.printObject = printObject;

exports.isCaptureMode = function() {
  return exports.output === bufferOutput;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief startCaptureMode
////////////////////////////////////////////////////////////////////////////////

exports.startCaptureMode = function () {
  var old = exports.output;

  exports.outputBuffer = '';
  exports.output = bufferOutput;

  return old;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stopCaptureMode
////////////////////////////////////////////////////////////////////////////////

exports.stopCaptureMode = function (old) {
  var buffer = exports.outputBuffer;

  exports.outputBuffer = '';
  if (old !== undefined) {
    exports.output = old;
  }
  else {
    exports.output = exports.stdOutput;
  }

  return buffer;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief startPager
////////////////////////////////////////////////////////////////////////////////

exports.startPager = function () {};

if (global.SYS_START_PAGER) {
  exports.startPager = global.SYS_START_PAGER;
  delete global.SYS_START_PAGER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stopPager
////////////////////////////////////////////////////////////////////////////////

exports.stopPager = function () {};

if (global.SYS_STOP_PAGER) {
  exports.stopPager = global.SYS_STOP_PAGER;
  delete global.SYS_STOP_PAGER;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief startPrettyPrint
////////////////////////////////////////////////////////////////////////////////

exports.startPrettyPrint = function (silent) {
  if (!usePrettyPrint && !silent) {
    exports.print('using pretty printing');
  }

  usePrettyPrint = true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stopPrettyPrint
////////////////////////////////////////////////////////////////////////////////

exports.stopPrettyPrint = function (silent) {
  if (usePrettyPrint && !silent) {
    exports.print('disabled pretty printing');
  }

  usePrettyPrint = false;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief startColorPrint
////////////////////////////////////////////////////////////////////////////////

exports.startColorPrint = function (color, silent) {
  var schemes = {
    arangodb: {
      COLOR_PUNCTUATION: exports.COLORS.COLOR_RESET,
      COLOR_STRING: exports.COLORS.COLOR_BOLD_MAGENTA,
      COLOR_NUMBER: exports.COLORS.COLOR_BOLD_GREEN,
      COLOR_INDEX: exports.COLORS.COLOR_BOLD_CYAN,
      COLOR_TRUE: exports.COLORS.COLOR_BOLD_MAGENTA,
      COLOR_FALSE: exports.COLORS.COLOR_BOLD_MAGENTA,
      COLOR_NULL: exports.COLORS.COLOR_BOLD_YELLOW,
      COLOR_UNDEFINED: exports.COLORS.COLOR_BOLD_YELLOW
    }
  };

  if (!useColor && !silent) {
    exports.print('starting color printing');
  }

  if (color === undefined || color === null) {
    color = null;
  }
  else if (typeof color === 'string') {
    color = color.toLowerCase();
    var c;

    if (schemes.hasOwnProperty(color)) {
      colors = schemes[color];

      for (c in exports.COLORS) {
        if (exports.COLORS.hasOwnProperty(c) && !colors.hasOwnProperty(c)) {
          colors[c] = exports.COLORS[c];
        }
      }
    }
    else {
      colors = exports.COLORS;

      var setColor = function (key) {
        [ 'COLOR_STRING', 'COLOR_NUMBER', 'COLOR_INDEX', 'COLOR_TRUE',
          'COLOR_FALSE', 'COLOR_NULL', 'COLOR_UNDEFINED' ].forEach(function (what) {
          colors[what] = exports.COLORS[key];
        });
      };

      for (c in exports.COLORS) {
        if (exports.COLORS.hasOwnProperty(c) &&
            c.replace(/^COLOR_/, '').toLowerCase() === color) {
          setColor(c);
          break;
        }
      }
    }
  }

  useColor = true;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stopColorPrint
////////////////////////////////////////////////////////////////////////////////

exports.stopColorPrint = function (silent) {
  if (useColor && !silent) {
    exports.print('disabled color printing');
  }

  useColor = false;
};

// -----------------------------------------------------------------------------
// --SECTION--                                          public utility functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief env
////////////////////////////////////////////////////////////////////////////////

if (typeof ENV !== 'undefined') {
  exports.env = new global.ENV();
  delete global.ENV;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief options
////////////////////////////////////////////////////////////////////////////////

if (typeof SYS_OPTIONS !== 'undefined') {
  exports.options = global.SYS_OPTIONS;
  delete global.SYS_OPTIONS;
}

// -----------------------------------------------------------------------------
// --SECTION--                                         global printing functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief print
////////////////////////////////////////////////////////////////////////////////

global.print = function print () {
  var internal = require('internal');
  internal.print.apply(internal.print, arguments);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief printf
////////////////////////////////////////////////////////////////////////////////

global.printf = function printf () {
  var internal = require('internal');
  internal.printf.apply(internal.printf, arguments);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print_plain
////////////////////////////////////////////////////////////////////////////////

global.print_plain = function print_plain() {
  var output = require('internal').output;
  var printRecursive = require('internal').printRecursive;

  for (let i = 0; i < arguments.length; ++i) {
    if (i > 0) {
      output(' ');
    }

    if (typeof arguments[i] === 'string') {
      output(arguments[i]);
    }
    else {
      var context = {
        names: [],
        seen: [],
        path: '~',
        level: 0,
        output: '',
        prettyPrint: false,
        useColor: false,
        customInspect: true
      };

      printRecursive(arguments[i], context);

      output(context.output);
    }
  }

  output('\n');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief start_pretty_print
////////////////////////////////////////////////////////////////////////////////

global.start_pretty_print = function start_pretty_print () {
  require('internal').startPrettyPrint();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop_pretty_print
////////////////////////////////////////////////////////////////////////////////

global.stop_pretty_print = function stop_pretty_print () {
  require('internal').stopPrettyPrint();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief start_color_print
////////////////////////////////////////////////////////////////////////////////

global.start_color_print = function start_color_print (color) {
  require('internal').startColorPrint(color, false);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop_color_print
////////////////////////////////////////////////////////////////////////////////

global.stop_color_print = function stop_color_print () {
  require('internal').stopColorPrint();
};


if (global.EXPORTS_SLOW_BUFFER) {
  Object.keys(global.EXPORTS_SLOW_BUFFER).forEach(function (key) {
    exports[key] = global.EXPORTS_SLOW_BUFFER[key];
  });
  delete global.EXPORTS_SLOW_BUFFER;
}

if (global.APP_PATH) {
  exports.appPath = global.APP_PATH;
  delete global.APP_PATH;
}

if (global.DEV_APP_PATH) {
  exports.devAppPath = global.APP_PATH;
  delete global.DEV_APP_PATH;
}

return exports;

}()));

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:

/*jshint maxlen: 240 */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from errors.dat
////////////////////////////////////////////////////////////////////////////////

(function () {
  "use strict";
  var internal = require("internal");

  internal.errors = {
    "ERROR_NO_ERROR"               : { "code" : 0, "message" : "no error" },
    "ERROR_FAILED"                 : { "code" : 1, "message" : "failed" },
    "ERROR_SYS_ERROR"              : { "code" : 2, "message" : "system error" },
    "ERROR_OUT_OF_MEMORY"          : { "code" : 3, "message" : "out of memory" },
    "ERROR_INTERNAL"               : { "code" : 4, "message" : "internal error" },
    "ERROR_ILLEGAL_NUMBER"         : { "code" : 5, "message" : "illegal number" },
    "ERROR_NUMERIC_OVERFLOW"       : { "code" : 6, "message" : "numeric overflow" },
    "ERROR_ILLEGAL_OPTION"         : { "code" : 7, "message" : "illegal option" },
    "ERROR_DEAD_PID"               : { "code" : 8, "message" : "dead process identifier" },
    "ERROR_NOT_IMPLEMENTED"        : { "code" : 9, "message" : "not implemented" },
    "ERROR_BAD_PARAMETER"          : { "code" : 10, "message" : "bad parameter" },
    "ERROR_FORBIDDEN"              : { "code" : 11, "message" : "forbidden" },
    "ERROR_OUT_OF_MEMORY_MMAP"     : { "code" : 12, "message" : "out of memory in mmap" },
    "ERROR_CORRUPTED_CSV"          : { "code" : 13, "message" : "csv is corrupt" },
    "ERROR_FILE_NOT_FOUND"         : { "code" : 14, "message" : "file not found" },
    "ERROR_CANNOT_WRITE_FILE"      : { "code" : 15, "message" : "cannot write file" },
    "ERROR_CANNOT_OVERWRITE_FILE"  : { "code" : 16, "message" : "cannot overwrite file" },
    "ERROR_TYPE_ERROR"             : { "code" : 17, "message" : "type error" },
    "ERROR_LOCK_TIMEOUT"           : { "code" : 18, "message" : "lock timeout" },
    "ERROR_CANNOT_CREATE_DIRECTORY" : { "code" : 19, "message" : "cannot create directory" },
    "ERROR_CANNOT_CREATE_TEMP_FILE" : { "code" : 20, "message" : "cannot create temporary file" },
    "ERROR_REQUEST_CANCELED"       : { "code" : 21, "message" : "canceled request" },
    "ERROR_DEBUG"                  : { "code" : 22, "message" : "intentional debug error" },
    "ERROR_AID_NOT_FOUND"          : { "code" : 23, "message" : "internal error with attribute ID in shaper" },
    "ERROR_LEGEND_INCOMPLETE"      : { "code" : 24, "message" : "internal error if a legend could not be created" },
    "ERROR_IP_ADDRESS_INVALID"     : { "code" : 25, "message" : "IP address is invalid" },
    "ERROR_LEGEND_NOT_IN_WAL_FILE" : { "code" : 26, "message" : "internal error if a legend for a marker does not yet exist in the same WAL file" },
    "ERROR_FILE_EXISTS"            : { "code" : 27, "message" : "file exists" },
    "ERROR_LOCKED"                 : { "code" : 28, "message" : "locked" },
    "ERROR_HTTP_BAD_PARAMETER"     : { "code" : 400, "message" : "bad parameter" },
    "ERROR_HTTP_UNAUTHORIZED"      : { "code" : 401, "message" : "unauthorized" },
    "ERROR_HTTP_FORBIDDEN"         : { "code" : 403, "message" : "forbidden" },
    "ERROR_HTTP_NOT_FOUND"         : { "code" : 404, "message" : "not found" },
    "ERROR_HTTP_METHOD_NOT_ALLOWED" : { "code" : 405, "message" : "method not supported" },
    "ERROR_HTTP_PRECONDITION_FAILED" : { "code" : 412, "message" : "precondition failed" },
    "ERROR_HTTP_SERVER_ERROR"      : { "code" : 500, "message" : "internal server error" },
    "ERROR_HTTP_CORRUPTED_JSON"    : { "code" : 600, "message" : "invalid JSON object" },
    "ERROR_HTTP_SUPERFLUOUS_SUFFICES" : { "code" : 601, "message" : "superfluous URL suffices" },
    "ERROR_ARANGO_ILLEGAL_STATE"   : { "code" : 1000, "message" : "illegal state" },
    "ERROR_ARANGO_SHAPER_FAILED"   : { "code" : 1001, "message" : "could not shape document" },
    "ERROR_ARANGO_DATAFILE_SEALED" : { "code" : 1002, "message" : "datafile sealed" },
    "ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE" : { "code" : 1003, "message" : "unknown type" },
    "ERROR_ARANGO_READ_ONLY"       : { "code" : 1004, "message" : "read only" },
    "ERROR_ARANGO_DUPLICATE_IDENTIFIER" : { "code" : 1005, "message" : "duplicate identifier" },
    "ERROR_ARANGO_DATAFILE_UNREADABLE" : { "code" : 1006, "message" : "datafile unreadable" },
    "ERROR_ARANGO_DATAFILE_EMPTY"  : { "code" : 1007, "message" : "datafile empty" },
    "ERROR_ARANGO_RECOVERY"        : { "code" : 1008, "message" : "logfile recovery error" },
    "ERROR_ARANGO_CORRUPTED_DATAFILE" : { "code" : 1100, "message" : "corrupted datafile" },
    "ERROR_ARANGO_ILLEGAL_PARAMETER_FILE" : { "code" : 1101, "message" : "illegal or unreadable parameter file" },
    "ERROR_ARANGO_CORRUPTED_COLLECTION" : { "code" : 1102, "message" : "corrupted collection" },
    "ERROR_ARANGO_MMAP_FAILED"     : { "code" : 1103, "message" : "mmap failed" },
    "ERROR_ARANGO_FILESYSTEM_FULL" : { "code" : 1104, "message" : "filesystem full" },
    "ERROR_ARANGO_NO_JOURNAL"      : { "code" : 1105, "message" : "no journal" },
    "ERROR_ARANGO_DATAFILE_ALREADY_EXISTS" : { "code" : 1106, "message" : "cannot create/rename datafile because it already exists" },
    "ERROR_ARANGO_DATADIR_LOCKED"  : { "code" : 1107, "message" : "database directory is locked" },
    "ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS" : { "code" : 1108, "message" : "cannot create/rename collection because directory already exists" },
    "ERROR_ARANGO_MSYNC_FAILED"    : { "code" : 1109, "message" : "msync failed" },
    "ERROR_ARANGO_DATADIR_UNLOCKABLE" : { "code" : 1110, "message" : "cannot lock database directory" },
    "ERROR_ARANGO_SYNC_TIMEOUT"    : { "code" : 1111, "message" : "sync timeout" },
    "ERROR_ARANGO_CONFLICT"        : { "code" : 1200, "message" : "conflict" },
    "ERROR_ARANGO_DATADIR_INVALID" : { "code" : 1201, "message" : "invalid database directory" },
    "ERROR_ARANGO_DOCUMENT_NOT_FOUND" : { "code" : 1202, "message" : "document not found" },
    "ERROR_ARANGO_COLLECTION_NOT_FOUND" : { "code" : 1203, "message" : "collection not found" },
    "ERROR_ARANGO_COLLECTION_PARAMETER_MISSING" : { "code" : 1204, "message" : "parameter 'collection' not found" },
    "ERROR_ARANGO_DOCUMENT_HANDLE_BAD" : { "code" : 1205, "message" : "illegal document handle" },
    "ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL" : { "code" : 1206, "message" : "maximal size of journal too small" },
    "ERROR_ARANGO_DUPLICATE_NAME"  : { "code" : 1207, "message" : "duplicate name" },
    "ERROR_ARANGO_ILLEGAL_NAME"    : { "code" : 1208, "message" : "illegal name" },
    "ERROR_ARANGO_NO_INDEX"        : { "code" : 1209, "message" : "no suitable index known" },
    "ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED" : { "code" : 1210, "message" : "unique constraint violated" },
    "ERROR_ARANGO_INDEX_NOT_FOUND" : { "code" : 1212, "message" : "index not found" },
    "ERROR_ARANGO_CROSS_COLLECTION_REQUEST" : { "code" : 1213, "message" : "cross collection request not allowed" },
    "ERROR_ARANGO_INDEX_HANDLE_BAD" : { "code" : 1214, "message" : "illegal index handle" },
    "ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED" : { "code" : 1215, "message" : "cap constraint already defined" },
    "ERROR_ARANGO_DOCUMENT_TOO_LARGE" : { "code" : 1216, "message" : "document too large" },
    "ERROR_ARANGO_COLLECTION_NOT_UNLOADED" : { "code" : 1217, "message" : "collection must be unloaded" },
    "ERROR_ARANGO_COLLECTION_TYPE_INVALID" : { "code" : 1218, "message" : "collection type invalid" },
    "ERROR_ARANGO_VALIDATION_FAILED" : { "code" : 1219, "message" : "validator failed" },
    "ERROR_ARANGO_PARSER_FAILED"   : { "code" : 1220, "message" : "parser failed" },
    "ERROR_ARANGO_DOCUMENT_KEY_BAD" : { "code" : 1221, "message" : "illegal document key" },
    "ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED" : { "code" : 1222, "message" : "unexpected document key" },
    "ERROR_ARANGO_DATADIR_NOT_WRITABLE" : { "code" : 1224, "message" : "server database directory not writable" },
    "ERROR_ARANGO_OUT_OF_KEYS"     : { "code" : 1225, "message" : "out of keys" },
    "ERROR_ARANGO_DOCUMENT_KEY_MISSING" : { "code" : 1226, "message" : "missing document key" },
    "ERROR_ARANGO_DOCUMENT_TYPE_INVALID" : { "code" : 1227, "message" : "invalid document type" },
    "ERROR_ARANGO_DATABASE_NOT_FOUND" : { "code" : 1228, "message" : "database not found" },
    "ERROR_ARANGO_DATABASE_NAME_INVALID" : { "code" : 1229, "message" : "database name invalid" },
    "ERROR_ARANGO_USE_SYSTEM_DATABASE" : { "code" : 1230, "message" : "operation only allowed in system database" },
    "ERROR_ARANGO_ENDPOINT_NOT_FOUND" : { "code" : 1231, "message" : "endpoint not found" },
    "ERROR_ARANGO_INVALID_KEY_GENERATOR" : { "code" : 1232, "message" : "invalid key generator" },
    "ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE" : { "code" : 1233, "message" : "edge attribute missing" },
    "ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING" : { "code" : 1234, "message" : "index insertion warning - attribute missing in document" },
    "ERROR_ARANGO_INDEX_CREATION_FAILED" : { "code" : 1235, "message" : "index creation failed" },
    "ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT" : { "code" : 1236, "message" : "write-throttling timeout" },
    "ERROR_ARANGO_COLLECTION_TYPE_MISMATCH" : { "code" : 1237, "message" : "collection type mismatch" },
    "ERROR_ARANGO_COLLECTION_NOT_LOADED" : { "code" : 1238, "message" : "collection not loaded" },
    "ERROR_ARANGO_DATAFILE_FULL"   : { "code" : 1300, "message" : "datafile full" },
    "ERROR_ARANGO_EMPTY_DATADIR"   : { "code" : 1301, "message" : "server database directory is empty" },
    "ERROR_REPLICATION_NO_RESPONSE" : { "code" : 1400, "message" : "no response" },
    "ERROR_REPLICATION_INVALID_RESPONSE" : { "code" : 1401, "message" : "invalid response" },
    "ERROR_REPLICATION_MASTER_ERROR" : { "code" : 1402, "message" : "master error" },
    "ERROR_REPLICATION_MASTER_INCOMPATIBLE" : { "code" : 1403, "message" : "master incompatible" },
    "ERROR_REPLICATION_MASTER_CHANGE" : { "code" : 1404, "message" : "master change" },
    "ERROR_REPLICATION_LOOP"       : { "code" : 1405, "message" : "loop detected" },
    "ERROR_REPLICATION_UNEXPECTED_MARKER" : { "code" : 1406, "message" : "unexpected marker" },
    "ERROR_REPLICATION_INVALID_APPLIER_STATE" : { "code" : 1407, "message" : "invalid applier state" },
    "ERROR_REPLICATION_UNEXPECTED_TRANSACTION" : { "code" : 1408, "message" : "invalid transaction" },
    "ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION" : { "code" : 1410, "message" : "invalid replication applier configuration" },
    "ERROR_REPLICATION_RUNNING"    : { "code" : 1411, "message" : "cannot perform operation while applier is running" },
    "ERROR_REPLICATION_APPLIER_STOPPED" : { "code" : 1412, "message" : "replication stopped" },
    "ERROR_REPLICATION_NO_START_TICK" : { "code" : 1413, "message" : "no start tick" },
    "ERROR_REPLICATION_START_TICK_NOT_PRESENT" : { "code" : 1414, "message" : "start tick not present" },
    "ERROR_CLUSTER_NO_AGENCY"      : { "code" : 1450, "message" : "could not connect to agency" },
    "ERROR_CLUSTER_NO_COORDINATOR_HEADER" : { "code" : 1451, "message" : "missing coordinator header" },
    "ERROR_CLUSTER_COULD_NOT_LOCK_PLAN" : { "code" : 1452, "message" : "could not lock plan in agency" },
    "ERROR_CLUSTER_COLLECTION_ID_EXISTS" : { "code" : 1453, "message" : "collection ID already exists" },
    "ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN" : { "code" : 1454, "message" : "could not create collection in plan" },
    "ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION" : { "code" : 1455, "message" : "could not read version in current in agency" },
    "ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION" : { "code" : 1456, "message" : "could not create collection" },
    "ERROR_CLUSTER_TIMEOUT"        : { "code" : 1457, "message" : "timeout in cluster operation" },
    "ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN" : { "code" : 1458, "message" : "could not remove collection from plan" },
    "ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT" : { "code" : 1459, "message" : "could not remove collection from current" },
    "ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN" : { "code" : 1460, "message" : "could not create database in plan" },
    "ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE" : { "code" : 1461, "message" : "could not create database" },
    "ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN" : { "code" : 1462, "message" : "could not remove database from plan" },
    "ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT" : { "code" : 1463, "message" : "could not remove database from current" },
    "ERROR_CLUSTER_SHARD_GONE"     : { "code" : 1464, "message" : "no responsible shard found" },
    "ERROR_CLUSTER_CONNECTION_LOST" : { "code" : 1465, "message" : "cluster internal HTTP connection broken" },
    "ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY" : { "code" : 1466, "message" : "must not specify _key for this collection" },
    "ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS" : { "code" : 1467, "message" : "got contradicting answers from different shards" },
    "ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN" : { "code" : 1468, "message" : "not all sharding attributes given" },
    "ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES" : { "code" : 1469, "message" : "must not change the value of a shard key attribute" },
    "ERROR_CLUSTER_UNSUPPORTED"    : { "code" : 1470, "message" : "unsupported operation or parameter" },
    "ERROR_CLUSTER_ONLY_ON_COORDINATOR" : { "code" : 1471, "message" : "this operation is only valid on a coordinator in a cluster" },
    "ERROR_CLUSTER_READING_PLAN_AGENCY" : { "code" : 1472, "message" : "error reading Plan in agency" },
    "ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION" : { "code" : 1473, "message" : "could not truncate collection" },
    "ERROR_CLUSTER_AQL_COMMUNICATION" : { "code" : 1474, "message" : "error in cluster internal communication for AQL" },
    "ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED" : { "code" : 1475, "message" : "document not found or sharding attributes changed" },
    "ERROR_CLUSTER_COULD_NOT_DETERMINE_ID" : { "code" : 1476, "message" : "could not determine my ID from my local info" },
    "ERROR_QUERY_KILLED"           : { "code" : 1500, "message" : "query killed" },
    "ERROR_QUERY_PARSE"            : { "code" : 1501, "message" : "%s" },
    "ERROR_QUERY_EMPTY"            : { "code" : 1502, "message" : "query is empty" },
    "ERROR_QUERY_SCRIPT"           : { "code" : 1503, "message" : "runtime error '%s'" },
    "ERROR_QUERY_NUMBER_OUT_OF_RANGE" : { "code" : 1504, "message" : "number out of range" },
    "ERROR_QUERY_VARIABLE_NAME_INVALID" : { "code" : 1510, "message" : "variable name '%s' has an invalid format" },
    "ERROR_QUERY_VARIABLE_REDECLARED" : { "code" : 1511, "message" : "variable '%s' is assigned multiple times" },
    "ERROR_QUERY_VARIABLE_NAME_UNKNOWN" : { "code" : 1512, "message" : "unknown variable '%s'" },
    "ERROR_QUERY_COLLECTION_LOCK_FAILED" : { "code" : 1521, "message" : "unable to read-lock collection %s" },
    "ERROR_QUERY_TOO_MANY_COLLECTIONS" : { "code" : 1522, "message" : "too many collections" },
    "ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED" : { "code" : 1530, "message" : "document attribute '%s' is assigned multiple times" },
    "ERROR_QUERY_FUNCTION_NAME_UNKNOWN" : { "code" : 1540, "message" : "usage of unknown function '%s()'" },
    "ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH" : { "code" : 1541, "message" : "invalid number of arguments for function '%s()', expected number of arguments: minimum: %d, maximum: %d" },
    "ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH" : { "code" : 1542, "message" : "invalid argument type in call to function '%s()'" },
    "ERROR_QUERY_INVALID_REGEX"    : { "code" : 1543, "message" : "invalid regex value" },
    "ERROR_QUERY_BIND_PARAMETERS_INVALID" : { "code" : 1550, "message" : "invalid structure of bind parameters" },
    "ERROR_QUERY_BIND_PARAMETER_MISSING" : { "code" : 1551, "message" : "no value specified for declared bind parameter '%s'" },
    "ERROR_QUERY_BIND_PARAMETER_UNDECLARED" : { "code" : 1552, "message" : "bind parameter '%s' was not declared in the query" },
    "ERROR_QUERY_BIND_PARAMETER_TYPE" : { "code" : 1553, "message" : "bind parameter '%s' has an invalid value or type" },
    "ERROR_QUERY_INVALID_LOGICAL_VALUE" : { "code" : 1560, "message" : "invalid logical value" },
    "ERROR_QUERY_INVALID_ARITHMETIC_VALUE" : { "code" : 1561, "message" : "invalid arithmetic value" },
    "ERROR_QUERY_DIVISION_BY_ZERO" : { "code" : 1562, "message" : "division by zero" },
    "ERROR_QUERY_ARRAY_EXPECTED"   : { "code" : 1563, "message" : "array expected" },
    "ERROR_QUERY_FAIL_CALLED"      : { "code" : 1569, "message" : "FAIL(%s) called" },
    "ERROR_QUERY_GEO_INDEX_MISSING" : { "code" : 1570, "message" : "no suitable geo index found for geo restriction on '%s'" },
    "ERROR_QUERY_FULLTEXT_INDEX_MISSING" : { "code" : 1571, "message" : "no suitable fulltext index found for fulltext query on '%s'" },
    "ERROR_QUERY_INVALID_DATE_VALUE" : { "code" : 1572, "message" : "invalid date value" },
    "ERROR_QUERY_MULTI_MODIFY"     : { "code" : 1573, "message" : "multi-modify query" },
    "ERROR_QUERY_MODIFY_IN_SUBQUERY" : { "code" : 1574, "message" : "modify operation in subquery" },
    "ERROR_QUERY_COMPILE_TIME_OPTIONS" : { "code" : 1575, "message" : "query options must be readable at query compile time" },
    "ERROR_QUERY_EXCEPTION_OPTIONS" : { "code" : 1576, "message" : "query options expected" },
    "ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION" : { "code" : 1577, "message" : "collection '%s' used as expression operand" },
    "ERROR_QUERY_DISALLOWED_DYNAMIC_CALL" : { "code" : 1578, "message" : "disallowed dynamic call to '%s'" },
    "ERROR_QUERY_ACCESS_AFTER_MODIFICATION" : { "code" : 1579, "message" : "access after data-modification" },
    "ERROR_QUERY_FUNCTION_INVALID_NAME" : { "code" : 1580, "message" : "invalid user function name" },
    "ERROR_QUERY_FUNCTION_INVALID_CODE" : { "code" : 1581, "message" : "invalid user function code" },
    "ERROR_QUERY_FUNCTION_NOT_FOUND" : { "code" : 1582, "message" : "user function '%s()' not found" },
    "ERROR_QUERY_FUNCTION_RUNTIME_ERROR" : { "code" : 1583, "message" : "user function runtime error: %s" },
    "ERROR_QUERY_BAD_JSON_PLAN"    : { "code" : 1590, "message" : "bad execution plan JSON" },
    "ERROR_QUERY_NOT_FOUND"        : { "code" : 1591, "message" : "query ID not found" },
    "ERROR_QUERY_IN_USE"           : { "code" : 1592, "message" : "query with this ID is in use" },
    "ERROR_CURSOR_NOT_FOUND"       : { "code" : 1600, "message" : "cursor not found" },
    "ERROR_CURSOR_BUSY"            : { "code" : 1601, "message" : "cursor is busy" },
    "ERROR_TRANSACTION_INTERNAL"   : { "code" : 1650, "message" : "internal transaction error" },
    "ERROR_TRANSACTION_NESTED"     : { "code" : 1651, "message" : "nested transactions detected" },
    "ERROR_TRANSACTION_UNREGISTERED_COLLECTION" : { "code" : 1652, "message" : "unregistered collection used in transaction" },
    "ERROR_TRANSACTION_DISALLOWED_OPERATION" : { "code" : 1653, "message" : "disallowed operation inside transaction" },
    "ERROR_TRANSACTION_ABORTED"    : { "code" : 1654, "message" : "transaction aborted" },
    "ERROR_USER_INVALID_NAME"      : { "code" : 1700, "message" : "invalid user name" },
    "ERROR_USER_INVALID_PASSWORD"  : { "code" : 1701, "message" : "invalid password" },
    "ERROR_USER_DUPLICATE"         : { "code" : 1702, "message" : "duplicate user" },
    "ERROR_USER_NOT_FOUND"         : { "code" : 1703, "message" : "user not found" },
    "ERROR_USER_CHANGE_PASSWORD"   : { "code" : 1704, "message" : "user must change his password" },
    "ERROR_APPLICATION_INVALID_NAME" : { "code" : 1750, "message" : "invalid application name" },
    "ERROR_APPLICATION_INVALID_MOUNT" : { "code" : 1751, "message" : "invalid mount" },
    "ERROR_APPLICATION_DOWNLOAD_FAILED" : { "code" : 1752, "message" : "application download failed" },
    "ERROR_APPLICATION_UPLOAD_FAILED" : { "code" : 1753, "message" : "application upload failed" },
    "ERROR_KEYVALUE_INVALID_KEY"   : { "code" : 1800, "message" : "invalid key declaration" },
    "ERROR_KEYVALUE_KEY_EXISTS"    : { "code" : 1801, "message" : "key already exists" },
    "ERROR_KEYVALUE_KEY_NOT_FOUND" : { "code" : 1802, "message" : "key not found" },
    "ERROR_KEYVALUE_KEY_NOT_UNIQUE" : { "code" : 1803, "message" : "key is not unique" },
    "ERROR_KEYVALUE_KEY_NOT_CHANGED" : { "code" : 1804, "message" : "key value not changed" },
    "ERROR_KEYVALUE_KEY_NOT_REMOVED" : { "code" : 1805, "message" : "key value not removed" },
    "ERROR_KEYVALUE_NO_VALUE"      : { "code" : 1806, "message" : "missing value" },
    "ERROR_TASK_INVALID_ID"        : { "code" : 1850, "message" : "invalid task id" },
    "ERROR_TASK_DUPLICATE_ID"      : { "code" : 1851, "message" : "duplicate task id" },
    "ERROR_TASK_NOT_FOUND"         : { "code" : 1852, "message" : "task not found" },
    "ERROR_GRAPH_INVALID_GRAPH"    : { "code" : 1901, "message" : "invalid graph" },
    "ERROR_GRAPH_COULD_NOT_CREATE_GRAPH" : { "code" : 1902, "message" : "could not create graph" },
    "ERROR_GRAPH_INVALID_VERTEX"   : { "code" : 1903, "message" : "invalid vertex" },
    "ERROR_GRAPH_COULD_NOT_CREATE_VERTEX" : { "code" : 1904, "message" : "could not create vertex" },
    "ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX" : { "code" : 1905, "message" : "could not change vertex" },
    "ERROR_GRAPH_INVALID_EDGE"     : { "code" : 1906, "message" : "invalid edge" },
    "ERROR_GRAPH_COULD_NOT_CREATE_EDGE" : { "code" : 1907, "message" : "could not create edge" },
    "ERROR_GRAPH_COULD_NOT_CHANGE_EDGE" : { "code" : 1908, "message" : "could not change edge" },
    "ERROR_GRAPH_TOO_MANY_ITERATIONS" : { "code" : 1909, "message" : "too many iterations - try increasing the value of 'maxIterations'" },
    "ERROR_GRAPH_INVALID_FILTER_RESULT" : { "code" : 1910, "message" : "invalid filter result" },
    "ERROR_GRAPH_COLLECTION_MULTI_USE" : { "code" : 1920, "message" : "multi use of edge collection in edge def" },
    "ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS" : { "code" : 1921, "message" : "edge collection already used in edge def" },
    "ERROR_GRAPH_CREATE_MISSING_NAME" : { "code" : 1922, "message" : "missing graph name" },
    "ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION" : { "code" : 1923, "message" : "malformed edge definition" },
    "ERROR_GRAPH_NOT_FOUND"        : { "code" : 1924, "message" : "graph not found" },
    "ERROR_GRAPH_DUPLICATE"        : { "code" : 1925, "message" : "graph already exists" },
    "ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST" : { "code" : 1926, "message" : "vertex collection does not exist or is not part of the graph" },
    "ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX" : { "code" : 1927, "message" : "not a vertex collection" },
    "ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION" : { "code" : 1928, "message" : "not in orphan collection" },
    "ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF" : { "code" : 1929, "message" : "collection already used in edge def" },
    "ERROR_GRAPH_EDGE_COLLECTION_NOT_USED" : { "code" : 1930, "message" : "edge collection not used in graph" },
    "ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION" : { "code" : 1931, "message" : " is not an ArangoCollection" },
    "ERROR_GRAPH_NO_GRAPH_COLLECTION" : { "code" : 1932, "message" : "collection _graphs does not exist" },
    "ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING" : { "code" : 1933, "message" : "Invalid example type. Has to be String, Array or Object" },
    "ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT" : { "code" : 1934, "message" : "Invalid example type. Has to be Array or Object" },
    "ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS" : { "code" : 1935, "message" : "Invalid number of arguments. Expected: " },
    "ERROR_GRAPH_INVALID_PARAMETER" : { "code" : 1936, "message" : "Invalid parameter type." },
    "ERROR_GRAPH_INVALID_ID"       : { "code" : 1937, "message" : "Invalid id" },
    "ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS" : { "code" : 1938, "message" : "collection used in orphans" },
    "ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST" : { "code" : 1939, "message" : "edge collection does not exist or is not part of the graph" },
    "ERROR_SESSION_UNKNOWN"        : { "code" : 1950, "message" : "unknown session" },
    "ERROR_SESSION_EXPIRED"        : { "code" : 1951, "message" : "session expired" },
    "SIMPLE_CLIENT_UNKNOWN_ERROR"  : { "code" : 2000, "message" : "unknown client error" },
    "SIMPLE_CLIENT_COULD_NOT_CONNECT" : { "code" : 2001, "message" : "could not connect to server" },
    "SIMPLE_CLIENT_COULD_NOT_WRITE" : { "code" : 2002, "message" : "could not write to server" },
    "SIMPLE_CLIENT_COULD_NOT_READ" : { "code" : 2003, "message" : "could not read from server" },
    "ERROR_MALFORMED_MANIFEST_FILE" : { "code" : 3000, "message" : "malformed manifest file" },
    "ERROR_INVALID_APPLICATION_MANIFEST" : { "code" : 3001, "message" : "manifest file is invalid" },
    "ERROR_MANIFEST_FILE_ATTRIBUTE_MISSING" : { "code" : 3002, "message" : "missing manifest attribute" },
    "ERROR_CANNOT_EXTRACT_APPLICATION_ROOT" : { "code" : 3003, "message" : "unable to extract app root path" },
    "ERROR_INVALID_FOXX_OPTIONS"   : { "code" : 3004, "message" : "invalid foxx options" },
    "ERROR_FAILED_TO_EXECUTE_SCRIPT" : { "code" : 3005, "message" : "failed to execute script" },
    "ERROR_SYNTAX_ERROR_IN_SCRIPT" : { "code" : 3006, "message" : "syntax error in script" },
    "ERROR_INVALID_MOUNTPOINT"     : { "code" : 3007, "message" : "mountpoint is invalid" },
    "ERROR_NO_FOXX_FOUND"          : { "code" : 3008, "message" : "No foxx found at this location" },
    "ERROR_APP_NOT_FOUND"          : { "code" : 3009, "message" : "App not found" },
    "ERROR_APP_NEEDS_CONFIGURATION" : { "code" : 3010, "message" : "App not configured" },
    "ERROR_MODULE_NOT_FOUND"       : { "code" : 3100, "message" : "cannot locate module" },
    "ERROR_MODULE_SYNTAX_ERROR"    : { "code" : 3101, "message" : "syntax error in module" },
    "ERROR_MODULE_BAD_WRAPPER"     : { "code" : 3102, "message" : "failed to wrap module" },
    "ERROR_MODULE_FAILURE"         : { "code" : 3103, "message" : "failed to invoke module" },
    "ERROR_MODULE_UNKNOWN_FILE_TYPE" : { "code" : 3110, "message" : "unknown file type" },
    "ERROR_MODULE_PATH_MUST_BE_ABSOLUTE" : { "code" : 3111, "message" : "path must be absolute" },
    "ERROR_MODULE_CAN_NOT_ESCAPE"  : { "code" : 3112, "message" : "cannot use '..' to escape top-level-directory" },
    "ERROR_MODULE_DRIVE_LETTER"    : { "code" : 3113, "message" : "drive local path is not supported" },
    "ERROR_MODULE_BAD_MODULE_ORIGIN" : { "code" : 3120, "message" : "corrupted module origin" },
    "ERROR_MODULE_BAD_PACKAGE_ORIGIN" : { "code" : 3121, "message" : "corrupted package origin" },
    "ERROR_MODULE_DOCUMENT_IS_EMPTY" : { "code" : 3125, "message" : "no content" },
    "ERROR_MODULE_MAIN_NOT_READABLE" : { "code" : 3130, "message" : "cannot read main file" },
    "ERROR_MODULE_MAIN_NOT_JS"     : { "code" : 3131, "message" : "main file is not of type 'js'" },
    "RESULT_ELEMENT_EXISTS"        : { "code" : 10000, "message" : "element not inserted into structure, because it already exists" },
    "RESULT_ELEMENT_NOT_FOUND"     : { "code" : 10001, "message" : "element not found in structure" },
    "ERROR_APP_ALREADY_EXISTS"     : { "code" : 20000, "message" : "newest version of app already installed" },
    "ERROR_QUEUE_ALREADY_EXISTS"   : { "code" : 21000, "message" : "named queue already exists" },
    "ERROR_DISPATCHER_IS_STOPPING" : { "code" : 21001, "message" : "dispatcher stopped" },
    "ERROR_QUEUE_UNKNOWN"          : { "code" : 21002, "message" : "named queue does not exist" },
    "ERROR_QUEUE_FULL"             : { "code" : 21003, "message" : "named queue is full" }
  };
}());


/*jshint -W051:true */
/*global jqconsole, Symbol */
/*eslint-disable */
global.DEFINE_MODULE('console', (function () {
'use strict';
/*eslint-enable */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "console"
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
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  Module "console"
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

var exports = {};
var internal = require('internal');
var sprintf = internal.sprintf;
var inspect = internal.inspect;

////////////////////////////////////////////////////////////////////////////////
/// @brief group level
////////////////////////////////////////////////////////////////////////////////

var groupLevel = '';

////////////////////////////////////////////////////////////////////////////////
/// @brief timers
////////////////////////////////////////////////////////////////////////////////

var timers;
try {
  timers = Object.create(null);
}
catch (e) {
  timers = {};
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief internal logging
////////////////////////////////////////////////////////////////////////////////

var log;

if (global.SYS_LOG) {
  // this will work when we are in arangod but not in the browser / web interface
  log = global.SYS_LOG;
  delete global.SYS_LOG;
}
else {
  // this will work in the web interface
  log = function (level, message) {
    if (typeof jqconsole !== 'undefined') {
      jqconsole.Write(message + '\n', 'jssuccess');
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal logging with group level
////////////////////////////////////////////////////////////////////////////////

function logGroup (level, msg) {
  log(level, groupLevel + msg);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief try to prettify
////////////////////////////////////////////////////////////////////////////////

function prepareArgs (args) {
  var ShapedJson = require('internal').ShapedJson;
  var result = [];

  if (args.length > 0 && typeof args[0] !== 'string') {
    result.push('%s');
  }

  for (let i = 0; i < args.length; ++i) {
    let arg = args[i];

    if (typeof arg === 'object') {
      if (ShapedJson !== undefined && arg instanceof ShapedJson) {
        arg = inspect(arg, {prettyPrint: false});
      }
      else if (arg === null) {
        arg = 'null';
      }
      else if (arg instanceof Date || arg instanceof RegExp) {
        arg = String(arg);
      }
      else if (Object.prototype.isPrototypeOf(arg) || Array.isArray(arg)) {
        arg = inspect(arg, {prettyPrint: false});
      }
    }

    result.push(arg);
  }

  return result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief assert
////////////////////////////////////////////////////////////////////////////////

exports.assert = function (condition) {
  if (condition) {
    return;
  }

  var args = Array.prototype.slice.call(arguments, 1);
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(args));
  }
  catch (e) {
    msg = msg = `${e}: ${args}`;
  }

  logGroup('error', msg);

  require('assert').ok(condition, msg);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief debug
////////////////////////////////////////////////////////////////////////////////

exports.debug = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  logGroup('debug', msg);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief debugLines
////////////////////////////////////////////////////////////////////////////////

exports.debugLines = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  var a = msg.split('\n');

  for (let i = 0; i < a.length; ++i) {
    logGroup('debug', a[i]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief dir
////////////////////////////////////////////////////////////////////////////////

exports.dir = function (object) {
  logGroup('info', inspect(object));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief error
////////////////////////////////////////////////////////////////////////////////

exports.error = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  logGroup('error', msg);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief errorLines
////////////////////////////////////////////////////////////////////////////////

exports.errorLines = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  var a = msg.split('\n');
  for (let i = 0; i < a.length; ++i) {
    logGroup('error', a[i]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief getline
////////////////////////////////////////////////////////////////////////////////

if (global.SYS_GETLINE) {
  exports.getline = global.SYS_GETLINE;
  delete global.SYS_GETLINE;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief group
////////////////////////////////////////////////////////////////////////////////

exports.group = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  groupLevel = groupLevel + '  ';
  logGroup('info', msg);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief groupCollapsed
////////////////////////////////////////////////////////////////////////////////

exports.groupCollapsed = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  logGroup('info', msg);
  groupLevel = groupLevel + '  ';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief groupEnd
////////////////////////////////////////////////////////////////////////////////

exports.groupEnd = function () {
  groupLevel = groupLevel.substr(2);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief info
////////////////////////////////////////////////////////////////////////////////

exports.info = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }
  logGroup('info', msg);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief infoLines
////////////////////////////////////////////////////////////////////////////////

exports.infoLines = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  var a = msg.split('\n');

  for (let i = 0; i < a.length; ++i) {
    logGroup('info', a[i]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief log
////////////////////////////////////////////////////////////////////////////////

exports.log = exports.info;

////////////////////////////////////////////////////////////////////////////////
/// @brief logLines
////////////////////////////////////////////////////////////////////////////////

exports.logLines = exports.infoLines;

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

exports.time = function (label) {
  if (typeof label !== 'string') {
    throw new Error('label must be a string');
  }

  var symbol = typeof Symbol === 'undefined' ? '%' + label : Symbol.for(label);

  timers[symbol] = Date.now();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief timeEnd
////////////////////////////////////////////////////////////////////////////////

exports.timeEnd = function(label) {
  var symbol = typeof Symbol === 'undefined' ? '%' + label : Symbol.for(label);
  var time = timers[symbol];

  if (!time) {
    throw new Error('No such label: ' + label);
  }

  var duration = Date.now() - time;

  delete timers[symbol];

  logGroup('info', sprintf('%s: %dms', label, duration));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief trace
////////////////////////////////////////////////////////////////////////////////

exports.trace = function () {
  var err = new Error();
  err.name = 'Trace';
  err.message = sprintf.apply(sprintf, prepareArgs(arguments));
  Error.captureStackTrace(err, exports.trace);
  var a = err.stack.split('\n');
  while (!a[a.length - 1]) {
    a.pop();
  }
  for (let i = 0; i < a.length; ++i) {
    logGroup('info', a[i]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief warn
////////////////////////////////////////////////////////////////////////////////

exports.warn = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  logGroup('warning', msg);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief warnLines
////////////////////////////////////////////////////////////////////////////////

exports.warnLines = function () {
  var msg;

  try {
    msg = sprintf.apply(sprintf, prepareArgs(arguments));
  }
  catch (e) {
    msg = `${e}: ${arguments}`;
  }

  var a = msg.split('\n');
  var i;

  for (i = 0; i < a.length; ++i) {
    logGroup('warning', a[i]);
  }
};

return exports;
}()));

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:

/*jshint -W051:true */
/*eslint-disable */
(function () {
'use strict';
/*eslint-enable */

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
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
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 Module "internal"
// -----------------------------------------------------------------------------

var exports = require('internal');

////////////////////////////////////////////////////////////////////////////////
/// @brief hide global variables
////////////////////////////////////////////////////////////////////////////////

if (global.ArangoConnection) {
  exports.ArangoConnection = global.ArangoConnection;
}

if (global.SYS_ARANGO) {
  exports.arango = global.SYS_ARANGO;
  delete global.SYS_ARANGO;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief write-ahead log functionality
////////////////////////////////////////////////////////////////////////////////

exports.wal = {
  flush: function (waitForSync, waitForCollector) {
    if (exports.arango) {
      var wfs = waitForSync ? 'true' : 'false';
      var wfc = waitForCollector ? 'true' : 'false';
      exports.arango.PUT('/_admin/wal/flush?waitForSync=' + wfs + '&waitForCollector=' + wfc, '');
      return;
    }

    throw 'not connected';
  },

  properties: function (value) {
    if (exports.arango) {
      if (value !== undefined) {
        return exports.arango.PUT('/_admin/wal/properties', JSON.stringify(value));
      }

      return exports.arango.GET('/_admin/wal/properties', '');
    }

    throw 'not connected';
  },

  transactions: function () {
    if (exports.arango) {
      return exports.arango.GET('/_admin/wal/transactions', '');
    }

    throw 'not connected';
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief reloads the AQL user functions
////////////////////////////////////////////////////////////////////////////////

exports.reloadAqlFunctions = function () {
  if (exports.arango) {
    exports.arango.POST('/_admin/aql/reload', '');
    return;
  }

  throw 'not connected';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the routing cache
////////////////////////////////////////////////////////////////////////////////

exports.reloadRouting = function () {
  if (exports.arango) {
    exports.arango.POST('/_admin/routing/reload', '');
    return;
  }

  throw 'not connected';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the routing cache
////////////////////////////////////////////////////////////////////////////////

exports.routingCache = function () {
  if (exports.arango) {
    return exports.arango.GET('/_admin/routing/routes', '');

  }

  throw 'not connected';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rebuilds the authentication cache
////////////////////////////////////////////////////////////////////////////////

exports.reloadAuth = function () {
  if (exports.arango) {
    exports.arango.POST('/_admin/auth/reload', '');
    return;
  }

  throw 'not connected';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief execute javascript file on the server
////////////////////////////////////////////////////////////////////////////////

exports.executeServer = function (body) {
  if (exports.arango) {
    return exports.arango.POST('/_admin/execute', body);
  }

  throw 'not connected';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a request in curl format
////////////////////////////////////////////////////////////////////////////////

exports.appendCurlRequest = function (shellAppender,jsonAppender, rawAppender) {
  return function (method, url, body, headers) {
    var response;
    var curl;
    var i;
    var jsonBody = false;

    if ((typeof body !== 'string') && (body !== undefined)) {
      jsonBody = true;
      body = exports.inspect(body);
    }

    curl = 'shell> curl ';

    if (method === 'POST') {
      response = exports.arango.POST_RAW(url, body, headers);
      curl += '-X ' + method + ' ';
    }
    else if (method === 'PUT') {
      response = exports.arango.PUT_RAW(url, body, headers);
      curl += '-X ' + method + ' ';
    }
    else if (method === 'GET') {
      response = exports.arango.GET_RAW(url, headers);
    }
    else if (method === 'DELETE') {
      response = exports.arango.DELETE_RAW(url, headers);
      curl += '-X ' + method + ' ';
    }
    else if (method === 'PATCH') {
      response = exports.arango.PATCH_RAW(url, body, headers);
      curl += '-X ' + method + ' ';
    }
    else if (method === 'HEAD') {
      response = exports.arango.HEAD_RAW(url, headers);
      curl += '-X ' + method + ' ';
    }
    else if (method === 'OPTION') {
      response = exports.arango.OPTION_RAW(url, body, headers);
      curl += '-X ' + method + ' ';
    }
    if (headers !== undefined && headers !== '') {
      for (i in headers) {
        if (headers.hasOwnProperty(i)) {
          curl += '--header \'' + i + ': ' + headers[i] + '\' ';
        }
      }
    }

    if (body !== undefined && body !== '') {
      curl += '--data-binary @- ';
    }

    curl += '--dump - http://localhost:8529' + url;

    shellAppender(curl);

    if (body !== undefined && body !== '' && body) {
      rawAppender(' &lt;&lt;EOF\n');
      if (jsonBody) {
        jsonAppender(body);
      }
      else {
        rawAppender(body);
      }
      rawAppender('\nEOF');
    }
    rawAppender('\n\n');
    return response;
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a raw response
////////////////////////////////////////////////////////////////////////////////

exports.appendRawResponse = function (appender, syntaxAppender) {
  return function (response) {
    var key;
    var headers = response.headers;

    // generate header
    appender('HTTP/1.1 ' + headers['http/1.1'] + '\n');

    for (key in headers) {
      if (headers.hasOwnProperty(key)) {
        if (key !== 'http/1.1' && key !== 'server' && key !== 'connection'
            && key !== 'content-length') {
          appender(key + ': ' + headers[key] + '\n');
        }
      }
    }

    appender('\n');

    // append body
    if (response.body !== undefined) {
      syntaxAppender(exports.inspect(response.body));
      appender('\n');
    }
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief logs a response in JSON
////////////////////////////////////////////////////////////////////////////////

exports.appendJsonResponse = function (appender, syntaxAppender) {
  return function (response) {
    var syntaxAppend = exports.appendRawResponse(syntaxAppender, syntaxAppender);

    // copy original body (this is necessary because 'response' is passed by reference)
    var copy = response.body;
    // overwrite body with parsed JSON && append
    response.body = JSON.parse(response.body);
    syntaxAppend(response);
    // restore original body
    response.body = copy;
  };
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief log function
////////////////////////////////////////////////////////////////////////////////

exports.log = function (level, msg) {
  exports.output(level, ': ', msg, '\n');
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sprintf wrapper
////////////////////////////////////////////////////////////////////////////////

try {
  if (typeof window !== 'undefined') {
    exports.sprintf = function (format) {
      var n = arguments.length;
      if (n === 0) {
        return '';
      }
      if (n <= 1) {
        return String(format);
      }

      var i;
      var args = [ ];
      for (i = 1; i < arguments.length; ++i) {
        args.push(arguments[i]);
      }
      i = 0;

      return format.replace(/%[dfs]/, function () {
        return String(args[i++]);
      });
    };
  }
}
catch (e) {
  // noop
}

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:

/*eslint no-extend-native:0 */
/*eslint-disable */
(function () {
'use strict';
/*eslint-enable */

////////////////////////////////////////////////////////////////////////////////
/// @brief monkey-patches to built-in prototypes
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
/// @author Lucas Dohmen
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


// -----------------------------------------------------------------------------
// --SECTION--                                                    monkey-patches
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief shallow copies properties
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, '_shallowCopy', {
  get() {
    var self = this;
    return this.propertyKeys.reduce(function (previous, key) {
      previous[key] = self[key];
      return previous;
    }, {});
  }
});

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the property keys
////////////////////////////////////////////////////////////////////////////////

Object.defineProperty(Object.prototype, 'propertyKeys', {
  get() {
    return Object.keys(this).filter(function (key) {
      return (key.charAt(0) !== '_' && key.charAt(0) !== '$');
    });
  }
});

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

}());

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:

/*jshint -W051:true */
/*global global:true, window, require */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoShell client API
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
/// @author Achim Brandt
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (typeof global === 'undefined' && typeof window !== 'undefined') {
  global = window;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
// @brief common globals
////////////////////////////////////////////////////////////////////////////////

global.Buffer = require("buffer").Buffer;
global.process = require("process");
global.setInterval = global.setInterval || function () {};
global.clearInterval = global.clearInterval || function () {};
global.setTimeout = global.setTimeout || function () {};
global.clearTimeout = global.clearTimeout || function () {};

////////////////////////////////////////////////////////////////////////////////
/// @brief template string generator for building an AQL query
////////////////////////////////////////////////////////////////////////////////

global.aqlQuery = function () {
  var strings = arguments[0];
  var bindVars = {};
  var query = strings[0];
  var name, value, i;
  for (i = 1; i < arguments.length; i++) {
    value = arguments[i];
    name = 'value' + (i - 1);
    if (value.constructor && value.constructor.name === 'ArangoCollection') {
      name = '@' + name;
      value = value.name();
    }
    bindVars[name] = value;
    query += '@' + name + strings[i];
  }
  return {query: query, bindVars: bindVars};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief start paging
////////////////////////////////////////////////////////////////////////////////

global.start_pager = function start_pager () {
  var internal = require("internal");
  internal.startPager();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief stop paging
////////////////////////////////////////////////////////////////////////////////

global.stop_pager = function stop_pager () {
  var internal = require("internal");
  internal.stopPager();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief print the overall help
////////////////////////////////////////////////////////////////////////////////

global.help = function help () {
  var internal = require("internal");
  var arangodb = require("org/arangodb");
  var arangosh = require("org/arangodb/arangosh");

  internal.print(arangosh.HELP);
  arangodb.ArangoDatabase.prototype._help();
  arangodb.ArangoCollection.prototype._help();
  arangodb.ArangoStatement.prototype._help();
  arangodb.ArangoQueryCursor.prototype._help();
  internal.print(arangosh.helpExtended);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief clear screen (poor man's way)
////////////////////////////////////////////////////////////////////////////////

global.clear = function clear () {
  var internal = require("internal");
  var result = '';

  for (var i = 0; i < 100; ++i) {
    result += '\n';
  }
  internal.print(result);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'console'
////////////////////////////////////////////////////////////////////////////////

global.console = global.console || require("console");

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'db'
////////////////////////////////////////////////////////////////////////////////

global.db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'arango'
////////////////////////////////////////////////////////////////////////////////

global.arango = require("org/arangodb").arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'fm'
////////////////////////////////////////////////////////////////////////////////

global.fm = require("org/arangodb/foxx/manager");

////////////////////////////////////////////////////////////////////////////////
/// @brief global 'ArangoStatement'
////////////////////////////////////////////////////////////////////////////////

global.ArangoStatement = require("org/arangodb/arango-statement").ArangoStatement;

////////////////////////////////////////////////////////////////////////////////
/// @brief shell tutorial
////////////////////////////////////////////////////////////////////////////////

global.tutorial = require("org/arangodb/tutorial");

// -----------------------------------------------------------------------------
// --SECTION--                                                        initialize
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints help
////////////////////////////////////////////////////////////////////////////////

var initHelp = function() {
  var internal = require("internal");

  if (internal.db) {
    try {
      internal.db._collections();
    }
    catch (e) {}
  }

  if (internal.quiet !== true) {
    require("org/arangodb").checkAvailableVersions();

    if (internal.arango && internal.arango.isConnected && internal.arango.isConnected()) {
      internal.print("Type 'tutorial' for a tutorial or 'help' to see common examples");
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief read rc file
////////////////////////////////////////////////////////////////////////////////

if (typeof window === 'undefined') {
  // We're in arangosh
  initHelp();

  // these variables are not defined in the browser context
  if (
    global.IS_EXECUTE_SCRIPT ||
    global.IS_EXECUTE_STRING ||
    global.IS_CHECK_SCRIPT ||
    global.IS_UNIT_TESTS ||
    global.IS_JS_LINT
  ) {
    try {
      // this will not work from within a browser
      var __fs__ = require("fs");
      var __rcf__ = __fs__.join(__fs__.home(), ".arangosh.rc");

      if (__fs__.exists(__rcf__)) {
        /*jshint evil: true */
        var __content__ = __fs__.read(__rcf__);
        eval(__content__);
      }
    }
    catch (e) {
      require("console").warn("arangosh.rc: %s", String(e));
    }
  }

  try {
    delete global.IS_EXECUTE_SCRIPT;
    delete global.IS_EXECUTE_STRING;
    delete global.IS_CHECK_SCRIPT;
    delete global.IS_UNIT_TESTS;
    delete global.IS_JS_LINT;
  } catch (e) {}
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @\\}\\)"
// End:
