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
    "ERROR_ARANGO_CORRUPTED_DATAFILE" : { "code" : 1100, "message" : "corrupted datafile" }, 
    "ERROR_ARANGO_ILLEGAL_PARAMETER_FILE" : { "code" : 1101, "message" : "illegal parameter file" }, 
    "ERROR_ARANGO_CORRUPTED_COLLECTION" : { "code" : 1102, "message" : "corrupted collection" }, 
    "ERROR_ARANGO_MMAP_FAILED"     : { "code" : 1103, "message" : "mmap failed" }, 
    "ERROR_ARANGO_FILESYSTEM_FULL" : { "code" : 1104, "message" : "filesystem full" }, 
    "ERROR_ARANGO_NO_JOURNAL"      : { "code" : 1105, "message" : "no journal" }, 
    "ERROR_ARANGO_DATAFILE_ALREADY_EXISTS" : { "code" : 1106, "message" : "cannot create/rename datafile because it already exists" }, 
    "ERROR_ARANGO_DATADIR_LOCKED"  : { "code" : 1107, "message" : "database directory is locked" }, 
    "ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS" : { "code" : 1108, "message" : "cannot create/rename collection because directory already exists" }, 
    "ERROR_ARANGO_MSYNC_FAILED"    : { "code" : 1109, "message" : "msync failed" }, 
    "ERROR_ARANGO_DATADIR_UNLOCKABLE" : { "code" : 1110, "message" : "cannot lock database directory" }, 
    "ERROR_ARANGO_CONFLICT"        : { "code" : 1200, "message" : "conflict" }, 
    "ERROR_ARANGO_DATADIR_INVALID" : { "code" : 1201, "message" : "invalid database directory" }, 
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
    "ERROR_ARANGO_DATAFILE_FULL"   : { "code" : 1300, "message" : "datafile full" }, 
    "ERROR_REPLICATION_NO_RESPONSE" : { "code" : 1400, "message" : "no response" }, 
    "ERROR_REPLICATION_INVALID_RESPONSE" : { "code" : 1401, "message" : "invalid response" }, 
    "ERROR_REPLICATION_MASTER_ERROR" : { "code" : 1402, "message" : "master error" }, 
    "ERROR_REPLICATION_MASTER_INCOMPATIBLE" : { "code" : 1403, "message" : "master incompatible" }, 
    "ERROR_REPLICATION_MASTER_CHANGE" : { "code" : 1404, "message" : "master change" }, 
    "ERROR_REPLICATION_LOOP"       : { "code" : 1405, "message" : "loop detected" }, 
    "ERROR_REPLICATION_UNEXPECTED_MARKER" : { "code" : 1406, "message" : "unexpected marker" }, 
    "ERROR_REPLICATION_INVALID_APPLIER_STATE" : { "code" : 1407, "message" : "invalid applier state" }, 
    "ERROR_REPLICATION_UNEXPECTED_TRANSACTION" : { "code" : 1408, "message" : "invalid transaction" }, 
    "ERROR_REPLICATION_INVALID_LOGGER_CONFIGURATION" : { "code" : 1409, "message" : "invalid replication logger configuration" }, 
    "ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION" : { "code" : 1410, "message" : "invalid replication applier configuration" }, 
    "ERROR_REPLICATION_RUNNING"    : { "code" : 1411, "message" : "cannot change applier configuration while running" }, 
    "ERROR_REPLICATION_APPLIER_STOPPED" : { "code" : 1412, "message" : "replication stopped" }, 
    "ERROR_REPLICATION_NO_START_TICK" : { "code" : 1413, "message" : "no start tick" }, 
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
    "ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES" : { "code" : 1469, "message" : "must not change the value of a shardkey attribute" }, 
    "ERROR_CLUSTER_UNSUPPORTED"    : { "code" : 1470, "message" : "unsupported operation or parameter" }, 
    "ERROR_CLUSTER_ONLY_ON_COORDINATOR" : { "code" : 1471, "message" : "this operation is only valid on a coordinator in a cluster" }, 
    "ERROR_CLUSTER_READING_PLAN_AGENCY" : { "code" : 1472, "message" : "error reading Plan in agency" }, 
    "ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION" : { "code" : 1473, "message" : "could not truncate collection" }, 
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
    "ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH" : { "code" : 1541, "message" : "invalid number of arguments for function '%s()'" }, 
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
    "ERROR_QUERY_INVALID_DATE_VALUE" : { "code" : 1572, "message" : "invalid date value" }, 
    "ERROR_QUERY_FUNCTION_INVALID_NAME" : { "code" : 1580, "message" : "invalid user function name" }, 
    "ERROR_QUERY_FUNCTION_INVALID_CODE" : { "code" : 1581, "message" : "invalid user function code" }, 
    "ERROR_QUERY_FUNCTION_NOT_FOUND" : { "code" : 1582, "message" : "user function '%s()' not found" }, 
    "ERROR_CURSOR_NOT_FOUND"       : { "code" : 1600, "message" : "cursor not found" }, 
    "ERROR_TRANSACTION_INTERNAL"   : { "code" : 1650, "message" : "internal transaction error" }, 
    "ERROR_TRANSACTION_NESTED"     : { "code" : 1651, "message" : "nested transactions detected" }, 
    "ERROR_TRANSACTION_UNREGISTERED_COLLECTION" : { "code" : 1652, "message" : "unregistered collection used in transaction" }, 
    "ERROR_TRANSACTION_DISALLOWED_OPERATION" : { "code" : 1653, "message" : "disallowed operation inside transaction" }, 
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
    "ERROR_GRAPH_TOO_MANY_ITERATIONS" : { "code" : 1909, "message" : "too many iterations" }, 
    "ERROR_GRAPH_INVALID_FILTER_RESULT" : { "code" : 1910, "message" : "invalid filter result" }, 
    "ERROR_SESSION_UNKNOWN"        : { "code" : 1950, "message" : "unknown session" }, 
    "ERROR_SESSION_EXPIRED"        : { "code" : 1951, "message" : "session expired" }, 
    "SIMPLE_CLIENT_UNKNOWN_ERROR"  : { "code" : 2000, "message" : "unknown client error" }, 
    "SIMPLE_CLIENT_COULD_NOT_CONNECT" : { "code" : 2001, "message" : "could not connect to server" }, 
    "SIMPLE_CLIENT_COULD_NOT_WRITE" : { "code" : 2002, "message" : "could not write to server" }, 
    "SIMPLE_CLIENT_COULD_NOT_READ" : { "code" : 2003, "message" : "could not read from server" }, 
    "ERROR_ARANGO_INDEX_BITARRAY_UPDATE_ATTRIBUTE_MISSING" : { "code" : 3402, "message" : "bitarray index update warning - attribute missing in revised document" }, 
    "ERROR_ARANGO_INDEX_BITARRAY_REMOVE_ITEM_MISSING" : { "code" : 3411, "message" : "bitarray index remove failure - item missing in index" }, 
    "ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE" : { "code" : 3413, "message" : "bitarray index insert failure - document attribute value unsupported in index" }, 
    "ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_ATTRIBUTES" : { "code" : 3415, "message" : "bitarray index creation failure - one or more index attributes are duplicated." }, 
    "ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_VALUES" : { "code" : 3417, "message" : "bitarray index creation failure - one or more index attribute values are duplicated." }, 
    "RESULT_KEY_EXISTS"            : { "code" : 10000, "message" : "element not inserted into structure, because key already exists" }, 
    "RESULT_ELEMENT_EXISTS"        : { "code" : 10001, "message" : "element not inserted into structure, because it already exists" }, 
    "RESULT_KEY_NOT_FOUND"         : { "code" : 10002, "message" : "key not found in structure" }, 
    "RESULT_ELEMENT_NOT_FOUND"     : { "code" : 10003, "message" : "element not found in structure" }
  };
}());

