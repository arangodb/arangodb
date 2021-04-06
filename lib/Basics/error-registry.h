
#ifndef ARANGODB_BASICS_ERROR_REGISTRY_H
#define ARANGODB_BASICS_ERROR_REGISTRY_H

#include "Basics/voc-errors.h"

namespace frozen {
template <>
struct elsa<ErrorCode> {
  constexpr auto operator()(ErrorCode const& value, std::size_t seed) const -> std::size_t {
    return elsa<int>{}.operator()(static_cast<int>(value), seed);
  }
};
}  // namespace frozen

#include <frozen/unordered_map.h>

namespace arangodb::error {
constexpr static frozen::unordered_map<ErrorCode, const char*, 342> ErrorMessages = {
    {TRI_ERROR_NO_ERROR,  // 0
      "no error"},
    {TRI_ERROR_FAILED,  // 1
      "failed"},
    {TRI_ERROR_SYS_ERROR,  // 2
      "system error"},
    {TRI_ERROR_OUT_OF_MEMORY,  // 3
      "out of memory"},
    {TRI_ERROR_INTERNAL,  // 4
      "internal error"},
    {TRI_ERROR_ILLEGAL_NUMBER,  // 5
      "illegal number"},
    {TRI_ERROR_NUMERIC_OVERFLOW,  // 6
      "numeric overflow"},
    {TRI_ERROR_ILLEGAL_OPTION,  // 7
      "illegal option"},
    {TRI_ERROR_DEAD_PID,  // 8
      "dead process identifier"},
    {TRI_ERROR_NOT_IMPLEMENTED,  // 9
      "not implemented"},
    {TRI_ERROR_BAD_PARAMETER,  // 10
      "bad parameter"},
    {TRI_ERROR_FORBIDDEN,  // 11
      "forbidden"},
    {TRI_ERROR_OUT_OF_MEMORY_MMAP,  // 12
      "out of memory in mmap"},
    {TRI_ERROR_CORRUPTED_CSV,  // 13
      "csv is corrupt"},
    {TRI_ERROR_FILE_NOT_FOUND,  // 14
      "file not found"},
    {TRI_ERROR_CANNOT_WRITE_FILE,  // 15
      "cannot write file"},
    {TRI_ERROR_CANNOT_OVERWRITE_FILE,  // 16
      "cannot overwrite file"},
    {TRI_ERROR_TYPE_ERROR,  // 17
      "type error"},
    {TRI_ERROR_LOCK_TIMEOUT,  // 18
      "lock timeout"},
    {TRI_ERROR_CANNOT_CREATE_DIRECTORY,  // 19
      "cannot create directory"},
    {TRI_ERROR_CANNOT_CREATE_TEMP_FILE,  // 20
      "cannot create temporary file"},
    {TRI_ERROR_REQUEST_CANCELED,  // 21
      "canceled request"},
    {TRI_ERROR_DEBUG,  // 22
      "intentional debug error"},
    {TRI_ERROR_IP_ADDRESS_INVALID,  // 25
      "IP address is invalid"},
    {TRI_ERROR_FILE_EXISTS,  // 27
      "file exists"},
    {TRI_ERROR_LOCKED,  // 28
      "locked"},
    {TRI_ERROR_DEADLOCK,  // 29
      "deadlock detected"},
    {TRI_ERROR_SHUTTING_DOWN,  // 30
      "shutdown in progress"},
    {TRI_ERROR_ONLY_ENTERPRISE,  // 31
      "only enterprise version"},
    {TRI_ERROR_RESOURCE_LIMIT,  // 32
      "resource limit exceeded"},
    {TRI_ERROR_ARANGO_ICU_ERROR,  // 33
      "icu error: %s"},
    {TRI_ERROR_CANNOT_READ_FILE,  // 34
      "cannot read file"},
    {TRI_ERROR_INCOMPATIBLE_VERSION,  // 35
      "incompatible server version"},
    {TRI_ERROR_DISABLED,  // 36
      "disabled"},
    {TRI_ERROR_MALFORMED_JSON,  // 37
      "malformed json"},
    {TRI_ERROR_HTTP_BAD_PARAMETER,  // 400
      "bad parameter"},
    {TRI_ERROR_HTTP_UNAUTHORIZED,  // 401
      "unauthorized"},
    {TRI_ERROR_HTTP_FORBIDDEN,  // 403
      "forbidden"},
    {TRI_ERROR_HTTP_NOT_FOUND,  // 404
      "not found"},
    {TRI_ERROR_HTTP_METHOD_NOT_ALLOWED,  // 405
      "method not supported"},
    {TRI_ERROR_HTTP_NOT_ACCEPTABLE,  // 406
      "request not acceptable"},
    {TRI_ERROR_HTTP_REQUEST_TIMEOUT,  // 408
      "request timeout"},
    {TRI_ERROR_HTTP_CONFLICT,  // 409
      "conflict"},
    {TRI_ERROR_HTTP_GONE,  // 410
      "content permanently deleted"},
    {TRI_ERROR_HTTP_PRECONDITION_FAILED,  // 412
      "precondition failed"},
    {TRI_ERROR_HTTP_SERVER_ERROR,  // 500
      "internal server error"},
    {TRI_ERROR_HTTP_NOT_IMPLEMENTED,  // 501
      "not implemented"},
    {TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,  // 503
      "service unavailable"},
    {TRI_ERROR_HTTP_GATEWAY_TIMEOUT,  // 504
      "gateway timeout"},
    {TRI_ERROR_HTTP_CORRUPTED_JSON,  // 600
      "invalid JSON object"},
    {TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES,  // 601
      "superfluous URL suffices"},
    {TRI_ERROR_ARANGO_ILLEGAL_STATE,  // 1000
      "illegal state"},
    {TRI_ERROR_ARANGO_DATAFILE_SEALED,  // 1002
      "datafile sealed"},
    {TRI_ERROR_ARANGO_READ_ONLY,  // 1004
      "read only"},
    {TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,  // 1005
      "duplicate identifier"},
    {TRI_ERROR_ARANGO_DATAFILE_UNREADABLE,  // 1006
      "datafile unreadable"},
    {TRI_ERROR_ARANGO_DATAFILE_EMPTY,  // 1007
      "datafile empty"},
    {TRI_ERROR_ARANGO_RECOVERY,  // 1008
      "logfile recovery error"},
    {TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND,  // 1009
      "datafile statistics not found"},
    {TRI_ERROR_ARANGO_CORRUPTED_DATAFILE,  // 1100
      "corrupted datafile"},
    {TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE,  // 1101
      "illegal or unreadable parameter file"},
    {TRI_ERROR_ARANGO_CORRUPTED_COLLECTION,  // 1102
      "corrupted collection"},
    {TRI_ERROR_ARANGO_MMAP_FAILED,  // 1103
      "mmap failed"},
    {TRI_ERROR_ARANGO_FILESYSTEM_FULL,  // 1104
      "filesystem full"},
    {TRI_ERROR_ARANGO_NO_JOURNAL,  // 1105
      "no journal"},
    {TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS,  // 1106
      "cannot create/rename datafile because it already exists"},
    {TRI_ERROR_ARANGO_DATADIR_LOCKED,  // 1107
      "database directory is locked"},
    {TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS,  // 1108
      "cannot create/rename collection because directory already exists"},
    {TRI_ERROR_ARANGO_MSYNC_FAILED,  // 1109
      "msync failed"},
    {TRI_ERROR_ARANGO_DATADIR_UNLOCKABLE,  // 1110
      "cannot lock database directory"},
    {TRI_ERROR_ARANGO_SYNC_TIMEOUT,  // 1111
      "sync timeout"},
    {TRI_ERROR_ARANGO_CONFLICT,  // 1200
      "conflict"},
    {TRI_ERROR_ARANGO_DATADIR_INVALID,  // 1201
      "invalid database directory"},
    {TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,  // 1202
      "document not found"},
    {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,  // 1203
      "collection or view not found"},
    {TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING,  // 1204
      "parameter 'collection' not found"},
    {TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD,  // 1205
      "illegal document identifier"},
    {TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL,  // 1206
      "maximal size of journal too small"},
    {TRI_ERROR_ARANGO_DUPLICATE_NAME,  // 1207
      "duplicate name"},
    {TRI_ERROR_ARANGO_ILLEGAL_NAME,  // 1208
      "illegal name"},
    {TRI_ERROR_ARANGO_NO_INDEX,  // 1209
      "no suitable index known"},
    {TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED,  // 1210
      "unique constraint violated"},
    {TRI_ERROR_ARANGO_INDEX_NOT_FOUND,  // 1212
      "index not found"},
    {TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST,  // 1213
      "cross collection request not allowed"},
    {TRI_ERROR_ARANGO_INDEX_HANDLE_BAD,  // 1214
      "illegal index identifier"},
    {TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE,  // 1216
      "document too large"},
    {TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED,  // 1217
      "collection must be unloaded"},
    {TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,  // 1218
      "collection type invalid"},
    {TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED,  // 1220
      "parsing attribute name definition failed"},
    {TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD,  // 1221
      "illegal document key"},
    {TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED,  // 1222
      "unexpected document key"},
    {TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE,  // 1224
      "server database directory not writable"},
    {TRI_ERROR_ARANGO_OUT_OF_KEYS,  // 1225
      "out of keys"},
    {TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING,  // 1226
      "missing document key"},
    {TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID,  // 1227
      "invalid document type"},
    {TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,  // 1228
      "database not found"},
    {TRI_ERROR_ARANGO_DATABASE_NAME_INVALID,  // 1229
      "database name invalid"},
    {TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE,  // 1230
      "operation only allowed in system database"},
    {TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR,  // 1232
      "invalid key generator"},
    {TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE,  // 1233
      "edge attribute missing or invalid"},
    {TRI_ERROR_ARANGO_INDEX_CREATION_FAILED,  // 1235
      "index creation failed"},
    {TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT,  // 1236
      "write-throttling timeout"},
    {TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH,  // 1237
      "collection type mismatch"},
    {TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED,  // 1238
      "collection not loaded"},
    {TRI_ERROR_ARANGO_DOCUMENT_REV_BAD,  // 1239
      "illegal document revision"},
    {TRI_ERROR_ARANGO_INCOMPLETE_READ,  // 1240
      "incomplete read"},
    {TRI_ERROR_ARANGO_DATAFILE_FULL,  // 1300
      "datafile full"},
    {TRI_ERROR_ARANGO_EMPTY_DATADIR,  // 1301
      "server database directory is empty"},
    {TRI_ERROR_ARANGO_TRY_AGAIN,  // 1302
      "operation should be tried again"},
    {TRI_ERROR_ARANGO_BUSY,  // 1303
      "engine is busy"},
    {TRI_ERROR_ARANGO_MERGE_IN_PROGRESS,  // 1304
      "merge in progress"},
    {TRI_ERROR_ARANGO_IO_ERROR,  // 1305
      "storage engine I/O error"},
    {TRI_ERROR_REPLICATION_NO_RESPONSE,  // 1400
      "no response"},
    {TRI_ERROR_REPLICATION_INVALID_RESPONSE,  // 1401
      "invalid response"},
    {TRI_ERROR_REPLICATION_LEADER_ERROR,  // 1402
      "leader error"},
    {TRI_ERROR_REPLICATION_LEADER_INCOMPATIBLE,  // 1403
      "leader incompatible"},
    {TRI_ERROR_REPLICATION_LEADER_CHANGE,  // 1404
      "leader change"},
    {TRI_ERROR_REPLICATION_LOOP,  // 1405
      "loop detected"},
    {TRI_ERROR_REPLICATION_UNEXPECTED_MARKER,  // 1406
      "unexpected marker"},
    {TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE,  // 1407
      "invalid applier state"},
    {TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION,  // 1408
      "invalid transaction"},
    {TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION,  // 1410
      "invalid replication applier configuration"},
    {TRI_ERROR_REPLICATION_RUNNING,  // 1411
      "cannot perform operation while applier is running"},
    {TRI_ERROR_REPLICATION_APPLIER_STOPPED,  // 1412
      "replication stopped"},
    {TRI_ERROR_REPLICATION_NO_START_TICK,  // 1413
      "no start tick"},
    {TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT,  // 1414
      "start tick not present"},
    {TRI_ERROR_REPLICATION_WRONG_CHECKSUM,  // 1416
      "wrong checksum"},
    {TRI_ERROR_REPLICATION_SHARD_NONEMPTY,  // 1417
      "shard not empty"},
    {TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED,  // 1447
      "follower transaction intermediate commit already performed"},
    {TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED,  // 1448
      "creating collection failed due to precondition"},
    {TRI_ERROR_CLUSTER_SERVER_UNKNOWN,  // 1449
      "got a request from an unknown server"},
    {TRI_ERROR_CLUSTER_TOO_MANY_SHARDS,  // 1450
      "too many shards"},
    {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN,  // 1454
      "could not create collection in plan"},
    {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION,  // 1456
      "could not create collection"},
    {TRI_ERROR_CLUSTER_TIMEOUT,  // 1457
      "timeout in cluster operation"},
    {TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN,  // 1458
      "could not remove collection from plan"},
    {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN,  // 1460
      "could not create database in plan"},
    {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE,  // 1461
      "could not create database"},
    {TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN,  // 1462
      "could not remove database from plan"},
    {TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT,  // 1463
      "could not remove database from current"},
    {TRI_ERROR_CLUSTER_SHARD_GONE,  // 1464
      "no responsible shard found"},
    {TRI_ERROR_CLUSTER_CONNECTION_LOST,  // 1465
      "cluster internal HTTP connection broken"},
    {TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY,  // 1466
      "must not specify _key for this collection"},
    {TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS,  // 1467
      "got contradicting answers from different shards"},
    {TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN,  // 1468
      "not all sharding attributes given"},
    {TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES,  // 1469
      "must not change the value of a shard key attribute"},
    {TRI_ERROR_CLUSTER_UNSUPPORTED,  // 1470
      "unsupported operation or parameter for clusters"},
    {TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR,  // 1471
      "this operation is only valid on a coordinator in a cluster"},
    {TRI_ERROR_CLUSTER_READING_PLAN_AGENCY,  // 1472
      "error reading Plan in agency"},
    {TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION,  // 1473
      "could not truncate collection"},
    {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,  // 1474
      "error in cluster internal communication for AQL"},
    {TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER,  // 1477
      "this operation is only valid on a DBserver in a cluster"},
    {TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,  // 1478
      "A cluster backend which was required for the operation could not be reached"},
    {TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC,  // 1481
      "collection is out of sync"},
    {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN,  // 1482
      "could not create index in plan"},
    {TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN,  // 1483
      "could not drop index in plan"},
    {TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE,  // 1484
      "chain of distributeShardsLike references"},
    {TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE,  // 1485
      "must not drop collection while another has a distributeShardsLike attribute pointing to it"},
    {TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE,  // 1486
      "must not have a distributeShardsLike attribute pointing to an unknown collection"},
    {TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS,  // 1487
      "the number of current dbservers is lower than the requested replicationFactor"},
    {TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER,  // 1488
      "a follower could not be dropped in agency"},
    {TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION,  // 1489
      "a shard leader refuses to perform a replication operation"},
    {TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION,  // 1490
      "a shard follower refuses to perform an operation that is not a replication"},
    {TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED,  // 1491
      "a (former) shard leader refuses to perform an operation, because it has resigned in the meantime"},
    {TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED,  // 1492
      "some agency operation failed"},
    {TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING,  // 1495
      "leadership challenge is ongoing"},
    {TRI_ERROR_CLUSTER_NOT_LEADER,  // 1496
      "not a leader"},
    {TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN,  // 1497
      "could not create view in plan"},
    {TRI_ERROR_CLUSTER_VIEW_ID_EXISTS,  // 1498
      "view ID already exists"},
    {TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION,  // 1499
      "could not drop collection in plan"},
    {TRI_ERROR_QUERY_KILLED,  // 1500
      "query killed"},
    {TRI_ERROR_QUERY_PARSE,  // 1501
      "%s"},
    {TRI_ERROR_QUERY_EMPTY,  // 1502
      "query is empty"},
    {TRI_ERROR_QUERY_SCRIPT,  // 1503
      "runtime error '%s'"},
    {TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE,  // 1504
      "number out of range"},
    {TRI_ERROR_QUERY_INVALID_GEO_VALUE,  // 1505
      "invalid geo coordinate value"},
    {TRI_ERROR_QUERY_VARIABLE_NAME_INVALID,  // 1510
      "variable name '%s' has an invalid format"},
    {TRI_ERROR_QUERY_VARIABLE_REDECLARED,  // 1511
      "variable '%s' is assigned multiple times"},
    {TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN,  // 1512
      "unknown variable '%s'"},
    {TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,  // 1521
      "unable to read-lock collection %s"},
    {TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS,  // 1522
      "too many collections/shards"},
    {TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN,  // 1540
      "usage of unknown function '%s()'"},
    {TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH,  // 1541
      "invalid number of arguments for function '%s()', expected number of arguments: minimum: %d, maximum: %d"},
    {TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,  // 1542
      "invalid argument type in call to function '%s()'"},
    {TRI_ERROR_QUERY_INVALID_REGEX,  // 1543
      "invalid regex value"},
    {TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID,  // 1550
      "invalid structure of bind parameters"},
    {TRI_ERROR_QUERY_BIND_PARAMETER_MISSING,  // 1551
      "no value specified for declared bind parameter '%s'"},
    {TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED,  // 1552
      "bind parameter '%s' was not declared in the query"},
    {TRI_ERROR_QUERY_BIND_PARAMETER_TYPE,  // 1553
      "bind parameter '%s' has an invalid value or type"},
    {TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE,  // 1561
      "invalid arithmetic value"},
    {TRI_ERROR_QUERY_DIVISION_BY_ZERO,  // 1562
      "division by zero"},
    {TRI_ERROR_QUERY_ARRAY_EXPECTED,  // 1563
      "array expected"},
    {TRI_ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION,  // 1568
      "collection '%s' used as expression operand"},
    {TRI_ERROR_QUERY_FAIL_CALLED,  // 1569
      "FAIL(%s) called"},
    {TRI_ERROR_QUERY_GEO_INDEX_MISSING,  // 1570
      "no suitable geo index found for geo restriction on '%s'"},
    {TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING,  // 1571
      "no suitable fulltext index found for fulltext query on '%s'"},
    {TRI_ERROR_QUERY_INVALID_DATE_VALUE,  // 1572
      "invalid date value"},
    {TRI_ERROR_QUERY_MULTI_MODIFY,  // 1573
      "multi-modify query"},
    {TRI_ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION,  // 1574
      "invalid aggregate expression"},
    {TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS,  // 1575
      "query options must be readable at query compile time"},
    {TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE,  // 1577
      "could not use forced index hint"},
    {TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL,  // 1578
      "disallowed dynamic call to '%s'"},
    {TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION,  // 1579
      "access after data-modification by %s"},
    {TRI_ERROR_QUERY_FUNCTION_INVALID_NAME,  // 1580
      "invalid user function name"},
    {TRI_ERROR_QUERY_FUNCTION_INVALID_CODE,  // 1581
      "invalid user function code"},
    {TRI_ERROR_QUERY_FUNCTION_NOT_FOUND,  // 1582
      "user function '%s()' not found"},
    {TRI_ERROR_QUERY_FUNCTION_RUNTIME_ERROR,  // 1583
      "user function runtime error: %s"},
    {TRI_ERROR_QUERY_BAD_JSON_PLAN,  // 1590
      "bad execution plan JSON"},
    {TRI_ERROR_QUERY_NOT_FOUND,  // 1591
      "query ID not found"},
    {TRI_ERROR_QUERY_USER_ASSERT,  // 1593
      "%s"},
    {TRI_ERROR_QUERY_USER_WARN,  // 1594
      "%s"},
    {TRI_ERROR_QUERY_WINDOW_AFTER_MODIFICATION,  // 1595
      "window operation after data-modification"},
    {TRI_ERROR_CURSOR_NOT_FOUND,  // 1600
      "cursor not found"},
    {TRI_ERROR_CURSOR_BUSY,  // 1601
      "cursor is busy"},
    {TRI_ERROR_VALIDATION_FAILED,  // 1620
      "schema validation failed"},
    {TRI_ERROR_VALIDATION_BAD_PARAMETER,  // 1621
      "invalid schema validation parameter"},
    {TRI_ERROR_TRANSACTION_INTERNAL,  // 1650
      "internal transaction error"},
    {TRI_ERROR_TRANSACTION_NESTED,  // 1651
      "nested transactions detected"},
    {TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION,  // 1652
      "unregistered collection used in transaction"},
    {TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,  // 1653
      "disallowed operation inside transaction"},
    {TRI_ERROR_TRANSACTION_ABORTED,  // 1654
      "transaction aborted"},
    {TRI_ERROR_TRANSACTION_NOT_FOUND,  // 1655
      "transaction not found"},
    {TRI_ERROR_USER_INVALID_NAME,  // 1700
      "invalid user name"},
    {TRI_ERROR_USER_DUPLICATE,  // 1702
      "duplicate user"},
    {TRI_ERROR_USER_NOT_FOUND,  // 1703
      "user not found"},
    {TRI_ERROR_USER_EXTERNAL,  // 1705
      "user is external"},
    {TRI_ERROR_SERVICE_DOWNLOAD_FAILED,  // 1752
      "service download failed"},
    {TRI_ERROR_SERVICE_UPLOAD_FAILED,  // 1753
      "service upload failed"},
    {TRI_ERROR_LDAP_CANNOT_INIT,  // 1800
      "cannot init a LDAP connection"},
    {TRI_ERROR_LDAP_CANNOT_SET_OPTION,  // 1801
      "cannot set a LDAP option"},
    {TRI_ERROR_LDAP_CANNOT_BIND,  // 1802
      "cannot bind to a LDAP server"},
    {TRI_ERROR_LDAP_CANNOT_UNBIND,  // 1803
      "cannot unbind from a LDAP server"},
    {TRI_ERROR_LDAP_CANNOT_SEARCH,  // 1804
      "cannot issue a LDAP search"},
    {TRI_ERROR_LDAP_CANNOT_START_TLS,  // 1805
      "cannot start a TLS LDAP session"},
    {TRI_ERROR_LDAP_FOUND_NO_OBJECTS,  // 1806
      "LDAP didn't found any objects"},
    {TRI_ERROR_LDAP_NOT_ONE_USER_FOUND,  // 1807
      "LDAP found zero ore more than one user"},
    {TRI_ERROR_LDAP_USER_NOT_IDENTIFIED,  // 1808
      "LDAP found a user, but its not the desired one"},
    {TRI_ERROR_LDAP_OPERATIONS_ERROR,  // 1809
      "LDAP returned an operations error"},
    {TRI_ERROR_LDAP_INVALID_MODE,  // 1820
      "invalid ldap mode"},
    {TRI_ERROR_TASK_INVALID_ID,  // 1850
      "invalid task id"},
    {TRI_ERROR_TASK_DUPLICATE_ID,  // 1851
      "duplicate task id"},
    {TRI_ERROR_TASK_NOT_FOUND,  // 1852
      "task not found"},
    {TRI_ERROR_GRAPH_INVALID_GRAPH,  // 1901
      "invalid graph"},
    {TRI_ERROR_GRAPH_INVALID_EDGE,  // 1906
      "invalid edge"},
    {TRI_ERROR_GRAPH_TOO_MANY_ITERATIONS,  // 1909
      "too many iterations - try increasing the value of 'maxIterations'"},
    {TRI_ERROR_GRAPH_INVALID_FILTER_RESULT,  // 1910
      "invalid filter result"},
    {TRI_ERROR_GRAPH_COLLECTION_MULTI_USE,  // 1920
      "multi use of edge collection in edge def"},
    {TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS,  // 1921
      "edge collection already used in edge def"},
    {TRI_ERROR_GRAPH_CREATE_MISSING_NAME,  // 1922
      "missing graph name"},
    {TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION,  // 1923
      "malformed edge definition"},
    {TRI_ERROR_GRAPH_NOT_FOUND,  // 1924
      "graph '%s' not found"},
    {TRI_ERROR_GRAPH_DUPLICATE,  // 1925
      "graph already exists"},
    {TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST,  // 1926
      "vertex collection does not exist or is not part of the graph"},
    {TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX,  // 1927
      "collection not a vertex collection"},
    {TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION,  // 1928
      "collection is not in list of orphan collections"},
    {TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF,  // 1929
      "collection already used in edge def"},
    {TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED,  // 1930
      "edge collection not used in graph"},
    {TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION,  // 1932
      "collection _graphs does not exist"},
    {TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS,  // 1935
      "Invalid number of arguments. Expected: "},
    {TRI_ERROR_GRAPH_INVALID_PARAMETER,  // 1936
      "Invalid parameter type."},
    {TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS,  // 1938
      "collection used in orphans"},
    {TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST,  // 1939
      "edge collection does not exist or is not part of the graph"},
    {TRI_ERROR_GRAPH_EMPTY,  // 1940
      "empty graph"},
    {TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT,  // 1941
      "internal graph data corrupt"},
    {TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST,  // 1943
      "malformed orphan list"},
    {TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT,  // 1944
      "edge definition collection is a document collection"},
    {TRI_ERROR_GRAPH_COLLECTION_IS_INITIAL,  // 1945
      "initial collection is not allowed to be removed manually"},
    {TRI_ERROR_GRAPH_NO_INITIAL_COLLECTION,  // 1946
      "no valid initial collection found"},
    {TRI_ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED,  // 1947
      "referenced vertex collection is not part of the graph"},
    {TRI_ERROR_SESSION_UNKNOWN,  // 1950
      "unknown session"},
    {TRI_ERROR_SESSION_EXPIRED,  // 1951
      "session expired"},
    {TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR,  // 2000
      "unknown client error"},
    {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT,  // 2001
      "could not connect to server"},
    {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE,  // 2002
      "could not write to server"},
    {TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ,  // 2003
      "could not read from server"},
    {TRI_ERROR_WAS_ERLAUBE,  // 2019
      "was erlaube?!"},
    {TRI_ERROR_INTERNAL_AQL,  // 2200
      "General internal AQL error"},
    {TRI_ERROR_WROTE_TOO_FEW_OUTPUT_REGISTERS,  // 2201
      "An AQL block wrote too few output registers"},
    {TRI_ERROR_WROTE_TOO_MANY_OUTPUT_REGISTERS,  // 2202
      "An AQL block wrote too many output registers"},
    {TRI_ERROR_WROTE_OUTPUT_REGISTER_TWICE,  // 2203
      "An AQL block wrote an output register twice"},
    {TRI_ERROR_WROTE_IN_WRONG_REGISTER,  // 2204
      "An AQL block wrote in a register that is not its output"},
    {TRI_ERROR_INPUT_REGISTERS_NOT_COPIED,  // 2205
      "An AQL block did not copy its input registers"},
    {TRI_ERROR_MALFORMED_MANIFEST_FILE,  // 3000
      "failed to parse manifest file"},
    {TRI_ERROR_INVALID_SERVICE_MANIFEST,  // 3001
      "manifest file is invalid"},
    {TRI_ERROR_SERVICE_FILES_MISSING,  // 3002
      "service files missing"},
    {TRI_ERROR_SERVICE_FILES_OUTDATED,  // 3003
      "service files outdated"},
    {TRI_ERROR_INVALID_FOXX_OPTIONS,  // 3004
      "service options are invalid"},
    {TRI_ERROR_INVALID_MOUNTPOINT,  // 3007
      "invalid mountpath"},
    {TRI_ERROR_SERVICE_NOT_FOUND,  // 3009
      "service not found"},
    {TRI_ERROR_SERVICE_NEEDS_CONFIGURATION,  // 3010
      "service needs configuration"},
    {TRI_ERROR_SERVICE_MOUNTPOINT_CONFLICT,  // 3011
      "service already exists"},
    {TRI_ERROR_SERVICE_MANIFEST_NOT_FOUND,  // 3012
      "missing manifest file"},
    {TRI_ERROR_SERVICE_OPTIONS_MALFORMED,  // 3013
      "failed to parse service options"},
    {TRI_ERROR_SERVICE_SOURCE_NOT_FOUND,  // 3014
      "source path not found"},
    {TRI_ERROR_SERVICE_SOURCE_ERROR,  // 3015
      "error resolving source"},
    {TRI_ERROR_SERVICE_UNKNOWN_SCRIPT,  // 3016
      "unknown script"},
    {TRI_ERROR_SERVICE_API_DISABLED,  // 3099
      "service api disabled"},
    {TRI_ERROR_MODULE_NOT_FOUND,  // 3100
      "cannot locate module"},
    {TRI_ERROR_MODULE_SYNTAX_ERROR,  // 3101
      "syntax error in module"},
    {TRI_ERROR_MODULE_FAILURE,  // 3103
      "failed to invoke module"},
    {TRI_ERROR_NO_SMART_COLLECTION,  // 4000
      "collection is not smart"},
    {TRI_ERROR_NO_SMART_GRAPH_ATTRIBUTE,  // 4001
      "smart graph attribute not given"},
    {TRI_ERROR_CANNOT_DROP_SMART_COLLECTION,  // 4002
      "cannot drop this smart collection"},
    {TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE,  // 4003
      "in smart vertex collections _key must be a string and prefixed with the value of the smart graph attribute"},
    {TRI_ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE,  // 4004
      "attribute cannot be used as smart graph attribute"},
    {TRI_ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH,  // 4005
      "smart graph attribute mismatch"},
    {TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE,  // 4006
      "invalid smart join attribute declaration"},
    {TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_JOIN_ATTRIBUTE,  // 4007
      "shard key value must be prefixed with the value of the smart join attribute"},
    {TRI_ERROR_NO_SMART_JOIN_ATTRIBUTE,  // 4008
      "smart join attribute not given or invalid"},
    {TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE,  // 4009
      "must not change the value of the smartJoinAttribute"},
    {TRI_ERROR_INVALID_DISJOINT_SMART_EDGE,  // 4010
      "non disjoint edge found"},
    {TRI_ERROR_AGENCY_MALFORMED_GOSSIP_MESSAGE,  // 20001
      "malformed gossip message"},
    {TRI_ERROR_AGENCY_MALFORMED_INQUIRE_REQUEST,  // 20002
      "malformed inquire request"},
    {TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT,  // 20011
      "Inform message must be an object."},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM,  // 20012
      "Inform message must contain uint parameter 'term'"},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ID,  // 20013
      "Inform message must contain string parameter 'id'"},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE,  // 20014
      "Inform message must contain array 'active'"},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL,  // 20015
      "Inform message must contain object 'pool'"},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING,  // 20016
      "Inform message must contain object 'min ping'"},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING,  // 20017
      "Inform message must contain object 'max ping'"},
    {TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT,  // 20018
      "Inform message must contain object 'timeoutMult'"},
    {TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS,  // 20021
      "Cannot rebuild readDB and spearHead"},
    {TRI_ERROR_AGENCY_MALFORMED_TRANSACTION,  // 20030
      "malformed agency transaction"},
    {TRI_ERROR_SUPERVISION_GENERAL_FAILURE,  // 20501
      "general supervision failure"},
    {TRI_ERROR_QUEUE_FULL,  // 21003
      "named queue is full"},
    {TRI_ERROR_ACTION_OPERATION_UNABORTABLE,  // 6002
      "this maintenance action cannot be stopped"},
    {TRI_ERROR_ACTION_UNFINISHED,  // 6003
      "maintenance action still processing"},
    {TRI_ERROR_NO_SUCH_ACTION,  // 6004
      "no such maintenance action"},
    {TRI_ERROR_HOT_BACKUP_INTERNAL,  // 7001
      "internal hot backup error"},
    {TRI_ERROR_HOT_RESTORE_INTERNAL,  // 7002
      "internal hot restore error"},
    {TRI_ERROR_BACKUP_TOPOLOGY,  // 7003
      "backup does not match this topology"},
    {TRI_ERROR_NO_SPACE_LEFT_ON_DEVICE,  // 7004
      "no space left on device"},
    {TRI_ERROR_FAILED_TO_UPLOAD_BACKUP,  // 7005
      "failed to upload hot backup set to remote target"},
    {TRI_ERROR_FAILED_TO_DOWNLOAD_BACKUP,  // 7006
      "failed to download hot backup set from remote source"},
    {TRI_ERROR_NO_SUCH_HOT_BACKUP,  // 7007
      "no such hot backup set can be found"},
    {TRI_ERROR_REMOTE_REPOSITORY_CONFIG_BAD,  // 7008
      "remote hotback repository configuration error"},
    {TRI_ERROR_LOCAL_LOCK_FAILED,  // 7009
      "some db servers cannot be reached for transaction locks"},
    {TRI_ERROR_LOCAL_LOCK_RETRY,  // 7010
      "some db servers cannot be reached for transaction locks"},
    {TRI_ERROR_HOT_BACKUP_CONFLICT,  // 7011
      "hot backup conflict"},
    {TRI_ERROR_HOT_BACKUP_DBSERVERS_AWOL,  // 7012
      "hot backup not all db servers reachable"},
    {TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN,  // 7021
      "analyzers in plan could not be modified"},
    {TRI_ERROR_AIR_EXECUTION_ERROR,  // 8001
      "error during AIR execution"},
};
}

#endif  // ARANGODB_BASICS_ERROR_REGISTRY_H
