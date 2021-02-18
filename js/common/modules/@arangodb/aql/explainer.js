/* jshint strict: false, maxlen: 300 */
/* global arango */

var db = require('@arangodb').db,
  internal = require('internal'),
  _ = require('lodash'),
  systemColors = internal.COLORS,
  print = internal.print,
  output = internal.output,
  colors = {};
var console = require('console');

// max elements to print from array/objects
const maxMembersToPrint = 20;

let uniqueValue = 0;

const isCoordinator = function () {
  let isCoordinator = false;
  if (internal.isArangod()) {
    isCoordinator = require('@arangodb/cluster').isCoordinator();
  } else {
    try {
      if (arango) {
        var result = arango.GET('/_admin/server/role');
        if (result.role === 'COORDINATOR') {
          isCoordinator = true;
        }
      }
    } catch (err) {
      // ignore error
    }
  }
  return isCoordinator;
};

const anonymize = function (doc) {
  if (Array.isArray(doc)) {
    return doc.map(anonymize);
  }
  if (typeof doc === 'string') {
    // make unique values because of unique indexes
    return Array(doc.length + 1).join('X') + uniqueValue++;
  }
  if (doc === null || typeof doc === 'number' || typeof doc === 'boolean') {
    return doc;
  }
  if (typeof doc === 'object') {
    let result = {};
    Object.keys(doc).forEach(function (key) {
      if (key.startsWith('_') || key.startsWith('@')) {
        // This excludes system attributes in examples
        // and collections in bindVars
        result[key] = doc[key];
      } else {
        result[key] = anonymize(doc[key]);
      }
    });
    return result;
  }
  return doc;
};

let stringBuilder = {
  output: '',
  prefix: '',

  appendLine: function (line) {
    if (line) {
      this.output += this.prefix + line;
    }
    this.output += '\n';
  },

  getOutput: function () {
    return this.output;
  },

  clearOutput: function () {
    this.output = '';
  },

  wrap: function (str, width) {
    let re = '.{1,' + width + '}(\\s|$)|\\S+?(\\s|$)';
    return str.match(new RegExp(re, 'g')).join('\n' + this.prefix).replace(/\n+/g, '\n ' + this.prefix);
  }

};

/* set colors for output */
function setColors(useSystemColors) {
  'use strict';

  ['COLOR_RESET',
    'COLOR_CYAN', 'COLOR_BLUE', 'COLOR_GREEN', 'COLOR_MAGENTA', 'COLOR_YELLOW', 'COLOR_RED', 'COLOR_WHITE',
    'COLOR_BOLD_CYAN', 'COLOR_BOLD_BLUE', 'COLOR_BOLD_GREEN', 'COLOR_BOLD_MAGENTA', 'COLOR_BOLD_YELLOW',
    'COLOR_BOLD_RED', 'COLOR_BOLD_WHITE'].forEach(function (c) {
      colors[c] = useSystemColors ? systemColors[c] : '';
    });
}

/* colorizer and output helper functions */

function bracketize(node, v) {
  'use strict';
  if (node && node.subNodes && node.subNodes.length > 1) {
    return '(' + v + ')';
  }
  return v;
}

function attributeUncolored(v) {
  'use strict';
  return '`' + v + '`';
}

function keyword(v) {
  'use strict';
  return colors.COLOR_CYAN + v + colors.COLOR_RESET;
}

function annotation(v) {
  'use strict';
  return colors.COLOR_BLUE + v + colors.COLOR_RESET;
}

function value(v) {
  'use strict';
  if (typeof v === 'string' && v.length > 1024) {
    return colors.COLOR_GREEN + v.substr(0, 1024) + '...' + colors.COLOR_RESET;
  }
  return colors.COLOR_GREEN + v + colors.COLOR_RESET;
}

function variable(v) {
  'use strict';
  if (v[0] === '#') {
    return colors.COLOR_MAGENTA + v + colors.COLOR_RESET;
  }
  return colors.COLOR_YELLOW + v + colors.COLOR_RESET;
}

function func(v) {
  'use strict';
  return colors.COLOR_GREEN + v + colors.COLOR_RESET;
}

function collection(v) {
  'use strict';
  return colors.COLOR_RED + v + colors.COLOR_RESET;
}

function view(v) {
  'use strict';
  return colors.COLOR_RED + v + colors.COLOR_RESET;
}

function attribute(v) {
  'use strict';
  return '`' + colors.COLOR_YELLOW + v + colors.COLOR_RESET + '`';
}

function header(v) {
  'use strict';
  return colors.COLOR_MAGENTA + v + colors.COLOR_RESET;
}

function section(v) {
  'use strict';
  return colors.COLOR_BOLD_BLUE + v + colors.COLOR_RESET;
}

// return n times ' '
function pad(n) {
  'use strict';
  if (n < 0) {
    // value seems invalid...
    n = 0;
  }
  return new Array(n).join(' ');
}

/* print query string */
function printQuery(query, cacheable) {
  'use strict';
  // restrict max length of printed query to avoid endless printing for
  // very long query strings
  var maxLength = 4096, headline = 'Query String (' + query.length + ' chars';
  if (query.length > maxLength) {
    headline += ' - truncated...';
    query = query.substr(0, maxLength / 2) + ' ... ' + query.substr(query.length - maxLength / 2);
  }
  headline += ', cacheable: ' + (cacheable ? 'true' : 'false');
  headline += '):';
  stringBuilder.appendLine(section(headline));
  stringBuilder.appendLine(' ' + value(stringBuilder.wrap(query, 100)));
  stringBuilder.appendLine();
}

/* print write query modification flags */
function printModificationFlags(flags) {
  'use strict';
  if (flags === undefined) {
    return;
  }
  stringBuilder.appendLine(section('Write query options:'));
  var keys = Object.keys(flags), maxLen = 'Option'.length;
  keys.forEach(function (k) {
    if (k.length > maxLen) {
      maxLen = k.length;
    }
  });
  stringBuilder.appendLine(' ' + header('Option') + pad(1 + maxLen - 'Option'.length) + '   ' + header('Value'));
  keys.forEach(function (k) {
    stringBuilder.appendLine(' ' + keyword(k) + pad(1 + maxLen - k.length) + '   ' + value(JSON.stringify(flags[k])));
  });
  stringBuilder.appendLine();
}

/* print optimizer rules */
function printRules(rules, stats) {
  'use strict';

  const maxIdLen = String('Id').length;
  stringBuilder.appendLine(section('Optimization rules applied:'));
  if (rules.length === 0) {
    stringBuilder.appendLine(' ' + value('none'));
  } else {
    stringBuilder.appendLine(' ' + pad(1 + maxIdLen - String('Id').length) + header('Id') + '   ' + header('RuleName'));
    for (let i = 0; i < rules.length; ++i) {
      stringBuilder.appendLine(' ' + pad(1 + maxIdLen - String(i + 1).length) + variable(String(i + 1)) + '   ' + keyword(rules[i]));
    }
  }
  stringBuilder.appendLine();
 
  if (!stats || !stats.rules) {
    return;
  }

  let maxNameLength = 0;
  let times = Object.keys(stats.rules).map(function(key) {
    if (key.length > maxNameLength) {
      maxNameLength = key.length;
    }
    return { name: key, time: stats.rules[key] };
  });
  times.sort(function(l, r) {
    // highest cost first
    return r.time - l.time;
  });
  // top few only
  times = times.slice(0, 5);

  stringBuilder.appendLine(section('Optimization rules with highest execution times:'));
  stringBuilder.appendLine(' ' + header('RuleName') + '   ' + pad(maxNameLength - 'RuleName'.length) + header('Duration [s]'));
  times.forEach(function(rule) {
    stringBuilder.appendLine(' ' + keyword(rule.name) + '   ' + pad(12 + maxNameLength - rule.name.length - rule.time.toFixed(5).length) + value(rule.time.toFixed(5)));
  });
  
  stringBuilder.appendLine();
  stringBuilder.appendLine(value(stats.rulesExecuted) + annotation(' rule(s) executed, ') + value(stats.plansCreated) + annotation(' plan(s) created'));
  stringBuilder.appendLine();
}

/* print warnings */
function printWarnings(warnings) {
  'use strict';
  if (!Array.isArray(warnings) || warnings.length === 0) {
    return;
  }

  stringBuilder.appendLine(section('Warnings:'));
  var maxIdLen = String('Code').length;
  stringBuilder.appendLine(' ' + pad(1 + maxIdLen - String('Code').length) + header('Code') + '   ' + header('Message'));
  for (var i = 0; i < warnings.length; ++i) {
    stringBuilder.appendLine(' ' + pad(1 + maxIdLen - String(warnings[i].code).length) + variable(warnings[i].code) + '   ' + keyword(warnings[i].message));
  }
  stringBuilder.appendLine();
}

/* print stats */
function printStats(stats) {
  'use strict';
  if (!stats) {
    return;
  }

  stringBuilder.appendLine(section('Query Statistics:'));
  var maxWELen = String('Writes Exec').length;
  var maxWILen = String('Writes Ign').length;
  var maxSFLen = String('Scan Full').length;
  var maxSILen = String('Scan Index').length;
  var maxFLen = String('Filtered').length;
  var maxETen = String('Exec Time [s]').length;
  stats.executionTime = stats.executionTime.toFixed(5);
  stringBuilder.appendLine(' ' + header('Writes Exec') + '   ' + header('Writes Ign') + '   ' + header('Scan Full') + '   ' +
    header('Scan Index') + '   ' + header('Filtered') + '   ' + header('Exec Time [s]'));

  stringBuilder.appendLine(' ' + pad(1 + maxWELen - String(stats.writesExecuted).length) + value(stats.writesExecuted) + '   ' +
    pad(1 + maxWILen - String(stats.writesIgnored).length) + value(stats.writesIgnored) + '   ' +
    pad(1 + maxSFLen - String(stats.scannedFull).length) + value(stats.scannedFull) + '   ' +
    pad(1 + maxSILen - String(stats.scannedIndex).length) + value(stats.scannedIndex) + '   ' +
    pad(1 + maxFLen - String(stats.filtered).length) + value(stats.filtered) + '   ' +
    pad(1 + maxETen - String(stats.executionTime).length) + value(stats.executionTime));
  stringBuilder.appendLine();
}

function printProfile(profile) {
  'use strict';
  if (!profile) {
    return;
  }

  stringBuilder.appendLine(section('Query Profile:'));
  let maxHeadLen = 0;
  let maxDurLen = 'Duration [s]'.length;
  Object.keys(profile).forEach(key => {
    if (key.length > maxHeadLen) {
      maxHeadLen = key.length;
    }
    if (profile[key].toFixed(5).length > maxDurLen) {
      maxDurLen = profile[key].toFixed(5).length;
    }
  });
  stringBuilder.appendLine(' ' + header('Query Stage') + pad(1 + maxHeadLen - String('Query Stage').length) + '   ' + pad(1 + maxDurLen - 'Duration [s]'.length) + header('Duration [s]'));
  Object.keys(profile).forEach(key => {
    stringBuilder.appendLine(' ' + keyword(key) + pad(1 + maxHeadLen - String(key).length) + '   ' + pad(1 + maxDurLen - profile[key].toFixed(5).length) + value(profile[key].toFixed(5)));
  });
  stringBuilder.appendLine();
}

/* print indexes used */
function printIndexes(indexes) {
  'use strict';

  stringBuilder.appendLine(section('Indexes used:'));

  if (indexes.length === 0) {
    stringBuilder.appendLine(' ' + value('none'));
  } else {
    var maxIdLen = String('By').length;
    var maxCollectionLen = String('Collection').length;
    var maxUniqueLen = String('Unique').length;
    var maxSparseLen = String('Sparse').length;
    var maxNameLen = String('Name').length;
    var maxTypeLen = String('Type').length;
    var maxSelectivityLen = String('Selectivity').length;
    var maxFieldsLen = String('Fields').length;
    indexes.forEach(function (index) {
      var l = String(index.node).length;
      if (l > maxIdLen) {
        maxIdLen = l;
      }
      l = index.name ? index.name.length : 0;
      if (l > maxNameLen) {
        maxNameLen = l;
      }
      l = index.type.length;
      if (l > maxTypeLen) {
        maxTypeLen = l;
      }
      l = index.fields.map(attributeUncolored).join(', ').length + '[  ]'.length;
      if (l > maxFieldsLen) {
        maxFieldsLen = l;
      }
      l = index.collection.length;
      if (l > maxCollectionLen) {
        maxCollectionLen = l;
      }
    });
    var line = ' ' + pad(1 + maxIdLen - String('By').length) + header('By') + '   ' +
      header('Name') + pad(1 + maxNameLen - 'Name'.length) + '   ' +
      header('Type') + pad(1 + maxTypeLen - 'Type'.length) + '   ' +
      header('Collection') + pad(1 + maxCollectionLen - 'Collection'.length) + '   ' +
      header('Unique') + pad(1 + maxUniqueLen - 'Unique'.length) + '   ' +
      header('Sparse') + pad(1 + maxSparseLen - 'Sparse'.length) + '   ' +
      header('Selectivity') + '   ' +
      header('Fields') + pad(1 + maxFieldsLen - 'Fields'.length) + '   ' +
      header('Ranges');

    stringBuilder.appendLine(line);

    for (var i = 0; i < indexes.length; ++i) {
      var uniqueness = (indexes[i].unique ? 'true' : 'false');
      var sparsity = (indexes[i].hasOwnProperty('sparse') ? (indexes[i].sparse ? 'true' : 'false') : 'n/a');
      var fields = '[ ' + indexes[i].fields.map(attribute).join(', ') + ' ]';
      var fieldsLen = indexes[i].fields.map(attributeUncolored).join(', ').length + '[  ]'.length;
      var ranges;
      if (indexes[i].hasOwnProperty('condition')) {
        ranges = indexes[i].condition;
      } else {
        ranges = '[ ' + indexes[i].ranges + ' ]';
      }

      let estimate;
      if (indexes[i].hasOwnProperty('selectivityEstimate')) {
        estimate = (indexes[i].selectivityEstimate * 100).toFixed(2) + ' %';
      } else if (indexes[i].unique) {
        // hard-code estimate to 100%
        estimate = '100.00 %';
      } else {
        estimate = 'n/a';
      }

      line = ' ' +
        pad(1 + maxIdLen - String(indexes[i].node).length) + variable(String(indexes[i].node)) + '   ' +
        collection(indexes[i].name) + pad(1 + maxNameLen - indexes[i].name.length) + '   ' +
        keyword(indexes[i].type) + pad(1 + maxTypeLen - indexes[i].type.length) + '   ' +
        collection(indexes[i].collection) + pad(1 + maxCollectionLen - indexes[i].collection.length) + '   ' +
        value(uniqueness) + pad(1 + maxUniqueLen - uniqueness.length) + '   ' +
        value(sparsity) + pad(1 + maxSparseLen - sparsity.length) + '   ' +
        pad(1 + maxSelectivityLen - estimate.length) + value(estimate) + '   ' +
        fields + pad(1 + maxFieldsLen - fieldsLen) + '   ' +
        ranges;

      stringBuilder.appendLine(line);
    }
  }
}

function printFunctions(functions) {
  'use strict';

  let funcArray = [];
  Object.keys(functions).forEach(function (f) {
    funcArray.push(functions[f]);
  });

  if (funcArray.length === 0) {
    return;
  }
  stringBuilder.appendLine();
  stringBuilder.appendLine(section('Functions used:'));

  let maxNameLen = String('Name').length;
  let maxDeterministicLen = String('Deterministic').length;
  let maxCacheableLen = String('Cacheable').length;
  let maxV8Len = String('Uses V8').length;
  funcArray.forEach(function (f) {
    let l = String(f.name).length;
    if (l > maxNameLen) {
      maxNameLen = l;
    }
  });
  let line = ' ' +
    header('Name') + pad(1 + maxNameLen - 'Name'.length) + '   ' +
    header('Deterministic') + pad(1 + maxDeterministicLen - 'Deterministic'.length) + '   ' +
    header('Cacheable') + pad(1 + maxCacheableLen - 'Cacheable'.length) + '   ' +
    header('Uses V8') + pad(1 + maxV8Len - 'Uses V8'.length);

  stringBuilder.appendLine(line);

  for (var i = 0; i < funcArray.length; ++i) {
    // prevent "undefined"
    let deterministic = String(funcArray[i].isDeterministic || false);
    let cacheable = String(funcArray[i].cacheable || false);
    let usesV8 = String(funcArray[i].usesV8 || false);
    line = ' ' +
      variable(funcArray[i].name) + pad(1 + maxNameLen - funcArray[i].name.length) + '   ' +
      value(deterministic) + pad(1 + maxDeterministicLen - deterministic.length) + '   ' +
      value(cacheable) + pad(1 + maxCacheableLen - cacheable.length) + '   ' +
      value(usesV8) + pad(1 + maxV8Len - usesV8.length);

    stringBuilder.appendLine(line);
  }
}

/* create a table with a given amount of columns and arbitrary many rows */
class PrintedTable {
  constructor(numColumns) {
    this.content = [];
    for (let i = 0; i < numColumns; ++i) {
      this.content.push({
        header: "",
        cells: [],
        size: 0
      });
    }
  }

  setHeader(index, value) {
    this.content[index].header = value;
    this.content[index].size = Math.max(this.content[index].size, value.length);
  }

  addCell(index, value, valueLength) {
    // Value might be empty
    value = value || "";
    valueLength = valueLength || value.length;
    this.content[index].cells.push({ formatted: value, size: valueLength });
    this.content[index].size = Math.max(this.content[index].size, valueLength);
  }

  alignNewEntry() {
    let rowsNeeded = Math.max(...this.content.map(c => c.cells.length));
    for (let c of this.content) {
      while (c.cells.length < rowsNeeded) {
        c.cells.push({ formatted: '', size: 0 });
      }
    }
  }

  print(builder) {
    let rowsNeeded = Math.max(...this.content.map(c => c.cells.length));
    // Print the header
    let line = ' ';
    let isFirst = true;
    for (let c of this.content) {
      line += (isFirst ? '' : pad(3)) + header(c.header) + pad(1 + c.size - c.header.length);
      isFirst = false;
    }
    builder.appendLine(line);

    // Print the cells
    for (let i = 0; i < rowsNeeded; ++i) {
      let line = ' ';
      let isFirst = true;
      for (let c of this.content) {
        if (c.cells.length > i) {
          line += (isFirst ? '' : pad(3)) + c.cells[i].formatted + pad(1 + c.size - c.cells[i].size);
        } else {
          line += (isFirst ? '' : pad(3)) + pad(1 + c.size);
        }
        isFirst = false;
      }
      builder.appendLine(line);
    }
  }
}

/* print traversal info */
function printTraversalDetails(traversals) {
  'use strict';
  if (traversals.length === 0) {
    return;
  }

  stringBuilder.appendLine();
  stringBuilder.appendLine(section('Traversals on graphs:'));

  let outTable = new PrintedTable(6);
  outTable.setHeader(0, 'Id');
  outTable.setHeader(1, 'Depth');
  outTable.setHeader(2, 'Vertex collections');
  outTable.setHeader(3, 'Edge collections');
  outTable.setHeader(4, 'Options');
  outTable.setHeader(5, 'Filter / Prune Conditions');


  var optify = function (options, colorize) {
    var opts = {
      bfs: options.bfs || undefined, /* only print if set to true to save room */
      neighbors: options.neighbors || undefined, /* only print if set to true to save room */
      uniqueVertices: options.uniqueVertices,
      uniqueEdges: options.uniqueEdges
    };

    var result = '';
    for (var att in opts) {
      if (result.length > 0) {
        result += ', ';
      }
      if (opts[att] === undefined) {
        continue;
      }
      if (colorize) {
        result += keyword(att) + ': ';
        if (typeof opts[att] === 'boolean') {
          result += value(opts[att] ? 'true' : 'false');
        } else {
          result += value(String(opts[att]));
        }
      } else {
        result += att + ': ';
        if (typeof opts[att] === 'boolean') {
          result += opts[att] ? 'true' : 'false';
        } else {
          result += String(opts[att]);
        }
      }
    }
    return result;
  };

  traversals.forEach(node => {
    outTable.alignNewEntry();
    outTable.addCell(0, String(node.id));
    outTable.addCell(1, node.minMaxDepth);
    outTable.addCell(2, node.vertexCollectionNameStr, node.vertexCollectionNameStrLen);
    outTable.addCell(3, node.edgeCollectionNameStr, node.edgeCollectionNameStrLen);
    if (node.hasOwnProperty('options')) {
      outTable.addCell(4, optify(node.options, true), optify(node.options, false).length);
    } else if (node.hasOwnProperty('traversalFlags')) {
      outTable.addCell(4, optify(node.traversalFlags, true), optify(node.options, false).length);
    }
    // else do not add a cell in 4
    if (node.hasOwnProperty('ConditionStr')) {
      outTable.addCell(5, 'FILTER ' + node.ConditionStr);
    }
    if (node.hasOwnProperty('PruneConditionStr')) {
      outTable.addCell(5, 'PRUNE ' + node.PruneConditionStr);
    }
  });
  outTable.print(stringBuilder);
}

/* print shortest_path info */
function printShortestPathDetails(shortestPaths) {
  'use strict';
  if (shortestPaths.length === 0) {
    return;
  }

  stringBuilder.appendLine();
  stringBuilder.appendLine(section('Shortest paths on graphs:'));

  var maxIdLen = String('Id').length;
  var maxVertexCollectionNameStrLen = String('Vertex collections').length;
  var maxEdgeCollectionNameStrLen = String('Edge collections').length;

  shortestPaths.forEach(function (node) {
    var l = String(node.id).length;
    if (l > maxIdLen) {
      maxIdLen = l;
    }

    if (node.hasOwnProperty('vertexCollectionNameStr')) {
      if (node.vertexCollectionNameStrLen > maxVertexCollectionNameStrLen) {
        maxVertexCollectionNameStrLen = node.vertexCollectionNameStrLen;
      }
    }
    if (node.hasOwnProperty('edgeCollectionNameStr')) {
      if (node.edgeCollectionNameStrLen > maxEdgeCollectionNameStrLen) {
        maxEdgeCollectionNameStrLen = node.edgeCollectionNameStrLen;
      }
    }
  });

  var line = ' ' + pad(1 + maxIdLen - String('Id').length) + header('Id') + '   ' +
    header('Vertex collections') + pad(1 + maxVertexCollectionNameStrLen - 'Vertex collections'.length) + '   ' +
    header('Edge collections') + pad(1 + maxEdgeCollectionNameStrLen - 'Edge collections'.length);

  stringBuilder.appendLine(line);

  for (let sp of shortestPaths) {
    line = ' ' + pad(1 + maxIdLen - String(sp.id).length) + sp.id + '   ';

    if (sp.hasOwnProperty('vertexCollectionNameStr')) {
      line += sp.vertexCollectionNameStr +
        pad(1 + maxVertexCollectionNameStrLen - sp.vertexCollectionNameStrLen) + '   ';
    } else {
      line += pad(1 + maxVertexCollectionNameStrLen) + '   ';
    }

    if (sp.hasOwnProperty('edgeCollectionNameStr')) {
      line += sp.edgeCollectionNameStr +
        pad(1 + maxEdgeCollectionNameStrLen - sp.edgeCollectionNameStrLen) + '   ';
    } else {
      line += pad(1 + maxEdgeCollectionNameStrLen) + '   ';
    }

    if (sp.hasOwnProperty('ConditionStr')) {
      line += sp.ConditionStr;
    }

    stringBuilder.appendLine(line);
  }
}

function printKShortestPathsDetails(shortestPaths) {
  'use strict';
  if (shortestPaths.length === 0) {
    return;
  }

  stringBuilder.appendLine();
  stringBuilder.appendLine(section('k shortest paths on graphs:'));

  var maxIdLen = String('Id').length;
  var maxVertexCollectionNameStrLen = String('Vertex collections').length;
  var maxEdgeCollectionNameStrLen = String('Edge collections').length;

  shortestPaths.forEach(function (node) {
    var l = String(node.id).length;
    if (l > maxIdLen) {
      maxIdLen = l;
    }

    if (node.hasOwnProperty('vertexCollectionNameStr')) {
      if (node.vertexCollectionNameStrLen > maxVertexCollectionNameStrLen) {
        maxVertexCollectionNameStrLen = node.vertexCollectionNameStrLen;
      }
    }
    if (node.hasOwnProperty('edgeCollectionNameStr')) {
      if (node.edgeCollectionNameStrLen > maxEdgeCollectionNameStrLen) {
        maxEdgeCollectionNameStrLen = node.edgeCollectionNameStrLen;
      }
    }
  });

  var line = ' ' + pad(1 + maxIdLen - String('Id').length) + header('Id') + '   ' +
    header('Vertex collections') + pad(1 + maxVertexCollectionNameStrLen - 'Vertex collections'.length) + '   ' +
    header('Edge collections') + pad(1 + maxEdgeCollectionNameStrLen - 'Edge collections'.length);

  stringBuilder.appendLine(line);

  for (let sp of shortestPaths) {
    line = ' ' + pad(1 + maxIdLen - String(sp.id).length) + sp.id + '   ';

    if (sp.hasOwnProperty('vertexCollectionNameStr')) {
      line += sp.vertexCollectionNameStr +
        pad(1 + maxVertexCollectionNameStrLen - sp.vertexCollectionNameStrLen) + '   ';
    } else {
      line += pad(1 + maxVertexCollectionNameStrLen) + '   ';
    }

    if (sp.hasOwnProperty('edgeCollectionNameStr')) {
      line += sp.edgeCollectionNameStr +
        pad(1 + maxEdgeCollectionNameStrLen - sp.edgeCollectionNameStrLen) + '   ';
    } else {
      line += pad(1 + maxEdgeCollectionNameStrLen) + '   ';
    }

    if (sp.hasOwnProperty('ConditionStr')) {
      line += sp.ConditionStr;
    }

    stringBuilder.appendLine(line);
  }
}


/* analyze and print execution plan */
function processQuery(query, explain, planIndex) {
  'use strict';
  var nodes = {},
    parents = {},
    rootNode = null,
    maxTypeLen = 0,
    maxSiteLen = 0,
    maxIdLen = String('Id').length,
    maxEstimateLen = String('Est.').length,
    maxCallsLen = String('Calls').length,
    maxItemsLen = String('Items').length,
    maxRuntimeLen = String('Runtime [s]').length,
    stats = explain.stats;

  let plan = explain.plan;
  if (planIndex !== undefined) {
    plan = explain.plans[planIndex];
  }

  /// mode with actual runtime stats per node
  let profileMode = stats && stats.hasOwnProperty('nodes');

  var recursiveWalk = function (partNodes, level, site) {
    let n = _.clone(partNodes);
    n.reverse();
    n.forEach(function (node) {
      // set location of execution node in cluster
      node.site = site;

      nodes[node.id] = node;
      if (level === 0 && node.dependencies.length === 0) {
        rootNode = node.id;
      }
      if (node.type === 'SubqueryNode') {
        // enter subquery
        recursiveWalk(node.subquery.nodes, level + 1, site);
      } else if (node.type === 'RemoteNode') {
        site = (site === 'COOR' ? 'DBS' : 'COOR');
      }
      node.dependencies.forEach(function (d) {
        if (!parents.hasOwnProperty(d)) {
          parents[d] = [];
        }
        parents[d].push(node.id);
      });

      if (String(node.id).length > maxIdLen) {
        maxIdLen = String(node.id).length;
      }
      if (String(node.type).length > maxTypeLen) {
        maxTypeLen = String(node.type).length;
      }
      if (String(node.site).length > maxSiteLen) {
        maxSiteLen = String(node.site).length;
      }
      if (!profileMode) { // not shown when we got actual runtime stats
        if (String(node.estimatedNrItems).length > maxEstimateLen) {
          maxEstimateLen = String(node.estimatedNrItems).length;
        }
      }
    });
  };
  recursiveWalk(plan.nodes, 0, 'COOR');

  if (profileMode) { // merge runtime info into plan
    stats.nodes.forEach(n => {
      if (nodes.hasOwnProperty(n.id)) {
        nodes[n.id].calls = (n.calls === undefined ? "n/a" : n.calls);
        nodes[n.id].items = (n.items === undefined ? "n/a" : n.items);
        nodes[n.id].runtime = (n.runtime === undefined ? "n/a" : n.runtime);

        if (String(nodes[n.id].calls).length > maxCallsLen) {
          maxCallsLen = String(nodes[n.id].calls).length;
        }
        if (String(nodes[n.id].items).length > maxItemsLen) {
          maxItemsLen = String(nodes[n.id].items).length;
        }
        let l;
        if (typeof nodes[n.id].runtime === 'number') {
          l = String(nodes[n.id].runtime.toFixed(3)).length;
        } else {
          l = nodes[n.id].runtime.length;
        }
        if (l > maxRuntimeLen) {
          maxRuntimeLen = l;
        }
      }
    });
    // by design the runtime is cumulative right now.
    // by subtracting the dependencies from parent runtime we get the runtime per node
    stats.nodes.forEach(n => {
      if (typeof n.runtime === 'number') {
        if (parents.hasOwnProperty(n.id)) {
          parents[n.id].forEach(pid => {
            if (typeof nodes[pid].runtime === 'number') {
              nodes[pid].runtime -= n.runtime;
            }
          });
        }
      }
    });
  }

  var references = {},
    collectionVariables = {},
    usedVariables = {},
    indexes = [],
    traversalDetails = [],
    shortestPathDetails = [],
    kShortestPathsDetails = [],
    functions = [],
    modificationFlags,
    isConst = true,
    currentNode = null;

  var variableName = function (node) {
    try {
      if (/^[0-9_]/.test(node.name)) {
        return variable('#' + node.name);
      }
    } catch (x) {
      require('console').warn("got unexpected invalid variable struct in explainer");
      require("console").trace();
      throw x;
    }

    if (collectionVariables.hasOwnProperty(node.id)) {
      usedVariables[node.name] = collectionVariables[node.id];
    }
    return variable(node.name);
  };

  var lateVariableCallback = function () {
    require("internal").print("Called");
    return (node) => variableName(node);
  };

  var buildExpression = function (node) {
    var binaryOperator = function (node, name) {
      if (name.match(/^[a-zA-Z]+$/)) {
        // make it a keyword
        name = keyword(name.toUpperCase());
      }
      var lhs = buildExpression(node.subNodes[0]);
      var rhs = buildExpression(node.subNodes[1]);
      if (node.subNodes.length === 3) {
        // array operator node... prepend "all" | "any" | "none" to node type
        name = node.subNodes[2].quantifier + ' ' + name;
      }
      if (node.sorted) {
        return lhs + ' ' + name + ' ' + annotation('/* sorted */') + ' ' + rhs;
      }
      return lhs + ' ' + name + ' ' + rhs;
    };

    isConst = isConst && (['value', 'object', 'object element', 'array'].indexOf(node.type) !== -1);

    switch (node.type) {
      case 'reference':
        if (references.hasOwnProperty(node.name)) {
          var ref = references[node.name];
          delete references[node.name];
          if (Array.isArray(ref)) {
            var out = buildExpression(ref[1]) + '[' + (new Array(ref[0] + 1).join('*'));
            if (ref[2].type !== 'no-op') {
              out += ' ' + keyword('FILTER') + ' ' + buildExpression(ref[2]);
            }
            if (ref[3].type !== 'no-op') {
              out += ' ' + keyword('LIMIT ') + ' ' + buildExpression(ref[3]);
            }
            if (ref[4].type !== 'no-op') {
              out += ' ' + keyword('RETURN ') + ' ' + buildExpression(ref[4]);
            }
            out += ']';
            return out;
          }
          return buildExpression(ref) + '[*]';
        }
        return variableName(node);
      case 'collection':
        return collection(node.name) + '   ' + annotation('/* all collection documents */');
      case 'value':
        return value(JSON.stringify(node.value));
      case 'object':
        if (node.hasOwnProperty('subNodes')) {
          if (node.subNodes.length > maxMembersToPrint) {
            // print only the first few values from the object
            return '{ ' + node.subNodes.slice(0, maxMembersToPrint).map(buildExpression).join(', ') + ', ... }';
          }
          return '{ ' + node.subNodes.map(buildExpression).join(', ') + ' }';
        }
        return '{ }';
      case 'object element':
        return value(JSON.stringify(node.name)) + ' : ' + buildExpression(node.subNodes[0]);
      case 'calculated object element':
        return '[ ' + buildExpression(node.subNodes[0]) + ' ] : ' + buildExpression(node.subNodes[1]);
      case 'array':
        if (node.hasOwnProperty('subNodes')) {
          if (node.subNodes.length > maxMembersToPrint) {
            // print only the first few values from the array
            return '[ ' + node.subNodes.slice(0, maxMembersToPrint).map(buildExpression).join(', ') + ', ... ]';
          }
          return '[ ' + node.subNodes.map(buildExpression).join(', ') + ' ]';
        }
        return '[ ]';
      case 'unary not':
        return '! ' + buildExpression(node.subNodes[0]);
      case 'unary plus':
        return '+ ' + buildExpression(node.subNodes[0]);
      case 'unary minus':
        return '- ' + buildExpression(node.subNodes[0]);
      case 'array limit':
        return buildExpression(node.subNodes[0]) + ', ' + buildExpression(node.subNodes[1]);
      case 'attribute access':
        return buildExpression(node.subNodes[0]) + '.' + attribute(node.name);
      case 'indexed access':
        return buildExpression(node.subNodes[0]) + '[' + buildExpression(node.subNodes[1]) + ']';
      case 'range':
        return buildExpression(node.subNodes[0]) + ' .. ' + buildExpression(node.subNodes[1]) + '   ' + annotation('/* range */');
      case 'expand':
      case 'expansion':
        if (node.subNodes.length > 2) {
          // [FILTER ...]
          references[node.subNodes[0].subNodes[0].name] = [node.levels, node.subNodes[0].subNodes[1], node.subNodes[2], node.subNodes[3], node.subNodes[4]];
        } else {
          // [*]
          references[node.subNodes[0].subNodes[0].name] = node.subNodes[0].subNodes[1];
        }
        return buildExpression(node.subNodes[1]);
      case 'user function call':
        return func(node.name) + '(' + ((node.subNodes && node.subNodes[0].subNodes) || []).map(buildExpression).join(', ') + ')' + '   ' + annotation('/* user-defined function */');
      case 'function call':
        return func(node.name) + '(' + ((node.subNodes && node.subNodes[0].subNodes) || []).map(buildExpression).join(', ') + ')';
      case 'plus':
        return '(' + binaryOperator(node, '+') + ')';
      case 'minus':
        return '(' + binaryOperator(node, '-') + ')';
      case 'times':
        return '(' + binaryOperator(node, '*') + ')';
      case 'division':
        return '(' + binaryOperator(node, '/') + ')';
      case 'modulus':
        return '(' + binaryOperator(node, '%') + ')';
      case 'compare not in':
      case 'array compare not in':
        return '(' + binaryOperator(node, 'not in') + ')';
      case 'compare in':
      case 'array compare in':
        return '(' + binaryOperator(node, 'in') + ')';
      case 'compare ==':
      case 'array compare ==':
        return '(' + binaryOperator(node, '==') + ')';
      case 'compare !=':
      case 'array compare !=':
        return '(' + binaryOperator(node, '!=') + ')';
      case 'compare >':
      case 'array compare >':
        return '(' + binaryOperator(node, '>') + ')';
      case 'compare >=':
      case 'array compare >=':
        return '(' + binaryOperator(node, '>=') + ')';
      case 'compare <':
      case 'array compare <':
        return '(' + binaryOperator(node, '<') + ')';
      case 'compare <=':
      case 'array compare <=':
        return '(' + binaryOperator(node, '<=') + ')';
      case 'logical or':
        return '(' + binaryOperator(node, '||') + ')';
      case 'logical and':
        return '(' + binaryOperator(node, '&&') + ')';
      case 'ternary':
        if (node.subNodes.length === 2) {
          return '(' + buildExpression(node.subNodes[0]) + ' ?: ' + buildExpression(node.subNodes[1]) + ')';
        }
        return '(' + buildExpression(node.subNodes[0]) + ' ? ' + buildExpression(node.subNodes[1]) + ' : ' + buildExpression(node.subNodes[2]) + ')';
      case 'n-ary or':
        if (node.hasOwnProperty('subNodes')) {
          return bracketize(node, node.subNodes.map(function (sub) { return buildExpression(sub); }).join(' || '));
        }
        return '';
      case 'n-ary and':
        if (node.hasOwnProperty('subNodes')) {
          return bracketize(node, node.subNodes.map(function (sub) { return buildExpression(sub); }).join(' && '));
        }
        return '';
      case 'parameter':
      case 'datasource parameter':
        return value('@' + node.name);
      default:
        return 'unhandled node type (' + node.type + ')';
    }
  };

  var buildSimpleExpression = function (simpleExpressions) {
    var rc = '';

    for (var indexNo in simpleExpressions) {
      if (simpleExpressions.hasOwnProperty(indexNo)) {
        if (rc.length > 0) {
          rc += ' AND ';
        }
        for (var i = 0; i < simpleExpressions[indexNo].length; i++) {
          var item = simpleExpressions[indexNo][i];
          rc += attribute('Path') + '.';
          if (item.isEdgeAccess) {
            rc += attribute('edges');
          } else {
            rc += attribute('vertices');
          }
          rc += '[' + value(indexNo) + '] -> ';
          rc += buildExpression(item.varAccess);
          rc += ' ' + item.comparisonTypeStr + ' ';
          rc += buildExpression(item.compareTo);
        }
      }
    }
    return rc;
  };

  var buildBound = function (attr, operators, bound) {
    var boundValue = bound.isConstant ? value(JSON.stringify(bound.bound)) : buildExpression(bound.bound);
    return attribute(attr) + ' ' + operators[bound.include ? 1 : 0] + ' ' + boundValue;
  };

  var buildRanges = function (ranges) {
    var results = [];
    ranges.forEach(function (range) {
      var attr = range.attr;

      if (range.lowConst.hasOwnProperty('bound') && range.highConst.hasOwnProperty('bound') &&
        JSON.stringify(range.lowConst.bound) === JSON.stringify(range.highConst.bound)) {
        range.equality = true;
      }

      if (range.equality) {
        if (range.lowConst.hasOwnProperty('bound')) {
          results.push(buildBound(attr, ['==', '=='], range.lowConst));
        } else if (range.hasOwnProperty('lows')) {
          range.lows.forEach(function (bound) {
            results.push(buildBound(attr, ['==', '=='], bound));
          });
        }
      } else {
        if (range.lowConst.hasOwnProperty('bound')) {
          results.push(buildBound(attr, ['>', '>='], range.lowConst));
        }
        if (range.highConst.hasOwnProperty('bound')) {
          results.push(buildBound(attr, ['<', '<='], range.highConst));
        }
        if (range.hasOwnProperty('lows')) {
          range.lows.forEach(function (bound) {
            results.push(buildBound(attr, ['>', '>='], bound));
          });
        }
        if (range.hasOwnProperty('highs')) {
          range.highs.forEach(function (bound) {
            results.push(buildBound(attr, ['<', '<='], bound));
          });
        }
      }
    });
    if (results.length > 1) {
      return '(' + results.join(' && ') + ')';
    }
    return results[0];
  };

  var projection = function (node) {
    if (node.projections && node.projections.length > 0) {
      let p = node.projections.map(function(p) {
        if (Array.isArray(p)) {
          return p.join('`.`', p);
        }
        return p;
      });
      return ', projections: `' + p.join('`, `') + '`';
    }
    return '';
  };

  const restriction = function (node) {
    if (node.restrictedTo) {
      return `, shard: ${node.restrictedTo}`;
    } else if (node.numberOfShards) {
      return `, ${node.numberOfShards} shard(s)`;
    }
    return '';
  };

  var iterateIndexes = function (idx, i, node, types, variable) {
    var what = (node.reverse ? 'reverse ' : '') + idx.type + ' index scan' + ((node.producesResult || !node.hasOwnProperty('producesResult')) ? (node.indexCoversProjections ? ', index only' : '') : ', scan only');
    if (types.length === 0 || what !== types[types.length - 1]) {
      types.push(what);
    }
    idx.collection = node.collection;
    idx.node = node.id;
    if (node.hasOwnProperty('condition') && node.condition.type && node.condition.type === 'n-ary or') {
      idx.condition = buildExpression(node.condition.subNodes[i]);
    } else {
      if (variable !== false && variable !== undefined) {
        idx.condition = variable;
      }
    }
    if (idx.condition === '' || idx.condition === undefined) {
      idx.condition = '*'; // empty condition. this is likely an index used for sorting or scanning only
    }
    indexes.push(idx);
  };

  var label = function (node) {
    var rc, v, vNames, e, eNames, edgeCols, i, d, directions, isLast;
    var parts = [];
    var types = [];
    var filter = '';
    // index in this array gives the traversal direction
    const translate = ['ANY', 'INBOUND', 'OUTBOUND'];
    switch (node.type) {
      case 'SingletonNode':
        return keyword('ROOT');
      case 'NoResultsNode':
        return keyword('EMPTY') + '   ' + annotation('/* empty result set */');
      case 'EnumerateCollectionNode':
        collectionVariables[node.outVariable.id] = node.collection;
        if (node.filter) {
          filter = '   ' + keyword('FILTER') + ' ' + buildExpression(node.filter) + '   ' + annotation('/* early pruning */');
        }
        return keyword('FOR') + ' ' + variableName(node.outVariable) + ' ' + keyword('IN') + ' ' + collection(node.collection) + '   ' + annotation('/* full collection scan' + (node.random ? ', random order' : '') + projection(node) + (node.satellite ? ', satellite' : '') + ((node.producesResult || !node.hasOwnProperty('producesResult')) ? '' : ', scan only') + (node.count ? ', with count optimization' : '') + `${restriction(node)} */`) + filter;
      case 'EnumerateListNode':
        return keyword('FOR') + ' ' + variableName(node.outVariable) + ' ' + keyword('IN') + ' ' + variableName(node.inVariable) + '   ' + annotation('/* list iteration */');
      case 'EnumerateViewNode':
        var condition = '';
        if (node.condition && node.condition.hasOwnProperty('type')) {
          condition = keyword(' SEARCH ') + buildExpression(node.condition);
        }

        var sortCondition = '';
        if (node.primarySort && Array.isArray(node.primarySort)) {
          var primarySortBuckets = node.primarySort.length;
          if (typeof node.primarySortBuckets === 'number') {
            primarySortBuckets = Math.min(node.primarySortBuckets, primarySortBuckets);
          }

          sortCondition = keyword(' SORT ') + node.primarySort.slice(0, primarySortBuckets).map(function (element) {
            return variableName(node.outVariable) + '.' + attribute(element.field) + ' ' + keyword(element.asc ? 'ASC' : 'DESC');
          }).join(', ');
        }

        var scorers = '';
        if (node.scorers && node.scorers.length > 0) {
          scorers = node.scorers.map(function (scorer) {
            return keyword(' LET ') + variableName(scorer) + ' = ' + buildExpression(scorer.node);
          }).join('');
        }
        let viewAnnotation = '/* view query';
        if (node.hasOwnProperty('outNmDocId') && node.hasOwnProperty('outNmColPtr')) {
          viewAnnotation += ' with late materialization';
        } else if (node.hasOwnProperty('noMaterialization') && node.noMaterialization) {
          viewAnnotation += ' without materialization';
        }
        if (node.hasOwnProperty('options')) {
          if (node.options.hasOwnProperty('countApproximate'))
          viewAnnotation += '. Count mode is ' + node.options.countApproximate;
        }
        viewAnnotation += ' */';
        let viewVariables = '';
        if (node.hasOwnProperty('viewValuesVars') && node.viewValuesVars.length > 0) {
          viewVariables = node.viewValuesVars.map(function (viewValuesColumn) {
            if (viewValuesColumn.hasOwnProperty('field')) {
              return keyword(' LET ') + variableName(viewValuesColumn) + ' = ' + variableName(node.outVariable) + '.' + attribute(viewValuesColumn.field);
            } else {
              return viewValuesColumn.viewStoredValuesVars.map(function (viewValuesVar) {
                return keyword(' LET ') + variableName(viewValuesVar) + ' = ' + variableName(node.outVariable) + '.' + attribute(viewValuesVar.field);
              }).join('');
            }
          }).join('');
        }
        return keyword('FOR ') + variableName(node.outVariable) + keyword(' IN ') +
               view(node.view) + condition + sortCondition + scorers + viewVariables +
               '   ' + annotation(viewAnnotation);
      case 'IndexNode':
        collectionVariables[node.outVariable.id] = node.collection;
        if (node.filter) {
          filter = '   ' + keyword('FILTER') + ' ' + buildExpression(node.filter) + '   ' + annotation('/* early pruning */');
        }
        let indexAnnotation = '';
        let indexVariables = '';
        if (node.hasOwnProperty('outNmDocId')) {
          indexAnnotation += '/* with late materialization */';
          if (node.hasOwnProperty('indexValuesVars') && node.indexValuesVars.length > 0) {
            indexVariables = node.indexValuesVars.map(function (indexValuesVar) {
              return keyword(' LET ') + variableName(indexValuesVar) + ' = ' + variableName(node.outVariable) + '.' + attribute(indexValuesVar.field);
            }).join('');
          }
        }
        if (node.count) {
          indexAnnotation += '/* with count optimization */';
        }
        node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, false); });
        return `${keyword('FOR')} ${variableName(node.outVariable)} ${keyword('IN')} ${collection(node.collection)}` + indexVariables +
          `   ${annotation(`/* ${types.join(', ')}${projection(node)}${node.satellite ? ', satellite' : ''}${restriction(node)} */`)} ` + filter +
          '   ' + annotation(indexAnnotation);

      case 'TraversalNode':
        if (node.hasOwnProperty("options")) {
          node.minMaxDepth = node.options.minDepth + '..' + node.options.maxDepth;
        } else if (node.hasOwnProperty("traversalFlags")) {
          node.minMaxDepth = node.traversalFlags.minDepth + '..' + node.traversalFlags.maxDepth;
        } else {
          node.minMaxDepth = '1..1';
        }
        node.minMaxDepthLen = node.minMaxDepth.length;

        rc = keyword('FOR ');
        if (node.hasOwnProperty('vertexOutVariable')) {
          if (node.options.hasOwnProperty('produceVertices') && !node.options.produceVertices) {
            parts.push(variableName(node.vertexOutVariable) + '  ' + annotation('/* vertex optimized away */'));
          } else {
            parts.push(variableName(node.vertexOutVariable) + '  ' + annotation('/* vertex */'));
          }
        } else {
          parts.push(annotation('/* vertex optimized away */'));
        }
        if (node.hasOwnProperty('edgeOutVariable')) {
          parts.push(variableName(node.edgeOutVariable) + '  ' + annotation('/* edge */'));
        }
        if (node.hasOwnProperty('pathOutVariable')) {
          parts.push(variableName(node.pathOutVariable) + '  ' + annotation('/* paths */'));
        }

        rc += parts.join(', ') + ' ' + keyword('IN') + ' ' +
          value(node.minMaxDepth) + '  ' + annotation('/* min..maxPathDepth */') + ' ';

        directions = [];
        for (i = 0; i < node.edgeCollections.length; ++i) {
          isLast = (i + 1 === node.edgeCollections.length);
          if (!isLast && node.edgeCollections[i] === node.edgeCollections[i + 1]) {
            // direction ANY is represented by two traversals: an INBOUND and an OUTBOUND traversal
            // on the same collection
            directions.push({ collection: node.edgeCollections[i], direction: 0 /* ANY */ });
            ++i;
          } else {
            directions.push({ collection: node.edgeCollections[i], direction: node.directions[i] });
          }
        }
        var allIndexes = [];
        for (i = 0; i < node.edgeCollections.length; ++i) {
          d = node.directions[i];
          // base indexes
          var ix = node.indexes.base[i];
          ix.collection = node.edgeCollections[i];
          ix.condition = keyword("base " + translate[d]);
          ix.level = -1;
          ix.direction = d;
          ix.node = node.id;
          allIndexes.push(ix);

          // level-specific indexes
          for (var l in node.indexes.levels) {
            ix = node.indexes.levels[l][i];
            ix.collection = node.edgeCollections[i];
            ix.condition = keyword("level " + parseInt(l, 10) + " " + translate[d]);
            ix.level = parseInt(l, 10);
            ix.direction = d;
            ix.node = node.id;
            allIndexes.push(ix);
          }
        }

        allIndexes.sort(function (l, r) {
          if (l.collection !== r.collection) {
            return l.collection < r.collection ? -1 : 1;
          }
          if (l.level !== r.level) {
            return l.level < r.level ? -1 : 1;
          }
          if (l.direction !== r.direction) {
            return l.direction < r.direction ? -1 : 1;
          }
          return 0;
        });

        rc += keyword(translate[node.defaultDirection]);
        if (node.hasOwnProperty('vertexId')) {
          rc += " '" + value(node.vertexId) + "' ";
        } else {
          rc += ' ' + variableName(node.inVariable) + ' ';
        }
        rc += annotation('/* startnode */') + '  ';

        if (Array.isArray(node.graph)) {
          if (directions.length > 0) {
            rc += collection(directions[0].collection);
            for (i = 1; i < directions.length; ++i) {
              rc += ', ' + keyword(translate[directions[i].direction]) + ' ' + collection(directions[i].collection);
            }
          } else {
            rc += annotation('/* no edge collections */');
          }
        } else {
          rc += keyword('GRAPH') + " '" + value(node.graph) + "'";
        }

        traversalDetails.push(node);
        if (node.hasOwnProperty('condition')) {
          node.ConditionStr = buildExpression(node.condition);
        }
        if (node.hasOwnProperty('expression')) {
          node.PruneConditionStr = buildExpression(node.expression);
        }

        e = [];
        eNames = [];
        if (node.hasOwnProperty('graphDefinition')) {
          v = [];
          vNames = [];
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('vertexCollections')) {
            node.options.vertexCollections.forEach(function (vcn) {
              v.push(collection(vcn));
              vNames.push(vcn);
            });
          } else {
            node.graphDefinition.vertexCollectionNames.forEach(function (vcn) {
              v.push(collection(vcn));
              vNames.push(vcn);
            });
          }
          node.vertexCollectionNameStr = v.join(', ');
          node.vertexCollectionNameStrLen = vNames.join(', ').length;

          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('edgeCollections')) {
            node.options.edgeCollections.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          } else {
            node.graphDefinition.edgeCollectionNames.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          }
          node.edgeCollectionNameStr = e.join(', ');
          node.edgeCollectionNameStrLen = eNames.join(', ').length;
        } else {
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('edgeCollections')) {
            node.options.edgeCollections.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          } else {
            edgeCols = node.graph || [];
            edgeCols.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          }
          node.edgeCollectionNameStr = e.join(', ');
          node.edgeCollectionNameStrLen = eNames.join(', ').length;
          node.graph = '<anonymous>';

          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('vertexCollections')) {
            v = [];
            node.options.vertexCollections.forEach(function (vcn) {
              v.push(collection(vcn));
            });
            node.vertexCollectionNameStr = v.join(', ');
            node.vertexCollectionNameStrLen = node.options.vertexCollections.join(', ').length;
          }
        }

        allIndexes.forEach(function (idx) {
          indexes.push(idx);
        });

        if (node.isLocalGraphNode) {
          if (node.isUsedAsSatellite) {
            rc += annotation(' /* local graph node, used as satellite */');
          } else {
            rc += annotation(' /* local graph node */');
          }
        }
        if (node.options && node.options.hasOwnProperty('parallelism') && node.options.parallelism > 1) {
          rc += annotation(' /* parallelism: ' + node.options.parallelism + ' */');
        }

        return rc;
      case 'ShortestPathNode': {
        if (node.hasOwnProperty('vertexOutVariable')) {
          parts.push(variableName(node.vertexOutVariable) + '  ' + annotation('/* vertex */'));
        }
        if (node.hasOwnProperty('edgeOutVariable')) {
          parts.push(variableName(node.edgeOutVariable) + '  ' + annotation('/* edge */'));
        }
        let defaultDirection = node.defaultDirection;
        rc = `${keyword("FOR")} ${parts.join(", ")} ${keyword("IN")} ${keyword(translate[defaultDirection])} ${keyword("SHORTEST_PATH")} `;
        if (node.hasOwnProperty('startVertexId')) {
          rc += `'${value(node.startVertexId)}'`;
        } else {
          rc += variableName(node.startInVariable);
        }
        rc += ` ${annotation("/* startnode */")} ${keyword("TO")} `;

        if (node.hasOwnProperty('targetVertexId')) {
          rc += `'${value(node.targetVertexId)}'`;
        } else {
          rc += variableName(node.targetInVariable);
        }
        rc += ` ${annotation("/* targetnode */")} `;

        if (Array.isArray(node.graph)) {
          directions = [];
          for (i = 0; i < node.edgeCollections.length; ++i) {
              isLast = (i + 1 === node.edgeCollections.length);
              d = node.directions[i];
              if (!isLast && node.edgeCollections[i] === node.edgeCollections[i + 1]) {
                  // direction ANY is represented by two traversals: an INBOUND and an OUTBOUND traversal
                  // on the same collection
                  d = 0; // ANY
                  ++i;
              }
              directions.push({ collection: node.edgeCollections[i], direction: d });
          }
          rc += directions.map(function (g, index) {
            var tmp = '';
            if (g.direction !== defaultDirection) {
              tmp += keyword(translate[g.direction]);
              tmp += ' ';
            }
            return tmp + collection(g.collection);
          }).join(', ');
        } else {
          rc += keyword('GRAPH') + " '" + value(node.graph) + "'";
        }

        shortestPathDetails.push(node);
        e = [];
        eNames = [];
        if (node.hasOwnProperty('graphDefinition')) {
          v = [];
          vNames = [];
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('vertexCollections')) {
            node.options.vertexCollections.forEach(function (vcn) {
              v.push(collection(vcn));
              vNames.push(vcn);
            });
          } else {
            node.graphDefinition.vertexCollectionNames.forEach(function (vcn) {
              v.push(collection(vcn));
              vNames.push(vcn);
            });
          }
          node.vertexCollectionNameStr = v.join(', ');
          node.vertexCollectionNameStrLen = vNames.join(', ').length;

          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('edgeCollections')) {
            node.options.edgeCollections.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          } else {
            node.graphDefinition.edgeCollectionNames.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          }
          node.edgeCollectionNameStr = e.join(', ');
          node.edgeCollectionNameStrLen = eNames.join(', ').length;
        } else {
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('edgeCollections')) {
            node.options.edgeCollections.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          } else {
            edgeCols = node.graph || [];
            edgeCols.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          }
          node.edgeCollectionNameStr = e.join(', ');
          node.edgeCollectionNameStrLen = eNames.join(', ').length;
          node.graph = '<anonymous>';
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('vertexCollections')) {
            v = [];
            node.options.vertexCollections.forEach(function (vcn) {
              v.push(collection(vcn));
            });
            node.vertexCollectionNameStr = v.join(', ');
            node.vertexCollectionNameStrLen = node.options.vertexCollections.join(', ').length;
          }
        }
        return rc;
      }
      case 'KShortestPathsNode': {
        if (node.hasOwnProperty('pathOutVariable')) {
          parts.push(variableName(node.pathOutVariable) + '  ' + annotation('/* path */'));
        }
        let defaultDirection = node.defaultDirection;
        let shortestPathType = node.shortestPathType || 'K_SHORTEST_PATHS';
        let depth = '';
        if (shortestPathType === 'K_PATHS') {
          if (node.hasOwnProperty("options")) {
            depth = value(node.options.minDepth + '..' + node.options.maxDepth);
          } else {
            depth = value('1..1');
          }
          depth += '  ' + annotation('/* min..maxPathDepth */') + ' ';
        }
        rc = `${keyword("FOR")} ${parts.join(", ")} ${keyword("IN")} ${depth}${keyword(translate[defaultDirection])} ${keyword(shortestPathType)} `;
        if (node.hasOwnProperty('startVertexId')) {
          rc += `'${value(node.startVertexId)}'`;
        } else {
          rc += variableName(node.startInVariable);
        }
        rc += ` ${annotation("/* startnode */")} ${keyword("TO")} `;

        if (node.hasOwnProperty('targetVertexId')) {
          rc += `'${value(node.targetVertexId)}'`;
        } else {
          rc += variableName(node.targetInVariable);
        }
        rc += ` ${annotation("/* targetnode */")} `;

        if (Array.isArray(node.graph)) {
          directions = [];
          for (i = 0; i < node.edgeCollections.length; ++i) {
              isLast = (i + 1 === node.edgeCollections.length);
              d = node.directions[i];
              if (!isLast && node.edgeCollections[i] === node.edgeCollections[i + 1]) {
                  // direction ANY is represented by two traversals: an INBOUND and an OUTBOUND traversal
                  // on the same collection
                  d = 0; // ANY
                  ++i;
              }
              directions.push({ collection: node.edgeCollections[i], direction: d });
          }
          rc += directions.map(function (g, index) {
            var tmp = '';
            if (g.direction !== defaultDirection) {
              tmp += keyword(translate[g.direction]);
              tmp += ' ';
            }
            return tmp + collection(g.collection);
          }).join(', ');
        } else {
          rc += keyword('GRAPH') + " '" + value(node.graph) + "'";
        }

        kShortestPathsDetails.push(node);
        e = [];
        eNames = [];
        if (node.hasOwnProperty('graphDefinition')) {
          v = [];
          vNames = [];
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('vertexCollections')) {
            node.options.vertexCollections.forEach(function (vcn) {
              v.push(collection(vcn));
              vNames.push(vcn);
            });
          } else {
            node.graphDefinition.vertexCollectionNames.forEach(function (vcn) {
              v.push(collection(vcn));
              vNames.push(vcn);
            });
          }
          node.vertexCollectionNameStr = v.join(', ');
          node.vertexCollectionNameStrLen = vNames.join(', ').length;

          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('edgeCollections')) {
            node.options.edgeCollections.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          } else {
            node.graphDefinition.edgeCollectionNames.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          }
          node.edgeCollectionNameStr = e.join(', ');
          node.edgeCollectionNameStrLen = eNames.join(', ').length;
        } else {
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('edgeCollections')) {
            node.options.edgeCollections.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          } else {
            edgeCols = node.graph || [];
            edgeCols.forEach(function (ecn) {
              e.push(collection(ecn));
              eNames.push(ecn);
            });
          }
          node.edgeCollectionNameStr = e.join(', ');
          node.edgeCollectionNameStrLen = eNames.join(', ').length;
          node.graph = '<anonymous>';
          if (node.hasOwnProperty('options') && node.options.hasOwnProperty('vertexCollections')) {
            v = [];
            node.options.vertexCollections.forEach(function (vcn) {
              v.push(collection(vcn));
            });
            node.vertexCollectionNameStr = v.join(', ');
            node.vertexCollectionNameStrLen = node.options.vertexCollections.join(', ').length;
          }
        }
        return rc;
      }
      case 'CalculationNode':
        (node.functions || []).forEach(function (f) {
          functions[f.name] = f;
        });
        return keyword('LET') + ' ' + variableName(node.outVariable) + ' = ' + buildExpression(node.expression) + '   ' + annotation('/* ' + node.expressionType + ' expression */');
      case 'FilterNode':
        return keyword('FILTER') + ' ' + variableName(node.inVariable);
      case 'AggregateNode': /* old-style COLLECT node */
        return keyword('COLLECT') + ' ' + node.aggregates.map(function (node) {
          return variableName(node.outVariable) + ' = ' + variableName(node.inVariable);
        }).join(', ') +
          (node.count ? ' ' + keyword('WITH COUNT') : '') +
          (node.outVariable ? ' ' + keyword('INTO') + ' ' + variableName(node.outVariable) : '') +
          (node.keepVariables ? ' ' + keyword('KEEP') + ' ' + node.keepVariables.map(function (variable) { return variableName(variable); }).join(', ') : '') +
          '   ' + annotation('/* ' + node.aggregationOptions.method + ' */');
      case 'CollectNode':
        var collect = keyword('COLLECT') + ' ' +
          node.groups.map(function (node) {
            return variableName(node.outVariable) + ' = ' + variableName(node.inVariable);
          }).join(', ');

        if (node.hasOwnProperty('aggregates') && node.aggregates.length > 0) {
          if (node.groups.length > 0) {
            collect += ' ';
          }
          collect += keyword('AGGREGATE') + ' ' +
            node.aggregates.map(function (node) {
              return variableName(node.outVariable) + ' = ' + func(node.type) + '(' + (node.inVariable ? variableName(node.inVariable) : '') + ')';
            }).join(', ');
        }
        collect +=
          (node.count ? ' ' + keyword('WITH COUNT') : '') +
          (node.outVariable ? ' ' + keyword('INTO') + ' ' + variableName(node.outVariable) : '') +
          ((node.expressionVariable && node.outVariable) ? " = " + variableName(node.expressionVariable) : "") +
          (node.keepVariables ? ' ' + keyword('KEEP') + ' ' + node.keepVariables.map(function (variable) { return variableName(variable.variable); }).join(', ') : '') +
          '   ' + annotation('/* ' + node.collectOptions.method + ' */');
        return collect;
      case 'SortNode':
        return keyword('SORT') + ' ' + node.elements.map(function (node) {
          return variableName(node.inVariable) + ' ' + keyword(node.ascending ? 'ASC' : 'DESC');
        }).join(', ') + annotation(`   /* sorting strategy: ${node.strategy.split("-").join(" ")} */`);
      case 'LimitNode':
        return keyword('LIMIT') + ' ' + value(JSON.stringify(node.offset)) + ', ' + value(JSON.stringify(node.limit)) + (node.fullCount ? '  ' + annotation('/* fullCount */') : '');
      case 'ReturnNode':
        return keyword('RETURN') + ' ' + variableName(node.inVariable);
      case 'SubqueryNode':
        return keyword('LET') + ' ' + variableName(node.outVariable) + ' = ...   ' + annotation('/* ' + (node.isConst ? 'const ' : '') + 'subquery */');
      case 'SubqueryStartNode':
        return `${keyword('LET')} ${variableName(node.subqueryOutVariable)} = ( ${annotation(`/* subquery begin */`)}` ;
      case 'SubqueryEndNode':
        if (node.inVariable) {
          return `${keyword('RETURN')}  ${variableName(node.inVariable)} ) ${annotation(`/* subquery end */`)}`;
        }
        // special hack for spliced subqueries that do not return any data
        return `) ${annotation(`/* subquery end */`)}`;
      case 'InsertNode': {
        modificationFlags = node.modificationFlags;
        let restrictString = '';
        if (node.restrictedTo) {
          restrictString = annotation('/* ' + restriction(node) + ' */');
        }
        return keyword('INSERT') + ' ' + variableName(node.inVariable) + ' ' + keyword('IN') + ' ' + collection(node.collection) + ' ' + restrictString;
      }
      case 'UpdateNode': {
        modificationFlags = node.modificationFlags;
        let inputExplain = '';
        let indexRef = '';
        if (node.hasOwnProperty('inKeyVariable')) {
          indexRef = `${variableName(node.inKeyVariable)}`;
          inputExplain = `${variableName(node.inKeyVariable)} ${keyword('WITH')} ${variableName(node.inDocVariable)}`;
        } else {
          indexRef = inputExplain = `${variableName(node.inDocVariable)}`;
        }
        let restrictString = '';
        if (node.restrictedTo) {
          restrictString = annotation('/* ' + restriction(node) + ' */');
        }
        node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
        return `${keyword('UPDATE')} ${inputExplain} ${keyword('IN')} ${collection(node.collection)} ${restrictString}`;
      }
      case 'ReplaceNode': {
        modificationFlags = node.modificationFlags;
        let inputExplain = '';
        let indexRef = '';
        if (node.hasOwnProperty('inKeyVariable')) {
          indexRef = `${variableName(node.inKeyVariable)}`;
          inputExplain = `${variableName(node.inKeyVariable)} ${keyword('WITH')} ${variableName(node.inDocVariable)}`;
        } else {
          indexRef = inputExplain = `${variableName(node.inDocVariable)}`;
        }
        let restrictString = '';
        if (node.restrictedTo) {
          restrictString = annotation('/* ' + restriction(node) + ' */');
        }
        node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
        return `${keyword('REPLACE')} ${inputExplain} ${keyword('IN')} ${collection(node.collection)} ${restrictString}`;
      }
      case 'UpsertNode':
        modificationFlags = node.modificationFlags;
        let indexRef = `${variableName(node.inDocVariable)}`;
        node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
        return keyword('UPSERT') + ' ' + variableName(node.inDocVariable) + ' ' + keyword('INSERT') + ' ' + variableName(node.insertVariable) + ' ' + keyword(node.isReplace ? 'REPLACE' : 'UPDATE') + ' ' + variableName(node.updateVariable) + ' ' + keyword('IN') + ' ' + collection(node.collection);
      case 'RemoveNode': {
        modificationFlags = node.modificationFlags;
        let restrictString = '';
        if (node.restrictedTo) {
          restrictString = annotation('/* ' + restriction(node) + ' */');
        }
        let indexRef = `${variableName(node.inVariable)}`;
        node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
        return `${keyword('REMOVE')} ${variableName(node.inVariable)} ${keyword('IN')} ${collection(node.collection)} ${restrictString}`;
      }
      case 'SingleRemoteOperationNode': {
        switch (node.mode) {
          case "IndexNode": {
            collectionVariables[node.outVariable.id] = node.collection;
            let indexRef = `${variable(JSON.stringify(node.key))}`;
            node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
            return `${keyword('FOR')} ${variableName(node.outVariable)} ${keyword('IN')} ${collection(node.collection)} ${keyword('FILTER')} ${variable('_key')} == ${indexRef} ${annotation(`/* primary index scan */`)}`;
            // `
          }
          case 'InsertNode': {
            modificationFlags = node.modificationFlags;
            collectionVariables[node.inVariable.id] = node.collection;
            let indexRef = `${variableName(node.inVariable)}`;
            if (node.hasOwnProperty('indexes')) {
              node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
            }
            return `${keyword('INSERT')} ${variableName(node.inVariable)} ${keyword('IN')} ${collection(node.collection)}`;
          }
          case 'UpdateNode': {
            modificationFlags = node.modificationFlags;
            let OLD = "";
            if (node.hasOwnProperty('inVariable')) {
              collectionVariables[node.inVariable.id] = node.collection;
              OLD = `${keyword('WITH')} ${variableName(node.inVariable)} `;
            }
            let indexRef;
            let keyCondition = "";
            let filterCondition;
            if (node.hasOwnProperty('key')) {
              keyCondition = `{ _key: ${variable(JSON.stringify(node.key))} } `;
              indexRef = `${variable(JSON.stringify(node.key))} `;
              filterCondition = `${variable('doc._key')} == ${variable(JSON.stringify(node.key))}`;
            } else if (node.hasOwnProperty('inVariable')) {
              keyCondition = `${variableName(node.inVariable)} `;
              indexRef = `${variableName(node.inVariable)}`;
            } else {
              keyCondition = "<UNSUPPORTED>";
              indexRef = "<UNSUPPORTED>";
            }
            if (node.hasOwnProperty('indexes')) {
              node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
            }
            let forStatement = "";
            if (node.replaceIndexNode) {
              forStatement = `${keyword('FOR')} ${variable('doc')} ${keyword('IN')} ${collection(node.collection)} ${keyword('FILTER')} ${filterCondition} `;
              keyCondition = `${variable('doc')} `;
            }
            return `${forStatement}${keyword('UPDATE')} ${keyCondition}${OLD}${keyword('IN')} ${collection(node.collection)}`;
          }
          case 'ReplaceNode': {
            modificationFlags = node.modificationFlags;
            let OLD = "";
            if (node.hasOwnProperty('inVariable')) {
              collectionVariables[node.inVariable.id] = node.collection;
              OLD = `${keyword('WITH')} ${variableName(node.inVariable)} `;
            }
            let indexRef;
            let keyCondition = "";
            let filterCondition;
            if (node.hasOwnProperty('key')) {
              keyCondition = `{ _key: ${variable(JSON.stringify(node.key))} } `;
              indexRef = `${variable(JSON.stringify(node.key))}`;
              filterCondition = `${variable('doc._key')} == ${variable(JSON.stringify(node.key))}`;
            } else if (node.hasOwnProperty('inVariable')) {
              keyCondition = `${variableName(node.inVariable)} `;
              indexRef = `${variableName(node.inVariable)}`;
            } else {
              keyCondition = "<UNSUPPORTED>";
              indexRef = "<UNSUPPORTED>";
            }
            if (node.hasOwnProperty('indexes')) {
              node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
            }
            let forStatement = "";
            if (node.replaceIndexNode) {
              forStatement = `${keyword('FOR')} ${variable('doc')} ${keyword('IN')} ${collection(node.collection)} ${keyword('FILTER')} ${filterCondition} `;
              keyCondition = `${variable('doc')} `;
            }
            return `${forStatement}${keyword('REPLACE')} ${keyCondition}${OLD}${keyword('IN')} ${collection(node.collection)}`;
          }
          case 'RemoveNode': {
            modificationFlags = node.modificationFlags;
            if (node.hasOwnProperty('inVariable')) {
              collectionVariables[node.inVariable.id] = node.collection;
            }
            let indexRef;
            let keyCondition;
            let filterCondition;
            if (node.hasOwnProperty('key')) {
              keyCondition = `{ _key: ${variable(JSON.stringify(node.key))} } `;
              indexRef = `${variable(JSON.stringify(node.key))}`;
              filterCondition = `${variable('doc._key')} == ${variable(JSON.stringify(node.key))}`;
            } else if (node.hasOwnProperty('inVariable')) {
              keyCondition = `${variableName(node.inVariable)} `;
              indexRef = `${variableName(node.inVariable)}`;
            } else {
              keyCondition = "<UNSUPPORTED>";
              indexRef = "<UNSUPPORTED>";
            }
            if (node.hasOwnProperty('indexes')) {
              node.indexes.forEach(function (idx, i) { iterateIndexes(idx, i, node, types, indexRef); });
            }
            let forStatement = "";
            if (node.replaceIndexNode) {
              forStatement = `${keyword('FOR')} ${variable('doc')} ${keyword('IN')} ${collection(node.collection)} ${keyword('FILTER')} ${filterCondition} `;
              keyCondition = `${variable('doc')} `;
            }
            return `${forStatement}${keyword('REMOVE')} ${keyCondition}${keyword('IN')} ${collection(node.collection)}`;
          }
        }
      }
        break;
      case 'RemoteNode':
        return keyword('REMOTE');
      case 'DistributeNode':
        return keyword('DISTRIBUTE') + ' ' + variableName(node.variable);
      case 'ScatterNode':
        return keyword('SCATTER');
      case 'GatherNode':
        let gatherAnnotations = [];
        if (node.parallelism === 'parallel') {
          gatherAnnotations.push('parallel');
        }
        if (node.sortmode !== 'unset') {
          gatherAnnotations.push('sort mode: ' + node.sortmode);
        }
        if (node.elements.length === 0) {
          gatherAnnotations.push('unsorted');
        }
        return keyword('GATHER') + ' ' + node.elements.map(function (node) {
          if (node.path && node.path.length) {
            return variableName(node.inVariable) + node.path.map(function (n) { return '.' + attribute(n); }) + ' ' + keyword(node.ascending ? 'ASC' : 'DESC');
          }
          return variableName(node.inVariable) + ' ' + keyword(node.ascending ? 'ASC' : 'DESC');
        }).join(', ') + (gatherAnnotations.length ? '  ' + annotation('/* ' + gatherAnnotations.join(', ') + ' */') : '');

      case 'MaterializeNode':
        return keyword('MATERIALIZE') + ' ' + variableName(node.outVariable);
      case 'MutexNode':
        return keyword('MUTEX') + '   ' + annotation('/* end async execution */');
      case 'AsyncNode':
        return keyword('ASYNC') + '   ' + annotation('/* begin async execution */');
      case 'WindowNode': {
        var window = keyword('WINDOW') + ' ';
        if (node.rangeVariable) {
          window += variableName(node.rangeVariable) + ' ' + keyword("WITH") + ' ';
        }
        window += `{ preceding: ${JSON.stringify(node.preceding)}, following: ${JSON.stringify(node.following)}} `;
        if (node.hasOwnProperty('aggregates') && node.aggregates.length > 0) {
          window += keyword('AGGREGATE') + ' ' +
            node.aggregates.map(function (node) {
              return variableName(node.outVariable) + ' = ' + func(node.type) + '(' + variableName(node.inVariable) + ')';
            }).join(', ');
        }
        return window;
      }
    }

    return 'unhandled node type (' + node.type + ')';
  };

  var level = 0, subqueries = [], subqueryCallbacks = [];
  var indent = function (level, isRoot) {
    return pad(1 + level + level) + (isRoot ? '* ' : '- ');
  };

  var preHandle = function (node) {
    usedVariables = {};
    currentNode = node.id;
    isConst = true;
    if (node.type === 'SubqueryNode' || node.type === 'SubqueryStartNode') {
      subqueries.push(level);
    }
    if (node.type === 'SubqueryEndNode' && !node.inVariable && subqueries.length > 0) {
      // special hack for spliced subqueries that do not return any data
      --level;
    }
  };

  var postHandle = function (node) {
    if (node.type === 'SubqueryEndNode' && subqueries.length > 0) {
      level = subqueries.pop();
    }
    var isLeafNode = !parents.hasOwnProperty(node.id);

    if (['EnumerateCollectionNode',
      'EnumerateListNode',
      'EnumerateViewNode',
      'IndexRangeNode',
      'IndexNode',
      'TraversalNode',
      'SubqueryStartNode',
      'SubqueryNode'].indexOf(node.type) !== -1) {
      level++;
    } else if (isLeafNode && subqueries.length > 0) {
      level = subqueries.pop();
    } else if (node.type === 'SingletonNode') {
      level++;
    }
  };

  var constNess = function () {
    if (isConst) {
      return '   ' + annotation('/* const assignment */');
    }
    return '';
  };

  var variablesUsed = function () {
    var used = [];
    for (var a in usedVariables) {
      if (usedVariables.hasOwnProperty(a)) {
        used.push(variable(a) + ' : ' + collection(usedVariables[a]));
      }
    }
    if (used.length > 0) {
      return '   ' + annotation('/* collections used:') + ' ' + used.join(', ') + ' ' + annotation('*/');
    }
    return '';
  };

  const isCoord = isCoordinator();

  var printNode = function (node) {
    preHandle(node);
    var line = ' ' +
      pad(1 + maxIdLen - String(node.id).length) + variable(node.id) + '   ' +
      keyword(node.type) + pad(1 + maxTypeLen - String(node.type).length) + '   ';

    if (isCoord) {
      line += variable(node.site) + pad(1 + maxSiteLen - String(node.site).length) + '  ';
    }

    if (profileMode) {
      if (node.calls === undefined) {
        node.calls = 'n/a';
      }
      if (node.items === undefined) {
        node.items = 'n/a';
      }
      let runtime = node.runtime;
      if (runtime === undefined) {
        runtime = 'n/a';
      } else {
        runtime = String(Math.abs(node.runtime).toFixed(5));
      }

      line += pad(1 + maxCallsLen - String(node.calls).length) + value(node.calls) + '   ' +
        pad(1 + maxItemsLen - String(node.items).length) + value(node.items) + '   ' +
        pad(1 + maxRuntimeLen - runtime.length) + value(runtime) + '   ' +
        indent(level, node.type === 'SingletonNode') + label(node);
    } else {
      line += pad(1 + maxEstimateLen - String(node.estimatedNrItems).length) + value(node.estimatedNrItems) + '   ' +
        indent(level, node.type === 'SingletonNode') + label(node);
    }

    if (node.type === 'CalculationNode') {
      line += variablesUsed() + constNess();
    }
    stringBuilder.appendLine(line);
    postHandle(node);
  };

  if (planIndex === undefined) {
    printQuery(query, explain.cacheable);
  }

  stringBuilder.appendLine(section('Execution plan:'));

  var line = ' ' +
    pad(1 + maxIdLen - String('Id').length) + header('Id') + '   ' +
    header('NodeType') + pad(1 + maxTypeLen - String('NodeType').length) + '   ';

  if (isCoord) {
    line += header('Site') + pad(1 + maxSiteLen - String('Site').length) + '  ';
  }

  if (profileMode) {
    line += pad(1 + maxCallsLen - String('Calls').length) + header('Calls') + '   ' +
      pad(1 + maxItemsLen - String('Items').length) + header('Items') + '   ' +
      pad(1 + maxRuntimeLen - String('Runtime [s]').length) + header('Runtime [s]') + '   ' +
      header('Comment');
  } else {
    line += pad(1 + maxEstimateLen - String('Est.').length) + header('Est.') + '   ' +
      header('Comment');
  }


  stringBuilder.appendLine(line);

  var walk = [rootNode];
  while (walk.length > 0) {
    var id = walk.pop();
    var node = nodes[id];
    printNode(node);
    if (parents.hasOwnProperty(id)) {
      walk = walk.concat(parents[id]);
    }
    if (node.type === 'SubqueryNode') {
      walk = walk.concat([node.subquery.nodes[0].id]);
    }
  }

  stringBuilder.appendLine();
  printIndexes(indexes);
  printFunctions(functions);
  printTraversalDetails(traversalDetails);
  printShortestPathDetails(shortestPathDetails);
  printKShortestPathsDetails(kShortestPathsDetails);
  stringBuilder.appendLine();

  printRules(plan.rules, explain.stats);
  printModificationFlags(modificationFlags);
  printWarnings(explain.warnings);
  if (profileMode) {
    printStats(explain.stats);
    printProfile(explain.profile);
  }
}

/* the exposed explain function */
function explain(data, options, shouldPrint) {
  'use strict';
  if (typeof data === 'string') {
    data = { query: data, options: options };
  }
  if (!(data instanceof Object)) {
    throw 'ArangoStatement needs initial data';
  }

  if (options === undefined) {
    options = data.options;
  }
  options = options || {};
  options.verbosePlans = true;
  setColors(options.colors === undefined ? true : options.colors);

  if (!options.profile) {
    options.profile = 2;
  }
  stringBuilder.clearOutput();
  let stmt = db._createStatement(data);
  let result = stmt.explain(options);
  if (options.allPlans) {
    // multiple plans
    printQuery(data.query);
    for (let i = 0; i < result.plans.length; ++i) {
      if (i > 0) {
        stringBuilder.appendLine();
      }
      stringBuilder.appendLine(section("Plan #" + (i + 1) + " of " + result.plans.length + " (estimated cost: " + result.plans[i].estimatedCost.toFixed(2) + ")"));
      stringBuilder.prefix = ' ';
      stringBuilder.appendLine();
      processQuery(data.query, result, i);
      stringBuilder.prefix = '';
    }
  } else {
    // single plan
    processQuery(data.query, result, undefined);
  }

  if (shouldPrint === undefined || shouldPrint) {
    print(stringBuilder.getOutput());
  } else {
    return stringBuilder.getOutput();
  }
}


/* the exposed profile query function */
function profileQuery(data, shouldPrint) {
  'use strict';
  if (!(data instanceof Object) || !data.hasOwnProperty("options")) {
    throw 'ArangoStatement needs initial data';
  }
  let options = data.options || {};
  options.silent = true;
  options.allPlans = false; // always turn this off, as it will not work with profiling
  setColors(options.colors === undefined ? true : options.colors);

  stringBuilder.clearOutput();
  let stmt = db._createStatement(data);
  let cursor = stmt.execute();
  let extra = cursor.getExtra();
  processQuery(data.query, extra, undefined);

  if (shouldPrint === undefined || shouldPrint) {
    print(stringBuilder.getOutput());
  } else {
    return stringBuilder.getOutput();
  }
}

/* the exposed debug function */
function debug(query, bindVars, options) {
  'use strict';
  let input = {};

  if (query instanceof Object) {
    if (typeof query.toAQL === 'function') {
      query = query.toAQL();
    }
    input = query;
  } else {
    input.query = query;
    if (bindVars !== undefined) {
      input.bindVars = anonymize(bindVars);
    }
    if (options !== undefined) {
      input.options = options;
    }
  }
  if (!input.options) {
    input.options = {};
  }
  input.options.explainRegisters = true;

  let result = {
    engine: db._engine(),
    version: db._version(true),
    database: db._name(),
    query: input,
    queryCache: require('@arangodb/aql/cache').properties(),
    collections: {},
    views: {}
  };

  try {
    // engine stats may be not available in cluster, at least in older versions
    result.engineStats = db._engineStats();
  } catch (err) {
    // we need to ignore any errors here
  }

  result.fancy = require('@arangodb/aql/explainer').explain(input, { colors: false }, false);

  let stmt = db._createStatement(input);
  result.explain = stmt.explain(input.options);

  let graphs = {};
  let collections = result.explain.plan.collections;
  let map = {};
  collections.forEach(function (c) {
    map[c.name] = true;
  });

  // export graphs
  let findGraphs = function (nodes) {
    nodes.forEach(function (node) {
      if (node.type === 'TraversalNode') {
        if (node.graph && node.graphDefinition) {
          // named graphs have their name in "graph", but non-graph traversal queries too
          // the distinction can thus be made only by peeking into graphDefinition, which
          // is only populated for named graphs
          try {
            graphs[node.graph] = db._graphs.document(node.graph);
          } catch (err) { }
        }
        if (node.graphDefinition) {
          try {
            node.graphDefinition.vertexCollectionNames.forEach(function (c) {
              if (!map.hasOwnProperty(c)) {
                map[c] = true;
                collections.push({ name: c });
              }
            });
          } catch (err) { }
          try {
            node.graphDefinition.edgeCollectionNames.forEach(function (c) {
              if (!map.hasOwnProperty(c)) {
                map[c] = true;
                collections.push({ name: c });
              }
            });
          } catch (err) { }
        }
      } else if (node.type === 'SubqueryNode') {
        // recurse into subqueries
        findGraphs(node.subquery.nodes);
      }
    });
  };
  // mangle with graphs used in query
  findGraphs(result.explain.plan.nodes);

  let handleCollection = function(collection) {
    let c = db._collection(collection.name);
    if (c === null) {
      // probably a view...
      let v = db._view(collection.name);
      if (v === null) {
        return;
      }

      result.views[collection.name] = {
        type: v.type(),
        properties: v.properties()
      };
    } else {
      // a collection
      if (c.type() === 3 && collection.name.match(/^_(local|from|to)_.+/)) {
        // an internal smart-graph collection. let's skip this
        return;
      }
      let examples;
      if (input.options.examples) {
        // include example data from collections
        let max = 10; // default number of documents
        if (typeof input.options.examples === 'number') {
          max = input.options.examples;
        }
        if (max > 100) {
          max = 100;
        } else if (max < 0) {
          max = 0;
        }
        examples = db._query("FOR doc IN @@collection LIMIT @max RETURN doc", { max, "@collection": collection.name }).toArray();
        if (input.options.anonymize) {
          examples = examples.map(anonymize);
        }
      }
      result.collections[collection.name] = {
        name: collection.name,
        type: c.type(),
        properties: c.properties(),
        indexes: c.getIndexes(true),
        count: c.count(),
        counts: c.count(true),
        examples
      };
    }
  };

  // add collection information
  collections.forEach(function (collection) {
    handleCollection(collection);
  });

  // add prototypes used for distributeShardsLike
  let sortedCollections = [];
  Object.values(result.collections).forEach(function(collection) {
    if (collection.properties.distributeShardsLike &&
        !result.collections.hasOwnProperty(collection.distributeShardsLike)) {
      handleCollection({ name: collection.name });
    }
    sortedCollections.push(collection);
  });

  sortedCollections.sort(function(l, r) {
    if (l.properties.distributeShardsLike && !r.properties.distributeShardsLike) {
      return 1;
    } else if (!l.properties.distributeShardsLike && r.properties.distributeShardsLike) {
      return -1;
    }
    if (l.type === 2 && r.type === 3) {
      return -1;
    } else if (r.type === 3 && l.type === 2) {
      return 1;
    }
    if (l.name < r.name) {
      return -1;
    } else if (l.name > r.name) {
      return 1;
    }
    // should not happen
    return 0;
  });

  result.collections = {};
  sortedCollections.forEach(function(c) {
    result.collections[c.name] = c;
  });

  result.graphs = graphs;
  return result;
}

function debugDump(filename, query, bindVars, options) {
  let result = debug(query, bindVars, options);
  require('fs').write(filename, JSON.stringify(result));
  require('console').log("stored query debug information in file '" + filename + "'");
}

function inspectDump(filename, outfile) {
  let internal = require('internal');
  if (outfile !== undefined) {
    internal.startCaptureMode();
  }

  let data = JSON.parse(require('fs').read(filename));
  if (data.version && data.version) {
    print("/* original data is from " + data.version["server-version"] + ", " + data.version.license + " */");
  }
  if (data.database) {
    print("/* original data gathered from database '" + data.database + "' */");
  }

  try {
    if (db._engine().name !== data.engine.name) {
      print("/* using different storage engine (' " + db._engine().name + "') than in debug information ('" + data.engine.name + "') */");
    }
    print();
  } catch (err) {
    // ignore errors here. db._engine() will fail if we are not connected
    // to a server instance. swallowing the error here allows us to continue
    // and analyze the dump offline
  }

  print("/* graphs */");
  let graphs = data.graphs || {};
  Object.keys(graphs).forEach(function (graph) {
    let details = graphs[graph];
    print("try { db._graphs.remove(" + JSON.stringify(graph) + "); } catch (err) {}");
    print("db._graphs.insert(" + JSON.stringify(details) + ");");
  });
  print();

  // all collections and indexes first, as data insertion may go wrong later
  print("/* collections and indexes setup */");
  const keys = Object.keys(data.collections);
  if (keys.length > 0) {
    // drop in reverse order, because of distributeShardsLike
    for (let i = keys.length; i > 0; --i) {
      let collection = keys[i - 1];
      let details = data.collections[collection];
      let name = details.name || collection;
      if (name[0] === '_') {
        // system collection
        print("try { db._drop(" + JSON.stringify(collection) + ", true); } catch (err) { print(String(err)); }");
      } else {
        print("try { db._drop(" + JSON.stringify(collection) + "); } catch (err) { print(String(err)); }");
      }
    }
    // create in forward order because of distributeShardsLike
    for (let i = 0; i < keys.length; ++i) {
      let collection = keys[i];
      let details = data.collections[collection];
      if (details.type === false || details.type === 3) {
        if (details.properties.isSmart) {
          delete details.properties.numberOfShards;
        }
        print("db._createEdgeCollection(" + JSON.stringify(collection) + ", " + JSON.stringify(details.properties) + ");");
      } else {
        print("db._create(" + JSON.stringify(collection) + ", " + JSON.stringify(details.properties) + ");");
      }
      details.indexes.forEach(function (index) {
        delete index.figures;
        delete index.selectivityEstimate;
        if (index.type !== 'primary' && index.type !== 'edge') {
          print("db[" + JSON.stringify(collection) + "].ensureIndex(" + JSON.stringify(index) + ");");
        }
      });
      print();
    }
  }
  print();

  // insert example data
  print("/* example data */");
  Object.keys(data.collections).forEach(function (collection) {
    let details = data.collections[collection];
    if (details.examples) {
      details.examples.forEach(function (example) {
        print("db[" + JSON.stringify(collection) + "].insert(" + JSON.stringify(example) + ", {isRestore: true});");
      });
    }
    let missing = details.count;
    if (details.examples) {
      missing -= details.examples.length;
    }
    if (missing > 0) {
      print("/* collection '" + collection + "' needs " + missing + " more document(s) */");
    }
    print();
  });
  print();

  // views
  print("/* views */");
  Object.keys(data.views || {}).forEach(function (view) {
    let details = data.views[view];
    print("db._dropView(" + JSON.stringify(view) + ");");
    print("db._createView(" + JSON.stringify(view) + ", " + JSON.stringify(details.type) + ", " + JSON.stringify(details.properties) + ");");
  });
  print();

  print("/* explain result */");
  print(data.fancy.trim().split(/\n/).map(function (line) { return "// " + line; }).join("\n"));
  print();

  print("/* explain command */");
  if (data.query.options) {
    delete data.query.options.anonymize;
    delete data.query.options.colors;
    delete data.query.options.examples;
  }
  print("db._explain(" + JSON.stringify(data.query) + ");");
  print();

  if (outfile !== undefined) {
    require('fs').write(outfile, internal.stopCaptureMode());
    require('console').log("stored query restore script in file '" + outfile + "'");
    require('console').log("to run it, execute  require('internal').load('" + outfile + "');");
  }
}

function explainQuerysRegisters(plan) {

  /**
   * @typedef Node
   * @type {
   *   {
   *     depth: number,
   *     id: number,
   *     nrRegsArray: number[],
   *     regsToClear: number[],
   *     regsToKeepStack: Array<number[]>,
   *     type: string,
   *     unusedRegsStack: Array<number[]>,
   *     varInfoList: Array<{VariableId: number, RegisterId: number, depth: number}>,
   *     varsSetHere: Array<{id: number, name: string, isDataFromCollection: boolean}>,
   *     varsUsedHere: Array<{id: number, name: string, isDataFromCollection: boolean}>,
   *   }
   * }
   */

  // Currently, we need nothing but the nodes from plan.
  const {nodes} = plan;
  const symbols = {
    clearRegister: '⮾',
    keepRegister: '↓',
    writeRegister: '→',
    readRegister: '←',
    unusedRegister: '-',
    subqueryBegin: '↘',
    subqueryEnd: '↙',
  };
  Object.freeze(symbols);

  const getNodesById = (planNodes, result = {}) => {
    for (let node of planNodes) {
      result[node.id] = node;
      if (node.type === 'SubqueryNode') {
        getNodesById(node.subquery.nodes, result);
      }
    }

    return result;
  };
  const nodesById = getNodesById(nodes);
  //////////////////////////////////////////////////////////////////////////////
  // First, we build up the array nodesData. This will (though not while its
  // being built!) contain all nodes in a linearized fashion in the "canonical"
  // order, that is, starting from the leaf (singleton) at index 0. Subqueries
  // are inlined in that way as well. Each subquery node will occur *twice*,
  // once before and once after the actual subquery.
  //////////////////////////////////////////////////////////////////////////////

  /**
   * @type {Array<{node:Node, direction:('open'|'close'|undefined)}>}
   */
  const nodesData = [];

  // TODO Check whether its okay to assume that the root node is always the last
  //      node.
  const rootNode = nodes[nodes.length - 1];

  const subqueryStack = [];
  for (let node = rootNode; node !== undefined; ) {
    const current = {node};
    nodesData.push(current);
    if (node.type === 'SubqueryNode') {
      subqueryStack.push(node);
      current.direction = 'close';
      node = node.subquery.nodes[node.subquery.nodes.length - 1];
    } else if (node.dependencies && node.dependencies.length > 0) {
      node = nodesById[node.dependencies[0]];
    } else if (subqueryStack.length > 0) {
      node = subqueryStack.pop();
      const current = {node};
      current.direction = 'open';
      nodesData.push(current);
      node = nodesById[node.dependencies[0]];
    } else {
      node = undefined;
    }
  }

  // for debugging. TODO remove it or replace it with saner exceptions
  const assert = (bool) => {
    if (!bool) {
      throw new Error();
    }
  };

  nodesData.reverse();

  //////////////////////////////////////////////////////////////////////////////
  // Second, we build up rowsInfos. Will contain one element per row/line that
  // shall be printed, with enough information that each field can be printed
  // and formatted on its own.
  //////////////////////////////////////////////////////////////////////////////

  /**
   * @typedef RowInfo
   * @type { { registerFields: string[], separator: string }
   *       | { registerFields: string[], meta: {depth: number, id: number, type: string} }
   *       | { registerFields: string[] }
   *       }
   */
  /**
   * @type {Array<RowInfo>} rowsInfos
   */
  const rowsInfos = nodesData.flatMap(data => {
    /**
     * @type {{node: Node, direction: ('open'|'close'|undefined)}} data
     */
    const {
      node: {
        depth, id, nrRegs: nrRegsArray, regsToClear, regsToKeepStack, type,
        unusedRegsStack, varInfoList, varsSetHere, varsUsedHere, regVarMapStack
      },
      direction
    } = data;
    const nrRegs = nrRegsArray[depth];
    const regIdByVarId = Object.fromEntries(
      varInfoList.filter(varInfo => varInfo.RegisterId < 1000).map(varInfo => [varInfo.VariableId, varInfo.RegisterId]));

    const meta = {id, type, depth};
    const result = [];

    const varName = (variable) => {
      if (/^[0-9_]/.test(variable.name)) {
        return '#' + variable.name;
      }
      return variable.name;
    };

    const regIdToVarNameSetHere = Object.fromEntries(
      varsSetHere.map(variable => [regIdByVarId[variable.id], varName(variable)]));

    const regIdToVarNameUsedHere = Object.fromEntries(
      varsUsedHere.map(variable => [regIdByVarId[variable.id], varName(variable)]));

    const regIdToVarNameMapStack = regVarMapStack.map(rvmap =>
        Object.fromEntries(_.toPairsIn(rvmap).map(
            (p) => [parseInt(p[0]), varName(p[1])]
        )
    ));

    /**
     * @param {number} nrRegs
     * @param { { regIdToVarNameSetHere: (undefined|!Object<number, string>),
     *            regIdToVarNameUsedHere: (undefined|!Object<number, string>),
     *            unusedRegs: (undefined|!number[]),
     *            regVarMap: (undefined|!Object<number, string>)}
     *          | { regsToClear: (undefined|!number[]), regsToKeep: (undefined|!number[]) } } regInfo
     * @return { !Array<string> }
     */
    const createRegisterFields = (nrRegs, regInfo = {}) => {
      const registerFields = [...Array(nrRegs)].map(_ => '');

      const hasRegIdToVarNameSetHere = regInfo.hasOwnProperty('regIdToVarNameSetHere');
      const hasRegIdToVarNameUsedHere = regInfo.hasOwnProperty('regIdToVarNameUsedHere');
      const hasUnusedRegs = regInfo.hasOwnProperty('unusedRegs');
      const hasRegsToClear = regInfo.hasOwnProperty('regsToClear');
      const hasRegsToKeep = regInfo.hasOwnProperty('regsToKeep');
      const hasRegVarMap = regInfo.hasOwnProperty('regVarMap');

      assert(!((hasRegIdToVarNameSetHere || hasRegIdToVarNameUsedHere || hasUnusedRegs) && (hasRegsToClear || hasRegsToKeep)));

      if (hasRegIdToVarNameUsedHere) {
        for (const [regId, varName] of Object.entries(regInfo.regIdToVarNameUsedHere)) {
          registerFields[regId] += symbols.readRegister + varName + ' ';
        }
      }
      if (hasRegIdToVarNameSetHere) {
        for (const [regId, varName] of Object.entries(regInfo.regIdToVarNameSetHere)) {
          registerFields[regId] += symbols.writeRegister + varName + ' ';
        }
      }
      if (hasUnusedRegs) {
        for (const regId of regInfo.unusedRegs) {
          if (registerFields[regId].length === 0) {
            registerFields[regId] += symbols.unusedRegister;
          }
        }
      }
      if (hasRegsToClear) {
        for (const regId of regInfo.regsToClear) {
          registerFields[regId] += symbols.clearRegister;
        }
      }
      if (hasRegsToKeep) {
        for (const regId of regInfo.regsToKeep) {
          registerFields[regId] += symbols.keepRegister;
        }
      }
      if (hasRegVarMap) {
        for (const [regId, name] of Object.entries(regInfo.regVarMap)) {
          if (registerFields[regId].length === 0) {
            registerFields[regId] = name;
          }
        }
      }


      return registerFields;
    };

    switch (type) {
      case 'SubqueryStartNode': {
        assert(regsToKeepStack.length >= 2);
        let regsToKeep = regsToKeepStack[regsToKeepStack.length - 2];
        let regVarMap = regIdToVarNameMapStack[regIdToVarNameMapStack.length - 2];
        const unusedRegs = unusedRegsStack[unusedRegsStack.length - 1];
        result.push({meta, registerFields: createRegisterFields(nrRegs, {regIdToVarNameUsedHere, unusedRegs, regVarMap})});
        result.push({registerFields: createRegisterFields(nrRegs, {regsToClear, regsToKeep})});
        result.push({separator: symbols.subqueryBegin, registerFields: createRegisterFields(nrRegs)});
        regsToKeep = regsToKeepStack[regsToKeepStack.length - 1];
        regVarMap = regIdToVarNameMapStack[regIdToVarNameMapStack.length - 1];
        result.push({meta, registerFields: createRegisterFields(nrRegs, {regIdToVarNameSetHere, regVarMap, unusedRegs})});
        result.push({registerFields: createRegisterFields(nrRegs, {regsToKeep})});
      }
        break;
      case 'SubqueryEndNode': {
        const nrRegsInner = nrRegsArray[depth-1];
        const unusedRegs = unusedRegsStack[unusedRegsStack.length - 1];
        result.push({meta, registerFields: createRegisterFields(nrRegsInner, {unusedRegs, regIdToVarNameUsedHere})});
        result.push({separator: symbols.subqueryEnd, registerFields: createRegisterFields(nrRegsInner)});
        const regsToKeep = regsToKeepStack[regsToKeepStack.length - 1];
        const regVarMap = regIdToVarNameMapStack[regIdToVarNameMapStack.length - 1];
        result.push({meta, registerFields: createRegisterFields(nrRegs, {regIdToVarNameSetHere, regVarMap})});
        result.push({registerFields: createRegisterFields(nrRegs, {regsToClear, regsToKeep})});
      }
        break;
      case 'SubqueryNode': {
        const regVarMap = regIdToVarNameMapStack[regIdToVarNameMapStack.length - 1];
        switch (direction) {
          case 'open': {
            // We could print regIdToVarNameUsedHere here...
            result.push({separator: symbols.subqueryBegin, registerFields: createRegisterFields(nrRegs)});
          }
            break;
          case 'close': {
            const regsToKeep = regsToKeepStack[regsToKeepStack.length - 1];
            const unusedRegs = unusedRegsStack[unusedRegsStack.length - 1];
            result.push({separator: symbols.subqueryEnd, registerFields: createRegisterFields(nrRegs)});
            result.push({meta, registerFields: createRegisterFields(nrRegs, {regIdToVarNameSetHere, unusedRegs, regVarMap})});
            result.push({registerFields: createRegisterFields(nrRegs, {regsToClear, regsToKeep})});
          }
            break;
        }
      }
        break;
      default: {
        const regsToKeep = regsToKeepStack[regsToKeepStack.length - 1];
        const unusedRegs = unusedRegsStack[unusedRegsStack.length - 1];
        const regVarMap = regIdToVarNameMapStack[regIdToVarNameMapStack.length - 1];
        result.push({meta, registerFields: createRegisterFields(nrRegs, {regIdToVarNameUsedHere, regIdToVarNameSetHere, unusedRegs, regVarMap})});
        result.push({registerFields: createRegisterFields(nrRegs, {regsToClear, regsToKeep})});
      }
    }

    return result;
    }
  );

  //////////////////////////////////////////////////////////////////////////////
  // Third, set up additional information we'll need for formatting whole lines,
  // like the maximum width of all columns.
  //////////////////////////////////////////////////////////////////////////////
  const columns = [
    {name: 'id', alignment: 'r'},
    {name: 'type', alignment: 'l'},
    {name: 'depth', alignment: 'r'},
  ];

  /**
   * @type {Object<string, number>}
   */
  const colWidths = Object.fromEntries(columns.map(col => [col.name, col.name.length]));
  const regColWidths = [];

  for (const row of rowsInfos) {
    if (row.hasOwnProperty('meta')) {
      for (const col of columns) {
        colWidths[col.name] = Math.max(("" + row.meta[col.name]).length, colWidths[col.name]);
      }
    }
    if (row.hasOwnProperty('registerFields')) {
      row.registerFields.forEach((value, i) => {
        regColWidths[i] = regColWidths[i] || 0;
        regColWidths[i] = Math.max(value.length, regColWidths[i]);
      });
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Fourth, do the actual formatting of each line and print them.
  //////////////////////////////////////////////////////////////////////////////

  /**
   * @param {string} value
   * @param {'l'|'r'|'c'} alignment
   * @param {number} width - Must be at least value.length
   * @returns {string} - Will be exactly `width` chars in length
   */
  const formatField = (value, alignment, width) => {
    const str = '' + value;
    const paddingWidth = width - str.length;

    switch (alignment) {
      case 'l':
        return str + ' '.repeat(paddingWidth);
      case 'r':
        return ' '.repeat(paddingWidth) + str;
      default:
        const halfPadding = Math.floor(paddingWidth / 2);
        const otherHalf = paddingWidth - halfPadding;
        // halfPadding <= otherHalf <= halfPadding + 1
        return ' '.repeat(halfPadding) + str + ' '.repeat(otherHalf);
    }
  };

  print(' ' + columns.map(col =>
    formatField(col.name, 'c', colWidths[col.name])
  ).join('│') + ' ');
  print('─' + Object.values(colWidths).map(w => '─'.repeat(w)).join('┼') + '─');

  for (const row of rowsInfos) {
    // print meta table
    if (row.hasOwnProperty('meta')) {
      output(' ');
      output(columns.map(col =>
        formatField(row.meta[col.name], col.alignment, colWidths[col.name])
      ).join('│'));
      output(' ');
    } else if (row.hasOwnProperty('separator')) {
      const widths = Object.values(colWidths);
      const width = _.sum(widths) + widths.length + 1;
      output(row.separator.repeat(width));
    } else {
      output(' ');
      output(Object.values(colWidths).map(w => ' '.repeat(w)).join('│'));
      output(' ');
    }
    output('  ');
    // print register table
    if (row.hasOwnProperty('registerFields')) {
      output('│');
      output(row.registerFields.map((value, r) =>
        formatField(value, 'c', regColWidths[r])
      ).join('│'));
      output('│');
    }
    output("\n");
  }


  //////////////////////////////////////////////////////////////////////////////
  // Print a legend
  //////////////////////////////////////////////////////////////////////////////
  // TODO Should this be optional?

  print(``);
  print(` Legend:`);
  print(` [ ${symbols.clearRegister} ]: Clear register`);
  print(` [ ${symbols.keepRegister} ]: Keep register`);
  print(` [ ${symbols.writeRegister} ]: Write register`);
  print(` [ ${symbols.readRegister} ]: Read register`);
  print(` [ ${symbols.unusedRegister} ]: Register is unused`);
  print(` [ ${symbols.subqueryBegin} ]: Entering a subquery`);
  print(` [ ${symbols.subqueryEnd} ]: Leaving a subquery`);
}

function explainRegisters(data, options, shouldPrint) {
  'use strict';

  console.warn("explainRegisters() is purposefully undocumented and may be changed or removed without notice.");

  if (typeof data === 'string') {
    data = { query: data, options: options };
  }
  if (!(data instanceof Object)) {
    throw 'ArangoStatement needs initial data';
  }

  if (options === undefined) {
    options = data.options;
  }
  options = options || {};
  options.verbosePlans = true;
  options.explainRegisters = true;
  setColors(options.colors === undefined ? true : options.colors);

  stringBuilder.clearOutput();
  let stmt = db._createStatement(data);
  let result = stmt.explain(options);
  if (options.allPlans) {
    // multiple plans
    printQuery(data.query);
    for (let i = 0; i < result.plans.length; ++i) {
      if (i > 0) {
        stringBuilder.appendLine();
      }
      stringBuilder.appendLine(section("Plan #" + (i + 1) + " of " + result.plans.length));
      stringBuilder.prefix = ' ';
      stringBuilder.appendLine();
      explainQuerysRegisters(result.plans[i]);
      stringBuilder.prefix = '';
    }
  } else {
    // single plan
    explainQuerysRegisters(result.plan);
  }

  if (shouldPrint === undefined || shouldPrint) {
    print(stringBuilder.getOutput());
  } else {
    return stringBuilder.getOutput();
  }
}

exports.explain = explain;
exports.explainRegisters = explainRegisters;
exports.profileQuery = profileQuery;
exports.debug = debug;
exports.debugDump = debugDump;
exports.inspectDump = inspectDump;
