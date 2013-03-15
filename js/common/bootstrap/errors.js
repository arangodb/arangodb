/*jslint indent: 2,
         nomen: true,
         maxlen: 240,
         sloppy: true,
         vars: true,
         white: true,
         plusplus: true */
/*global require */

////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from errors.dat
////////////////////////////////////////////////////////////////////////////////

(function () {
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
    "ERROR_HTTP_BAD_PARAMETER"     : { "code" : 400, "message" : "bad parameter" }, 
    "ERROR_HTTP_FORBIDDEN"         : { "code" : 403, "message" : "forbidden" }, 
    "ERROR_HTTP_NOT_FOUND"         : { "code" : 404, "message" : "not found" }, 
    "ERROR_HTTP_METHOD_NOT_ALLOWED" : { "code" : 405, "message" : "method not supported" }, 
    "ERROR_HTTP_SERVER_ERROR"      : { "code" : 500, "message" : "internal server error" }, 
    "ERROR_HTTP_CORRUPTED_JSON"    : { "code" : 600, "message" : "invalid JSON object" }, 
    "ERROR_HTTP_SUPERFLUOUS_SUFFICES" : { "code" : 601, "message" : "superfluous URL suffices" }, 
    "ERROR_ARANGO_ILLEGAL_STATE"   : { "code" : 1000, "message" : "illegal state" }, 
    "ERROR_ARANGO_SHAPER_FAILED"   : { "code" : 1001, "message" : "illegal shaper" }, 
    "ERROR_ARANGO_DATAFILE_SEALED" : { "code" : 1002, "message" : "datafile sealed" }, 
    "ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE" : { "code" : 1003, "message" : "unknown type" }, 
    "ERROR_ARANGO_READ_ONLY"       : { "code" : 1004, "message" : "ready only" }, 
    "ERROR_ARANGO_DUPLICATE_IDENTIFIER" : { "code" : 1005, "message" : "duplicate identifier" }, 
    "ERROR_ARANGO_DATAFILE_UNREADABLE" : { "code" : 1006, "message" : "datafile unreadable" }, 
    "ERROR_ARANGO_CORRUPTED_DATAFILE" : { "code" : 1100, "message" : "corrupted datafile" }, 
    "ERROR_ARANGO_ILLEGAL_PARAMETER_FILE" : { "code" : 1101, "message" : "illegal parameter file" }, 
    "ERROR_ARANGO_CORRUPTED_COLLECTION" : { "code" : 1102, "message" : "corrupted collection" }, 
    "ERROR_ARANGO_MMAP_FAILED"     : { "code" : 1103, "message" : "mmap failed" }, 
    "ERROR_ARANGO_FILESYSTEM_FULL" : { "code" : 1104, "message" : "filesystem full" }, 
    "ERROR_ARANGO_NO_JOURNAL"      : { "code" : 1105, "message" : "no journal" }, 
    "ERROR_ARANGO_DATAFILE_ALREADY_EXISTS" : { "code" : 1106, "message" : "cannot create/rename datafile because it already exists" }, 
    "ERROR_ARANGO_DATABASE_LOCKED" : { "code" : 1107, "message" : "database is locked" }, 
    "ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS" : { "code" : 1108, "message" : "cannot create/rename collection because directory already exists" }, 
    "ERROR_ARANGO_MSYNC_FAILED"    : { "code" : 1109, "message" : "msync failed" }, 
    "ERROR_ARANGO_CONFLICT"        : { "code" : 1200, "message" : "conflict" }, 
    "ERROR_ARANGO_WRONG_VOCBASE_PATH" : { "code" : 1201, "message" : "wrong path for database" }, 
    "ERROR_ARANGO_DOCUMENT_NOT_FOUND" : { "code" : 1202, "message" : "document not found" }, 
    "ERROR_ARANGO_COLLECTION_NOT_FOUND" : { "code" : 1203, "message" : "collection not found" }, 
    "ERROR_ARANGO_COLLECTION_PARAMETER_MISSING" : { "code" : 1204, "message" : "parameter 'collection' not found" }, 
    "ERROR_ARANGO_DOCUMENT_HANDLE_BAD" : { "code" : 1205, "message" : "illegal document handle" }, 
    "ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL" : { "code" : 1206, "message" : "maixmal size of journal too small" }, 
    "ERROR_ARANGO_DUPLICATE_NAME"  : { "code" : 1207, "message" : "duplicate name" }, 
    "ERROR_ARANGO_ILLEGAL_NAME"    : { "code" : 1208, "message" : "illegal name" }, 
    "ERROR_ARANGO_NO_INDEX"        : { "code" : 1209, "message" : "no suitable index known" }, 
    "ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED" : { "code" : 1210, "message" : "unique constraint violated" }, 
    "ERROR_ARANGO_GEO_INDEX_VIOLATED" : { "code" : 1211, "message" : "geo index violated" }, 
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
    "ERROR_ARANGO_INDEX_NEEDS_RESIZE" : { "code" : 1223, "message" : "index needs resizing" }, 
    "ERROR_ARANGO_DATADIR_NOT_WRITABLE" : { "code" : 1224, "message" : "database directory not writable" }, 
    "ERROR_ARANGO_OUT_OF_KEYS"     : { "code" : 1225, "message" : "out of keys" }, 
    "ERROR_ARANGO_DATAFILE_FULL"   : { "code" : 1300, "message" : "datafile full" }, 
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
    "ERROR_QUERY_FUNCTION_NAME_UNKNOWN" : { "code" : 1540, "message" : "usage of unknown function '%s'" }, 
    "ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH" : { "code" : 1541, "message" : "invalid number of arguments for function '%s'" }, 
    "ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH" : { "code" : 1542, "message" : "invalid argument type used in call to function '%s()'" }, 
    "ERROR_QUERY_INVALID_REGEX"    : { "code" : 1543, "message" : "invalid regex argument value used in call to function '%s()'" }, 
    "ERROR_QUERY_BIND_PARAMETERS_INVALID" : { "code" : 1550, "message" : "invalid structure of bind parameters" }, 
    "ERROR_QUERY_BIND_PARAMETER_MISSING" : { "code" : 1551, "message" : "no value specified for declared bind parameter '%s'" }, 
    "ERROR_QUERY_BIND_PARAMETER_UNDECLARED" : { "code" : 1552, "message" : "bind parameter '%s' was not declared in the query" }, 
    "ERROR_QUERY_BIND_PARAMETER_TYPE" : { "code" : 1553, "message" : "bind parameter '%s' has an invalid value or type" }, 
    "ERROR_QUERY_INVALID_LOGICAL_VALUE" : { "code" : 1560, "message" : "invalid logical value" }, 
    "ERROR_QUERY_INVALID_ARITHMETIC_VALUE" : { "code" : 1561, "message" : "invalid arithmetic value" }, 
    "ERROR_QUERY_DIVISION_BY_ZERO" : { "code" : 1562, "message" : "division by zero" }, 
    "ERROR_QUERY_LIST_EXPECTED"    : { "code" : 1563, "message" : "list expected" }, 
    "ERROR_QUERY_FAIL_CALLED"      : { "code" : 1569, "message" : "FAIL(%s) called" }, 
    "ERROR_QUERY_GEO_INDEX_MISSING" : { "code" : 1570, "message" : "no suitable geo index found for geo restriction on '%s'" }, 
    "ERROR_QUERY_FULLTEXT_INDEX_MISSING" : { "code" : 1571, "message" : "no suitable fulltext index found for fulltext query on '%s'" }, 
    "ERROR_CURSOR_NOT_FOUND"       : { "code" : 1600, "message" : "cursor not found" }, 
    "ERROR_TRANSACTION_INCOMPLETE" : { "code" : 1650, "message" : "transaction definition is incomplete" }, 
    "ERROR_TRANSACTION_INVALID_STATE" : { "code" : 1651, "message" : "invalid transaction state" }, 
    "ERROR_TRANSACTION_NESTED"     : { "code" : 1652, "message" : "nested transactions detected" }, 
    "ERROR_TRANSACTION_INTERNAL"   : { "code" : 1653, "message" : "internal transaction error" }, 
    "ERROR_TRANSACTION_UNREGISTERED_COLLECTION" : { "code" : 1654, "message" : "unregistered collection used in transaction" }, 
    "ERROR_USER_INVALID_NAME"      : { "code" : 1700, "message" : "invalid user name" }, 
    "ERROR_USER_INVALID_PASSWORD"  : { "code" : 1701, "message" : "invalid password" }, 
    "ERROR_USER_DUPLICATE"         : { "code" : 1702, "message" : "duplicate user" }, 
    "ERROR_USER_NOT_FOUND"         : { "code" : 1703, "message" : "user not found" }, 
    "ERROR_KEYVALUE_INVALID_KEY"   : { "code" : 1800, "message" : "invalid key declaration" }, 
    "ERROR_KEYVALUE_KEY_EXISTS"    : { "code" : 1801, "message" : "key already exists" }, 
    "ERROR_KEYVALUE_KEY_NOT_FOUND" : { "code" : 1802, "message" : "key not found" }, 
    "ERROR_KEYVALUE_KEY_NOT_UNIQUE" : { "code" : 1803, "message" : "key is not unique" }, 
    "ERROR_KEYVALUE_KEY_NOT_CHANGED" : { "code" : 1804, "message" : "key value not changed" }, 
    "ERROR_KEYVALUE_KEY_NOT_REMOVED" : { "code" : 1805, "message" : "key value not removed" }, 
    "ERROR_KEYVALUE_NO_VALUE"      : { "code" : 1806, "message" : "missing value" }, 
    "ERROR_GRAPH_INVALID_GRAPH"    : { "code" : 1901, "message" : "invalid graph" }, 
    "ERROR_GRAPH_COULD_NOT_CREATE_GRAPH" : { "code" : 1902, "message" : "could not create graph" }, 
    "ERROR_GRAPH_INVALID_VERTEX"   : { "code" : 1903, "message" : "invalid vertex" }, 
    "ERROR_GRAPH_COULD_NOT_CREATE_VERTEX" : { "code" : 1904, "message" : "could not create vertex" }, 
    "ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX" : { "code" : 1905, "message" : "could not change vertex" }, 
    "ERROR_GRAPH_INVALID_EDGE"     : { "code" : 1906, "message" : "invalid edge" }, 
    "ERROR_GRAPH_COULD_NOT_CREATE_EDGE" : { "code" : 1907, "message" : "could not create edge" }, 
    "ERROR_GRAPH_COULD_NOT_CHANGE_EDGE" : { "code" : 1908, "message" : "could not change edge" }, 
    "ERROR_SESSION_INVALID_SESSION" : { "code" : 1951, "message" : "invalid session" }, 
    "ERROR_SESSION_COULD_NOT_CREATE_SESSION" : { "code" : 1952, "message" : "could not create session" }, 
    "ERROR_SESSION_COULD_NOT_CHANGE_SESSION" : { "code" : 1953, "message" : "could not change session" }, 
    "ERROR_SESSION_INVALID_FORM"   : { "code" : 1961, "message" : "invalid form" }, 
    "ERROR_SESSION_COULD_NOT_CREATE_FORM" : { "code" : 1962, "message" : "could not create form" }, 
    "ERROR_SESSION_COULD_NOT_CHANGE_FORM" : { "code" : 1963, "message" : "could not change form" }, 
    "SIMPLE_CLIENT_UNKNOWN_ERROR"  : { "code" : 2000, "message" : "unknown client error" }, 
    "SIMPLE_CLIENT_COULD_NOT_CONNECT" : { "code" : 2001, "message" : "could not connect to server" }, 
    "SIMPLE_CLIENT_COULD_NOT_WRITE" : { "code" : 2002, "message" : "could not write to server" }, 
    "SIMPLE_CLIENT_COULD_NOT_READ" : { "code" : 2003, "message" : "could not read from server" }, 
    "ERROR_ARANGO_INDEX_PQ_INSERT_FAILED" : { "code" : 3100, "message" : "priority queue insert failure" }, 
    "ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED" : { "code" : 3110, "message" : "priority queue remove failure" }, 
    "ERROR_ARANGO_INDEX_PQ_REMOVE_ITEM_MISSING" : { "code" : 3111, "message" : "priority queue remove failure - item missing in index" }, 
    "ERROR_ARANGO_INDEX_HASH_INSERT_ITEM_DUPLICATED" : { "code" : 3312, "message" : "(non-unique) hash index insert failure - document duplicated in index" }, 
    "ERROR_ARANGO_INDEX_SKIPLIST_INSERT_ITEM_DUPLICATED" : { "code" : 3313, "message" : "(non-unique) skiplist index insert failure - document duplicated in index" }, 
    "WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING" : { "code" : 3200, "message" : "hash index insertion warning - attribute missing in document" }, 
    "WARNING_ARANGO_INDEX_HASH_UPDATE_ATTRIBUTE_MISSING" : { "code" : 3202, "message" : "hash index update warning - attribute missing in revised document" }, 
    "WARNING_ARANGO_INDEX_HASH_REMOVE_ITEM_MISSING" : { "code" : 3211, "message" : "hash index remove failure - item missing in index" }, 
    "WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING" : { "code" : 3300, "message" : "skiplist index insertion warning - attribute missing in document" }, 
    "WARNING_ARANGO_INDEX_SKIPLIST_UPDATE_ATTRIBUTE_MISSING" : { "code" : 3302, "message" : "skiplist index update warning - attribute missing in revised document" }, 
    "WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING" : { "code" : 3311, "message" : "skiplist index remove failure - item missing in index" }, 
    "WARNING_ARANGO_INDEX_BITARRAY_DOCUMENT_ATTRIBUTE_MISSING" : { "code" : 3400, "message" : "bitarray index insertion warning - attribute missing in document" }, 
    "WARNING_ARANGO_INDEX_BITARRAY_UPDATE_ATTRIBUTE_MISSING" : { "code" : 3402, "message" : "bitarray index update warning - attribute missing in revised document" }, 
    "WARNING_ARANGO_INDEX_BITARRAY_REMOVE_ITEM_MISSING" : { "code" : 3411, "message" : "bitarray index remove failure - item missing in index" }, 
    "ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE" : { "code" : 3413, "message" : "bitarray index insert failure - document attribute value unsupported in index" }, 
    "ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_ATTRIBUTES" : { "code" : 3415, "message" : "bitarray index creation failure - one or more index attributes are duplicated." }, 
    "ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_VALUES" : { "code" : 3417, "message" : "bitarray index creation failure - one or more index attribute values are duplicated." }, 
    "RESULT_KEY_EXISTS"            : { "code" : 10000, "message" : "element not inserted into structure, because key already exists" }, 
    "RESULT_ELEMENT_EXISTS"        : { "code" : 10001, "message" : "element not inserted into structure, because it already exists" }, 
    "RESULT_KEY_NOT_FOUND"         : { "code" : 10002, "message" : "key not found in structure" }, 
    "RESULT_ELEMENT_NOT_FOUND"     : { "code" : 10003, "message" : "element not found in structure" }
  };
}());

