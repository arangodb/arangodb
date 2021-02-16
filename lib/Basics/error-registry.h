
#ifndef ARANGODB_BASICS_ERROR_REGISTRY_H
#define ARANGODB_BASICS_ERROR_REGISTRY_H

#include <frozen/unordered_map.h>

#include "Basics/voc-errors.h"

namespace arangodb::error {
constexpr static frozen::unordered_map<int, const char*, 367> ErrorMessages = {
    {int(TRI_ERROR_NO_ERROR),  // 0
      "no error"},
    {int(TRI_ERROR_FAILED),  // 1
      "failed"},
    {int(TRI_ERROR_SYS_ERROR),  // 2
      "system error"},
    {int(TRI_ERROR_OUT_OF_MEMORY),  // 3
      "out of memory"},
    {int(TRI_ERROR_INTERNAL),  // 4
      "internal error"},
    {int(TRI_ERROR_ILLEGAL_NUMBER),  // 5
      "illegal number"},
    {int(TRI_ERROR_NUMERIC_OVERFLOW),  // 6
      "numeric overflow"},
    {int(TRI_ERROR_ILLEGAL_OPTION),  // 7
      "illegal option"},
    {int(TRI_ERROR_DEAD_PID),  // 8
      "dead process identifier"},
    {int(TRI_ERROR_NOT_IMPLEMENTED),  // 9
      "not implemented"},
    {int(TRI_ERROR_BAD_PARAMETER),  // 10
      "bad parameter"},
    {int(TRI_ERROR_FORBIDDEN),  // 11
      "forbidden"},
    {int(TRI_ERROR_OUT_OF_MEMORY_MMAP),  // 12
      "out of memory in mmap"},
    {int(TRI_ERROR_CORRUPTED_CSV),  // 13
      "csv is corrupt"},
    {int(TRI_ERROR_FILE_NOT_FOUND),  // 14
      "file not found"},
    {int(TRI_ERROR_CANNOT_WRITE_FILE),  // 15
      "cannot write file"},
    {int(TRI_ERROR_CANNOT_OVERWRITE_FILE),  // 16
      "cannot overwrite file"},
    {int(TRI_ERROR_TYPE_ERROR),  // 17
      "type error"},
    {int(TRI_ERROR_LOCK_TIMEOUT),  // 18
      "lock timeout"},
    {int(TRI_ERROR_CANNOT_CREATE_DIRECTORY),  // 19
      "cannot create directory"},
    {int(TRI_ERROR_CANNOT_CREATE_TEMP_FILE),  // 20
      "cannot create temporary file"},
    {int(TRI_ERROR_REQUEST_CANCELED),  // 21
      "canceled request"},
    {int(TRI_ERROR_DEBUG),  // 22
      "intentional debug error"},
    {int(TRI_ERROR_IP_ADDRESS_INVALID),  // 25
      "IP address is invalid"},
    {int(TRI_ERROR_FILE_EXISTS),  // 27
      "file exists"},
    {int(TRI_ERROR_LOCKED),  // 28
      "locked"},
    {int(TRI_ERROR_DEADLOCK),  // 29
      "deadlock detected"},
    {int(TRI_ERROR_SHUTTING_DOWN),  // 30
      "shutdown in progress"},
    {int(TRI_ERROR_ONLY_ENTERPRISE),  // 31
      "only enterprise version"},
    {int(TRI_ERROR_RESOURCE_LIMIT),  // 32
      "resource limit exceeded"},
    {int(TRI_ERROR_ARANGO_ICU_ERROR),  // 33
      "icu error: %s"},
    {int(TRI_ERROR_CANNOT_READ_FILE),  // 34
      "cannot read file"},
    {int(TRI_ERROR_INCOMPATIBLE_VERSION),  // 35
      "incompatible server version"},
    {int(TRI_ERROR_DISABLED),  // 36
      "disabled"},
    {int(TRI_ERROR_MALFORMED_JSON),  // 37
      "malformed json"},
    {int(TRI_ERROR_PROMISE_ABANDONED),  // 38
      "promise abandoned"},
    {int(TRI_ERROR_HTTP_BAD_PARAMETER),  // 400
      "bad parameter"},
    {int(TRI_ERROR_HTTP_UNAUTHORIZED),  // 401
      "unauthorized"},
    {int(TRI_ERROR_HTTP_FORBIDDEN),  // 403
      "forbidden"},
    {int(TRI_ERROR_HTTP_NOT_FOUND),  // 404
      "not found"},
    {int(TRI_ERROR_HTTP_METHOD_NOT_ALLOWED),  // 405
      "method not supported"},
    {int(TRI_ERROR_HTTP_NOT_ACCEPTABLE),  // 406
      "request not acceptable"},
    {int(TRI_ERROR_HTTP_REQUEST_TIMEOUT),  // 408
      "request timeout"},
    {int(TRI_ERROR_HTTP_CONFLICT),  // 409
      "conflict"},
    {int(TRI_ERROR_HTTP_GONE),  // 410
      "content permanently deleted"},
    {int(TRI_ERROR_HTTP_PRECONDITION_FAILED),  // 412
      "precondition failed"},
    {int(TRI_ERROR_HTTP_SERVER_ERROR),  // 500
      "internal server error"},
    {int(TRI_ERROR_HTTP_NOT_IMPLEMENTED),  // 501
      "not implemented"},
    {int(TRI_ERROR_HTTP_SERVICE_UNAVAILABLE),  // 503
      "service unavailable"},
    {int(TRI_ERROR_HTTP_GATEWAY_TIMEOUT),  // 504
      "gateway timeout"},
    {int(TRI_ERROR_HTTP_CORRUPTED_JSON),  // 600
      "invalid JSON object"},
    {int(TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES),  // 601
      "superfluous URL suffices"},
    {int(TRI_ERROR_ARANGO_ILLEGAL_STATE),  // 1000
      "illegal state"},
    {int(TRI_ERROR_ARANGO_DATAFILE_SEALED),  // 1002
      "datafile sealed"},
    {int(TRI_ERROR_ARANGO_READ_ONLY),  // 1004
      "read only"},
    {int(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER),  // 1005
      "duplicate identifier"},
    {int(TRI_ERROR_ARANGO_DATAFILE_UNREADABLE),  // 1006
      "datafile unreadable"},
    {int(TRI_ERROR_ARANGO_DATAFILE_EMPTY),  // 1007
      "datafile empty"},
    {int(TRI_ERROR_ARANGO_RECOVERY),  // 1008
      "logfile recovery error"},
    {int(TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND),  // 1009
      "datafile statistics not found"},
    {int(TRI_ERROR_ARANGO_CORRUPTED_DATAFILE),  // 1100
      "corrupted datafile"},
    {int(TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE),  // 1101
      "illegal or unreadable parameter file"},
    {int(TRI_ERROR_ARANGO_CORRUPTED_COLLECTION),  // 1102
      "corrupted collection"},
    {int(TRI_ERROR_ARANGO_MMAP_FAILED),  // 1103
      "mmap failed"},
    {int(TRI_ERROR_ARANGO_FILESYSTEM_FULL),  // 1104
      "filesystem full"},
    {int(TRI_ERROR_ARANGO_NO_JOURNAL),  // 1105
      "no journal"},
    {int(TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS),  // 1106
      "cannot create/rename datafile because it already exists"},
    {int(TRI_ERROR_ARANGO_DATADIR_LOCKED),  // 1107
      "database directory is locked"},
    {int(TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS),  // 1108
      "cannot create/rename collection because directory already exists"},
    {int(TRI_ERROR_ARANGO_MSYNC_FAILED),  // 1109
      "msync failed"},
    {int(TRI_ERROR_ARANGO_DATADIR_UNLOCKABLE),  // 1110
      "cannot lock database directory"},
    {int(TRI_ERROR_ARANGO_SYNC_TIMEOUT),  // 1111
      "sync timeout"},
    {int(TRI_ERROR_ARANGO_CONFLICT),  // 1200
      "conflict"},
    {int(TRI_ERROR_ARANGO_DATADIR_INVALID),  // 1201
      "invalid database directory"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND),  // 1202
      "document not found"},
    {int(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND),  // 1203
      "collection or view not found"},
    {int(TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING),  // 1204
      "parameter 'collection' not found"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD),  // 1205
      "illegal document identifier"},
    {int(TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL),  // 1206
      "maximal size of journal too small"},
    {int(TRI_ERROR_ARANGO_DUPLICATE_NAME),  // 1207
      "duplicate name"},
    {int(TRI_ERROR_ARANGO_ILLEGAL_NAME),  // 1208
      "illegal name"},
    {int(TRI_ERROR_ARANGO_NO_INDEX),  // 1209
      "no suitable index known"},
    {int(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED),  // 1210
      "unique constraint violated"},
    {int(TRI_ERROR_ARANGO_INDEX_NOT_FOUND),  // 1212
      "index not found"},
    {int(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST),  // 1213
      "cross collection request not allowed"},
    {int(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD),  // 1214
      "illegal index identifier"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE),  // 1216
      "document too large"},
    {int(TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED),  // 1217
      "collection must be unloaded"},
    {int(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID),  // 1218
      "collection type invalid"},
    {int(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED),  // 1220
      "parsing attribute name definition failed"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD),  // 1221
      "illegal document key"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED),  // 1222
      "unexpected document key"},
    {int(TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE),  // 1224
      "server database directory not writable"},
    {int(TRI_ERROR_ARANGO_OUT_OF_KEYS),  // 1225
      "out of keys"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING),  // 1226
      "missing document key"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID),  // 1227
      "invalid document type"},
    {int(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND),  // 1228
      "database not found"},
    {int(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID),  // 1229
      "database name invalid"},
    {int(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE),  // 1230
      "operation only allowed in system database"},
    {int(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR),  // 1232
      "invalid key generator"},
    {int(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE),  // 1233
      "edge attribute missing or invalid"},
    {int(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED),  // 1235
      "index creation failed"},
    {int(TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT),  // 1236
      "write-throttling timeout"},
    {int(TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH),  // 1237
      "collection type mismatch"},
    {int(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED),  // 1238
      "collection not loaded"},
    {int(TRI_ERROR_ARANGO_DOCUMENT_REV_BAD),  // 1239
      "illegal document revision"},
    {int(TRI_ERROR_ARANGO_INCOMPLETE_READ),  // 1240
      "incomplete read"},
    {int(TRI_ERROR_ARANGO_DATAFILE_FULL),  // 1300
      "datafile full"},
    {int(TRI_ERROR_ARANGO_EMPTY_DATADIR),  // 1301
      "server database directory is empty"},
    {int(TRI_ERROR_ARANGO_TRY_AGAIN),  // 1302
      "operation should be tried again"},
    {int(TRI_ERROR_ARANGO_BUSY),  // 1303
      "engine is busy"},
    {int(TRI_ERROR_ARANGO_MERGE_IN_PROGRESS),  // 1304
      "merge in progress"},
    {int(TRI_ERROR_ARANGO_IO_ERROR),  // 1305
      "storage engine I/O error"},
    {int(TRI_ERROR_REPLICATION_NO_RESPONSE),  // 1400
      "no response"},
    {int(TRI_ERROR_REPLICATION_INVALID_RESPONSE),  // 1401
      "invalid response"},
    {int(TRI_ERROR_REPLICATION_LEADER_ERROR),  // 1402
      "leader error"},
    {int(TRI_ERROR_REPLICATION_LEADER_INCOMPATIBLE),  // 1403
      "leader incompatible"},
    {int(TRI_ERROR_REPLICATION_LEADER_CHANGE),  // 1404
      "leader change"},
    {int(TRI_ERROR_REPLICATION_LOOP),  // 1405
      "loop detected"},
    {int(TRI_ERROR_REPLICATION_UNEXPECTED_MARKER),  // 1406
      "unexpected marker"},
    {int(TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE),  // 1407
      "invalid applier state"},
    {int(TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION),  // 1408
      "invalid transaction"},
    {int(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION),  // 1410
      "invalid replication applier configuration"},
    {int(TRI_ERROR_REPLICATION_RUNNING),  // 1411
      "cannot perform operation while applier is running"},
    {int(TRI_ERROR_REPLICATION_APPLIER_STOPPED),  // 1412
      "replication stopped"},
    {int(TRI_ERROR_REPLICATION_NO_START_TICK),  // 1413
      "no start tick"},
    {int(TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT),  // 1414
      "start tick not present"},
    {int(TRI_ERROR_REPLICATION_WRONG_CHECKSUM),  // 1416
      "wrong checksum"},
    {int(TRI_ERROR_REPLICATION_SHARD_NONEMPTY),  // 1417
      "shard not empty"},
    {int(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED),  // 1447
      "follower transaction intermediate commit already performed"},
    {int(TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED),  // 1448
      "creating collection failed due to precondition"},
    {int(TRI_ERROR_CLUSTER_SERVER_UNKNOWN),  // 1449
      "got a request from an unkown server"},
    {int(TRI_ERROR_CLUSTER_TOO_MANY_SHARDS),  // 1450
      "too many shards"},
    {int(TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS),  // 1453
      "collection ID already exists"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN),  // 1454
      "could not create collection in plan"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION),  // 1456
      "could not create collection"},
    {int(TRI_ERROR_CLUSTER_TIMEOUT),  // 1457
      "timeout in cluster operation"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN),  // 1458
      "could not remove collection from plan"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT),  // 1459
      "could not remove collection from current"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN),  // 1460
      "could not create database in plan"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE),  // 1461
      "could not create database"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN),  // 1462
      "could not remove database from plan"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT),  // 1463
      "could not remove database from current"},
    {int(TRI_ERROR_CLUSTER_SHARD_GONE),  // 1464
      "no responsible shard found"},
    {int(TRI_ERROR_CLUSTER_CONNECTION_LOST),  // 1465
      "cluster internal HTTP connection broken"},
    {int(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY),  // 1466
      "must not specify _key for this collection"},
    {int(TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS),  // 1467
      "got contradicting answers from different shards"},
    {int(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN),  // 1468
      "not all sharding attributes given"},
    {int(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES),  // 1469
      "must not change the value of a shard key attribute"},
    {int(TRI_ERROR_CLUSTER_UNSUPPORTED),  // 1470
      "unsupported operation or parameter for clusters"},
    {int(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR),  // 1471
      "this operation is only valid on a coordinator in a cluster"},
    {int(TRI_ERROR_CLUSTER_READING_PLAN_AGENCY),  // 1472
      "error reading Plan in agency"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION),  // 1473
      "could not truncate collection"},
    {int(TRI_ERROR_CLUSTER_AQL_COMMUNICATION),  // 1474
      "error in cluster internal communication for AQL"},
    {int(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER),  // 1477
      "this operation is only valid on a DBserver in a cluster"},
    {int(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE),  // 1478
      "A cluster backend which was required for the operation could not be reached"},
    {int(TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC),  // 1481
      "collection is out of sync"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN),  // 1482
      "could not create index in plan"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN),  // 1483
      "could not drop index in plan"},
    {int(TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE),  // 1484
      "chain of distributeShardsLike references"},
    {int(TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE),  // 1485
      "must not drop collection while another has a distributeShardsLike attribute pointing to it"},
    {int(TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE),  // 1486
      "must not have a distributeShardsLike attribute pointing to an unknown collection"},
    {int(TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS),  // 1487
      "the number of current dbservers is lower than the requested replicationFactor"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER),  // 1488
      "a follower could not be dropped in agency"},
    {int(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION),  // 1489
      "a shard leader refuses to perform a replication operation"},
    {int(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION),  // 1490
      "a shard follower refuses to perform an operation that is not a replication"},
    {int(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED),  // 1491
      "a (former) shard leader refuses to perform an operation, because it has resigned in the meantime"},
    {int(TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED),  // 1492
      "some agency operation failed"},
    {int(TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING),  // 1495
      "leadership challenge is ongoing"},
    {int(TRI_ERROR_CLUSTER_NOT_LEADER),  // 1496
      "not a leader"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN),  // 1497
      "could not create view in plan"},
    {int(TRI_ERROR_CLUSTER_VIEW_ID_EXISTS),  // 1498
      "view ID already exists"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION),  // 1499
      "could not drop collection in plan"},
    {int(TRI_ERROR_QUERY_KILLED),  // 1500
      "query killed"},
    {int(TRI_ERROR_QUERY_PARSE),  // 1501
      "%s"},
    {int(TRI_ERROR_QUERY_EMPTY),  // 1502
      "query is empty"},
    {int(TRI_ERROR_QUERY_SCRIPT),  // 1503
      "runtime error '%s'"},
    {int(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE),  // 1504
      "number out of range"},
    {int(TRI_ERROR_QUERY_INVALID_GEO_VALUE),  // 1505
      "invalid geo coordinate value"},
    {int(TRI_ERROR_QUERY_VARIABLE_NAME_INVALID),  // 1510
      "variable name '%s' has an invalid format"},
    {int(TRI_ERROR_QUERY_VARIABLE_REDECLARED),  // 1511
      "variable '%s' is assigned multiple times"},
    {int(TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN),  // 1512
      "unknown variable '%s'"},
    {int(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED),  // 1521
      "unable to read-lock collection %s"},
    {int(TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS),  // 1522
      "too many collections/shards"},
    {int(TRI_ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED),  // 1530
      "document attribute '%s' is assigned multiple times"},
    {int(TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN),  // 1540
      "usage of unknown function '%s()'"},
    {int(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH),  // 1541
      "invalid number of arguments for function '%s()', expected number of arguments: minimum: %d, maximum: %d"},
    {int(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH),  // 1542
      "invalid argument type in call to function '%s()'"},
    {int(TRI_ERROR_QUERY_INVALID_REGEX),  // 1543
      "invalid regex value"},
    {int(TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID),  // 1550
      "invalid structure of bind parameters"},
    {int(TRI_ERROR_QUERY_BIND_PARAMETER_MISSING),  // 1551
      "no value specified for declared bind parameter '%s'"},
    {int(TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED),  // 1552
      "bind parameter '%s' was not declared in the query"},
    {int(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE),  // 1553
      "bind parameter '%s' has an invalid value or type"},
    {int(TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE),  // 1560
      "invalid logical value"},
    {int(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE),  // 1561
      "invalid arithmetic value"},
    {int(TRI_ERROR_QUERY_DIVISION_BY_ZERO),  // 1562
      "division by zero"},
    {int(TRI_ERROR_QUERY_ARRAY_EXPECTED),  // 1563
      "array expected"},
    {int(TRI_ERROR_QUERY_FAIL_CALLED),  // 1569
      "FAIL(%s) called"},
    {int(TRI_ERROR_QUERY_GEO_INDEX_MISSING),  // 1570
      "no suitable geo index found for geo restriction on '%s'"},
    {int(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING),  // 1571
      "no suitable fulltext index found for fulltext query on '%s'"},
    {int(TRI_ERROR_QUERY_INVALID_DATE_VALUE),  // 1572
      "invalid date value"},
    {int(TRI_ERROR_QUERY_MULTI_MODIFY),  // 1573
      "multi-modify query"},
    {int(TRI_ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION),  // 1574
      "invalid aggregate expression"},
    {int(TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS),  // 1575
      "query options must be readable at query compile time"},
    {int(TRI_ERROR_QUERY_EXCEPTION_OPTIONS),  // 1576
      "query options expected"},
    {int(TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE),  // 1577
      "could not use forced index hint"},
    {int(TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL),  // 1578
      "disallowed dynamic call to '%s'"},
    {int(TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION),  // 1579
      "access after data-modification by %s"},
    {int(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME),  // 1580
      "invalid user function name"},
    {int(TRI_ERROR_QUERY_FUNCTION_INVALID_CODE),  // 1581
      "invalid user function code"},
    {int(TRI_ERROR_QUERY_FUNCTION_NOT_FOUND),  // 1582
      "user function '%s()' not found"},
    {int(TRI_ERROR_QUERY_FUNCTION_RUNTIME_ERROR),  // 1583
      "user function runtime error: %s"},
    {int(TRI_ERROR_QUERY_BAD_JSON_PLAN),  // 1590
      "bad execution plan JSON"},
    {int(TRI_ERROR_QUERY_NOT_FOUND),  // 1591
      "query ID not found"},
    {int(TRI_ERROR_QUERY_USER_ASSERT),  // 1593
      "%s"},
    {int(TRI_ERROR_QUERY_USER_WARN),  // 1594
      "%s"},
    {int(TRI_ERROR_QUERY_WINDOW_AFTER_MODIFICATION),  // 1595
      "window operation after data-modification"},
    {int(TRI_ERROR_CURSOR_NOT_FOUND),  // 1600
      "cursor not found"},
    {int(TRI_ERROR_CURSOR_BUSY),  // 1601
      "cursor is busy"},
    {int(TRI_ERROR_VALIDATION_FAILED),  // 1620
      "schema validation failed"},
    {int(TRI_ERROR_VALIDATION_BAD_PARAMETER),  // 1621
      "invalid schema validation parameter"},
    {int(TRI_ERROR_TRANSACTION_INTERNAL),  // 1650
      "internal transaction error"},
    {int(TRI_ERROR_TRANSACTION_NESTED),  // 1651
      "nested transactions detected"},
    {int(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION),  // 1652
      "unregistered collection used in transaction"},
    {int(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION),  // 1653
      "disallowed operation inside transaction"},
    {int(TRI_ERROR_TRANSACTION_ABORTED),  // 1654
      "transaction aborted"},
    {int(TRI_ERROR_TRANSACTION_NOT_FOUND),  // 1655
      "transaction not found"},
    {int(TRI_ERROR_USER_INVALID_NAME),  // 1700
      "invalid user name"},
    {int(TRI_ERROR_USER_DUPLICATE),  // 1702
      "duplicate user"},
    {int(TRI_ERROR_USER_NOT_FOUND),  // 1703
      "user not found"},
    {int(TRI_ERROR_USER_EXTERNAL),  // 1705
      "user is external"},
    {int(TRI_ERROR_SERVICE_DOWNLOAD_FAILED),  // 1752
      "service download failed"},
    {int(TRI_ERROR_SERVICE_UPLOAD_FAILED),  // 1753
      "service upload failed"},
    {int(TRI_ERROR_LDAP_CANNOT_INIT),  // 1800
      "cannot init a LDAP connection"},
    {int(TRI_ERROR_LDAP_CANNOT_SET_OPTION),  // 1801
      "cannot set a LDAP option"},
    {int(TRI_ERROR_LDAP_CANNOT_BIND),  // 1802
      "cannot bind to a LDAP server"},
    {int(TRI_ERROR_LDAP_CANNOT_UNBIND),  // 1803
      "cannot unbind from a LDAP server"},
    {int(TRI_ERROR_LDAP_CANNOT_SEARCH),  // 1804
      "cannot issue a LDAP search"},
    {int(TRI_ERROR_LDAP_CANNOT_START_TLS),  // 1805
      "cannot start a TLS LDAP session"},
    {int(TRI_ERROR_LDAP_FOUND_NO_OBJECTS),  // 1806
      "LDAP didn't found any objects"},
    {int(TRI_ERROR_LDAP_NOT_ONE_USER_FOUND),  // 1807
      "LDAP found zero ore more than one user"},
    {int(TRI_ERROR_LDAP_USER_NOT_IDENTIFIED),  // 1808
      "LDAP found a user, but its not the desired one"},
    {int(TRI_ERROR_LDAP_OPERATIONS_ERROR),  // 1809
      "LDAP returned an operations error"},
    {int(TRI_ERROR_LDAP_INVALID_MODE),  // 1820
      "invalid ldap mode"},
    {int(TRI_ERROR_TASK_INVALID_ID),  // 1850
      "invalid task id"},
    {int(TRI_ERROR_TASK_DUPLICATE_ID),  // 1851
      "duplicate task id"},
    {int(TRI_ERROR_TASK_NOT_FOUND),  // 1852
      "task not found"},
    {int(TRI_ERROR_GRAPH_INVALID_GRAPH),  // 1901
      "invalid graph"},
    {int(TRI_ERROR_GRAPH_COULD_NOT_CREATE_GRAPH),  // 1902
      "could not create graph"},
    {int(TRI_ERROR_GRAPH_INVALID_VERTEX),  // 1903
      "invalid vertex"},
    {int(TRI_ERROR_GRAPH_COULD_NOT_CREATE_VERTEX),  // 1904
      "could not create vertex"},
    {int(TRI_ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX),  // 1905
      "could not change vertex"},
    {int(TRI_ERROR_GRAPH_INVALID_EDGE),  // 1906
      "invalid edge"},
    {int(TRI_ERROR_GRAPH_COULD_NOT_CREATE_EDGE),  // 1907
      "could not create edge"},
    {int(TRI_ERROR_GRAPH_COULD_NOT_CHANGE_EDGE),  // 1908
      "could not change edge"},
    {int(TRI_ERROR_GRAPH_TOO_MANY_ITERATIONS),  // 1909
      "too many iterations - try increasing the value of 'maxIterations'"},
    {int(TRI_ERROR_GRAPH_INVALID_FILTER_RESULT),  // 1910
      "invalid filter result"},
    {int(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE),  // 1920
      "multi use of edge collection in edge def"},
    {int(TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS),  // 1921
      "edge collection already used in edge def"},
    {int(TRI_ERROR_GRAPH_CREATE_MISSING_NAME),  // 1922
      "missing graph name"},
    {int(TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION),  // 1923
      "malformed edge definition"},
    {int(TRI_ERROR_GRAPH_NOT_FOUND),  // 1924
      "graph '%s' not found"},
    {int(TRI_ERROR_GRAPH_DUPLICATE),  // 1925
      "graph already exists"},
    {int(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST),  // 1926
      "vertex collection does not exist or is not part of the graph"},
    {int(TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX),  // 1927
      "collection not a vertex collection"},
    {int(TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION),  // 1928
      "collection is not in list of orphan collections"},
    {int(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF),  // 1929
      "collection already used in edge def"},
    {int(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED),  // 1930
      "edge collection not used in graph"},
    {int(TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION),  // 1932
      "collection _graphs does not exist"},
    {int(TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING),  // 1933
      "Invalid example type. Has to be String, Array or Object"},
    {int(TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT),  // 1934
      "Invalid example type. Has to be Array or Object"},
    {int(TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS),  // 1935
      "Invalid number of arguments. Expected: "},
    {int(TRI_ERROR_GRAPH_INVALID_PARAMETER),  // 1936
      "Invalid parameter type."},
    {int(TRI_ERROR_GRAPH_INVALID_ID),  // 1937
      "Invalid id"},
    {int(TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS),  // 1938
      "collection used in orphans"},
    {int(TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST),  // 1939
      "edge collection does not exist or is not part of the graph"},
    {int(TRI_ERROR_GRAPH_EMPTY),  // 1940
      "empty graph"},
    {int(TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT),  // 1941
      "internal graph data corrupt"},
    {int(TRI_ERROR_GRAPH_INTERNAL_EDGE_COLLECTION_ALREADY_SET),  // 1942
      "edge collection already set"},
    {int(TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST),  // 1943
      "malformed orphan list"},
    {int(TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT),  // 1944
      "edge definition collection is a document collection"},
    {int(TRI_ERROR_GRAPH_COLLECTION_IS_INITIAL),  // 1945
      "initial collection is not allowed to be removed manually"},
    {int(TRI_ERROR_GRAPH_NO_INITIAL_COLLECTION),  // 1946
      "no valid initial collection found"},
    {int(TRI_ERROR_SESSION_UNKNOWN),  // 1950
      "unknown session"},
    {int(TRI_ERROR_SESSION_EXPIRED),  // 1951
      "session expired"},
    {int(TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR),  // 2000
      "unknown client error"},
    {int(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT),  // 2001
      "could not connect to server"},
    {int(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE),  // 2002
      "could not write to server"},
    {int(TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ),  // 2003
      "could not read from server"},
    {int(TRI_ERROR_WAS_ERLAUBE),  // 2019
      "was erlaube?!"},
    {int(TRI_ERROR_INTERNAL_AQL),  // 2200
      "General internal AQL error"},
    {int(TRI_ERROR_WROTE_TOO_FEW_OUTPUT_REGISTERS),  // 2201
      "An AQL block wrote too few output registers"},
    {int(TRI_ERROR_WROTE_TOO_MANY_OUTPUT_REGISTERS),  // 2202
      "An AQL block wrote too many output registers"},
    {int(TRI_ERROR_WROTE_OUTPUT_REGISTER_TWICE),  // 2203
      "An AQL block wrote an output register twice"},
    {int(TRI_ERROR_WROTE_IN_WRONG_REGISTER),  // 2204
      "An AQL block wrote in a register that is not its output"},
    {int(TRI_ERROR_INPUT_REGISTERS_NOT_COPIED),  // 2205
      "An AQL block did not copy its input registers"},
    {int(TRI_ERROR_MALFORMED_MANIFEST_FILE),  // 3000
      "failed to parse manifest file"},
    {int(TRI_ERROR_INVALID_SERVICE_MANIFEST),  // 3001
      "manifest file is invalid"},
    {int(TRI_ERROR_SERVICE_FILES_MISSING),  // 3002
      "service files missing"},
    {int(TRI_ERROR_SERVICE_FILES_OUTDATED),  // 3003
      "service files outdated"},
    {int(TRI_ERROR_INVALID_FOXX_OPTIONS),  // 3004
      "service options are invalid"},
    {int(TRI_ERROR_INVALID_MOUNTPOINT),  // 3007
      "invalid mountpath"},
    {int(TRI_ERROR_SERVICE_NOT_FOUND),  // 3009
      "service not found"},
    {int(TRI_ERROR_SERVICE_NEEDS_CONFIGURATION),  // 3010
      "service needs configuration"},
    {int(TRI_ERROR_SERVICE_MOUNTPOINT_CONFLICT),  // 3011
      "service already exists"},
    {int(TRI_ERROR_SERVICE_MANIFEST_NOT_FOUND),  // 3012
      "missing manifest file"},
    {int(TRI_ERROR_SERVICE_OPTIONS_MALFORMED),  // 3013
      "failed to parse service options"},
    {int(TRI_ERROR_SERVICE_SOURCE_NOT_FOUND),  // 3014
      "source path not found"},
    {int(TRI_ERROR_SERVICE_SOURCE_ERROR),  // 3015
      "error resolving source"},
    {int(TRI_ERROR_SERVICE_UNKNOWN_SCRIPT),  // 3016
      "unknown script"},
    {int(TRI_ERROR_SERVICE_API_DISABLED),  // 3099
      "service api disabled"},
    {int(TRI_ERROR_MODULE_NOT_FOUND),  // 3100
      "cannot locate module"},
    {int(TRI_ERROR_MODULE_SYNTAX_ERROR),  // 3101
      "syntax error in module"},
    {int(TRI_ERROR_MODULE_FAILURE),  // 3103
      "failed to invoke module"},
    {int(TRI_ERROR_NO_SMART_COLLECTION),  // 4000
      "collection is not smart"},
    {int(TRI_ERROR_NO_SMART_GRAPH_ATTRIBUTE),  // 4001
      "smart graph attribute not given"},
    {int(TRI_ERROR_CANNOT_DROP_SMART_COLLECTION),  // 4002
      "cannot drop this smart collection"},
    {int(TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE),  // 4003
      "in smart vertex collections _key must be a string and prefixed with the value of the smart graph attribute"},
    {int(TRI_ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE),  // 4004
      "attribute cannot be used as smart graph attribute"},
    {int(TRI_ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH),  // 4005
      "smart graph attribute mismatch"},
    {int(TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE),  // 4006
      "invalid smart join attribute declaration"},
    {int(TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_JOIN_ATTRIBUTE),  // 4007
      "shard key value must be prefixed with the value of the smart join attribute"},
    {int(TRI_ERROR_NO_SMART_JOIN_ATTRIBUTE),  // 4008
      "smart join attribute not given or invalid"},
    {int(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE),  // 4009
      "must not change the value of the smartJoinAttribute"},
    {int(TRI_ERROR_INVALID_DISJOINT_SMART_EDGE),  // 4010
      "non disjoint edge found"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_FAILED),  // 5000
      "error during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY),  // 5001
      "not enough (healthy) db servers"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED),  // 5002
      "replication factor violated during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS),  // 5003
      "no dbservers during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS),  // 5004
      "mismatching leaders during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS),  // 5005
      "mismatching followers during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES),  // 5006
      "inconsistent attributes during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS),  // 5007
      "mismatching shards during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_JOB_FAILED),  // 5008
      "move shard job failed during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_JOB_DISAPPEARED),  // 5009
      "move shard job disappeared during cluster repairs"},
    {int(TRI_ERROR_CLUSTER_REPAIRS_OPERATION_FAILED),  // 5010
      "agency transaction failed during cluster repairs"},
    {int(TRI_ERROR_AGENCY_MALFORMED_GOSSIP_MESSAGE),  // 20001
      "malformed gossip message"},
    {int(TRI_ERROR_AGENCY_MALFORMED_INQUIRE_REQUEST),  // 20002
      "malformed inquire request"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT),  // 20011
      "Inform message must be an object."},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM),  // 20012
      "Inform message must contain uint parameter 'term'"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ID),  // 20013
      "Inform message must contain string parameter 'id'"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE),  // 20014
      "Inform message must contain array 'active'"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL),  // 20015
      "Inform message must contain object 'pool'"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING),  // 20016
      "Inform message must contain object 'min ping'"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING),  // 20017
      "Inform message must contain object 'max ping'"},
    {int(TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT),  // 20018
      "Inform message must contain object 'timeoutMult'"},
    {int(TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS),  // 20021
      "Cannot rebuild readDB and spearHead"},
    {int(TRI_ERROR_AGENCY_MALFORMED_TRANSACTION),  // 20030
      "malformed agency transaction"},
    {int(TRI_ERROR_SUPERVISION_GENERAL_FAILURE),  // 20501
      "general supervision failure"},
    {int(TRI_ERROR_QUEUE_FULL),  // 21003
      "named queue is full"},
    {int(TRI_ERROR_ACTION_OPERATION_UNABORTABLE),  // 6002
      "this maintenance action cannot be stopped"},
    {int(TRI_ERROR_ACTION_UNFINISHED),  // 6003
      "maintenance action still processing"},
    {int(TRI_ERROR_NO_SUCH_ACTION),  // 6004
      "no such maintenance action"},
    {int(TRI_ERROR_HOT_BACKUP_INTERNAL),  // 7001
      "internal hot backup error"},
    {int(TRI_ERROR_HOT_RESTORE_INTERNAL),  // 7002
      "internal hot restore error"},
    {int(TRI_ERROR_BACKUP_TOPOLOGY),  // 7003
      "backup does not match this topology"},
    {int(TRI_ERROR_NO_SPACE_LEFT_ON_DEVICE),  // 7004
      "no space left on device"},
    {int(TRI_ERROR_FAILED_TO_UPLOAD_BACKUP),  // 7005
      "failed to upload hot backup set to remote target"},
    {int(TRI_ERROR_FAILED_TO_DOWNLOAD_BACKUP),  // 7006
      "failed to download hot backup set from remote source"},
    {int(TRI_ERROR_NO_SUCH_HOT_BACKUP),  // 7007
      "no such hot backup set can be found"},
    {int(TRI_ERROR_REMOTE_REPOSITORY_CONFIG_BAD),  // 7008
      "remote hotback repository configuration error"},
    {int(TRI_ERROR_LOCAL_LOCK_FAILED),  // 7009
      "some db servers cannot be reached for transaction locks"},
    {int(TRI_ERROR_LOCAL_LOCK_RETRY),  // 7010
      "some db servers cannot be reached for transaction locks"},
    {int(TRI_ERROR_HOT_BACKUP_CONFLICT),  // 7011
      "hot backup conflict"},
    {int(TRI_ERROR_HOT_BACKUP_DBSERVERS_AWOL),  // 7012
      "hot backup not all db servers reachable"},
    {int(TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN),  // 7021
      "analyzers in plan could not be modified"},
    {int(TRI_ERROR_AIR_EXECUTION_ERROR),  // 8001
      "error during AIR execution"},
};
}

#endif  // ARANGODB_BASICS_ERROR_REGISTRY_H
