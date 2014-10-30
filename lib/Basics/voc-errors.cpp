////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from errors.dat
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "./lib/Basics/voc-errors.h"

void TRI_InitialiseErrorMessages () {
  REG_ERROR(ERROR_NO_ERROR, "no error");
  REG_ERROR(ERROR_FAILED, "failed");
  REG_ERROR(ERROR_SYS_ERROR, "system error");
  REG_ERROR(ERROR_OUT_OF_MEMORY, "out of memory");
  REG_ERROR(ERROR_INTERNAL, "internal error");
  REG_ERROR(ERROR_ILLEGAL_NUMBER, "illegal number");
  REG_ERROR(ERROR_NUMERIC_OVERFLOW, "numeric overflow");
  REG_ERROR(ERROR_ILLEGAL_OPTION, "illegal option");
  REG_ERROR(ERROR_DEAD_PID, "dead process identifier");
  REG_ERROR(ERROR_NOT_IMPLEMENTED, "not implemented");
  REG_ERROR(ERROR_BAD_PARAMETER, "bad parameter");
  REG_ERROR(ERROR_FORBIDDEN, "forbidden");
  REG_ERROR(ERROR_OUT_OF_MEMORY_MMAP, "out of memory in mmap");
  REG_ERROR(ERROR_CORRUPTED_CSV, "csv is corrupt");
  REG_ERROR(ERROR_FILE_NOT_FOUND, "file not found");
  REG_ERROR(ERROR_CANNOT_WRITE_FILE, "cannot write file");
  REG_ERROR(ERROR_CANNOT_OVERWRITE_FILE, "cannot overwrite file");
  REG_ERROR(ERROR_TYPE_ERROR, "type error");
  REG_ERROR(ERROR_LOCK_TIMEOUT, "lock timeout");
  REG_ERROR(ERROR_CANNOT_CREATE_DIRECTORY, "cannot create directory");
  REG_ERROR(ERROR_CANNOT_CREATE_TEMP_FILE, "cannot create temporary file");
  REG_ERROR(ERROR_REQUEST_CANCELED, "canceled request");
  REG_ERROR(ERROR_DEBUG, "intentional debug error");
  REG_ERROR(ERROR_AID_NOT_FOUND, "internal error with attribute ID in shaper");
  REG_ERROR(ERROR_LEGEND_INCOMPLETE, "internal error if a legend could not be created");
  REG_ERROR(ERROR_IP_ADDRESS_INVALID, "IP address is invalid");
  REG_ERROR(ERROR_LEGEND_NOT_IN_WAL_FILE, "internal error if a legend for a marker does not yet exist in the same WAL file");
  REG_ERROR(ERROR_FILE_EXISTS, "file exists");
  REG_ERROR(ERROR_HTTP_BAD_PARAMETER, "bad parameter");
  REG_ERROR(ERROR_HTTP_UNAUTHORIZED, "unauthorized");
  REG_ERROR(ERROR_HTTP_FORBIDDEN, "forbidden");
  REG_ERROR(ERROR_HTTP_NOT_FOUND, "not found");
  REG_ERROR(ERROR_HTTP_METHOD_NOT_ALLOWED, "method not supported");
  REG_ERROR(ERROR_HTTP_PRECONDITION_FAILED, "precondition failed");
  REG_ERROR(ERROR_HTTP_SERVER_ERROR, "internal server error");
  REG_ERROR(ERROR_HTTP_CORRUPTED_JSON, "invalid JSON object");
  REG_ERROR(ERROR_HTTP_SUPERFLUOUS_SUFFICES, "superfluous URL suffices");
  REG_ERROR(ERROR_ARANGO_ILLEGAL_STATE, "illegal state");
  REG_ERROR(ERROR_ARANGO_SHAPER_FAILED, "could not shape document");
  REG_ERROR(ERROR_ARANGO_DATAFILE_SEALED, "datafile sealed");
  REG_ERROR(ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE, "unknown type");
  REG_ERROR(ERROR_ARANGO_READ_ONLY, "read only");
  REG_ERROR(ERROR_ARANGO_DUPLICATE_IDENTIFIER, "duplicate identifier");
  REG_ERROR(ERROR_ARANGO_DATAFILE_UNREADABLE, "datafile unreadable");
  REG_ERROR(ERROR_ARANGO_DATAFILE_EMPTY, "datafile empty");
  REG_ERROR(ERROR_ARANGO_RECOVERY, "logfile recovery error");
  REG_ERROR(ERROR_ARANGO_CORRUPTED_DATAFILE, "corrupted datafile");
  REG_ERROR(ERROR_ARANGO_ILLEGAL_PARAMETER_FILE, "illegal parameter file");
  REG_ERROR(ERROR_ARANGO_CORRUPTED_COLLECTION, "corrupted collection");
  REG_ERROR(ERROR_ARANGO_MMAP_FAILED, "mmap failed");
  REG_ERROR(ERROR_ARANGO_FILESYSTEM_FULL, "filesystem full");
  REG_ERROR(ERROR_ARANGO_NO_JOURNAL, "no journal");
  REG_ERROR(ERROR_ARANGO_DATAFILE_ALREADY_EXISTS, "cannot create/rename datafile because it already exists");
  REG_ERROR(ERROR_ARANGO_DATADIR_LOCKED, "database directory is locked");
  REG_ERROR(ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS, "cannot create/rename collection because directory already exists");
  REG_ERROR(ERROR_ARANGO_MSYNC_FAILED, "msync failed");
  REG_ERROR(ERROR_ARANGO_DATADIR_UNLOCKABLE, "cannot lock database directory");
  REG_ERROR(ERROR_ARANGO_SYNC_TIMEOUT, "sync timeout");
  REG_ERROR(ERROR_ARANGO_CONFLICT, "conflict");
  REG_ERROR(ERROR_ARANGO_DATADIR_INVALID, "invalid database directory");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_NOT_FOUND, "document not found");
  REG_ERROR(ERROR_ARANGO_COLLECTION_NOT_FOUND, "collection not found");
  REG_ERROR(ERROR_ARANGO_COLLECTION_PARAMETER_MISSING, "parameter 'collection' not found");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_HANDLE_BAD, "illegal document handle");
  REG_ERROR(ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL, "maximal size of journal too small");
  REG_ERROR(ERROR_ARANGO_DUPLICATE_NAME, "duplicate name");
  REG_ERROR(ERROR_ARANGO_ILLEGAL_NAME, "illegal name");
  REG_ERROR(ERROR_ARANGO_NO_INDEX, "no suitable index known");
  REG_ERROR(ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED, "unique constraint violated");
  REG_ERROR(ERROR_ARANGO_GEO_INDEX_VIOLATED, "geo index violated");
  REG_ERROR(ERROR_ARANGO_INDEX_NOT_FOUND, "index not found");
  REG_ERROR(ERROR_ARANGO_CROSS_COLLECTION_REQUEST, "cross collection request not allowed");
  REG_ERROR(ERROR_ARANGO_INDEX_HANDLE_BAD, "illegal index handle");
  REG_ERROR(ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED, "cap constraint already defined");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_TOO_LARGE, "document too large");
  REG_ERROR(ERROR_ARANGO_COLLECTION_NOT_UNLOADED, "collection must be unloaded");
  REG_ERROR(ERROR_ARANGO_COLLECTION_TYPE_INVALID, "collection type invalid");
  REG_ERROR(ERROR_ARANGO_VALIDATION_FAILED, "validator failed");
  REG_ERROR(ERROR_ARANGO_PARSER_FAILED, "parser failed");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_KEY_BAD, "illegal document key");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED, "unexpected document key");
  REG_ERROR(ERROR_ARANGO_DATADIR_NOT_WRITABLE, "server database directory not writable");
  REG_ERROR(ERROR_ARANGO_OUT_OF_KEYS, "out of keys");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_KEY_MISSING, "missing document key");
  REG_ERROR(ERROR_ARANGO_DOCUMENT_TYPE_INVALID, "invalid document type");
  REG_ERROR(ERROR_ARANGO_DATABASE_NOT_FOUND, "database not found");
  REG_ERROR(ERROR_ARANGO_DATABASE_NAME_INVALID, "database name invalid");
  REG_ERROR(ERROR_ARANGO_USE_SYSTEM_DATABASE, "operation only allowed in system database");
  REG_ERROR(ERROR_ARANGO_ENDPOINT_NOT_FOUND, "endpoint not found");
  REG_ERROR(ERROR_ARANGO_INVALID_KEY_GENERATOR, "invalid key generator");
  REG_ERROR(ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE, "edge attribute missing");
  REG_ERROR(ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING, "index insertion warning - attribute missing in document");
  REG_ERROR(ERROR_ARANGO_INDEX_CREATION_FAILED, "index creation failed");
  REG_ERROR(ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT, "write-throttling timeout");
  REG_ERROR(ERROR_ARANGO_DATAFILE_FULL, "datafile full");
  REG_ERROR(ERROR_ARANGO_EMPTY_DATADIR, "server database directory is empty");
  REG_ERROR(ERROR_REPLICATION_NO_RESPONSE, "no response");
  REG_ERROR(ERROR_REPLICATION_INVALID_RESPONSE, "invalid response");
  REG_ERROR(ERROR_REPLICATION_MASTER_ERROR, "master error");
  REG_ERROR(ERROR_REPLICATION_MASTER_INCOMPATIBLE, "master incompatible");
  REG_ERROR(ERROR_REPLICATION_MASTER_CHANGE, "master change");
  REG_ERROR(ERROR_REPLICATION_LOOP, "loop detected");
  REG_ERROR(ERROR_REPLICATION_UNEXPECTED_MARKER, "unexpected marker");
  REG_ERROR(ERROR_REPLICATION_INVALID_APPLIER_STATE, "invalid applier state");
  REG_ERROR(ERROR_REPLICATION_UNEXPECTED_TRANSACTION, "invalid transaction");
  REG_ERROR(ERROR_REPLICATION_INVALID_LOGGER_CONFIGURATION, "invalid replication logger configuration");
  REG_ERROR(ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION, "invalid replication applier configuration");
  REG_ERROR(ERROR_REPLICATION_RUNNING, "cannot change applier configuration while running");
  REG_ERROR(ERROR_REPLICATION_APPLIER_STOPPED, "replication stopped");
  REG_ERROR(ERROR_REPLICATION_NO_START_TICK, "no start tick");
  REG_ERROR(ERROR_CLUSTER_NO_AGENCY, "could not connect to agency");
  REG_ERROR(ERROR_CLUSTER_NO_COORDINATOR_HEADER, "missing coordinator header");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_LOCK_PLAN, "could not lock plan in agency");
  REG_ERROR(ERROR_CLUSTER_COLLECTION_ID_EXISTS, "collection ID already exists");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN, "could not create collection in plan");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION, "could not read version in current in agency");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION, "could not create collection");
  REG_ERROR(ERROR_CLUSTER_TIMEOUT, "timeout in cluster operation");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN, "could not remove collection from plan");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT, "could not remove collection from current");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN, "could not create database in plan");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE, "could not create database");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN, "could not remove database from plan");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT, "could not remove database from current");
  REG_ERROR(ERROR_CLUSTER_SHARD_GONE, "no responsible shard found");
  REG_ERROR(ERROR_CLUSTER_CONNECTION_LOST, "cluster internal HTTP connection broken");
  REG_ERROR(ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY, "must not specify _key for this collection");
  REG_ERROR(ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS, "got contradicting answers from different shards");
  REG_ERROR(ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN, "not all sharding attributes given");
  REG_ERROR(ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES, "must not change the value of a shard key attribute");
  REG_ERROR(ERROR_CLUSTER_UNSUPPORTED, "unsupported operation or parameter");
  REG_ERROR(ERROR_CLUSTER_ONLY_ON_COORDINATOR, "this operation is only valid on a coordinator in a cluster");
  REG_ERROR(ERROR_CLUSTER_READING_PLAN_AGENCY, "error reading Plan in agency");
  REG_ERROR(ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION, "could not truncate collection");
  REG_ERROR(ERROR_CLUSTER_AQL_COMMUNICATION, "error in cluster internal communication for AQL");
  REG_ERROR(ERROR_QUERY_KILLED, "query killed");
  REG_ERROR(ERROR_QUERY_PARSE, "%s");
  REG_ERROR(ERROR_QUERY_EMPTY, "query is empty");
  REG_ERROR(ERROR_QUERY_SCRIPT, "runtime error '%s'");
  REG_ERROR(ERROR_QUERY_NUMBER_OUT_OF_RANGE, "number out of range");
  REG_ERROR(ERROR_QUERY_VARIABLE_NAME_INVALID, "variable name '%s' has an invalid format");
  REG_ERROR(ERROR_QUERY_VARIABLE_REDECLARED, "variable '%s' is assigned multiple times");
  REG_ERROR(ERROR_QUERY_VARIABLE_NAME_UNKNOWN, "unknown variable '%s'");
  REG_ERROR(ERROR_QUERY_COLLECTION_LOCK_FAILED, "unable to read-lock collection %s");
  REG_ERROR(ERROR_QUERY_TOO_MANY_COLLECTIONS, "too many collections");
  REG_ERROR(ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED, "document attribute '%s' is assigned multiple times");
  REG_ERROR(ERROR_QUERY_FUNCTION_NAME_UNKNOWN, "usage of unknown function '%s()'");
  REG_ERROR(ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH, "invalid number of arguments for function '%s()', expected number of arguments: minimum: %d, maximum: %d");
  REG_ERROR(ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, "invalid argument type used in call to function '%s()'");
  REG_ERROR(ERROR_QUERY_INVALID_REGEX, "invalid regex argument value used in call to function '%s()'");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETERS_INVALID, "invalid structure of bind parameters");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_MISSING, "no value specified for declared bind parameter '%s'");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_UNDECLARED, "bind parameter '%s' was not declared in the query");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_TYPE, "bind parameter '%s' has an invalid value or type");
  REG_ERROR(ERROR_QUERY_INVALID_LOGICAL_VALUE, "invalid logical value");
  REG_ERROR(ERROR_QUERY_INVALID_ARITHMETIC_VALUE, "invalid arithmetic value");
  REG_ERROR(ERROR_QUERY_DIVISION_BY_ZERO, "division by zero");
  REG_ERROR(ERROR_QUERY_LIST_EXPECTED, "list expected");
  REG_ERROR(ERROR_QUERY_FAIL_CALLED, "FAIL(%s) called");
  REG_ERROR(ERROR_QUERY_GEO_INDEX_MISSING, "no suitable geo index found for geo restriction on '%s'");
  REG_ERROR(ERROR_QUERY_FULLTEXT_INDEX_MISSING, "no suitable fulltext index found for fulltext query on '%s'");
  REG_ERROR(ERROR_QUERY_INVALID_DATE_VALUE, "invalid date value");
  REG_ERROR(ERROR_QUERY_MULTI_MODIFY, "multi-modify query");
  REG_ERROR(ERROR_QUERY_MODIFY_IN_SUBQUERY, "modify operation in subquery");
  REG_ERROR(ERROR_QUERY_COMPILE_TIME_OPTIONS, "query options must be readable at query compile time");
  REG_ERROR(ERROR_QUERY_EXCEPTION_OPTIONS, "query options expected");
  REG_ERROR(ERROR_QUERY_BAD_JSON_PLAN, "JSON describing execution plan was bad");
  REG_ERROR(ERROR_QUERY_NOT_FOUND, "query ID not found");
  REG_ERROR(ERROR_QUERY_IN_USE, "query with this ID is in use");
  REG_ERROR(ERROR_QUERY_FUNCTION_INVALID_NAME, "invalid user function name");
  REG_ERROR(ERROR_QUERY_FUNCTION_INVALID_CODE, "invalid user function code");
  REG_ERROR(ERROR_QUERY_FUNCTION_NOT_FOUND, "user function '%s()' not found");
  REG_ERROR(ERROR_CURSOR_NOT_FOUND, "cursor not found");
  REG_ERROR(ERROR_TRANSACTION_INTERNAL, "internal transaction error");
  REG_ERROR(ERROR_TRANSACTION_NESTED, "nested transactions detected");
  REG_ERROR(ERROR_TRANSACTION_UNREGISTERED_COLLECTION, "unregistered collection used in transaction");
  REG_ERROR(ERROR_TRANSACTION_DISALLOWED_OPERATION, "disallowed operation inside transaction");
  REG_ERROR(ERROR_TRANSACTION_ABORTED, "transaction aborted");
  REG_ERROR(ERROR_USER_INVALID_NAME, "invalid user name");
  REG_ERROR(ERROR_USER_INVALID_PASSWORD, "invalid password");
  REG_ERROR(ERROR_USER_DUPLICATE, "duplicate user");
  REG_ERROR(ERROR_USER_NOT_FOUND, "user not found");
  REG_ERROR(ERROR_USER_CHANGE_PASSWORD, "user must change his password");
  REG_ERROR(ERROR_APPLICATION_INVALID_NAME, "invalid application name");
  REG_ERROR(ERROR_APPLICATION_INVALID_MOUNT, "invalid mount");
  REG_ERROR(ERROR_APPLICATION_DOWNLOAD_FAILED, "application download failed");
  REG_ERROR(ERROR_APPLICATION_UPLOAD_FAILED, "application upload failed");
  REG_ERROR(ERROR_KEYVALUE_INVALID_KEY, "invalid key declaration");
  REG_ERROR(ERROR_KEYVALUE_KEY_EXISTS, "key already exists");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_FOUND, "key not found");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_UNIQUE, "key is not unique");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_CHANGED, "key value not changed");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_REMOVED, "key value not removed");
  REG_ERROR(ERROR_KEYVALUE_NO_VALUE, "missing value");
  REG_ERROR(ERROR_TASK_INVALID_ID, "invalid task id");
  REG_ERROR(ERROR_TASK_DUPLICATE_ID, "duplicate task id");
  REG_ERROR(ERROR_TASK_NOT_FOUND, "task not found");
  REG_ERROR(ERROR_GRAPH_INVALID_GRAPH, "invalid graph");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, "could not create graph");
  REG_ERROR(ERROR_GRAPH_INVALID_VERTEX, "invalid vertex");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, "could not create vertex");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX, "could not change vertex");
  REG_ERROR(ERROR_GRAPH_INVALID_EDGE, "invalid edge");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CREATE_EDGE, "could not create edge");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CHANGE_EDGE, "could not change edge");
  REG_ERROR(ERROR_GRAPH_TOO_MANY_ITERATIONS, "too many iterations");
  REG_ERROR(ERROR_GRAPH_INVALID_FILTER_RESULT, "invalid filter result");
  REG_ERROR(ERROR_GRAPH_COLLECTION_MULTI_USE, "multi use of edge collection in edge def");
  REG_ERROR(ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS, "edge collection already used in edge def");
  REG_ERROR(ERROR_GRAPH_CREATE_MISSING_NAME, "missing graph name");
  REG_ERROR(ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION, "malformed edge def");
  REG_ERROR(ERROR_GRAPH_NOT_FOUND, "graph not found");
  REG_ERROR(ERROR_GRAPH_DUPLICATE, "graph already exists");
  REG_ERROR(ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST, "collection does not exist");
  REG_ERROR(ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX, "not a vertex collection");
  REG_ERROR(ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION, "not in orphan collection");
  REG_ERROR(ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF, "collection used in edge def");
  REG_ERROR(ERROR_GRAPH_EDGE_COLLECTION_NOT_USED, "edge collection not used in graph");
  REG_ERROR(ERROR_GRAPH_NO_AN_ARANGO_COLLECTION, " is not an ArangoCollection");
  REG_ERROR(ERROR_GRAPH_NO_GRAPH_COLLECTION, "collection _graphs does not exist");
  REG_ERROR(ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING, "Invalid example type. Has to be String, Array or Object");
  REG_ERROR(ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT, "Invalid example type. Has to be Array or Object");
  REG_ERROR(ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS, "Invalid number of arguments. Expected: ");
  REG_ERROR(ERROR_GRAPH_INVALID_PARAMETER, "Invalid parameter type.");
  REG_ERROR(ERROR_GRAPH_INVALID_ID, "Invalid id");
  REG_ERROR(ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS, "collection used in orphans");
  REG_ERROR(ERROR_SESSION_UNKNOWN, "unknown session");
  REG_ERROR(ERROR_SESSION_EXPIRED, "session expired");
  REG_ERROR(SIMPLE_CLIENT_UNKNOWN_ERROR, "unknown client error");
  REG_ERROR(SIMPLE_CLIENT_COULD_NOT_CONNECT, "could not connect to server");
  REG_ERROR(SIMPLE_CLIENT_COULD_NOT_WRITE, "could not write to server");
  REG_ERROR(SIMPLE_CLIENT_COULD_NOT_READ, "could not read from server");
  REG_ERROR(RESULT_KEY_EXISTS, "element not inserted into structure, because key already exists");
  REG_ERROR(RESULT_ELEMENT_EXISTS, "element not inserted into structure, because it already exists");
  REG_ERROR(RESULT_KEY_NOT_FOUND, "key not found in structure");
  REG_ERROR(RESULT_ELEMENT_NOT_FOUND, "element not found in structure");
  REG_ERROR(ERROR_APP_ALREADY_EXISTS, "newest version of app already installed");
  REG_ERROR(ERROR_QUEUE_ALREADY_EXISTS, "named queue already exists");
  REG_ERROR(ERROR_DISPATCHER_IS_STOPPING, "dispatcher stopped");
  REG_ERROR(ERROR_QUEUE_UNKNOWN, "named queue does not exist");
  REG_ERROR(ERROR_QUEUE_FULL, "named queue is full");
}
