/*jshint browserify: true */
'use strict';
exports.keywords = [
  'asc',
  'collect',
  'desc',
  'distinct',
  'false',
  'filter',
  'for',
  'in',
  'insert',
  'into',
  'new',
  'let',
  'limit',
  'old',
  'null',
  'remove',
  'replace',
  'return',
  'sort',
  'true',
  'update',
  'with'
];

exports.builtins = {
  // Conversion
  TO_BOOL: 1, TO_NUMBER: 1, TO_STRING: 1, TO_LIST: 1,
  // Type checks
  IS_NULL: 1, IS_BOOL: 1, IS_NUMBER: 1, IS_STRING: 1, IS_LIST: 1, IS_DOCUMENT: 1,
  // String functions
  CONCAT: [[1, Infinity]], CONCAT_SEPARATOR: [[2, Infinity]],
  CHAR_LENGTH: 1, LENGTH: 1, LOWER: 1, UPPER: 1, SUBSTRING: [2, 3],
  LEFT: 2, RIGHT: 2, TRIM: [1, 2], REVERSE: 1, CONTAINS: [2, 3], LIKE: 3,
  LTRIM: [1, 2], RTRIM: [1, 2], FIND_FIRST: [2, 3, 4], FIND_LAST: [2, 3, 4],
  SPLIT: [1, 2, 3], SUBSTITUTE: [2, 3, 4], MD5: 1, SHA1: 1, RANDOM_TOKEN: 1,
  // Numeric functions
  FLOOR: 1, CEIL: 1, ROUND: 1, ABS: 1, SQRT: 1, RAND: 0,
  // Date functions
  DATE_TIMESTAMP: [1, [3, 7]], DATE_ISO8601: [1, [3, 7]],
  DATE_DAYOFWEEK: 1, DATE_YEAR: 1, DATE_MONTH: 1, DATE_DAY: 1,
  DATE_HOUR: 1, DATE_MINUTE: 1, DATE_SECOND: 1, DATE_MILLISECOND: 1,
  DATE_NOW: 0,
  // List functions
  /*LENGTH: 1,*/ FLATTEN: [1, 2], MIN: 1, MAX: 1, AVERAGE: 1, SUM: 1,
  MEDIAN: 1, PERCENTILE: [2, 3], VARIANCE_POPULATION: 1, VARIANCE_SAMPLE: 1,
  STDDEV_POPULATION: 1, STDDEV_SAMPLE: 1, /*REVERSE: 1,*/
  FIRST: 1, LAST: 1, NTH: 2, POSITION: [2, 3], SLICE: [2, 3],
  UNIQUE: 1, UNION: [[1, Infinity]], UNION_DISTINCT: [[1, Infinity]],
  MINUS: [[1, Infinity]], INTERSECTION: [[1, Infinity]],
  CALL: [[1, Infinity]], APPLY: [[1, Infinity]],
  PUSH: [2, 3], APPEND: [2, 3], POP: 1, SHIFT: 1, UNSHIFT: [2, 3],
  REMOVE_VALUE: [2, 3], REMOVE_VALUES: 2, REMOVE_NTH: 2,
  // Document functions
  MATCHES: [2, 3], MERGE: [[1, Infinity]], MERGE_RECURSIVE: [[1, Infinity]],
  TRANSLATE: [2, 3], HAS: 2, ATTRIBUTES: [[1, 3]], UNSET: [[1, Infinity]],
  KEEP: [[2, Infinity]], PARSE_IDENTIFIER: 1, ZIP: 2,
  // Geo functions
  NEAR: [5, 6], WITHIN: [5, 6], WITHIN_RECTANGLE: 5, IS_IN_POLYGON: [2, 3],
  // Fulltext functions
  FULLTEXT: 3,
  // Graph functions
  PATHS: [3, 4], TRAVERSAL: [5, 6], TRAVERSAL_TREE: [5, 6],
  SHORTEST_PATH: [5, 6], EDGES: [3, 4], NEIGHBORS: [4, 5],
  GRAPH_PATHS: [1, 2], GRAPH_SHORTEST_PATH: [3, 4], GRAPH_DISTANCE_TO: [3, 4],
  GRAPH_TRAVERSAL: [3, 4], GRAPH_TRAVERSAL_TREE: [4, 5], GRAPH_EDGES: [2, 3],
  GRAPH_VERTICES: [2, 3], GRAPH_NEIGHBORS: [2, 3], GRAPH_COMMON_NEIGHBORS: [3, 4, 5],
  GRAPH_COMMON_PROPERTIES: [3, 4], GRAPH_ECCENTRICITY: [1, 2],
  GRAPH_BETWEENNESS: [1, 2], GRAPH_CLOSENESS: [1, 2],
  GRAPH_ABSOLUTE_ECCENTRICITY: [2, 3], GRAPH_ABSOLUTE_BETWEENNESS: [2, 3],
  GRAPH_ABSOLUTE_CLOSENESS: [2, 3], GRAPH_DIAMETER: [1, 2], GRAPH_RADIUS: [1, 2],
  // Control flow functions
  NOT_NULL: [[1, Infinity]], FIRST_LIST: [[1, Infinity]],
  FIRST_DOCUMENT: [[1, Infinity]],
  // Miscellaneous functions
  COLLECTIONS: 0, CURRENT_USER: 0, DOCUMENT: [1, 2], SKIPLIST: [[2, 4]]
};

exports.deprecatedBuiltins = [
  'SKIPLIST'
];
