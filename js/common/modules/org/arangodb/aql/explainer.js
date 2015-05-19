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

function passthru (v) {
  'use strict';
  return v;
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
  stringBuilder.appendLine(" " + value(wrap(query, 100).replace(/\n/g, "\n ", query)));
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

  if (indexes.length > 0) {
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
      l = index.fields.map(passthru).join(", ").length;
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
      var fieldsLen = indexes[i].fields.map(passthru).join(", ").length;
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
          return buildExpression(references[node.name]) + "[*]";
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
      case "attribute access":
        return buildExpression(node.subNodes[0]) + "." + attribute(node.name);
      case "indexed access":
        return buildExpression(node.subNodes[0]) + "[" + buildExpression(node.subNodes[1]) + "]";
      case "range":
        return buildExpression(node.subNodes[0]) + " .. " + buildExpression(node.subNodes[1]) + "   " + annotation("/* range */");
      case "expand":
        references[node.subNodes[0].subNodes[0].name] = node.subNodes[0].subNodes[1];
        return buildExpression(node.subNodes[1]);
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
        return keyword("FOR") + " " + variableName(node.outVariable) + " " + keyword("IN") + " " + collection(node.collection) + "   " + annotation("/* " + (node.reverse ? "reverse " : "") + node.index.type + " index scan") + annotation("*/");
      case "CalculationNode":
        return keyword("LET") + " " + variableName(node.outVariable) + " = " + buildExpression(node.expression);
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
        return keyword("UPSERT") + " " + variableName(node.inDocVariable) + " " + keyword("INSERT") + " " + variableName(node.insertVariable) + " " + keyword(node.isReplace ? "REPLACE" : "UPDATE") + variableName(node.updateVariable) + " " + keyword("IN") + " " + collection(node.collection);
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

  var returnNodes = [];

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
  printRules(plan.rules);
  printModificationFlags(modificationFlags);
  printWarnings(explain.warnings);
}

/* the exposed function */
function explain (data, options) {
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
  print(stringBuilder.getOutput());

  return stringBuilder.getOutput();
}

exports.explain = explain;

