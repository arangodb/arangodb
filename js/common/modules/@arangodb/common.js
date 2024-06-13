/* global arango:true, ArangoClusterInfo */

'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Dr. Frank Celler
// / @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
// / @author Copyright 2020, ArangoDB GmbH, Cologne, Germany
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
    if (rawValue && (rawValue.isArangoCollection || rawValue.isArangoView)) {
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
// / @brief checks server's license status
// //////////////////////////////////////////////////////////////////////////////

exports.enterpriseLicenseVisibility = function() {

  // This is only a best effort to inform the customer of any upcoming / existing
  // enterprise license issues. If the counterpart does not expose the license
  // API (non-enterprise and <= 3.9), nothing is displayed.

  let license;
  try {
    license = arango.GET("/_admin/license");
  } catch (e) {
    return;
  }

  if (license.hasOwnProperty("status") &&
      license.hasOwnProperty("features") &&
      license.features.hasOwnProperty("expires")) {

    let expires = new Date(1000 * license.features.expires);
    switch (license.status) {
    case "expiring":
      console.warn("Your license is expiring soon on " + expires + ".");
      console.warn("Please contact your ArangoDB sales representative or sales@arangodb.com to renew your license.");
      return;
    case "expired":
      let readonly = new Date(1000 * (license.features.expires + 3600 * 24 * 14));
      console.error("Your server's license has expired.");
      console.error("Its operation will be restricted to read-only mode on " + readonly + "!");
      console.error("Please contact your ArangoDB sales representative or sales@arangodb.com to renew your license immediately.");
      return;
    case "read-only":
      console.error("Your server's license expired on " + expires + ".");
      console.error("Its operation has been restricted to read-only mode!");
      console.error("Please contact your ArangoDB sales representative or sales@arangodb.com to renew your license immediately.");
      return;
    case "good":
    default:
      return;
    }

  }

};

// //////////////////////////////////////////////////////////////////////////////
// / @brief generate info message for newer version(s) available
// //////////////////////////////////////////////////////////////////////////////

exports.checkAvailableVersions = function() {
  let version = internal.version;
  let isServer = require('@arangodb').isServer;
  let console = require('console');
  let log;

  if (isServer) {
    log = console.info;
  } else {
    log = internal.print;
  }

  if (internal.isEnterprise()) {
    exports.enterpriseLicenseVisibility();
  }

  if (version.match(/beta|alpha|preview|milestone|devel/) !== null && internal.quiet !== true) {
    log(
      "You are using a milestone/alpha/beta/preview version ('" +
        version +
        "') of ArangoDB"
    );
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
