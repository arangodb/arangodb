////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from errors.dat
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/common.h>
#include "BasicsC/voc-errors.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocError
/// @{
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseErrorMessages (void) {
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
  REG_ERROR(ERROR_HTTP_BAD_PARAMETER, "bad parameter");
  REG_ERROR(ERROR_HTTP_FORBIDDEN, "forbidden");
  REG_ERROR(ERROR_HTTP_NOT_FOUND, "not found");
  REG_ERROR(ERROR_HTTP_METHOD_NOT_ALLOWED, "method not supported");
  REG_ERROR(ERROR_HTTP_SERVER_ERROR, "internal server error");
  REG_ERROR(ERROR_HTTP_CORRUPTED_JSON, "invalid JSON object");
  REG_ERROR(ERROR_HTTP_SUPERFLUOUS_SUFFICES, "superfluous URL suffices");
  REG_ERROR(ERROR_AVOCADO_ILLEGAL_STATE, "illegal state");
  REG_ERROR(ERROR_AVOCADO_SHAPER_FAILED, "illegal shaper");
  REG_ERROR(ERROR_AVOCADO_DATAFILE_SEALED, "datafile sealed");
  REG_ERROR(ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE, "unknown type");
  REG_ERROR(ERROR_AVOCADO_READ_ONLY, "ready only");
  REG_ERROR(ERROR_AVOCADO_DUPLICATE_IDENTIFIER, "duplicate identifier");
  REG_ERROR(ERROR_AVOCADO_CORRUPTED_DATAFILE, "corrupted datafile");
  REG_ERROR(ERROR_AVOCADO_ILLEGAL_PARAMETER_FILE, "illegal parameter file");
  REG_ERROR(ERROR_AVOCADO_CORRUPTED_COLLECTION, "corrupted collection");
  REG_ERROR(ERROR_AVOCADO_MMAP_FAILED, "mmap failed");
  REG_ERROR(ERROR_AVOCADO_FILESYSTEM_FULL, "filesystem full");
  REG_ERROR(ERROR_AVOCADO_NO_JOURNAL, "no journal");
  REG_ERROR(ERROR_AVOCADO_DATAFILE_ALREADY_EXISTS, "cannot create/rename datafile because it already exists");
  REG_ERROR(ERROR_AVOCADO_DATABASE_LOCKED, "database is locked");
  REG_ERROR(ERROR_AVOCADO_COLLECTION_DIRECTORY_ALREADY_EXISTS, "cannot create/rename collection because directory already exists");
  REG_ERROR(ERROR_AVOCADO_CONFLICT, "conflict");
  REG_ERROR(ERROR_AVOCADO_WRONG_VOCBASE_PATH, "wrong path for database");
  REG_ERROR(ERROR_AVOCADO_DOCUMENT_NOT_FOUND, "document not found");
  REG_ERROR(ERROR_AVOCADO_COLLECTION_NOT_FOUND, "collection not found");
  REG_ERROR(ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING, "parameter 'collection' not found");
  REG_ERROR(ERROR_AVOCADO_DOCUMENT_HANDLE_BAD, "illegal document handle");
  REG_ERROR(ERROR_AVOCADO_MAXIMAL_SIZE_TOO_SMALL, "maixaml size of journal too small");
  REG_ERROR(ERROR_AVOCADO_DUPLICATE_NAME, "duplicate name");
  REG_ERROR(ERROR_AVOCADO_ILLEGAL_NAME, "illegal name");
  REG_ERROR(ERROR_AVOCADO_NO_INDEX, "no suitable index known");
  REG_ERROR(ERROR_AVOCADO_UNIQUE_CONSTRAINT_VIOLATED, "unique constraint violated");
  REG_ERROR(ERROR_AVOCADO_GEO_INDEX_VIOLATED, "geo index violated");
  REG_ERROR(ERROR_AVOCADO_INDEX_NOT_FOUND, "index not found");
  REG_ERROR(ERROR_AVOCADO_CROSS_COLLECTION_REQUEST, "cross collection request not allowed");
  REG_ERROR(ERROR_AVOCADO_INDEX_HANDLE_BAD, "illegal index handle");
  REG_ERROR(ERROR_AVOCADO_DATAFILE_FULL, "datafile full");
  REG_ERROR(ERROR_QUERY_KILLED, "query killed");
  REG_ERROR(ERROR_QUERY_PARSE, "parse error: %s");
  REG_ERROR(ERROR_QUERY_EMPTY, "query is empty");
  REG_ERROR(ERROR_QUERY_SPECIFICATION_INVALID, "query specification invalid");
  REG_ERROR(ERROR_QUERY_NUMBER_OUT_OF_RANGE, "number '%s' is out of range");
  REG_ERROR(ERROR_QUERY_TOO_MANY_JOINS, "too many joins.");
  REG_ERROR(ERROR_QUERY_COLLECTION_NAME_INVALID, "collection name '%s' is invalid");
  REG_ERROR(ERROR_QUERY_COLLECTION_ALIAS_INVALID, "collection alias '%s' is invalid");
  REG_ERROR(ERROR_QUERY_COLLECTION_ALIAS_REDECLARED, "collection alias '%s' is declared multiple times in the same query");
  REG_ERROR(ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED, "collection alias '%s' is used but was not declared in the from clause");
  REG_ERROR(ERROR_QUERY_COLLECTION_NOT_FOUND, "unable to open collection '%s'");
  REG_ERROR(ERROR_QUERY_GEO_RESTRICTION_INVALID, "geo restriction for alias '%s' is invalid");
  REG_ERROR(ERROR_QUERY_GEO_INDEX_MISSING, "no suitable geo index found for geo restriction on '%s'");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_MISSING, "no value specified for declared bind parameter '%s'");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_REDECLARED, "value for bind parameter '%s' is declared multiple times");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_UNDECLARED, "bind parameter '%s' was not declared in the query");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID, "invalid value for bind parameter '%s'");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE, "bind parameter number '%s' out of range");
  REG_ERROR(ERROR_QUERY_FUNCTION_NAME_UNKNOWN, "usage of unknown function '%s'");
  REG_ERROR(ERROR_QUERY_RUNTIME_ERROR, "runtime error in query");
  REG_ERROR(ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, "limit value '%s' is out of range");
  REG_ERROR(ERROR_QUERY_VARIABLE_REDECLARED, "variable '%s' is assigned multiple times");
  REG_ERROR(ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED, "document attribute '%s' is assigned multiple times");
  REG_ERROR(ERROR_QUERY_VARIABLE_NAME_INVALID, "variable name '%s' has an invalid format");
  REG_ERROR(ERROR_QUERY_BIND_PARAMETERS_INVALID, "invalid structure of bind parameters");
  REG_ERROR(ERROR_CURSOR_NOT_FOUND, "cursor not found");
  REG_ERROR(ERROR_SESSION_USERHANDLER_URL_INVALID, "expecting <prefix>/user/<username>");
  REG_ERROR(ERROR_SESSION_USERHANDLER_CANNOT_CREATE_USER, "cannot create user");
  REG_ERROR(ERROR_SESSION_USERHANDLER_ROLE_NOT_FOUND, "role not found");
  REG_ERROR(ERROR_SESSION_USERHANDLER_NO_CREATE_PERMISSION, "no permission to create user with that role");
  REG_ERROR(ERROR_SESSION_USERHANDLER_USER_NOT_FOUND, "user not found");
  REG_ERROR(ERROR_SESSION_USERHANDLER_CANNOT_CHANGE_PW, "cannot manage password for user");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_URL_INVALID1, "expecting POST <prefix>/session");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_URL_INVALID2, "expecting GET <prefix>/session/<sid>");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_URL_INVALID3, "expecting PUT <prefix>/session/<sid>/<method>");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_URL_INVALID4, "expecting DELETE <prefix>/session/<sid>");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_SESSION_UNKNOWN, "unknown session");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_SESSION_NOT_BOUND, "session has not bound to user");
  REG_ERROR(ERROR_SESSION_SESSIONHANDLER_CANNOT_LOGIN, "cannot login with session");
  REG_ERROR(ERROR_SESSION_USERSHANDLER_INVALID_URL, "expecting GET <prefix>/users");
  REG_ERROR(ERROR_SESSION_DIRECTORYSERVER_INVALID_URL, "expecting /directory/sessionvoc/<token>");
  REG_ERROR(ERROR_SESSION_DIRECTORYSERVER_NOT_CONFIGURED, "directory server is not configured");
  REG_ERROR(ERROR_KEYVALUE_INVALID_KEY, "invalid key declaration");
  REG_ERROR(ERROR_KEYVALUE_KEY_EXISTS, "key already exists");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_FOUND, "key not found");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_UNIQUE, "key is not unique");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_CHANGED, "key value not changed");
  REG_ERROR(ERROR_KEYVALUE_KEY_NOT_REMOVED, "key value not removed");
  REG_ERROR(ERROR_GRAPH_INVALID_GRAPH, "invalid graph");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CREATE_GRAPH, "could not create graph");
  REG_ERROR(ERROR_GRAPH_INVALID_VERTEX, "invalid vertex");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CREATE_VERTEX, "could not create vertex");
  REG_ERROR(ERROR_GRAPH_INVALID_EDGE, "invalid edge");
  REG_ERROR(ERROR_GRAPH_COULD_NOT_CREATE_EDGE, "could not create edge");
  REG_ERROR(SIMPLE_CLIENT_UNKNOWN_ERROR, "unknown client error");
  REG_ERROR(SIMPLE_CLIENT_COULD_NOT_CONNECT, "could not connect to server");
  REG_ERROR(SIMPLE_CLIENT_COULD_NOT_WRITE, "could not write to server");
  REG_ERROR(SIMPLE_CLIENT_COULD_NOT_READ, "could not read from server");
  REG_ERROR(ERROR_AVOCADO_INDEX_PQ_INSERT_FAILED, "priority queue insert failure");
  REG_ERROR(ERROR_AVOCADO_INDEX_PQ_REMOVE_FAILED, "priority queue remove failure");
  REG_ERROR(ERROR_AVOCADO_INDEX_PQ_REMOVE_ITEM_MISSING, "priority queue remove failure - item missing in index");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

