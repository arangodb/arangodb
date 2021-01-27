/* global arango:true, ArangoClusterInfo */

'use strict';

// ////////////////////////////////////////////////////////////////////////////
// @brief JavaScript base module
//
// @file
//
// DISCLAIMER
//
// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
//
// Licensed under the Apache License, Version 2.0 (the "License")
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Copyright holder is triAGENS GmbH, Cologne, Germany
//
// @author Dr. Frank Celler
// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var internal = require('internal');
var fs = require('fs');

// //////////////////////////////////////////////////////////////////////////////
// / @brief errors
// //////////////////////////////////////////////////////////////////////////////

Object.keys(internal.errors).forEach(function(key) {
  exports[key] = internal.errors[key].code;
});

function isAqlQuery(query) {
  return Boolean(query && typeof query.query === "string" && query.bindVars);
}

function isGeneratedAqlQuery(query) {
  return isAqlQuery(query) && typeof query._source === "function";
}

function isAqlLiteral(literal) {
  return Boolean(literal && typeof literal.toAQL === "function");
}

exports.aql = function aql(templateStrings, ...args) {
  const strings = [...templateStrings];
  const bindVars = {};
  const bindVals = [];
  let query = strings[0];
  for (let i = 0; i < args.length; i++) {
    const rawValue = args[i];
    let value = rawValue;
    if (isGeneratedAqlQuery(rawValue)) {
      const src = rawValue._source();
      if (src.args.length) {
        query += src.strings[0];
        args.splice(i, 1, ...src.args);
        strings.splice(
          i,
          2,
          strings[i] + src.strings[0],
          ...src.strings.slice(1, src.args.length),
          src.strings[src.args.length] + strings[i + 1]
        );
      } else {
        query += rawValue.query + strings[i + 1];
        args.splice(i, 1);
        strings.splice(i, 2, strings[i] + rawValue.query + strings[i + 1]);
      }
      i -= 1;
      continue;
    }
    if (rawValue === undefined) {
      query += strings[i + 1];
      continue;
    }
    if (isAqlLiteral(rawValue)) {
      query += `${rawValue.toAQL()}${strings[i + 1]}`;
      continue;
    }
    const index = bindVals.indexOf(rawValue);
    const isKnown = index !== -1;
    let name = `value${isKnown ? index : bindVals.length}`;
    if (rawValue && rawValue.isArangoCollection) {
      name = `@${name}`;
      value = rawValue.name();
    }
    if (!isKnown) {
      bindVals.push(rawValue);
      bindVars[name] = value;
    }
    query += `@${name}${strings[i + 1]}`;
  }
  return {
    query,
    bindVars,
    _source: () => ({ strings, args })
  };
};

exports.aql.literal = function (value) {
  if (isAqlLiteral(value)) {
    return value;
  }
  return {
    toAQL() {
      if (value === undefined) {
        return "";
      }
      return String(value);
    }
  };
};

exports.aql.join = function (values, sep = " ") {
  if (!values.length) {
    return exports.aql``;
  }
  if (values.length === 1) {
    return exports.aql`${values[0]}`;
  }
  return exports.aql(
    ["", ...Array(values.length - 1).fill(sep), ""],
    ...values
  );
};

exports.isValidDocumentKey = function (documentKey) {
  if (!documentKey) return false;
  // see VocBase/KeyGenerator.cpp keyCharLookupTable
  return /^[-_!$%'()*+,.:;=@0-9a-z]+$/i.test(documentKey);
};

exports.errors = internal.errors;

exports.time = internal.time;

// //////////////////////////////////////////////////////////////////////////////
// / @brief ArangoError
// //////////////////////////////////////////////////////////////////////////////

exports.ArangoError = internal.ArangoError;

// //////////////////////////////////////////////////////////////////////////////
// / @brief defines a module
// //////////////////////////////////////////////////////////////////////////////

exports.defineModule = function(path, file) {
  let content = fs.read(file);
  let mc = internal.db._collection('_modules');

  path = module.normalize(path);
  let m = mc.firstExample({path: path});

  if (m === null) {
    mc.save({path: path, content: content});
  } else {
    mc.replace(m, {path: path, content: content});
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief normalizeURL
// /
// / If @FA{path} starts with "." or "..", then it is a relative path.
// / Otherwise it is an absolute path. Normalizing will remove `//`,
// / `/./`, `/../` from the url - expect in the beginning, where it keeps
// / `../` and or at most one `./`.
// /
// / If @FA{path} is empty, the url `./` will be returned.
// //////////////////////////////////////////////////////////////////////////////

exports.normalizeURL = function(path) {
  var i;
  var n;
  var p;
  var q;
  var r;
  var x;

  if (path === '') {
    return './';
  }

  p = path.split('/');

  // relative path
  if (p[0] === '.' || p[0] === '..') {
    r = p[0] + '/';
    p.shift();
    q = p;
  } else if (p[0] === '') {
    // absolute path
    r = '/';
    p.shift();
    q = p;
  } else {
    // assume that the path is relative
    r = './';
    q = p;
  }

  // normalize path
  n = [];

  for (i = 0; i < q.length; ++i) {
    x = q[i];

    if (x === '..') {
      if (n.length === 0) {
        if (r === '../') {
          n.push(x);
        } else if (r === './') {
          r = '../';
        } else {
          throw "cannot use '..' to escape top-level-directory";
        }
      } else if (n[n.length - 1] === '..') {
        n.push(x);
      } else {
        n.pop();
      }
    } else if (x !== '' && x !== '.') {
      n.push(x);
    }
  }

  return r + n.join('/');
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief inspect
// //////////////////////////////////////////////////////////////////////////////

exports.inspect = internal.inspect;

// //////////////////////////////////////////////////////////////////////////////
// / @brief output
// /
// / In order to allow "capture" output to work, we cannot assigne the
// / function here.
// //////////////////////////////////////////////////////////////////////////////

exports.output = function() {
  internal.output.apply(internal.output, arguments);
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief print
// //////////////////////////////////////////////////////////////////////////////

exports.print = internal.print;

// //////////////////////////////////////////////////////////////////////////////
// / @brief printf
// //////////////////////////////////////////////////////////////////////////////

exports.printf = internal.printf;

// //////////////////////////////////////////////////////////////////////////////
// / @brief sprintf
// //////////////////////////////////////////////////////////////////////////////

exports.sprintf = internal.sprintf;

// //////////////////////////////////////////////////////////////////////////////
// / @brief printObject
// //////////////////////////////////////////////////////////////////////////////

exports.printObject = internal.printObject;

// //////////////////////////////////////////////////////////////////////////////
// / @brief 2D ASCII table printing
// //////////////////////////////////////////////////////////////////////////////

exports.printTable = function(list, columns, options) {
  options = options || {};
  if (options.totalString === undefined) {
    options.totalString = '%s document(s)\n';
  }

  var pad = '...';
  var descriptions, matrix, col, what, j;

  if (columns === undefined) {
    what = list[0];
  } else if (Array.isArray(columns)) {
    what = {};

    columns.forEach(function(col) {
      what[col] = null;
    });
  } else {
    what = columns;
  }

  j = 0;
  descriptions = [];
  matrix = [[]];

  for (col in what) {
    if (what.hasOwnProperty(col)) {
      var fixedLength = null;

      if (columns && columns.hasOwnProperty(col) && columns[col] > 0) {
        fixedLength = columns[col] >= pad.length ? columns[col] : pad.length;
      }

      // header
      var name = col;

      // rename header?
      if (options.hasOwnProperty('rename')) {
        if (options.rename.hasOwnProperty(col)) {
          name = options.rename[col];
        }
      }

      descriptions.push({
        id: col,
        fixedLength: fixedLength,
        length: fixedLength || name.length,
      });

      matrix[0][j++] = name;
    }
  }

  // determine values & max widths
  list.forEach(function(row, i) {
    matrix[i + 1] = [];
    descriptions.forEach(function(col) {
      if (row.hasOwnProperty(col.id)) {
        var value;
        if (options.prettyStrings && typeof row[col.id] === 'string') {
          value = row[col.id];
        } else {
          value = JSON.stringify(row[col.id]) || '';
        }

        matrix[i + 1].push(value);

        if (value.length > col.length && !col.fixedLength) {
          col.length = Math.min(value.length, 100);
        }
      } else {
        // undefined
        matrix[i + 1].push('');
      }
    });
  });

  var divider = function() {
    var parts = [];
    descriptions.forEach(function(desc) {
      parts.push(exports.stringPadding('', desc.length, '-', 'r'));
    });

    if (options.framed) {
      return '+-' + parts.join('-+-') + '-+\n';
    }

    return parts.join('   ') + '\n';
  };

  var compose = function() {
    var result = '';

    if (options.framed) {
      result += divider();
    }
    matrix.forEach(function(row, i) {
      var parts = [];

      row.forEach(function(col, j) {
        var len = descriptions[j].length,
          value = row[j];
        if (value.length > len) {
          value = value.substr(0, len - pad.length) + pad;
        }
        parts.push(exports.stringPadding(value, len, ' ', 'r'));
      });

      if (options.framed) {
        result += '| ' + parts.join(' | ') + ' |\n';
      } else {
        result += parts.join('   ') + '\n';
      }

      if (i === 0) {
        result += divider();
      }
    });

    result += divider();

    if (!options.hideTotal) {
      result += internal.sprintf(options.totalString, String(list.length));
    }
    return result;
  };

  if (!Array.isArray(list)) {
    // not an array
    return;
  }

  if (list.length === 0) {
    exports.print(options.emptyString || 'no document(s)');
  } else {
    exports.print(compose());
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief stringPadding
// //////////////////////////////////////////////////////////////////////////////

exports.stringPadding = function(str, len, pad, dir) {
  // yes, this is more code than new Array(length).join(chr), but it makes jslint happy
  function fill(length, chr) {
    var result = '',
      i;
    for (i = 0; i < length; ++i) {
      result += chr;
    }
    return result;
  }

  if (typeof len === 'undefined') {
    len = 0;
  }
  if (typeof pad === 'undefined') {
    pad = ' ';
  }

  if (len + 1 >= str.length) {
    switch (dir || 'r') {
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

// //////////////////////////////////////////////////////////////////////////////
// / @brief throws an error in case of missing file
// //////////////////////////////////////////////////////////////////////////////

exports.throwFileNotFound = function(msg) {
  throw new exports.ArangoError({
    errorNum: exports.errors.ERROR_FILE_NOT_FOUND.code,
    errorMessage:
      exports.errors.ERROR_FILE_NOT_FOUND.message + ': ' + String(msg),
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief throws an error in case of a bad parameter
// //////////////////////////////////////////////////////////////////////////////

exports.throwBadParameter = function(msg) {
  throw new exports.ArangoError({
    errorNum: exports.errors.ERROR_BAD_PARAMETER.code,
    errorMessage:
      exports.errors.ERROR_BAD_PARAMETER.message + ': ' + String(msg),
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief checks parameter, throws an error if missing
// //////////////////////////////////////////////////////////////////////////////

exports.checkParameter = function(usage, descs, vars) {
  var i;

  for (i = 0; i < descs.length; ++i) {
    var desc = descs[i];

    if (typeof vars[i] === 'undefined') {
      exports.throwBadParameter(desc[0] + ' missing, usage: ' + usage);
    }

    if (typeof vars[i] !== desc[1]) {
      exports.throwBadParameter(
        desc[0] +
          " should be a '" +
          desc[1] +
          "', " +
          "not '" +
          typeof vars[i] +
          "'"
      );
    }
  }
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief generate info message for newer version(s) available
// //////////////////////////////////////////////////////////////////////////////

exports.checkAvailableVersions = function() {
  var version = internal.version;
  var isServer = require('@arangodb').isServer;
  var console = require('console');
  var log;

  if (isServer) {
    log = console.info;
  } else {
    log = internal.print;
  }

  let isStable = true;
  if (version.match(/beta|alpha|preview|milestone|devel/) !== null) {
    isStable = false;
    if (internal.quiet !== true) {
      log(
        "You are using a milestone/alpha/beta/preview version ('" +
          version +
          "') of ArangoDB"
      );
    }
  } 

  if (isServer && internal.isEnterprise()) {
    // don't check for version updates in arangod in Enterprise Edition
    return;
  }
  if (!isServer && internal.isEnterprise() && isStable) {
    // don't check for version updates in arangosh in stable Enterprise Edition
    return;
  }
  
  try {
    var hash = "A" + Math.random();
    var engine = "unknown";
    var platform = internal.platform;
    var license = (internal.isEnterprise() ? "enterprise" : "community");

    if (isServer) {
      engine = internal.db._engine().name;
      var role = global.ArangoServerState.role();

      if (role === "COORDINATOR") {
        try {
          var c = ArangoClusterInfo.getCoordinators().length.toString(16).toUpperCase();
          var d = ArangoClusterInfo.getDBServers().length.toString(16).toUpperCase();
        } catch (err) {
          hash = "1-FFFF-0001-arangod";
        }
        hash = "1-" + c + "-" + d + "-" + global.ArangoServerState.id();
      } else if (role === "PRIMARY") {
        hash = "1-FFFF-0002-arangod";
      } else if (role === "AGENT") {
        hash = "1-FFFF-0003-arangod";
      } else if (role === "SINGLE") {
        hash = "1-FFFF-0004-arangod";
      } else {
        hash = "1-FFFF-0005-arangod";
      }

      hash = internal.base64Encode(hash);
    } else {
      try {
        var result = arango.GET('/_admin/status?overview=true');
        version = result.version;
        hash = result.hash;
        engine = result.engine;
        platform = result.platform;
        license = result.license;
      } catch (err) {
        if (console && console.debug) {
          console.debug('cannot check for newer version: ', err.stack);
        }
      }
    }

    var u =
      'https://www.arangodb.com/versions.php?'
        + 'version=' + encodeURIComponent(version)
        + '&platform=' + encodeURIComponent(platform)
        + '&engine=' + encodeURIComponent(engine)
        + '&license=' + encodeURIComponent(license)
        + '&source=' + (isServer ? "arangod" : "arangosh")
        + '&hash=' + encodeURIComponent(hash);
    var d2 = internal.download(u, '', {timeout: 3});
    var v = JSON.parse(d2.body);

    if (!isServer && internal.quiet !== true) {
      if (v.hasOwnProperty('bugfix')) {
        log(
          "Please note that a new bugfix version '" +
            v.bugfix.version +
            "' is available"
        );
      }

      if (v.hasOwnProperty('minor')) {
        log(
          "Please note that a new minor version '" +
            v.minor.version +
            "' is available"
        );
      }

      if (v.hasOwnProperty('major')) {
        log(
          "Please note that a new major version '" +
            v.major.version +
            "' is available"
        );
      }
    }
  } catch (err) {
    if (console && console.debug) {
      console.debug('cannot check for newer version: ', err.stack);
    }
  }
};

exports.query = function query (strings, ...args) {
  if (!Array.isArray(strings)) {
    const options = strings;
    const extra = args;
    return function queryWithOptions(strings, ...args) {
      return internal.db._query(exports.aql(strings, ...args), options, ...extra);
    };
  }
  return internal.db._query(exports.aql(strings, ...args));
};
