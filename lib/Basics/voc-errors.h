
#ifndef TRIAGENS_BASICS_VOC_ERRORS_H
#define TRIAGENS_BASICS_VOC_ERRORS_H 1

////////////////////////////////////////////////////////////////////////////////
/// Error codes and meanings
///
/// The following errors might be raised when running ArangoDB:
///
/// - 0: @LIT{no error}
///   No error has occurred.
/// - 1: @LIT{failed}
///   Will be raised when a general error occurred.
/// - 2: @LIT{system error}
///   Will be raised when operating system error occurred.
/// - 3: @LIT{out of memory}
///   Will be raised when there is a memory shortage.
/// - 4: @LIT{internal error}
///   Will be raised when an internal error occurred.
/// - 5: @LIT{illegal number}
///   Will be raised when an illegal representation of a number was given.
/// - 6: @LIT{numeric overflow}
///   Will be raised when a numeric overflow occurred.
/// - 7: @LIT{illegal option}
///   Will be raised when an unknown option was supplied by the user.
/// - 8: @LIT{dead process identifier}
///   Will be raised when a PID without a living process was found.
/// - 9: @LIT{not implemented}
///   Will be raised when hitting an unimplemented feature.
/// - 10: @LIT{bad parameter}
///   Will be raised when the parameter does not fulfill the requirements.
/// - 11: @LIT{forbidden}
///   Will be raised when you are missing permission for the operation.
/// - 12: @LIT{out of memory in mmap}
///   Will be raised when there is a memory shortage.
/// - 13: @LIT{csv is corrupt}
///   Will be raised when encountering a corrupt csv line.
/// - 14: @LIT{file not found}
///   Will be raised when a file is not found.
/// - 15: @LIT{cannot write file}
///   Will be raised when a file cannot be written.
/// - 16: @LIT{cannot overwrite file}
///   Will be raised when an attempt is made to overwrite an existing file.
/// - 17: @LIT{type error}
///   Will be raised when a type error is unencountered.
/// - 18: @LIT{lock timeout}
///   Will be raised when there's a timeout waiting for a lock.
/// - 19: @LIT{cannot create directory}
///   Will be raised when an attempt to create a directory fails.
/// - 20: @LIT{cannot create temporary file}
///   Will be raised when an attempt to create a temporary file fails.
/// - 21: @LIT{canceled request}
///   Will be raised when a request is canceled by the user.
/// - 22: @LIT{intentional debug error}
///   Will be raised intentionally during debugging.
/// - 25: @LIT{IP address is invalid}
///   Will be raised when the structure of an IP address is invalid.
/// - 27: @LIT{file exists}
///   Will be raised when a file already exists.
/// - 28: @LIT{locked}
///   Will be raised when a resource or an operation is locked.
/// - 29: @LIT{deadlock detected}
///   Will be raised when a deadlock is detected when accessing collections.
/// - 30: @LIT{shutdown in progress}
///   Will be raised when a call cannot succeed because a server shutdown is
///   already in progress.
/// - 31: @LIT{only enterprise version}
///   Will be raised when an enterprise-feature is requested from the community
///   edition.
/// - 32: @LIT{resource limit exceeded}
///   Will be raised when the resources used by an operation exceed the
///   configured maximum value.
/// - 400: @LIT{bad parameter}
///   Will be raised when the HTTP request does not fulfill the requirements.
/// - 401: @LIT{unauthorized}
///   Will be raised when authorization is required but the user is not
///   authorized.
/// - 403: @LIT{forbidden}
///   Will be raised when the operation is forbidden.
/// - 404: @LIT{not found}
///   Will be raised when an URI is unknown.
/// - 405: @LIT{method not supported}
///   Will be raised when an unsupported HTTP method is used for an operation.
/// - 406: @LIT{request not acceptable}
///   Will be raised when an unsupported HTTP content type is used for an
///   operation, or if a request is not acceptable for a leader or follower.
/// - 412: @LIT{precondition failed}
///   Will be raised when a precondition for an HTTP request is not met.
/// - 500: @LIT{internal server error}
///   Will be raised when an internal server is encountered.
/// - 503: @LIT{service unavailable}
///   Will be raised when a service is temporarily unavailable.
/// - 600: @LIT{invalid JSON object}
///   Will be raised when a string representation of a JSON object is corrupt.
/// - 601: @LIT{superfluous URL suffices}
///   Will be raised when the URL contains superfluous suffices.
/// - 1000: @LIT{illegal state}
///   Internal error that will be raised when the datafile is not in the
///   required state.
/// - 1002: @LIT{datafile sealed}
///   Internal error that will be raised when trying to write to a datafile.
/// - 1004: @LIT{read only}
///   Internal error that will be raised when trying to write to a read-only
///   datafile or collection.
/// - 1005: @LIT{duplicate identifier}
///   Internal error that will be raised when a identifier duplicate is
///   detected.
/// - 1006: @LIT{datafile unreadable}
///   Internal error that will be raised when a datafile is unreadable.
/// - 1007: @LIT{datafile empty}
///   Internal error that will be raised when a datafile is empty.
/// - 1008: @LIT{logfile recovery error}
///   Will be raised when an error occurred during WAL log file recovery.
/// - 1009: @LIT{datafile statistics not found}
///   Will be raised when a required datafile statistics object was not found.
/// - 1100: @LIT{corrupted datafile}
///   Will be raised when a corruption is detected in a datafile.
/// - 1101: @LIT{illegal or unreadable parameter file}
///   Will be raised if a parameter file is corrupted or cannot be read.
/// - 1102: @LIT{corrupted collection}
///   Will be raised when a collection contains one or more corrupted data
///   files.
/// - 1103: @LIT{mmap failed}
///   Will be raised when the system call mmap failed.
/// - 1104: @LIT{filesystem full}
///   Will be raised when the filesystem is full.
/// - 1105: @LIT{no journal}
///   Will be raised when a journal cannot be created.
/// - 1106: @LIT{cannot create/rename datafile because it already exists}
///   Will be raised when the datafile cannot be created or renamed because a
///   file of the same name already exists.
/// - 1107: @LIT{database directory is locked}
///   Will be raised when the database directory is locked by a different
///   process.
/// - 1108: @LIT{cannot create/rename collection because directory already exists}
///   Will be raised when the collection cannot be created because a directory
///   of the same name already exists.
/// - 1109: @LIT{msync failed}
///   Will be raised when the system call msync failed.
/// - 1110: @LIT{cannot lock database directory}
///   Will be raised when the server cannot lock the database directory on
///   startup.
/// - 1111: @LIT{sync timeout}
///   Will be raised when the server waited too long for a datafile to be
///   synced to disk.
/// - 1200: @LIT{conflict}
///   Will be raised when updating or deleting a document and a conflict has
///   been detected.
/// - 1201: @LIT{invalid database directory}
///   Will be raised when a non-existing database directory was specified when
///   starting the database.
/// - 1202: @LIT{document not found}
///   Will be raised when a document with a given identifier or handle is
///   unknown.
/// - 1203: @LIT{collection not found}
///   Will be raised when a collection with the given identifier or name is
///   unknown.
/// - 1204: @LIT{parameter 'collection' not found}
///   Will be raised when the collection parameter is missing.
/// - 1205: @LIT{illegal document handle}
///   Will be raised when a document handle is corrupt.
/// - 1206: @LIT{maximal size of journal too small}
///   Will be raised when the maximal size of the journal is too small.
/// - 1207: @LIT{duplicate name}
///   Will be raised when a name duplicate is detected.
/// - 1208: @LIT{illegal name}
///   Will be raised when an illegal name is detected.
/// - 1209: @LIT{no suitable index known}
///   Will be raised when no suitable index for the query is known.
/// - 1210: @LIT{unique constraint violated}
///   Will be raised when there is a unique constraint violation.
/// - 1211: @LIT{view not found}
///   Will be raised when a view with the given identifier or name is unknown.
/// - 1212: @LIT{index not found}
///   Will be raised when an index with a given identifier is unknown.
/// - 1213: @LIT{cross collection request not allowed}
///   Will be raised when a cross-collection is requested.
/// - 1214: @LIT{illegal index handle}
///   Will be raised when a index handle is corrupt.
/// - 1216: @LIT{document too large}
///   Will be raised when the document cannot fit into any datafile because of
///   it is too large.
/// - 1217: @LIT{collection must be unloaded}
///   Will be raised when a collection should be unloaded, but has a different
///   status.
/// - 1218: @LIT{collection type invalid}
///   Will be raised when an invalid collection type is used in a request.
/// - 1219: @LIT{validator failed}
///   Will be raised when the validation of an attribute of a structure failed.
/// - 1220: @LIT{parsing attribute name definition failed}
///   Will be raised when parsing an attribute name definition failed.
/// - 1221: @LIT{illegal document key}
///   Will be raised when a document key is corrupt.
/// - 1222: @LIT{unexpected document key}
///   Will be raised when a user-defined document key is supplied for
///   collections with auto key generation.
/// - 1224: @LIT{server database directory not writable}
///   Will be raised when the server's database directory is not writable for
///   the current user.
/// - 1225: @LIT{out of keys}
///   Will be raised when a key generator runs out of keys.
/// - 1226: @LIT{missing document key}
///   Will be raised when a document key is missing.
/// - 1227: @LIT{invalid document type}
///   Will be raised when there is an attempt to create a document with an
///   invalid type.
/// - 1228: @LIT{database not found}
///   Will be raised when a non-existing database is accessed.
/// - 1229: @LIT{database name invalid}
///   Will be raised when an invalid database name is used.
/// - 1230: @LIT{operation only allowed in system database}
///   Will be raised when an operation is requested in a database other than
///   the system database.
/// - 1231: @LIT{endpoint not found}
///   Will be raised when there is an attempt to delete a non-existing endpoint.
/// - 1232: @LIT{invalid key generator}
///   Will be raised when an invalid key generator description is used.
/// - 1233: @LIT{edge attribute missing or invalid}
///   will be raised when the _from or _to values of an edge are undefined or
///   contain an invalid value.
/// - 1234: @LIT{index insertion warning - attribute missing in document}
///   Will be raised when an attempt to insert a document into an index is
///   caused by in the document not having one or more attributes which the
///   index is built on.
/// - 1235: @LIT{index creation failed}
///   Will be raised when an attempt to create an index has failed.
/// - 1236: @LIT{write-throttling timeout}
///   Will be raised when the server is write-throttled and a write operation
///   has waited too long for the server to process queued operations.
/// - 1237: @LIT{collection type mismatch}
///   Will be raised when a collection has a different type from what has been
///   expected.
/// - 1238: @LIT{collection not loaded}
///   Will be raised when a collection is accessed that is not yet loaded.
/// - 1239: @LIT{illegal document revision}
///   Will be raised when a document revision is corrupt or is missing where
///   needed.
/// - 1300: @LIT{datafile full}
///   Will be raised when the datafile reaches its limit.
/// - 1301: @LIT{server database directory is empty}
///   Will be raised when encountering an empty server database directory.
/// - 1302: @LIT{operation should be tried again}
///   Will be raised when an operation should be retried.
/// - 1303: @LIT{engine is busy}
///   Will be raised when storage engine is busy.
/// - 1304: @LIT{merge in progress}
///   Will be raised when storage engine has a datafile merge in progress and
///   cannot complete the operation.
/// - 1305: @LIT{storage engine I/O error}
///   Will be raised when storage engine encounters an I/O error.
/// - 1400: @LIT{no response}
///   Will be raised when the replication applier does not receive any or an
///   incomplete response from the master.
/// - 1401: @LIT{invalid response}
///   Will be raised when the replication applier receives an invalid response
///   from the master.
/// - 1402: @LIT{master error}
///   Will be raised when the replication applier receives a server error from
///   the master.
/// - 1403: @LIT{master incompatible}
///   Will be raised when the replication applier connects to a master that has
///   an incompatible version.
/// - 1404: @LIT{master change}
///   Will be raised when the replication applier connects to a different
///   master than before.
/// - 1405: @LIT{loop detected}
///   Will be raised when the replication applier is asked to connect to itself
///   for replication.
/// - 1406: @LIT{unexpected marker}
///   Will be raised when an unexpected marker is found in the replication log
///   stream.
/// - 1407: @LIT{invalid applier state}
///   Will be raised when an invalid replication applier state file is found.
/// - 1408: @LIT{invalid transaction}
///   Will be raised when an unexpected transaction id is found.
/// - 1410: @LIT{invalid replication applier configuration}
///   Will be raised when the configuration for the replication applier is
///   invalid.
/// - 1411: @LIT{cannot perform operation while applier is running}
///   Will be raised when there is an attempt to perform an operation while the
///   replication applier is running.
/// - 1412: @LIT{replication stopped}
///   Special error code used to indicate the replication applier was stopped
///   by a user.
/// - 1413: @LIT{no start tick}
///   Will be raised when the replication applier is started without a known
///   start tick value.
/// - 1414: @LIT{start tick not present}
///   Will be raised when the replication applier fetches data using a start
///   tick, but that start tick is not present on the logger server anymore.
/// - 1415: @LIT{the checksum format is wrong}
///    "Will be raised when the format of the checksum is wrong")
/// - 1416: @LIT{wrong checksum}
///   Will be raised when a new born follower submits a wrong checksum
/// - 1450: @LIT{could not connect to agency}
///   Will be raised when none of the agency servers can be connected to.
/// - 1451: @LIT{missing coordinator header}
///   Will be raised when a DB server in a cluster receives a HTTP request
///   without a coordinator header.
/// - 1452: @LIT{could not lock plan in agency}
///   Will be raised when a coordinator in a cluster cannot lock the Plan
///   hierarchy in the agency.
/// - 1453: @LIT{collection ID already exists}
///   Will be raised when a coordinator in a cluster tries to create a
///   collection and the collection ID already exists.
/// - 1454: @LIT{could not create collection in plan}
///   Will be raised when a coordinator in a cluster cannot create an entry for
///   a new collection in the Plan hierarchy in the agency.
/// - 1455: @LIT{could not read version in current in agency}
///   Will be raised when a coordinator in a cluster cannot read the Version
///   entry in the Current hierarchy in the agency.
/// - 1456: @LIT{could not create collection}
///   Will be raised when a coordinator in a cluster notices that some
///   DBServers report problems when creating shards for a new collection.
/// - 1457: @LIT{timeout in cluster operation}
///   Will be raised when a coordinator in a cluster runs into a timeout for
///   some cluster wide operation.
/// - 1458: @LIT{could not remove collection from plan}
///   Will be raised when a coordinator in a cluster cannot remove an entry for
///   a collection in the Plan hierarchy in the agency.
/// - 1459: @LIT{could not remove collection from current}
///   Will be raised when a coordinator in a cluster cannot remove an entry for
///   a collection in the Current hierarchy in the agency.
/// - 1460: @LIT{could not create database in plan}
///   Will be raised when a coordinator in a cluster cannot create an entry for
///   a new database in the Plan hierarchy in the agency.
/// - 1461: @LIT{could not create database}
///   Will be raised when a coordinator in a cluster notices that some
///   DBServers report problems when creating databases for a new cluster wide
///   database.
/// - 1462: @LIT{could not remove database from plan}
///   Will be raised when a coordinator in a cluster cannot remove an entry for
///   a database in the Plan hierarchy in the agency.
/// - 1463: @LIT{could not remove database from current}
///   Will be raised when a coordinator in a cluster cannot remove an entry for
///   a database in the Current hierarchy in the agency.
/// - 1464: @LIT{no responsible shard found}
///   Will be raised when a coordinator in a cluster cannot determine the shard
///   that is responsible for a given document.
/// - 1465: @LIT{cluster internal HTTP connection broken}
///   Will be raised when a coordinator in a cluster loses an HTTP connection
///   to a DBserver in the cluster whilst transferring data.
/// - 1466: @LIT{must not specify _key for this collection}
///   Will be raised when a coordinator in a cluster finds that the _key
///   attribute was specified in a sharded collection the uses not only _key as
///   sharding attribute.
/// - 1467: @LIT{got contradicting answers from different shards}
///   Will be raised if a coordinator in a cluster gets conflicting results
///   from different shards, which should never happen.
/// - 1468: @LIT{not all sharding attributes given}
///   Will be raised if a coordinator tries to find out which shard is
///   responsible for a partial document, but cannot do this because not all
///   sharding attributes are specified.
/// - 1469: @LIT{must not change the value of a shard key attribute}
///   Will be raised if there is an attempt to update the value of a shard
///   attribute.
/// - 1470: @LIT{unsupported operation or parameter}
///   Will be raised when there is an attempt to carry out an operation that is
///   not supported in the context of a sharded collection.
/// - 1471: @LIT{this operation is only valid on a coordinator in a cluster}
///   Will be raised if there is an attempt to run a coordinator-only operation
///   on a different type of node.
/// - 1472: @LIT{error reading Plan in agency}
///   Will be raised if a coordinator or DBserver cannot read the Plan in the
///   agency.
/// - 1473: @LIT{could not truncate collection}
///   Will be raised if a coordinator cannot truncate all shards of a cluster
///   collection.
/// - 1474: @LIT{error in cluster internal communication for AQL}
///   Will be raised if the internal communication of the cluster for AQL
///   produces an error.
/// - 1475: @LIT{document not found or sharding attributes changed}
///   Will be raised when a document with a given identifier or handle is
///   unknown, or if the sharding attributes have been changed in a REPLACE
///   operation in the cluster.
/// - 1476: @LIT{could not determine my ID from my local info}
///   Will be raised if a cluster server at startup could not determine its own
///   ID from the local info provided.
/// - 1477: @LIT{this operation is only valid on a DBserver in a cluster}
///   Will be raised if there is an attempt to run a DBserver-only operation on
///   a different type of node.
/// - 1478: @LIT{A cluster backend which was required for the operation could not be reached}
///   Will be raised if a required db server can't be reached.
/// - 1479: @LIT{An endpoint couldn't be found}
///   An endpoint couldn't be found
/// - 1480: @LIT{Invalid agency structure}
///   The structure in the agency is invalid
/// - 1481: @LIT{collection is out of sync}
///   Will be raised if a collection needed during query execution is out of
///   sync. This currently can only happen when using satellite collections
/// - 1482: @LIT{could not create index in plan}
///   Will be raised when a coordinator in a cluster cannot create an entry for
///   a new index in the Plan hierarchy in the agency.
/// - 1483: @LIT{could not drop index in plan}
///   Will be raised when a coordinator in a cluster cannot remove an index
///   from the Plan hierarchy in the agency.
/// - 1484: @LIT{chain of distributeShardsLike references}
///   Will be raised if one tries to create a collection with a
///   distributeShardsLike attribute which points to another collection that
///   also has one.
/// - 1485: @LIT{must not drop collection while another has a distributeShardsLike attribute pointing to it}
///   Will be raised if one tries to drop a collection to which another
///   collection points with its distributeShardsLike attribute.
/// - 1486: @LIT{must not have a distributeShardsLike attribute pointing to an unknown collection}
///   Will be raised if one tries to create a collection which points to an
///   unknown collection in its distributeShardsLike attribute.
/// - 1487: @LIT{the number of current dbservers is lower than the requested replicationFactor}
///   Will be raised if one tries to create a collection with a
///   replicationFactor greater than the available number of DBServers.
/// - 1488: @LIT{a follower could not be dropped in agency}
///   Will be raised if a follower that ought to be dropped could not be
///   dropped in the agency (under Current).
/// - 1489: @LIT{a shard leader refuses to perform a replication operation}
///   Will be raised if a replication operation is refused by a shard leader.
/// - 1490: @LIT{a shard follower refuses to perform an operation that is not a replication}
///   Will be raised if a non-replication operation is refused by a shard
///   follower.
/// - 1491: @LIT{a (former) shard leader refuses to perform an operation, because it has resigned in the meantime}
///   Will be raised if a non-replication operation is refused by a former
///   shard leader that has found out that it is no longer the leader.
/// - 1492: @LIT{some agency operation failed}
///   Will be raised if after various retries an agency operation could not be
///   performed successfully.
/// - 1493: @LIT{conflicting replication factor with distributeShardsLike parameter assignment}
///   Will be raised if intended replication factor does not match that of the
///   prototype shard given in ditributeShardsLike parameter.
/// - 1494: @LIT{conflicting shard number with distributeShardsLike parameter assignment}
///   Will be raised if intended number of shards does not match that of the
///   prototype shard given in ditributeShardsLike parameter.
/// - 1500: @LIT{query killed}
///   Will be raised when a running query is killed by an explicit admin
///   command.
/// - 1501: @LIT{\%s}
///   Will be raised when query is parsed and is found to be syntactically
///   invalid.
/// - 1502: @LIT{query is empty}
///   Will be raised when an empty query is specified.
/// - 1503: @LIT{runtime error '\%s'}
///   Will be raised when a runtime error is caused by the query.
/// - 1504: @LIT{number out of range}
///   Will be raised when a number is outside the expected range.
/// - 1510: @LIT{variable name '\%s' has an invalid format}
///   Will be raised when an invalid variable name is used.
/// - 1511: @LIT{variable '\%s' is assigned multiple times}
///   Will be raised when a variable gets re-assigned in a query.
/// - 1512: @LIT{unknown variable '\%s'}
///   Will be raised when an unknown variable is used or the variable is
///   undefined the context it is used.
/// - 1521: @LIT{unable to read-lock collection \%s}
///   Will be raised when a read lock on the collection cannot be acquired.
/// - 1522: @LIT{too many collections}
///   Will be raised when the number of collections in a query is beyond the
///   allowed value.
/// - 1530: @LIT{document attribute '\%s' is assigned multiple times}
///   Will be raised when a document attribute is re-assigned.
/// - 1540: @LIT{usage of unknown function '\%s()'}
///   Will be raised when an undefined function is called.
/// - 1541: @LIT{invalid number of arguments for function '\%s()', expected number of arguments: minimum: \%d, maximum: \%d}
///   Will be raised when the number of arguments used in a function call does
///   not match the expected number of arguments for the function.
/// - 1542: @LIT{invalid argument type in call to function '\%s()'}
///   Will be raised when the type of an argument used in a function call does
///   not match the expected argument type.
/// - 1543: @LIT{invalid regex value}
///   Will be raised when an invalid regex argument value is used in a call to
///   a function that expects a regex.
/// - 1550: @LIT{invalid structure of bind parameters}
///   Will be raised when the structure of bind parameters passed has an
///   unexpected format.
/// - 1551: @LIT{no value specified for declared bind parameter '\%s'}
///   Will be raised when a bind parameter was declared in the query but the
///   query is being executed with no value for that parameter.
/// - 1552: @LIT{bind parameter '\%s' was not declared in the query}
///   Will be raised when a value gets specified for an undeclared bind
///   parameter.
/// - 1553: @LIT{bind parameter '\%s' has an invalid value or type}
///   Will be raised when a bind parameter has an invalid value or type.
/// - 1560: @LIT{invalid logical value}
///   Will be raised when a non-boolean value is used in a logical operation.
/// - 1561: @LIT{invalid arithmetic value}
///   Will be raised when a non-numeric value is used in an arithmetic
///   operation.
/// - 1562: @LIT{division by zero}
///   Will be raised when there is an attempt to divide by zero.
/// - 1563: @LIT{array expected}
///   Will be raised when a non-array operand is used for an operation that
///   expects an array argument operand.
/// - 1569: @LIT{FAIL(\%s) called}
///   Will be raised when the function FAIL() is called from inside a query.
/// - 1570: @LIT{no suitable geo index found for geo restriction on '\%s'}
///   Will be raised when a geo restriction was specified but no suitable geo
///   index is found to resolve it.
/// - 1571: @LIT{no suitable fulltext index found for fulltext query on '\%s'}
///   Will be raised when a fulltext query is performed on a collection without
///   a suitable fulltext index.
/// - 1572: @LIT{invalid date value}
///   Will be raised when a value cannot be converted to a date.
/// - 1573: @LIT{multi-modify query}
///   Will be raised when an AQL query contains more than one data-modifying
///   operation.
/// - 1574: @LIT{invalid aggregate expression}
///   Will be raised when an AQL query contains an invalid aggregate expression.
/// - 1575: @LIT{query options must be readable at query compile time}
///   Will be raised when an AQL data-modification query contains options that
///   cannot be figured out at query compile time.
/// - 1576: @LIT{query options expected}
///   Will be raised when an AQL data-modification query contains an invalid
///   options specification.
/// - 1577: @LIT{collection '\%s' used as expression operand}
///   Will be raised when a collection is used as an operand in an AQL
///   expression.
/// - 1578: @LIT{disallowed dynamic call to '\%s'}
///   Will be raised when a dynamic function call is made to a function that
///   cannot be called dynamically.
/// - 1579: @LIT{access after data-modification by \%s}
///   Will be raised when collection data are accessed after a
///   data-modification operation.
/// - 1580: @LIT{invalid user function name}
///   Will be raised when a user function with an invalid name is registered.
/// - 1581: @LIT{invalid user function code}
///   Will be raised when a user function is registered with invalid code.
/// - 1582: @LIT{user function '\%s()' not found}
///   Will be raised when a user function is accessed but not found.
/// - 1583: @LIT{user function runtime error: \%s}
///   Will be raised when a user function throws a runtime exception.
/// - 1590: @LIT{bad execution plan JSON}
///   Will be raised when an HTTP API for a query got an invalid JSON object.
/// - 1591: @LIT{query ID not found}
///   Will be raised when an Id of a query is not found by the HTTP API.
/// - 1592: @LIT{query with this ID is in use}
///   Will be raised when an Id of a query is found by the HTTP API but the
///   query is in use.
/// - 1600: @LIT{cursor not found}
///   Will be raised when a cursor is requested via its id but a cursor with
///   that id cannot be found.
/// - 1601: @LIT{cursor is busy}
///   Will be raised when a cursor is requested via its id but a concurrent
///   request is still using the cursor.
/// - 1650: @LIT{internal transaction error}
///   Will be raised when a wrong usage of transactions is detected. this is an
///   internal error and indicates a bug in ArangoDB.
/// - 1651: @LIT{nested transactions detected}
///   Will be raised when transactions are nested.
/// - 1652: @LIT{unregistered collection used in transaction}
///   Will be raised when a collection is used in the middle of a transaction
///   but was not registered at transaction start.
/// - 1653: @LIT{disallowed operation inside transaction}
///   Will be raised when a disallowed operation is carried out in a
///   transaction.
/// - 1654: @LIT{transaction aborted}
///   Will be raised when a transaction was aborted.
/// - 1700: @LIT{invalid user name}
///   Will be raised when an invalid user name is used.
/// - 1701: @LIT{invalid password}
///   Will be raised when an invalid password is used.
/// - 1702: @LIT{duplicate user}
///   Will be raised when a user name already exists.
/// - 1703: @LIT{user not found}
///   Will be raised when a user name is updated that does not exist.
/// - 1704: @LIT{user must change his password}
///   Will be raised when the user must change his password.
/// - 1750: @LIT{invalid service name}
///   Will be raised when an invalid service name is specified.
/// - 1751: @LIT{invalid mount}
///   Will be raised when an invalid mount is specified.
/// - 1752: @LIT{service download failed}
///   Will be raised when a service download from the central repository failed.
/// - 1753: @LIT{service upload failed}
///   Will be raised when a service upload from the client to the ArangoDB
///   server failed.
/// - 1800: @LIT{cannot init a LDAP connection}
///   can not init a LDAP connection
/// - 1801: @LIT{cannot set a LDAP option}
///   can not set a LDAP option
/// - 1802: @LIT{cannot bind to a LDAP server}
///   can not bind to a LDAP server
/// - 1803: @LIT{cannot unbind from a LDAP server}
///   can not unbind from a LDAP server
/// - 1804: @LIT{cannot issue a LDAP search}
///   can not search the LDAP server
/// - 1805: @LIT{cannot start a TLS LDAP session}
///   can not star a TLS LDAP session
/// - 1806: @LIT{LDAP didn't found any objects}
///   LDAP didn't found any objects with the specified search query
/// - 1807: @LIT{LDAP found zero ore more than one user}
///   LDAP found zero ore more than one user
/// - 1808: @LIT{LDAP found a user, but its not the desired one}
///   LDAP found a user, but its not the desired one
/// - 1820: @LIT{invalid ldap mode}
///   cant distinguish a valid mode for provided ldap configuration
/// - 1850: @LIT{invalid task id}
///   Will be raised when a task is created with an invalid id.
/// - 1851: @LIT{duplicate task id}
///   Will be raised when a task id is created with a duplicate id.
/// - 1852: @LIT{task not found}
///   Will be raised when a task with the specified id could not be found.
/// - 1901: @LIT{invalid graph}
///   Will be raised when an invalid name is passed to the server.
/// - 1902: @LIT{could not create graph}
///   Will be raised when an invalid name, vertices or edges is passed to the
///   server.
/// - 1903: @LIT{invalid vertex}
///   Will be raised when an invalid vertex id is passed to the server.
/// - 1904: @LIT{could not create vertex}
///   Will be raised when the vertex could not be created.
/// - 1905: @LIT{could not change vertex}
///   Will be raised when the vertex could not be changed.
/// - 1906: @LIT{invalid edge}
///   Will be raised when an invalid edge id is passed to the server.
/// - 1907: @LIT{could not create edge}
///   Will be raised when the edge could not be created.
/// - 1908: @LIT{could not change edge}
///   Will be raised when the edge could not be changed.
/// - 1909: @LIT{too many iterations - try increasing the value of 'maxIterations'}
///   Will be raised when too many iterations are done in a graph traversal.
/// - 1910: @LIT{invalid filter result}
///   Will be raised when an invalid filter result is returned in a graph
///   traversal.
/// - 1920: @LIT{multi use of edge collection in edge def}
///   an edge collection may only be used once in one edge definition of a
///   graph.
/// - 1921: @LIT{edge collection already used in edge def}
///    is already used by another graph in a different edge definition.
/// - 1922: @LIT{missing graph name}
///   a graph name is required to create a graph.
/// - 1923: @LIT{malformed edge definition}
///   the edge definition is malformed. It has to be an array of objects.
/// - 1924: @LIT{graph not found}
///   a graph with this name could not be found.
/// - 1925: @LIT{graph already exists}
///   a graph with this name already exists.
/// - 1926: @LIT{vertex collection does not exist or is not part of the graph}
///   the specified vertex collection does not exist or is not part of the
///   graph.
/// - 1927: @LIT{not a vertex collection}
///   the collection is not a vertex collection.
/// - 1928: @LIT{not in orphan collection}
///   Vertex collection not in orphan collection of the graph.
/// - 1929: @LIT{collection already used in edge def}
///   The collection is already used in an edge definition of the graph.
/// - 1930: @LIT{edge collection not used in graph}
///   The edge collection is not used in any edge definition of the graph.
/// - 1931: @LIT{ is not an ArangoCollection}
///   The collection is not an ArangoCollection.
/// - 1932: @LIT{collection _graphs does not exist}
///   collection _graphs does not exist.
/// - 1933: @LIT{Invalid example type. Has to be String, Array or Object}
///   Invalid example type. Has to be String, Array or Object.
/// - 1934: @LIT{Invalid example type. Has to be Array or Object}
///   Invalid example type. Has to be Array or Object.
/// - 1935: @LIT{Invalid number of arguments. Expected: }
///   Invalid number of arguments. Expected: 
/// - 1936: @LIT{Invalid parameter type.}
///   Invalid parameter type.
/// - 1937: @LIT{Invalid id}
///   Invalid id
/// - 1938: @LIT{collection used in orphans}
///   The collection is already used in the orphans of the graph.
/// - 1939: @LIT{edge collection does not exist or is not part of the graph}
///   the specified edge collection does not exist or is not part of the graph.
/// - 1940: @LIT{empty graph}
///   The requested graph has no edge collections.
/// - 1950: @LIT{unknown session}
///   Will be raised when an invalid/unknown session id is passed to the server.
/// - 1951: @LIT{session expired}
///   Will be raised when a session is expired.
/// - 2000: @LIT{unknown client error}
///   This error should not happen.
/// - 2001: @LIT{could not connect to server}
///   Will be raised when the client could not connect to the server.
/// - 2002: @LIT{could not write to server}
///   Will be raised when the client could not write data.
/// - 2003: @LIT{could not read from server}
///   Will be raised when the client could not read data.
/// - 2100: @LIT{Request aborted}
///   Request was aborted.
/// - 2101: @LIT{Communication was disabled}
///   Communication was disabled.
/// - 3000: @LIT{failed to parse manifest file}
///   The service manifest file is not well-formed JSON.
/// - 3001: @LIT{manifest file is invalid}
///   The service manifest contains invalid values.
/// - 3002: @LIT{service files missing}
///   The service folder or bundle does not exist on this server.
/// - 3003: @LIT{service files outdated}
///   The local service bundle does not match the checksum in the database.
/// - 3004: @LIT{service options are invalid}
///   The service options contain invalid values.
/// - 3007: @LIT{invalid mountpath}
///   The service mountpath contains invalid characters.
/// - 3009: @LIT{service not found}
///   No service found at the given mountpath.
/// - 3010: @LIT{service needs configuration}
///   The service is missing configuration or dependencies.
/// - 3011: @LIT{service already exists}
///   A service already exists at the given mountpath.
/// - 3012: @LIT{missing manifest file}
///   The service directory does not contain a manifest file.
/// - 3013: @LIT{failed to parse service options}
///   The service options are not well-formed JSON.
/// - 3014: @LIT{source path not found}
///   The source path does not match a file or directory.
/// - 3015: @LIT{error resolving source}
///   The source path could not be resolved.
/// - 3016: @LIT{unknown script}
///   The service does not have a script with this name.
/// - 3100: @LIT{cannot locate module}
///   The module path could not be resolved.
/// - 3101: @LIT{syntax error in module}
///   The module could not be parsed because of a syntax error.
/// - 3103: @LIT{failed to invoke module}
///   Failed to invoke the module in its context.
/// - 4000: @LIT{collection is not smart}
///   The requested collection needs to be smart, but it ain't
/// - 4001: @LIT{smart graph attribute not given}
///   The given document does not have the smart graph attribute set.
/// - 4002: @LIT{cannot drop this smart collection}
///   This smart collection cannot be dropped, it dictates sharding in the
///   graph.
/// - 4003: @LIT{in smart vertex collections _key must be prefixed with the value of the smart graph attribute}
///   In a smart vertex collection _key must be prefixed with the value of the
///   smart graph attribute.
/// - 4004: @LIT{attribute cannot be used as smart graph attribute}
///   The given smartGraph attribute is illegal and connot be used for
///   sharding. All system attributes are forbidden.
/// - 20001: @LIT{Illegal inquiry syntax}
///   Inquiry handles a list of string clientIds: [<clientId>,...].
/// - 20011: @LIT{Inform message must be an object.}
///   The inform message in the agency must be an object.
/// - 20012: @LIT{Inform message must contain uint parameter 'term'}
///   The inform message in the agency must contain a uint parameter 'term'.
/// - 20013: @LIT{Inform message must contain string parameter 'id'}
///   The inform message in the agency must contain a string parameter 'id'.
/// - 20014: @LIT{Inform message must contain array 'active'}
///   The inform message in the agency must contain an array 'active'.
/// - 20015: @LIT{Inform message must contain object 'pool'}
///   The inform message in the agency must contain an object 'pool'.
/// - 20016: @LIT{Inform message must contain object 'min ping'}
///   The inform message in the agency must contain an object 'min ping'.
/// - 20017: @LIT{Inform message must contain object 'max ping'}
///   The inform message in the agency must contain an object 'max ping'.
/// - 20018: @LIT{Inform message must contain object 'timeoutMult'}
///   The inform message in the agency must contain an object 'timeoutMult'.
/// - 20020: @LIT{Inquiry failed}
///   Inquiry by clientId failed
/// - 20021: @LIT{Cannot rebuild readDB and spearHead}
///   Will be raised if the readDB or the spearHead cannot be rebuilt from the
///   replicated log.
/// - 20501: @LIT{general supervision failure}
///   General supervision failure.
/// - 21001: @LIT{dispatcher stopped}
///   Will be returned if a shutdown is in progress.
/// - 21002: @LIT{named queue does not exist}
///   Will be returned if a queue with this name does not exist.
/// - 21003: @LIT{named queue is full}
///   Will be returned if a queue with this name is full.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief helper macro to define an error string
////////////////////////////////////////////////////////////////////////////////

#define REG_ERROR(id, label) TRI_set_errno_string(TRI_ ## id, label);

////////////////////////////////////////////////////////////////////////////////
/// @brief register all errors for ArangoDB
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeErrorMessages ();

////////////////////////////////////////////////////////////////////////////////
/// @brief 0: ERROR_NO_ERROR
///
/// no error
///
/// No error has occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_NO_ERROR                                                (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1: ERROR_FAILED
///
/// failed
///
/// Will be raised when a general error occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_FAILED                                                  (1)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2: ERROR_SYS_ERROR
///
/// system error
///
/// Will be raised when operating system error occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SYS_ERROR                                               (2)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3: ERROR_OUT_OF_MEMORY
///
/// out of memory
///
/// Will be raised when there is a memory shortage.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_OUT_OF_MEMORY                                           (3)

////////////////////////////////////////////////////////////////////////////////
/// @brief 4: ERROR_INTERNAL
///
/// internal error
///
/// Will be raised when an internal error occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_INTERNAL                                                (4)

////////////////////////////////////////////////////////////////////////////////
/// @brief 5: ERROR_ILLEGAL_NUMBER
///
/// illegal number
///
/// Will be raised when an illegal representation of a number was given.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ILLEGAL_NUMBER                                          (5)

////////////////////////////////////////////////////////////////////////////////
/// @brief 6: ERROR_NUMERIC_OVERFLOW
///
/// numeric overflow
///
/// Will be raised when a numeric overflow occurred.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_NUMERIC_OVERFLOW                                        (6)

////////////////////////////////////////////////////////////////////////////////
/// @brief 7: ERROR_ILLEGAL_OPTION
///
/// illegal option
///
/// Will be raised when an unknown option was supplied by the user.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ILLEGAL_OPTION                                          (7)

////////////////////////////////////////////////////////////////////////////////
/// @brief 8: ERROR_DEAD_PID
///
/// dead process identifier
///
/// Will be raised when a PID without a living process was found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_DEAD_PID                                                (8)

////////////////////////////////////////////////////////////////////////////////
/// @brief 9: ERROR_NOT_IMPLEMENTED
///
/// not implemented
///
/// Will be raised when hitting an unimplemented feature.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_NOT_IMPLEMENTED                                         (9)

////////////////////////////////////////////////////////////////////////////////
/// @brief 10: ERROR_BAD_PARAMETER
///
/// bad parameter
///
/// Will be raised when the parameter does not fulfill the requirements.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_BAD_PARAMETER                                           (10)

////////////////////////////////////////////////////////////////////////////////
/// @brief 11: ERROR_FORBIDDEN
///
/// forbidden
///
/// Will be raised when you are missing permission for the operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_FORBIDDEN                                               (11)

////////////////////////////////////////////////////////////////////////////////
/// @brief 12: ERROR_OUT_OF_MEMORY_MMAP
///
/// out of memory in mmap
///
/// Will be raised when there is a memory shortage.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_OUT_OF_MEMORY_MMAP                                      (12)

////////////////////////////////////////////////////////////////////////////////
/// @brief 13: ERROR_CORRUPTED_CSV
///
/// csv is corrupt
///
/// Will be raised when encountering a corrupt csv line.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CORRUPTED_CSV                                           (13)

////////////////////////////////////////////////////////////////////////////////
/// @brief 14: ERROR_FILE_NOT_FOUND
///
/// file not found
///
/// Will be raised when a file is not found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_FILE_NOT_FOUND                                          (14)

////////////////////////////////////////////////////////////////////////////////
/// @brief 15: ERROR_CANNOT_WRITE_FILE
///
/// cannot write file
///
/// Will be raised when a file cannot be written.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CANNOT_WRITE_FILE                                       (15)

////////////////////////////////////////////////////////////////////////////////
/// @brief 16: ERROR_CANNOT_OVERWRITE_FILE
///
/// cannot overwrite file
///
/// Will be raised when an attempt is made to overwrite an existing file.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CANNOT_OVERWRITE_FILE                                   (16)

////////////////////////////////////////////////////////////////////////////////
/// @brief 17: ERROR_TYPE_ERROR
///
/// type error
///
/// Will be raised when a type error is unencountered.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TYPE_ERROR                                              (17)

////////////////////////////////////////////////////////////////////////////////
/// @brief 18: ERROR_LOCK_TIMEOUT
///
/// lock timeout
///
/// Will be raised when there's a timeout waiting for a lock.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LOCK_TIMEOUT                                            (18)

////////////////////////////////////////////////////////////////////////////////
/// @brief 19: ERROR_CANNOT_CREATE_DIRECTORY
///
/// cannot create directory
///
/// Will be raised when an attempt to create a directory fails.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CANNOT_CREATE_DIRECTORY                                 (19)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20: ERROR_CANNOT_CREATE_TEMP_FILE
///
/// cannot create temporary file
///
/// Will be raised when an attempt to create a temporary file fails.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CANNOT_CREATE_TEMP_FILE                                 (20)

////////////////////////////////////////////////////////////////////////////////
/// @brief 21: ERROR_REQUEST_CANCELED
///
/// canceled request
///
/// Will be raised when a request is canceled by the user.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REQUEST_CANCELED                                        (21)

////////////////////////////////////////////////////////////////////////////////
/// @brief 22: ERROR_DEBUG
///
/// intentional debug error
///
/// Will be raised intentionally during debugging.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_DEBUG                                                   (22)

////////////////////////////////////////////////////////////////////////////////
/// @brief 25: ERROR_IP_ADDRESS_INVALID
///
/// IP address is invalid
///
/// Will be raised when the structure of an IP address is invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_IP_ADDRESS_INVALID                                      (25)

////////////////////////////////////////////////////////////////////////////////
/// @brief 27: ERROR_FILE_EXISTS
///
/// file exists
///
/// Will be raised when a file already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_FILE_EXISTS                                             (27)

////////////////////////////////////////////////////////////////////////////////
/// @brief 28: ERROR_LOCKED
///
/// locked
///
/// Will be raised when a resource or an operation is locked.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LOCKED                                                  (28)

////////////////////////////////////////////////////////////////////////////////
/// @brief 29: ERROR_DEADLOCK
///
/// deadlock detected
///
/// Will be raised when a deadlock is detected when accessing collections.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_DEADLOCK                                                (29)

////////////////////////////////////////////////////////////////////////////////
/// @brief 30: ERROR_SHUTTING_DOWN
///
/// shutdown in progress
///
/// Will be raised when a call cannot succeed because a server shutdown is
/// already in progress.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SHUTTING_DOWN                                           (30)

////////////////////////////////////////////////////////////////////////////////
/// @brief 31: ERROR_ONLY_ENTERPRISE
///
/// only enterprise version
///
/// Will be raised when an enterprise-feature is requested from the community
/// edition.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ONLY_ENTERPRISE                                         (31)

////////////////////////////////////////////////////////////////////////////////
/// @brief 32: ERROR_RESOURCE_LIMIT
///
/// resource limit exceeded
///
/// Will be raised when the resources used by an operation exceed the
/// configured maximum value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_RESOURCE_LIMIT                                          (32)

////////////////////////////////////////////////////////////////////////////////
/// @brief 400: ERROR_HTTP_BAD_PARAMETER
///
/// bad parameter
///
/// Will be raised when the HTTP request does not fulfill the requirements.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_BAD_PARAMETER                                      (400)

////////////////////////////////////////////////////////////////////////////////
/// @brief 401: ERROR_HTTP_UNAUTHORIZED
///
/// unauthorized
///
/// Will be raised when authorization is required but the user is not
/// authorized.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_UNAUTHORIZED                                       (401)

////////////////////////////////////////////////////////////////////////////////
/// @brief 403: ERROR_HTTP_FORBIDDEN
///
/// forbidden
///
/// Will be raised when the operation is forbidden.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_FORBIDDEN                                          (403)

////////////////////////////////////////////////////////////////////////////////
/// @brief 404: ERROR_HTTP_NOT_FOUND
///
/// not found
///
/// Will be raised when an URI is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_NOT_FOUND                                          (404)

////////////////////////////////////////////////////////////////////////////////
/// @brief 405: ERROR_HTTP_METHOD_NOT_ALLOWED
///
/// method not supported
///
/// Will be raised when an unsupported HTTP method is used for an operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_METHOD_NOT_ALLOWED                                 (405)

////////////////////////////////////////////////////////////////////////////////
/// @brief 406: ERROR_HTTP_NOT_ACCEPTABLE
///
/// request not acceptable
///
/// Will be raised when an unsupported HTTP content type is used for an
/// operation, or if a request is not acceptable for a leader or follower.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_NOT_ACCEPTABLE                                     (406)

////////////////////////////////////////////////////////////////////////////////
/// @brief 412: ERROR_HTTP_PRECONDITION_FAILED
///
/// precondition failed
///
/// Will be raised when a precondition for an HTTP request is not met.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_PRECONDITION_FAILED                                (412)

////////////////////////////////////////////////////////////////////////////////
/// @brief 500: ERROR_HTTP_SERVER_ERROR
///
/// internal server error
///
/// Will be raised when an internal server is encountered.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_SERVER_ERROR                                       (500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 503: ERROR_HTTP_SERVICE_UNAVAILABLE
///
/// service unavailable
///
/// Will be raised when a service is temporarily unavailable.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_SERVICE_UNAVAILABLE                                (503)

////////////////////////////////////////////////////////////////////////////////
/// @brief 600: ERROR_HTTP_CORRUPTED_JSON
///
/// invalid JSON object
///
/// Will be raised when a string representation of a JSON object is corrupt.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_CORRUPTED_JSON                                     (600)

////////////////////////////////////////////////////////////////////////////////
/// @brief 601: ERROR_HTTP_SUPERFLUOUS_SUFFICES
///
/// superfluous URL suffices
///
/// Will be raised when the URL contains superfluous suffices.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES                               (601)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1000: ERROR_ARANGO_ILLEGAL_STATE
///
/// illegal state
///
/// Internal error that will be raised when the datafile is not in the required
/// state.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_ILLEGAL_STATE                                    (1000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1002: ERROR_ARANGO_DATAFILE_SEALED
///
/// datafile sealed
///
/// Internal error that will be raised when trying to write to a datafile.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_SEALED                                  (1002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1004: ERROR_ARANGO_READ_ONLY
///
/// read only
///
/// Internal error that will be raised when trying to write to a read-only
/// datafile or collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_READ_ONLY                                        (1004)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1005: ERROR_ARANGO_DUPLICATE_IDENTIFIER
///
/// duplicate identifier
///
/// Internal error that will be raised when a identifier duplicate is detected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER                             (1005)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1006: ERROR_ARANGO_DATAFILE_UNREADABLE
///
/// datafile unreadable
///
/// Internal error that will be raised when a datafile is unreadable.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_UNREADABLE                              (1006)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1007: ERROR_ARANGO_DATAFILE_EMPTY
///
/// datafile empty
///
/// Internal error that will be raised when a datafile is empty.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_EMPTY                                   (1007)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1008: ERROR_ARANGO_RECOVERY
///
/// logfile recovery error
///
/// Will be raised when an error occurred during WAL log file recovery.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_RECOVERY                                         (1008)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1009: ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND
///
/// datafile statistics not found
///
/// Will be raised when a required datafile statistics object was not found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND                    (1009)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1100: ERROR_ARANGO_CORRUPTED_DATAFILE
///
/// corrupted datafile
///
/// Will be raised when a corruption is detected in a datafile.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_CORRUPTED_DATAFILE                               (1100)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1101: ERROR_ARANGO_ILLEGAL_PARAMETER_FILE
///
/// illegal or unreadable parameter file
///
/// Will be raised if a parameter file is corrupted or cannot be read.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE                           (1101)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1102: ERROR_ARANGO_CORRUPTED_COLLECTION
///
/// corrupted collection
///
/// Will be raised when a collection contains one or more corrupted data files.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_CORRUPTED_COLLECTION                             (1102)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1103: ERROR_ARANGO_MMAP_FAILED
///
/// mmap failed
///
/// Will be raised when the system call mmap failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_MMAP_FAILED                                      (1103)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1104: ERROR_ARANGO_FILESYSTEM_FULL
///
/// filesystem full
///
/// Will be raised when the filesystem is full.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_FILESYSTEM_FULL                                  (1104)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1105: ERROR_ARANGO_NO_JOURNAL
///
/// no journal
///
/// Will be raised when a journal cannot be created.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_NO_JOURNAL                                       (1105)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1106: ERROR_ARANGO_DATAFILE_ALREADY_EXISTS
///
/// cannot create/rename datafile because it already exists
///
/// Will be raised when the datafile cannot be created or renamed because a
/// file of the same name already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS                          (1106)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1107: ERROR_ARANGO_DATADIR_LOCKED
///
/// database directory is locked
///
/// Will be raised when the database directory is locked by a different process.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATADIR_LOCKED                                   (1107)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1108: ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS
///
/// cannot create/rename collection because directory already exists
///
/// Will be raised when the collection cannot be created because a directory of
/// the same name already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS              (1108)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1109: ERROR_ARANGO_MSYNC_FAILED
///
/// msync failed
///
/// Will be raised when the system call msync failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_MSYNC_FAILED                                     (1109)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1110: ERROR_ARANGO_DATADIR_UNLOCKABLE
///
/// cannot lock database directory
///
/// Will be raised when the server cannot lock the database directory on
/// startup.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATADIR_UNLOCKABLE                               (1110)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1111: ERROR_ARANGO_SYNC_TIMEOUT
///
/// sync timeout
///
/// Will be raised when the server waited too long for a datafile to be synced
/// to disk.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_SYNC_TIMEOUT                                     (1111)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1200: ERROR_ARANGO_CONFLICT
///
/// conflict
///
/// Will be raised when updating or deleting a document and a conflict has been
/// detected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_CONFLICT                                         (1200)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1201: ERROR_ARANGO_DATADIR_INVALID
///
/// invalid database directory
///
/// Will be raised when a non-existing database directory was specified when
/// starting the database.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATADIR_INVALID                                  (1201)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1202: ERROR_ARANGO_DOCUMENT_NOT_FOUND
///
/// document not found
///
/// Will be raised when a document with a given identifier or handle is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND                               (1202)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1203: ERROR_ARANGO_COLLECTION_NOT_FOUND
///
/// collection not found
///
/// Will be raised when a collection with the given identifier or name is
/// unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND                             (1203)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1204: ERROR_ARANGO_COLLECTION_PARAMETER_MISSING
///
/// parameter 'collection' not found
///
/// Will be raised when the collection parameter is missing.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING                     (1204)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1205: ERROR_ARANGO_DOCUMENT_HANDLE_BAD
///
/// illegal document handle
///
/// Will be raised when a document handle is corrupt.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD                              (1205)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1206: ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL
///
/// maximal size of journal too small
///
/// Will be raised when the maximal size of the journal is too small.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL                           (1206)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1207: ERROR_ARANGO_DUPLICATE_NAME
///
/// duplicate name
///
/// Will be raised when a name duplicate is detected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DUPLICATE_NAME                                   (1207)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1208: ERROR_ARANGO_ILLEGAL_NAME
///
/// illegal name
///
/// Will be raised when an illegal name is detected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_ILLEGAL_NAME                                     (1208)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1209: ERROR_ARANGO_NO_INDEX
///
/// no suitable index known
///
/// Will be raised when no suitable index for the query is known.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_NO_INDEX                                         (1209)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1210: ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED
///
/// unique constraint violated
///
/// Will be raised when there is a unique constraint violation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED                       (1210)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1211: ERROR_ARANGO_VIEW_NOT_FOUND
///
/// view not found
///
/// Will be raised when a view with the given identifier or name is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_VIEW_NOT_FOUND                                   (1211)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1212: ERROR_ARANGO_INDEX_NOT_FOUND
///
/// index not found
///
/// Will be raised when an index with a given identifier is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_NOT_FOUND                                  (1212)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1213: ERROR_ARANGO_CROSS_COLLECTION_REQUEST
///
/// cross collection request not allowed
///
/// Will be raised when a cross-collection is requested.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST                         (1213)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1214: ERROR_ARANGO_INDEX_HANDLE_BAD
///
/// illegal index handle
///
/// Will be raised when a index handle is corrupt.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_HANDLE_BAD                                 (1214)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1216: ERROR_ARANGO_DOCUMENT_TOO_LARGE
///
/// document too large
///
/// Will be raised when the document cannot fit into any datafile because of it
/// is too large.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE                               (1216)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1217: ERROR_ARANGO_COLLECTION_NOT_UNLOADED
///
/// collection must be unloaded
///
/// Will be raised when a collection should be unloaded, but has a different
/// status.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED                          (1217)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1218: ERROR_ARANGO_COLLECTION_TYPE_INVALID
///
/// collection type invalid
///
/// Will be raised when an invalid collection type is used in a request.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID                          (1218)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1219: ERROR_ARANGO_VALIDATION_FAILED
///
/// validator failed
///
/// Will be raised when the validation of an attribute of a structure failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_VALIDATION_FAILED                                (1219)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1220: ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED
///
/// parsing attribute name definition failed
///
/// Will be raised when parsing an attribute name definition failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED                          (1220)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1221: ERROR_ARANGO_DOCUMENT_KEY_BAD
///
/// illegal document key
///
/// Will be raised when a document key is corrupt.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD                                 (1221)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1222: ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED
///
/// unexpected document key
///
/// Will be raised when a user-defined document key is supplied for collections
/// with auto key generation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED                          (1222)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1224: ERROR_ARANGO_DATADIR_NOT_WRITABLE
///
/// server database directory not writable
///
/// Will be raised when the server's database directory is not writable for the
/// current user.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE                             (1224)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1225: ERROR_ARANGO_OUT_OF_KEYS
///
/// out of keys
///
/// Will be raised when a key generator runs out of keys.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_OUT_OF_KEYS                                      (1225)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1226: ERROR_ARANGO_DOCUMENT_KEY_MISSING
///
/// missing document key
///
/// Will be raised when a document key is missing.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING                             (1226)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1227: ERROR_ARANGO_DOCUMENT_TYPE_INVALID
///
/// invalid document type
///
/// Will be raised when there is an attempt to create a document with an
/// invalid type.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID                            (1227)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1228: ERROR_ARANGO_DATABASE_NOT_FOUND
///
/// database not found
///
/// Will be raised when a non-existing database is accessed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATABASE_NOT_FOUND                               (1228)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1229: ERROR_ARANGO_DATABASE_NAME_INVALID
///
/// database name invalid
///
/// Will be raised when an invalid database name is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATABASE_NAME_INVALID                            (1229)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1230: ERROR_ARANGO_USE_SYSTEM_DATABASE
///
/// operation only allowed in system database
///
/// Will be raised when an operation is requested in a database other than the
/// system database.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE                              (1230)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1231: ERROR_ARANGO_ENDPOINT_NOT_FOUND
///
/// endpoint not found
///
/// Will be raised when there is an attempt to delete a non-existing endpoint.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_ENDPOINT_NOT_FOUND                               (1231)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1232: ERROR_ARANGO_INVALID_KEY_GENERATOR
///
/// invalid key generator
///
/// Will be raised when an invalid key generator description is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR                            (1232)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1233: ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE
///
/// edge attribute missing or invalid
///
/// will be raised when the _from or _to values of an edge are undefined or
/// contain an invalid value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE                           (1233)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1234: ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING
///
/// index insertion warning - attribute missing in document
///
/// Will be raised when an attempt to insert a document into an index is caused
/// by in the document not having one or more attributes which the index is
/// built on.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING                 (1234)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1235: ERROR_ARANGO_INDEX_CREATION_FAILED
///
/// index creation failed
///
/// Will be raised when an attempt to create an index has failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_CREATION_FAILED                            (1235)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1236: ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT
///
/// write-throttling timeout
///
/// Will be raised when the server is write-throttled and a write operation has
/// waited too long for the server to process queued operations.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT                           (1236)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1237: ERROR_ARANGO_COLLECTION_TYPE_MISMATCH
///
/// collection type mismatch
///
/// Will be raised when a collection has a different type from what has been
/// expected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH                         (1237)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1238: ERROR_ARANGO_COLLECTION_NOT_LOADED
///
/// collection not loaded
///
/// Will be raised when a collection is accessed that is not yet loaded.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED                            (1238)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1239: ERROR_ARANGO_DOCUMENT_REV_BAD
///
/// illegal document revision
///
/// Will be raised when a document revision is corrupt or is missing where
/// needed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_REV_BAD                                 (1239)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1300: ERROR_ARANGO_DATAFILE_FULL
///
/// datafile full
///
/// Will be raised when the datafile reaches its limit.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_FULL                                    (1300)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1301: ERROR_ARANGO_EMPTY_DATADIR
///
/// server database directory is empty
///
/// Will be raised when encountering an empty server database directory.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_EMPTY_DATADIR                                    (1301)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1302: ERROR_ARANGO_TRY_AGAIN
///
/// operation should be tried again
///
/// Will be raised when an operation should be retried.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_TRY_AGAIN                                        (1302)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1303: ERROR_ARANGO_BUSY
///
/// engine is busy
///
/// Will be raised when storage engine is busy.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_BUSY                                             (1303)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1304: ERROR_ARANGO_MERGE_IN_PROGRESS
///
/// merge in progress
///
/// Will be raised when storage engine has a datafile merge in progress and
/// cannot complete the operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_MERGE_IN_PROGRESS                                (1304)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1305: ERROR_ARANGO_IO_ERROR
///
/// storage engine I/O error
///
/// Will be raised when storage engine encounters an I/O error.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_IO_ERROR                                         (1305)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1400: ERROR_REPLICATION_NO_RESPONSE
///
/// no response
///
/// Will be raised when the replication applier does not receive any or an
/// incomplete response from the master.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_NO_RESPONSE                                 (1400)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1401: ERROR_REPLICATION_INVALID_RESPONSE
///
/// invalid response
///
/// Will be raised when the replication applier receives an invalid response
/// from the master.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_INVALID_RESPONSE                            (1401)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1402: ERROR_REPLICATION_MASTER_ERROR
///
/// master error
///
/// Will be raised when the replication applier receives a server error from
/// the master.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_MASTER_ERROR                                (1402)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1403: ERROR_REPLICATION_MASTER_INCOMPATIBLE
///
/// master incompatible
///
/// Will be raised when the replication applier connects to a master that has
/// an incompatible version.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE                         (1403)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1404: ERROR_REPLICATION_MASTER_CHANGE
///
/// master change
///
/// Will be raised when the replication applier connects to a different master
/// than before.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_MASTER_CHANGE                               (1404)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1405: ERROR_REPLICATION_LOOP
///
/// loop detected
///
/// Will be raised when the replication applier is asked to connect to itself
/// for replication.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_LOOP                                        (1405)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1406: ERROR_REPLICATION_UNEXPECTED_MARKER
///
/// unexpected marker
///
/// Will be raised when an unexpected marker is found in the replication log
/// stream.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_UNEXPECTED_MARKER                           (1406)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1407: ERROR_REPLICATION_INVALID_APPLIER_STATE
///
/// invalid applier state
///
/// Will be raised when an invalid replication applier state file is found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE                       (1407)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1408: ERROR_REPLICATION_UNEXPECTED_TRANSACTION
///
/// invalid transaction
///
/// Will be raised when an unexpected transaction id is found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION                      (1408)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1410: ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION
///
/// invalid replication applier configuration
///
/// Will be raised when the configuration for the replication applier is
/// invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION               (1410)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1411: ERROR_REPLICATION_RUNNING
///
/// cannot perform operation while applier is running
///
/// Will be raised when there is an attempt to perform an operation while the
/// replication applier is running.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_RUNNING                                     (1411)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1412: ERROR_REPLICATION_APPLIER_STOPPED
///
/// replication stopped
///
/// Special error code used to indicate the replication applier was stopped by
/// a user.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_APPLIER_STOPPED                             (1412)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1413: ERROR_REPLICATION_NO_START_TICK
///
/// no start tick
///
/// Will be raised when the replication applier is started without a known
/// start tick value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_NO_START_TICK                               (1413)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1414: ERROR_REPLICATION_START_TICK_NOT_PRESENT
///
/// start tick not present
///
/// Will be raised when the replication applier fetches data using a start
/// tick, but that start tick is not present on the logger server anymore.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT                      (1414)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1415: ERROR_REPLICATION_WRONG_CHECKSUM_FORMAT
///
/// the checksum format is wrong
///
///  "Will be raised when the format of the checksum is wrong")
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_WRONG_CHECKSUM_FORMAT                       (1415)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1416: ERROR_REPLICATION_WRONG_CHECKSUM
///
/// wrong checksum
///
/// Will be raised when a new born follower submits a wrong checksum
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_REPLICATION_WRONG_CHECKSUM                              (1416)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1450: ERROR_CLUSTER_NO_AGENCY
///
/// could not connect to agency
///
/// Will be raised when none of the agency servers can be connected to.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_NO_AGENCY                                       (1450)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1451: ERROR_CLUSTER_NO_COORDINATOR_HEADER
///
/// missing coordinator header
///
/// Will be raised when a DB server in a cluster receives a HTTP request
/// without a coordinator header.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_NO_COORDINATOR_HEADER                           (1451)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1452: ERROR_CLUSTER_COULD_NOT_LOCK_PLAN
///
/// could not lock plan in agency
///
/// Will be raised when a coordinator in a cluster cannot lock the Plan
/// hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN                             (1452)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1453: ERROR_CLUSTER_COLLECTION_ID_EXISTS
///
/// collection ID already exists
///
/// Will be raised when a coordinator in a cluster tries to create a collection
/// and the collection ID already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS                            (1453)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1454: ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN
///
/// could not create collection in plan
///
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new collection in the Plan hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN             (1454)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1455: ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION
///
/// could not read version in current in agency
///
/// Will be raised when a coordinator in a cluster cannot read the Version
/// entry in the Current hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION                  (1455)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1456: ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION
///
/// could not create collection
///
/// Will be raised when a coordinator in a cluster notices that some DBServers
/// report problems when creating shards for a new collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION                     (1456)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1457: ERROR_CLUSTER_TIMEOUT
///
/// timeout in cluster operation
///
/// Will be raised when a coordinator in a cluster runs into a timeout for some
/// cluster wide operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_TIMEOUT                                         (1457)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1458: ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN
///
/// could not remove collection from plan
///
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// collection in the Plan hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN             (1458)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1459: ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT
///
/// could not remove collection from current
///
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// collection in the Current hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT          (1459)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1460: ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN
///
/// could not create database in plan
///
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new database in the Plan hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN               (1460)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1461: ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE
///
/// could not create database
///
/// Will be raised when a coordinator in a cluster notices that some DBServers
/// report problems when creating databases for a new cluster wide database.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE                       (1461)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1462: ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN
///
/// could not remove database from plan
///
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// database in the Plan hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN               (1462)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1463: ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT
///
/// could not remove database from current
///
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// database in the Current hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT            (1463)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1464: ERROR_CLUSTER_SHARD_GONE
///
/// no responsible shard found
///
/// Will be raised when a coordinator in a cluster cannot determine the shard
/// that is responsible for a given document.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_SHARD_GONE                                      (1464)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1465: ERROR_CLUSTER_CONNECTION_LOST
///
/// cluster internal HTTP connection broken
///
/// Will be raised when a coordinator in a cluster loses an HTTP connection to
/// a DBserver in the cluster whilst transferring data.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_CONNECTION_LOST                                 (1465)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1466: ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY
///
/// must not specify _key for this collection
///
/// Will be raised when a coordinator in a cluster finds that the _key
/// attribute was specified in a sharded collection the uses not only _key as
/// sharding attribute.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY                            (1466)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1467: ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS
///
/// got contradicting answers from different shards
///
/// Will be raised if a coordinator in a cluster gets conflicting results from
/// different shards, which should never happen.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS                       (1467)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1468: ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN
///
/// not all sharding attributes given
///
/// Will be raised if a coordinator tries to find out which shard is
/// responsible for a partial document, but cannot do this because not all
/// sharding attributes are specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN               (1468)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1469: ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES
///
/// must not change the value of a shard key attribute
///
/// Will be raised if there is an attempt to update the value of a shard
/// attribute.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES             (1469)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1470: ERROR_CLUSTER_UNSUPPORTED
///
/// unsupported operation or parameter
///
/// Will be raised when there is an attempt to carry out an operation that is
/// not supported in the context of a sharded collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_UNSUPPORTED                                     (1470)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1471: ERROR_CLUSTER_ONLY_ON_COORDINATOR
///
/// this operation is only valid on a coordinator in a cluster
///
/// Will be raised if there is an attempt to run a coordinator-only operation
/// on a different type of node.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR                             (1471)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1472: ERROR_CLUSTER_READING_PLAN_AGENCY
///
/// error reading Plan in agency
///
/// Will be raised if a coordinator or DBserver cannot read the Plan in the
/// agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_READING_PLAN_AGENCY                             (1472)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1473: ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION
///
/// could not truncate collection
///
/// Will be raised if a coordinator cannot truncate all shards of a cluster
/// collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION                   (1473)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1474: ERROR_CLUSTER_AQL_COMMUNICATION
///
/// error in cluster internal communication for AQL
///
/// Will be raised if the internal communication of the cluster for AQL
/// produces an error.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_AQL_COMMUNICATION                               (1474)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1475: ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED
///
/// document not found or sharding attributes changed
///
/// Will be raised when a document with a given identifier or handle is
/// unknown, or if the sharding attributes have been changed in a REPLACE
/// operation in the cluster.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED (1475)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1476: ERROR_CLUSTER_COULD_NOT_DETERMINE_ID
///
/// could not determine my ID from my local info
///
/// Will be raised if a cluster server at startup could not determine its own
/// ID from the local info provided.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_DETERMINE_ID                          (1476)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1477: ERROR_CLUSTER_ONLY_ON_DBSERVER
///
/// this operation is only valid on a DBserver in a cluster
///
/// Will be raised if there is an attempt to run a DBserver-only operation on a
/// different type of node.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER                                (1477)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1478: ERROR_CLUSTER_BACKEND_UNAVAILABLE
///
/// A cluster backend which was required for the operation could not be reached
///
/// Will be raised if a required db server can't be reached.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE                             (1478)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1479: ERROR_CLUSTER_UNKNOWN_CALLBACK_ENDPOINT
///
/// An endpoint couldn't be found
///
/// An endpoint couldn't be found
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_UNKNOWN_CALLBACK_ENDPOINT                       (1479)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1480: ERROR_CLUSTER_AGENCY_STRUCTURE_INVALID
///
/// Invalid agency structure
///
/// The structure in the agency is invalid
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_AGENCY_STRUCTURE_INVALID                        (1480)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1481: ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC
///
/// collection is out of sync
///
/// Will be raised if a collection needed during query execution is out of
/// sync. This currently can only happen when using satellite collections
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC                      (1481)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1482: ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN
///
/// could not create index in plan
///
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new index in the Plan hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN                  (1482)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1483: ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN
///
/// could not drop index in plan
///
/// Will be raised when a coordinator in a cluster cannot remove an index from
/// the Plan hierarchy in the agency.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN                    (1483)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1484: ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE
///
/// chain of distributeShardsLike references
///
/// Will be raised if one tries to create a collection with a
/// distributeShardsLike attribute which points to another collection that also
/// has one.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE                   (1484)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1485: ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE
///
/// must not drop collection while another has a distributeShardsLike attribute
/// pointing to it
///
/// Will be raised if one tries to drop a collection to which another
/// collection points with its distributeShardsLike attribute.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE   (1485)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1486: ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE
///
/// must not have a distributeShardsLike attribute pointing to an unknown
/// collection
///
/// Will be raised if one tries to create a collection which points to an
/// unknown collection in its distributeShardsLike attribute.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE                    (1486)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1487: ERROR_CLUSTER_INSUFFICIENT_DBSERVERS
///
/// the number of current dbservers is lower than the requested
/// replicationFactor
///
/// Will be raised if one tries to create a collection with a replicationFactor
/// greater than the available number of DBServers.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS                          (1487)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1488: ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER
///
/// a follower could not be dropped in agency
///
/// Will be raised if a follower that ought to be dropped could not be dropped
/// in the agency (under Current).
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER                         (1488)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1489: ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION
///
/// a shard leader refuses to perform a replication operation
///
/// Will be raised if a replication operation is refused by a shard leader.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION                (1489)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1490: ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION
///
/// a shard follower refuses to perform an operation that is not a replication
///
/// Will be raised if a non-replication operation is refused by a shard
/// follower.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION                (1490)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1491: ERROR_CLUSTER_SHARD_LEADER_RESIGNED
///
/// a (former) shard leader refuses to perform an operation, because it has
/// resigned in the meantime
///
/// Will be raised if a non-replication operation is refused by a former shard
/// leader that has found out that it is no longer the leader.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED                           (1491)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1492: ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED
///
/// some agency operation failed
///
/// Will be raised if after various retries an agency operation could not be
/// performed successfully.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED                     (1492)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1493: ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_REPLICATION_FACTOR
///
/// conflicting replication factor with distributeShardsLike parameter
/// assignment
///
/// Will be raised if intended replication factor does not match that of the
/// prototype shard given in ditributeShardsLike parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_REPLICATION_FACTOR       (1493)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1494: ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_NUMBER_OF_SHARDS
///
/// conflicting shard number with distributeShardsLike parameter assignment
///
/// Will be raised if intended number of shards does not match that of the
/// prototype shard given in ditributeShardsLike parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_NUMBER_OF_SHARDS         (1494)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1500: ERROR_QUERY_KILLED
///
/// query killed
///
/// Will be raised when a running query is killed by an explicit admin command.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_KILLED                                            (1500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1501: ERROR_QUERY_PARSE
///
/// %s
///
/// Will be raised when query is parsed and is found to be syntactically
/// invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_PARSE                                             (1501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1502: ERROR_QUERY_EMPTY
///
/// query is empty
///
/// Will be raised when an empty query is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_EMPTY                                             (1502)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1503: ERROR_QUERY_SCRIPT
///
/// runtime error '%s'
///
/// Will be raised when a runtime error is caused by the query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_SCRIPT                                            (1503)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1504: ERROR_QUERY_NUMBER_OUT_OF_RANGE
///
/// number out of range
///
/// Will be raised when a number is outside the expected range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               (1504)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1510: ERROR_QUERY_VARIABLE_NAME_INVALID
///
/// variable name '%s' has an invalid format
///
/// Will be raised when an invalid variable name is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_VARIABLE_NAME_INVALID                             (1510)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1511: ERROR_QUERY_VARIABLE_REDECLARED
///
/// variable '%s' is assigned multiple times
///
/// Will be raised when a variable gets re-assigned in a query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_VARIABLE_REDECLARED                               (1511)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1512: ERROR_QUERY_VARIABLE_NAME_UNKNOWN
///
/// unknown variable '%s'
///
/// Will be raised when an unknown variable is used or the variable is
/// undefined the context it is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN                             (1512)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1521: ERROR_QUERY_COLLECTION_LOCK_FAILED
///
/// unable to read-lock collection %s
///
/// Will be raised when a read lock on the collection cannot be acquired.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED                            (1521)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1522: ERROR_QUERY_TOO_MANY_COLLECTIONS
///
/// too many collections
///
/// Will be raised when the number of collections in a query is beyond the
/// allowed value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS                              (1522)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1530: ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED
///
/// document attribute '%s' is assigned multiple times
///
/// Will be raised when a document attribute is re-assigned.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED                     (1530)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1540: ERROR_QUERY_FUNCTION_NAME_UNKNOWN
///
/// usage of unknown function '%s()'
///
/// Will be raised when an undefined function is called.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN                             (1540)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1541: ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH
///
/// invalid number of arguments for function '%s()', expected number of
/// arguments: minimum: %d, maximum: %d
///
/// Will be raised when the number of arguments used in a function call does
/// not match the expected number of arguments for the function.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH                 (1541)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1542: ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH
///
/// invalid argument type in call to function '%s()'
///
/// Will be raised when the type of an argument used in a function call does
/// not match the expected argument type.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH                   (1542)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1543: ERROR_QUERY_INVALID_REGEX
///
/// invalid regex value
///
/// Will be raised when an invalid regex argument value is used in a call to a
/// function that expects a regex.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_INVALID_REGEX                                     (1543)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1550: ERROR_QUERY_BIND_PARAMETERS_INVALID
///
/// invalid structure of bind parameters
///
/// Will be raised when the structure of bind parameters passed has an
/// unexpected format.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID                           (1550)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1551: ERROR_QUERY_BIND_PARAMETER_MISSING
///
/// no value specified for declared bind parameter '%s'
///
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            (1551)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1552: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
///
/// bind parameter '%s' was not declared in the query
///
/// Will be raised when a value gets specified for an undeclared bind parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         (1552)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1553: ERROR_QUERY_BIND_PARAMETER_TYPE
///
/// bind parameter '%s' has an invalid value or type
///
/// Will be raised when a bind parameter has an invalid value or type.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_TYPE                               (1553)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1560: ERROR_QUERY_INVALID_LOGICAL_VALUE
///
/// invalid logical value
///
/// Will be raised when a non-boolean value is used in a logical operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE                             (1560)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1561: ERROR_QUERY_INVALID_ARITHMETIC_VALUE
///
/// invalid arithmetic value
///
/// Will be raised when a non-numeric value is used in an arithmetic operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE                          (1561)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1562: ERROR_QUERY_DIVISION_BY_ZERO
///
/// division by zero
///
/// Will be raised when there is an attempt to divide by zero.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_DIVISION_BY_ZERO                                  (1562)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1563: ERROR_QUERY_ARRAY_EXPECTED
///
/// array expected
///
/// Will be raised when a non-array operand is used for an operation that
/// expects an array argument operand.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_ARRAY_EXPECTED                                    (1563)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1569: ERROR_QUERY_FAIL_CALLED
///
/// FAIL(%s) called
///
/// Will be raised when the function FAIL() is called from inside a query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FAIL_CALLED                                       (1569)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1570: ERROR_QUERY_GEO_INDEX_MISSING
///
/// no suitable geo index found for geo restriction on '%s'
///
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 (1570)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1571: ERROR_QUERY_FULLTEXT_INDEX_MISSING
///
/// no suitable fulltext index found for fulltext query on '%s'
///
/// Will be raised when a fulltext query is performed on a collection without a
/// suitable fulltext index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING                            (1571)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1572: ERROR_QUERY_INVALID_DATE_VALUE
///
/// invalid date value
///
/// Will be raised when a value cannot be converted to a date.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_INVALID_DATE_VALUE                                (1572)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1573: ERROR_QUERY_MULTI_MODIFY
///
/// multi-modify query
///
/// Will be raised when an AQL query contains more than one data-modifying
/// operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_MULTI_MODIFY                                      (1573)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1574: ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION
///
/// invalid aggregate expression
///
/// Will be raised when an AQL query contains an invalid aggregate expression.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION                      (1574)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1575: ERROR_QUERY_COMPILE_TIME_OPTIONS
///
/// query options must be readable at query compile time
///
/// Will be raised when an AQL data-modification query contains options that
/// cannot be figured out at query compile time.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS                              (1575)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1576: ERROR_QUERY_EXCEPTION_OPTIONS
///
/// query options expected
///
/// Will be raised when an AQL data-modification query contains an invalid
/// options specification.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_EXCEPTION_OPTIONS                                 (1576)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1577: ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION
///
/// collection '%s' used as expression operand
///
/// Will be raised when a collection is used as an operand in an AQL expression.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION                     (1577)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1578: ERROR_QUERY_DISALLOWED_DYNAMIC_CALL
///
/// disallowed dynamic call to '%s'
///
/// Will be raised when a dynamic function call is made to a function that
/// cannot be called dynamically.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL                           (1578)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1579: ERROR_QUERY_ACCESS_AFTER_MODIFICATION
///
/// access after data-modification by %s
///
/// Will be raised when collection data are accessed after a data-modification
/// operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION                         (1579)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1580: ERROR_QUERY_FUNCTION_INVALID_NAME
///
/// invalid user function name
///
/// Will be raised when a user function with an invalid name is registered.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_INVALID_NAME                             (1580)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1581: ERROR_QUERY_FUNCTION_INVALID_CODE
///
/// invalid user function code
///
/// Will be raised when a user function is registered with invalid code.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_INVALID_CODE                             (1581)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1582: ERROR_QUERY_FUNCTION_NOT_FOUND
///
/// user function '%s()' not found
///
/// Will be raised when a user function is accessed but not found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_NOT_FOUND                                (1582)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1583: ERROR_QUERY_FUNCTION_RUNTIME_ERROR
///
/// user function runtime error: %s
///
/// Will be raised when a user function throws a runtime exception.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_RUNTIME_ERROR                            (1583)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1590: ERROR_QUERY_BAD_JSON_PLAN
///
/// bad execution plan JSON
///
/// Will be raised when an HTTP API for a query got an invalid JSON object.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BAD_JSON_PLAN                                     (1590)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1591: ERROR_QUERY_NOT_FOUND
///
/// query ID not found
///
/// Will be raised when an Id of a query is not found by the HTTP API.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_NOT_FOUND                                         (1591)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1592: ERROR_QUERY_IN_USE
///
/// query with this ID is in use
///
/// Will be raised when an Id of a query is found by the HTTP API but the query
/// is in use.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_IN_USE                                            (1592)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1600: ERROR_CURSOR_NOT_FOUND
///
/// cursor not found
///
/// Will be raised when a cursor is requested via its id but a cursor with that
/// id cannot be found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CURSOR_NOT_FOUND                                        (1600)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1601: ERROR_CURSOR_BUSY
///
/// cursor is busy
///
/// Will be raised when a cursor is requested via its id but a concurrent
/// request is still using the cursor.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CURSOR_BUSY                                             (1601)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1650: ERROR_TRANSACTION_INTERNAL
///
/// internal transaction error
///
/// Will be raised when a wrong usage of transactions is detected. this is an
/// internal error and indicates a bug in ArangoDB.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_INTERNAL                                    (1650)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1651: ERROR_TRANSACTION_NESTED
///
/// nested transactions detected
///
/// Will be raised when transactions are nested.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_NESTED                                      (1651)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1652: ERROR_TRANSACTION_UNREGISTERED_COLLECTION
///
/// unregistered collection used in transaction
///
/// Will be raised when a collection is used in the middle of a transaction but
/// was not registered at transaction start.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION                     (1652)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1653: ERROR_TRANSACTION_DISALLOWED_OPERATION
///
/// disallowed operation inside transaction
///
/// Will be raised when a disallowed operation is carried out in a transaction.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION                        (1653)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1654: ERROR_TRANSACTION_ABORTED
///
/// transaction aborted
///
/// Will be raised when a transaction was aborted.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_ABORTED                                     (1654)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1700: ERROR_USER_INVALID_NAME
///
/// invalid user name
///
/// Will be raised when an invalid user name is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_INVALID_NAME                                       (1700)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1701: ERROR_USER_INVALID_PASSWORD
///
/// invalid password
///
/// Will be raised when an invalid password is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_INVALID_PASSWORD                                   (1701)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1702: ERROR_USER_DUPLICATE
///
/// duplicate user
///
/// Will be raised when a user name already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_DUPLICATE                                          (1702)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1703: ERROR_USER_NOT_FOUND
///
/// user not found
///
/// Will be raised when a user name is updated that does not exist.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_NOT_FOUND                                          (1703)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1704: ERROR_USER_CHANGE_PASSWORD
///
/// user must change his password
///
/// Will be raised when the user must change his password.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_CHANGE_PASSWORD                                    (1704)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1750: ERROR_SERVICE_INVALID_NAME
///
/// invalid service name
///
/// Will be raised when an invalid service name is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_INVALID_NAME                                    (1750)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1751: ERROR_SERVICE_INVALID_MOUNT
///
/// invalid mount
///
/// Will be raised when an invalid mount is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_INVALID_MOUNT                                   (1751)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1752: ERROR_SERVICE_DOWNLOAD_FAILED
///
/// service download failed
///
/// Will be raised when a service download from the central repository failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_DOWNLOAD_FAILED                                 (1752)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1753: ERROR_SERVICE_UPLOAD_FAILED
///
/// service upload failed
///
/// Will be raised when a service upload from the client to the ArangoDB server
/// failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_UPLOAD_FAILED                                   (1753)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1800: ERROR_LDAP_CANNOT_INIT
///
/// cannot init a LDAP connection
///
/// can not init a LDAP connection
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_CANNOT_INIT                                        (1800)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1801: ERROR_LDAP_CANNOT_SET_OPTION
///
/// cannot set a LDAP option
///
/// can not set a LDAP option
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_CANNOT_SET_OPTION                                  (1801)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1802: ERROR_LDAP_CANNOT_BIND
///
/// cannot bind to a LDAP server
///
/// can not bind to a LDAP server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_CANNOT_BIND                                        (1802)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1803: ERROR_LDAP_CANNOT_UNBIND
///
/// cannot unbind from a LDAP server
///
/// can not unbind from a LDAP server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_CANNOT_UNBIND                                      (1803)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1804: ERROR_LDAP_CANNOT_SEARCH
///
/// cannot issue a LDAP search
///
/// can not search the LDAP server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_CANNOT_SEARCH                                      (1804)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1805: ERROR_LDAP_CANNOT_START_TLS
///
/// cannot start a TLS LDAP session
///
/// can not star a TLS LDAP session
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_CANNOT_START_TLS                                   (1805)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1806: ERROR_LDAP_FOUND_NO_OBJECTS
///
/// LDAP didn't found any objects
///
/// LDAP didn't found any objects with the specified search query
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_FOUND_NO_OBJECTS                                   (1806)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1807: ERROR_LDAP_NOT_ONE_USER_FOUND
///
/// LDAP found zero ore more than one user
///
/// LDAP found zero ore more than one user
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_NOT_ONE_USER_FOUND                                 (1807)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1808: ERROR_LDAP_USER_NOT_IDENTIFIED
///
/// LDAP found a user, but its not the desired one
///
/// LDAP found a user, but its not the desired one
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_USER_NOT_IDENTIFIED                                (1808)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1820: ERROR_LDAP_INVALID_MODE
///
/// invalid ldap mode
///
/// cant distinguish a valid mode for provided ldap configuration
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LDAP_INVALID_MODE                                       (1820)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1850: ERROR_TASK_INVALID_ID
///
/// invalid task id
///
/// Will be raised when a task is created with an invalid id.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TASK_INVALID_ID                                         (1850)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1851: ERROR_TASK_DUPLICATE_ID
///
/// duplicate task id
///
/// Will be raised when a task id is created with a duplicate id.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TASK_DUPLICATE_ID                                       (1851)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1852: ERROR_TASK_NOT_FOUND
///
/// task not found
///
/// Will be raised when a task with the specified id could not be found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TASK_NOT_FOUND                                          (1852)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1901: ERROR_GRAPH_INVALID_GRAPH
///
/// invalid graph
///
/// Will be raised when an invalid name is passed to the server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_GRAPH                                     (1901)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1902: ERROR_GRAPH_COULD_NOT_CREATE_GRAPH
///
/// could not create graph
///
/// Will be raised when an invalid name, vertices or edges is passed to the
/// server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CREATE_GRAPH                            (1902)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1903: ERROR_GRAPH_INVALID_VERTEX
///
/// invalid vertex
///
/// Will be raised when an invalid vertex id is passed to the server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_VERTEX                                    (1903)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1904: ERROR_GRAPH_COULD_NOT_CREATE_VERTEX
///
/// could not create vertex
///
/// Will be raised when the vertex could not be created.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CREATE_VERTEX                           (1904)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1905: ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX
///
/// could not change vertex
///
/// Will be raised when the vertex could not be changed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX                           (1905)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1906: ERROR_GRAPH_INVALID_EDGE
///
/// invalid edge
///
/// Will be raised when an invalid edge id is passed to the server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_EDGE                                      (1906)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1907: ERROR_GRAPH_COULD_NOT_CREATE_EDGE
///
/// could not create edge
///
/// Will be raised when the edge could not be created.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CREATE_EDGE                             (1907)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1908: ERROR_GRAPH_COULD_NOT_CHANGE_EDGE
///
/// could not change edge
///
/// Will be raised when the edge could not be changed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CHANGE_EDGE                             (1908)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1909: ERROR_GRAPH_TOO_MANY_ITERATIONS
///
/// too many iterations - try increasing the value of 'maxIterations'
///
/// Will be raised when too many iterations are done in a graph traversal.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_TOO_MANY_ITERATIONS                               (1909)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1910: ERROR_GRAPH_INVALID_FILTER_RESULT
///
/// invalid filter result
///
/// Will be raised when an invalid filter result is returned in a graph
/// traversal.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_FILTER_RESULT                             (1910)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1920: ERROR_GRAPH_COLLECTION_MULTI_USE
///
/// multi use of edge collection in edge def
///
/// an edge collection may only be used once in one edge definition of a graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COLLECTION_MULTI_USE                              (1920)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1921: ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS
///
/// edge collection already used in edge def
///
///  is already used by another graph in a different edge definition.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS                    (1921)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1922: ERROR_GRAPH_CREATE_MISSING_NAME
///
/// missing graph name
///
/// a graph name is required to create a graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_CREATE_MISSING_NAME                               (1922)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1923: ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION
///
/// malformed edge definition
///
/// the edge definition is malformed. It has to be an array of objects.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION                  (1923)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1924: ERROR_GRAPH_NOT_FOUND
///
/// graph not found
///
/// a graph with this name could not be found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_NOT_FOUND                                         (1924)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1925: ERROR_GRAPH_DUPLICATE
///
/// graph already exists
///
/// a graph with this name already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_DUPLICATE                                         (1925)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1926: ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST
///
/// vertex collection does not exist or is not part of the graph
///
/// the specified vertex collection does not exist or is not part of the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST                         (1926)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1927: ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX
///
/// not a vertex collection
///
/// the collection is not a vertex collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX                      (1927)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1928: ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION
///
/// not in orphan collection
///
/// Vertex collection not in orphan collection of the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION                          (1928)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1929: ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF
///
/// collection already used in edge def
///
/// The collection is already used in an edge definition of the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF                       (1929)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1930: ERROR_GRAPH_EDGE_COLLECTION_NOT_USED
///
/// edge collection not used in graph
///
/// The edge collection is not used in any edge definition of the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED                          (1930)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1931: ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION
///
///  is not an ArangoCollection
///
/// The collection is not an ArangoCollection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION                          (1931)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1932: ERROR_GRAPH_NO_GRAPH_COLLECTION
///
/// collection _graphs does not exist
///
/// collection _graphs does not exist.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION                               (1932)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1933: ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING
///
/// Invalid example type. Has to be String, Array or Object
///
/// Invalid example type. Has to be String, Array or Object.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING               (1933)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1934: ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT
///
/// Invalid example type. Has to be Array or Object
///
/// Invalid example type. Has to be Array or Object.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT                      (1934)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1935: ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS
///
/// Invalid number of arguments. Expected: 
///
/// Invalid number of arguments. Expected: 
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS                       (1935)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1936: ERROR_GRAPH_INVALID_PARAMETER
///
/// Invalid parameter type.
///
/// Invalid parameter type.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_PARAMETER                                 (1936)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1937: ERROR_GRAPH_INVALID_ID
///
/// Invalid id
///
/// Invalid id
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_ID                                        (1937)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1938: ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS
///
/// collection used in orphans
///
/// The collection is already used in the orphans of the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS                        (1938)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1939: ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST
///
/// edge collection does not exist or is not part of the graph
///
/// the specified edge collection does not exist or is not part of the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST                           (1939)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1940: ERROR_GRAPH_EMPTY
///
/// empty graph
///
/// The requested graph has no edge collections.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_EMPTY                                             (1940)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1950: ERROR_SESSION_UNKNOWN
///
/// unknown session
///
/// Will be raised when an invalid/unknown session id is passed to the server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_UNKNOWN                                         (1950)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1951: ERROR_SESSION_EXPIRED
///
/// session expired
///
/// Will be raised when a session is expired.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_EXPIRED                                         (1951)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2000: SIMPLE_CLIENT_UNKNOWN_ERROR
///
/// unknown client error
///
/// This error should not happen.
////////////////////////////////////////////////////////////////////////////////

#define TRI_SIMPLE_CLIENT_UNKNOWN_ERROR                                   (2000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2001: SIMPLE_CLIENT_COULD_NOT_CONNECT
///
/// could not connect to server
///
/// Will be raised when the client could not connect to the server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT                               (2001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2002: SIMPLE_CLIENT_COULD_NOT_WRITE
///
/// could not write to server
///
/// Will be raised when the client could not write data.
////////////////////////////////////////////////////////////////////////////////

#define TRI_SIMPLE_CLIENT_COULD_NOT_WRITE                                 (2002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2003: SIMPLE_CLIENT_COULD_NOT_READ
///
/// could not read from server
///
/// Will be raised when the client could not read data.
////////////////////////////////////////////////////////////////////////////////

#define TRI_SIMPLE_CLIENT_COULD_NOT_READ                                  (2003)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2100: COMMUNICATOR_REQUEST_ABORTED
///
/// Request aborted
///
/// Request was aborted.
////////////////////////////////////////////////////////////////////////////////

#define TRI_COMMUNICATOR_REQUEST_ABORTED                                  (2100)

////////////////////////////////////////////////////////////////////////////////
/// @brief 2101: COMMUNICATOR_DISABLED
///
/// Communication was disabled
///
/// Communication was disabled.
////////////////////////////////////////////////////////////////////////////////

#define TRI_COMMUNICATOR_DISABLED                                         (2101)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3000: ERROR_MALFORMED_MANIFEST_FILE
///
/// failed to parse manifest file
///
/// The service manifest file is not well-formed JSON.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_MALFORMED_MANIFEST_FILE                                 (3000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3001: ERROR_INVALID_SERVICE_MANIFEST
///
/// manifest file is invalid
///
/// The service manifest contains invalid values.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_INVALID_SERVICE_MANIFEST                                (3001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3002: ERROR_SERVICE_FILES_MISSING
///
/// service files missing
///
/// The service folder or bundle does not exist on this server.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_FILES_MISSING                                   (3002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3003: ERROR_SERVICE_FILES_OUTDATED
///
/// service files outdated
///
/// The local service bundle does not match the checksum in the database.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_FILES_OUTDATED                                  (3003)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3004: ERROR_INVALID_FOXX_OPTIONS
///
/// service options are invalid
///
/// The service options contain invalid values.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_INVALID_FOXX_OPTIONS                                    (3004)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3007: ERROR_INVALID_MOUNTPOINT
///
/// invalid mountpath
///
/// The service mountpath contains invalid characters.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_INVALID_MOUNTPOINT                                      (3007)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3009: ERROR_SERVICE_NOT_FOUND
///
/// service not found
///
/// No service found at the given mountpath.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_NOT_FOUND                                       (3009)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3010: ERROR_SERVICE_NEEDS_CONFIGURATION
///
/// service needs configuration
///
/// The service is missing configuration or dependencies.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_NEEDS_CONFIGURATION                             (3010)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3011: ERROR_SERVICE_MOUNTPOINT_CONFLICT
///
/// service already exists
///
/// A service already exists at the given mountpath.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_MOUNTPOINT_CONFLICT                             (3011)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3012: ERROR_SERVICE_MANIFEST_NOT_FOUND
///
/// missing manifest file
///
/// The service directory does not contain a manifest file.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_MANIFEST_NOT_FOUND                              (3012)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3013: ERROR_SERVICE_OPTIONS_MALFORMED
///
/// failed to parse service options
///
/// The service options are not well-formed JSON.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_OPTIONS_MALFORMED                               (3013)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3014: ERROR_SERVICE_SOURCE_NOT_FOUND
///
/// source path not found
///
/// The source path does not match a file or directory.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_SOURCE_NOT_FOUND                                (3014)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3015: ERROR_SERVICE_SOURCE_ERROR
///
/// error resolving source
///
/// The source path could not be resolved.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_SOURCE_ERROR                                    (3015)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3016: ERROR_SERVICE_UNKNOWN_SCRIPT
///
/// unknown script
///
/// The service does not have a script with this name.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SERVICE_UNKNOWN_SCRIPT                                  (3016)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3100: ERROR_MODULE_NOT_FOUND
///
/// cannot locate module
///
/// The module path could not be resolved.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_MODULE_NOT_FOUND                                        (3100)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3101: ERROR_MODULE_SYNTAX_ERROR
///
/// syntax error in module
///
/// The module could not be parsed because of a syntax error.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_MODULE_SYNTAX_ERROR                                     (3101)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3103: ERROR_MODULE_FAILURE
///
/// failed to invoke module
///
/// Failed to invoke the module in its context.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_MODULE_FAILURE                                          (3103)

////////////////////////////////////////////////////////////////////////////////
/// @brief 4000: ERROR_NO_SMART_COLLECTION
///
/// collection is not smart
///
/// The requested collection needs to be smart, but it ain't
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_NO_SMART_COLLECTION                                     (4000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 4001: ERROR_NO_SMART_GRAPH_ATTRIBUTE
///
/// smart graph attribute not given
///
/// The given document does not have the smart graph attribute set.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_NO_SMART_GRAPH_ATTRIBUTE                                (4001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 4002: ERROR_CANNOT_DROP_SMART_COLLECTION
///
/// cannot drop this smart collection
///
/// This smart collection cannot be dropped, it dictates sharding in the graph.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_CANNOT_DROP_SMART_COLLECTION                            (4002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 4003: ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE
///
/// in smart vertex collections _key must be prefixed with the value of the
/// smart graph attribute
///
/// In a smart vertex collection _key must be prefixed with the value of the
/// smart graph attribute.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE         (4003)

////////////////////////////////////////////////////////////////////////////////
/// @brief 4004: ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE
///
/// attribute cannot be used as smart graph attribute
///
/// The given smartGraph attribute is illegal and connot be used for sharding.
/// All system attributes are forbidden.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE                           (4004)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20001: ERROR_AGENCY_INQUIRY_SYNTAX
///
/// Illegal inquiry syntax
///
/// Inquiry handles a list of string clientIds: [<clientId>,...].
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INQUIRY_SYNTAX                                   (20001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20011: ERROR_AGENCY_INFORM_MUST_BE_OBJECT
///
/// Inform message must be an object.
///
/// The inform message in the agency must be an object.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT                            (20011)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20012: ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM
///
/// Inform message must contain uint parameter 'term'
///
/// The inform message in the agency must contain a uint parameter 'term'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM                         (20012)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20013: ERROR_AGENCY_INFORM_MUST_CONTAIN_ID
///
/// Inform message must contain string parameter 'id'
///
/// The inform message in the agency must contain a string parameter 'id'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ID                           (20013)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20014: ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE
///
/// Inform message must contain array 'active'
///
/// The inform message in the agency must contain an array 'active'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE                       (20014)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20015: ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL
///
/// Inform message must contain object 'pool'
///
/// The inform message in the agency must contain an object 'pool'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL                         (20015)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20016: ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING
///
/// Inform message must contain object 'min ping'
///
/// The inform message in the agency must contain an object 'min ping'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING                     (20016)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20017: ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING
///
/// Inform message must contain object 'max ping'
///
/// The inform message in the agency must contain an object 'max ping'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING                     (20017)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20018: ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT
///
/// Inform message must contain object 'timeoutMult'
///
/// The inform message in the agency must contain an object 'timeoutMult'.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT                 (20018)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20020: ERROR_AGENCY_INQUIRE_CLIENT_ID_MUST_BE_STRING
///
/// Inquiry failed
///
/// Inquiry by clientId failed
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_INQUIRE_CLIENT_ID_MUST_BE_STRING                 (20020)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20021: ERROR_AGENCY_CANNOT_REBUILD_DBS
///
/// Cannot rebuild readDB and spearHead
///
/// Will be raised if the readDB or the spearHead cannot be rebuilt from the
/// replicated log.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS                               (20021)

////////////////////////////////////////////////////////////////////////////////
/// @brief 20501: ERROR_SUPERVISION_GENERAL_FAILURE
///
/// general supervision failure
///
/// General supervision failure.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SUPERVISION_GENERAL_FAILURE                             (20501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 21001: ERROR_DISPATCHER_IS_STOPPING
///
/// dispatcher stopped
///
/// Will be returned if a shutdown is in progress.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_DISPATCHER_IS_STOPPING                                  (21001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 21002: ERROR_QUEUE_UNKNOWN
///
/// named queue does not exist
///
/// Will be returned if a queue with this name does not exist.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUEUE_UNKNOWN                                           (21002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 21003: ERROR_QUEUE_FULL
///
/// named queue is full
///
/// Will be returned if a queue with this name is full.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUEUE_FULL                                              (21003)

#endif

