#ifndef ARANGODB_BASICS_VOC_ERRORS_H
#define ARANGODB_BASICS_VOC_ERRORS_H 1

/// Error codes and meanings
/// The following errors might be raised when running ArangoDB:


/// 0: ERROR_NO_ERROR
/// "no error"
/// No error has occurred.
constexpr int TRI_ERROR_NO_ERROR                                                = 0;

/// 1: ERROR_FAILED
/// "failed"
/// Will be raised when a general error occurred.
constexpr int TRI_ERROR_FAILED                                                  = 1;

/// 2: ERROR_SYS_ERROR
/// "system error"
/// Will be raised when operating system error occurred.
constexpr int TRI_ERROR_SYS_ERROR                                               = 2;

/// 3: ERROR_OUT_OF_MEMORY
/// "out of memory"
/// Will be raised when there is a memory shortage.
constexpr int TRI_ERROR_OUT_OF_MEMORY                                           = 3;

/// 4: ERROR_INTERNAL
/// "internal error"
/// Will be raised when an internal error occurred.
constexpr int TRI_ERROR_INTERNAL                                                = 4;

/// 5: ERROR_ILLEGAL_NUMBER
/// "illegal number"
/// Will be raised when an illegal representation of a number was given.
constexpr int TRI_ERROR_ILLEGAL_NUMBER                                          = 5;

/// 6: ERROR_NUMERIC_OVERFLOW
/// "numeric overflow"
/// Will be raised when a numeric overflow occurred.
constexpr int TRI_ERROR_NUMERIC_OVERFLOW                                        = 6;

/// 7: ERROR_ILLEGAL_OPTION
/// "illegal option"
/// Will be raised when an unknown option was supplied by the user.
constexpr int TRI_ERROR_ILLEGAL_OPTION                                          = 7;

/// 8: ERROR_DEAD_PID
/// "dead process identifier"
/// Will be raised when a PID without a living process was found.
constexpr int TRI_ERROR_DEAD_PID                                                = 8;

/// 9: ERROR_NOT_IMPLEMENTED
/// "not implemented"
/// Will be raised when hitting an unimplemented feature.
constexpr int TRI_ERROR_NOT_IMPLEMENTED                                         = 9;

/// 10: ERROR_BAD_PARAMETER
/// "bad parameter"
/// Will be raised when the parameter does not fulfill the requirements.
constexpr int TRI_ERROR_BAD_PARAMETER                                           = 10;

/// 11: ERROR_FORBIDDEN
/// "forbidden"
/// Will be raised when you are missing permission for the operation.
constexpr int TRI_ERROR_FORBIDDEN                                               = 11;

/// 12: ERROR_OUT_OF_MEMORY_MMAP
/// "out of memory in mmap"
/// Will be raised when there is a memory shortage.
constexpr int TRI_ERROR_OUT_OF_MEMORY_MMAP                                      = 12;

/// 13: ERROR_CORRUPTED_CSV
/// "csv is corrupt"
/// Will be raised when encountering a corrupt csv line.
constexpr int TRI_ERROR_CORRUPTED_CSV                                           = 13;

/// 14: ERROR_FILE_NOT_FOUND
/// "file not found"
/// Will be raised when a file is not found.
constexpr int TRI_ERROR_FILE_NOT_FOUND                                          = 14;

/// 15: ERROR_CANNOT_WRITE_FILE
/// "cannot write file"
/// Will be raised when a file cannot be written.
constexpr int TRI_ERROR_CANNOT_WRITE_FILE                                       = 15;

/// 16: ERROR_CANNOT_OVERWRITE_FILE
/// "cannot overwrite file"
/// Will be raised when an attempt is made to overwrite an existing file.
constexpr int TRI_ERROR_CANNOT_OVERWRITE_FILE                                   = 16;

/// 17: ERROR_TYPE_ERROR
/// "type error"
/// Will be raised when a type error is encountered.
constexpr int TRI_ERROR_TYPE_ERROR                                              = 17;

/// 18: ERROR_LOCK_TIMEOUT
/// "lock timeout"
/// Will be raised when there's a timeout waiting for a lock.
constexpr int TRI_ERROR_LOCK_TIMEOUT                                            = 18;

/// 19: ERROR_CANNOT_CREATE_DIRECTORY
/// "cannot create directory"
/// Will be raised when an attempt to create a directory fails.
constexpr int TRI_ERROR_CANNOT_CREATE_DIRECTORY                                 = 19;

/// 20: ERROR_CANNOT_CREATE_TEMP_FILE
/// "cannot create temporary file"
/// Will be raised when an attempt to create a temporary file fails.
constexpr int TRI_ERROR_CANNOT_CREATE_TEMP_FILE                                 = 20;

/// 21: ERROR_REQUEST_CANCELED
/// "canceled request"
/// Will be raised when a request is canceled by the user.
constexpr int TRI_ERROR_REQUEST_CANCELED                                        = 21;

/// 22: ERROR_DEBUG
/// "intentional debug error"
/// Will be raised intentionally during debugging.
constexpr int TRI_ERROR_DEBUG                                                   = 22;

/// 25: ERROR_IP_ADDRESS_INVALID
/// "IP address is invalid"
/// Will be raised when the structure of an IP address is invalid.
constexpr int TRI_ERROR_IP_ADDRESS_INVALID                                      = 25;

/// 27: ERROR_FILE_EXISTS
/// "file exists"
/// Will be raised when a file already exists.
constexpr int TRI_ERROR_FILE_EXISTS                                             = 27;

/// 28: ERROR_LOCKED
/// "locked"
/// Will be raised when a resource or an operation is locked.
constexpr int TRI_ERROR_LOCKED                                                  = 28;

/// 29: ERROR_DEADLOCK
/// "deadlock detected"
/// Will be raised when a deadlock is detected when accessing collections.
constexpr int TRI_ERROR_DEADLOCK                                                = 29;

/// 30: ERROR_SHUTTING_DOWN
/// "shutdown in progress"
/// Will be raised when a call cannot succeed because a server shutdown is
/// already in progress.
constexpr int TRI_ERROR_SHUTTING_DOWN                                           = 30;

/// 31: ERROR_ONLY_ENTERPRISE
/// "only enterprise version"
/// Will be raised when an enterprise-feature is requested from the community
/// edition.
constexpr int TRI_ERROR_ONLY_ENTERPRISE                                         = 31;

/// 32: ERROR_RESOURCE_LIMIT
/// "resource limit exceeded"
/// Will be raised when the resources used by an operation exceed the
/// configured maximum value.
constexpr int TRI_ERROR_RESOURCE_LIMIT                                          = 32;

/// 33: ERROR_ARANGO_ICU_ERROR
/// "icu error: %s"
/// will be raised if icu operations failed
constexpr int TRI_ERROR_ARANGO_ICU_ERROR                                        = 33;

/// 34: ERROR_CANNOT_READ_FILE
/// "cannot read file"
/// Will be raised when a file cannot be read.
constexpr int TRI_ERROR_CANNOT_READ_FILE                                        = 34;

/// 35: ERROR_INCOMPATIBLE_VERSION
/// "incompatible server version"
/// Will be raised when a server is running an incompatible version of ArangoDB.
constexpr int TRI_ERROR_INCOMPATIBLE_VERSION                                    = 35;

/// 36: ERROR_DISABLED
/// "disabled"
/// Will be raised when a requested resource is not enabled.
constexpr int TRI_ERROR_DISABLED                                                = 36;

/// 400: ERROR_HTTP_BAD_PARAMETER
/// "bad parameter"
/// Will be raised when the HTTP request does not fulfill the requirements.
constexpr int TRI_ERROR_HTTP_BAD_PARAMETER                                      = 400;

/// 401: ERROR_HTTP_UNAUTHORIZED
/// "unauthorized"
/// Will be raised when authorization is required but the user is not
/// authorized.
constexpr int TRI_ERROR_HTTP_UNAUTHORIZED                                       = 401;

/// 403: ERROR_HTTP_FORBIDDEN
/// "forbidden"
/// Will be raised when the operation is forbidden.
constexpr int TRI_ERROR_HTTP_FORBIDDEN                                          = 403;

/// 404: ERROR_HTTP_NOT_FOUND
/// "not found"
/// Will be raised when an URI is unknown.
constexpr int TRI_ERROR_HTTP_NOT_FOUND                                          = 404;

/// 405: ERROR_HTTP_METHOD_NOT_ALLOWED
/// "method not supported"
/// Will be raised when an unsupported HTTP method is used for an operation.
constexpr int TRI_ERROR_HTTP_METHOD_NOT_ALLOWED                                 = 405;

/// 406: ERROR_HTTP_NOT_ACCEPTABLE
/// "request not acceptable"
/// Will be raised when an unsupported HTTP content type is used for an
/// operation, or if a request is not acceptable for a leader or follower.
constexpr int TRI_ERROR_HTTP_NOT_ACCEPTABLE                                     = 406;

/// 412: ERROR_HTTP_PRECONDITION_FAILED
/// "precondition failed"
/// Will be raised when a precondition for an HTTP request is not met.
constexpr int TRI_ERROR_HTTP_PRECONDITION_FAILED                                = 412;

/// 500: ERROR_HTTP_SERVER_ERROR
/// "internal server error"
/// Will be raised when an internal server is encountered.
constexpr int TRI_ERROR_HTTP_SERVER_ERROR                                       = 500;

/// 503: ERROR_HTTP_SERVICE_UNAVAILABLE
/// "service unavailable"
/// Will be raised when a service is temporarily unavailable.
constexpr int TRI_ERROR_HTTP_SERVICE_UNAVAILABLE                                = 503;

/// 504: ERROR_HTTP_GATEWAY_TIMEOUT
/// "gateway timeout"
/// Will be raised when a service contacted by ArangoDB does not respond in a
/// timely manner.
constexpr int TRI_ERROR_HTTP_GATEWAY_TIMEOUT                                    = 504;

/// 600: ERROR_HTTP_CORRUPTED_JSON
/// "invalid JSON object"
/// Will be raised when a string representation of a JSON object is corrupt.
constexpr int TRI_ERROR_HTTP_CORRUPTED_JSON                                     = 600;

/// 601: ERROR_HTTP_SUPERFLUOUS_SUFFICES
/// "superfluous URL suffices"
/// Will be raised when the URL contains superfluous suffices.
constexpr int TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES                               = 601;

/// 1000: ERROR_ARANGO_ILLEGAL_STATE
/// "illegal state"
/// Internal error that will be raised when the datafile is not in the required
/// state.
constexpr int TRI_ERROR_ARANGO_ILLEGAL_STATE                                    = 1000;

/// 1002: ERROR_ARANGO_DATAFILE_SEALED
/// "datafile sealed"
/// Internal error that will be raised when trying to write to a datafile.
constexpr int TRI_ERROR_ARANGO_DATAFILE_SEALED                                  = 1002;

/// 1004: ERROR_ARANGO_READ_ONLY
/// "read only"
/// Internal error that will be raised when trying to write to a read-only
/// datafile or collection.
constexpr int TRI_ERROR_ARANGO_READ_ONLY                                        = 1004;

/// 1005: ERROR_ARANGO_DUPLICATE_IDENTIFIER
/// "duplicate identifier"
/// Internal error that will be raised when a identifier duplicate is detected.
constexpr int TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER                             = 1005;

/// 1006: ERROR_ARANGO_DATAFILE_UNREADABLE
/// "datafile unreadable"
/// Internal error that will be raised when a datafile is unreadable.
constexpr int TRI_ERROR_ARANGO_DATAFILE_UNREADABLE                              = 1006;

/// 1007: ERROR_ARANGO_DATAFILE_EMPTY
/// "datafile empty"
/// Internal error that will be raised when a datafile is empty.
constexpr int TRI_ERROR_ARANGO_DATAFILE_EMPTY                                   = 1007;

/// 1008: ERROR_ARANGO_RECOVERY
/// "logfile recovery error"
/// Will be raised when an error occurred during WAL log file recovery.
constexpr int TRI_ERROR_ARANGO_RECOVERY                                         = 1008;

/// 1009: ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND
/// "datafile statistics not found"
/// Will be raised when a required datafile statistics object was not found.
constexpr int TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND                    = 1009;

/// 1100: ERROR_ARANGO_CORRUPTED_DATAFILE
/// "corrupted datafile"
/// Will be raised when a corruption is detected in a datafile.
constexpr int TRI_ERROR_ARANGO_CORRUPTED_DATAFILE                               = 1100;

/// 1101: ERROR_ARANGO_ILLEGAL_PARAMETER_FILE
/// "illegal or unreadable parameter file"
/// Will be raised if a parameter file is corrupted or cannot be read.
constexpr int TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE                           = 1101;

/// 1102: ERROR_ARANGO_CORRUPTED_COLLECTION
/// "corrupted collection"
/// Will be raised when a collection contains one or more corrupted data files.
constexpr int TRI_ERROR_ARANGO_CORRUPTED_COLLECTION                             = 1102;

/// 1103: ERROR_ARANGO_MMAP_FAILED
/// "mmap failed"
/// Will be raised when the system call mmap failed.
constexpr int TRI_ERROR_ARANGO_MMAP_FAILED                                      = 1103;

/// 1104: ERROR_ARANGO_FILESYSTEM_FULL
/// "filesystem full"
/// Will be raised when the filesystem is full.
constexpr int TRI_ERROR_ARANGO_FILESYSTEM_FULL                                  = 1104;

/// 1105: ERROR_ARANGO_NO_JOURNAL
/// "no journal"
/// Will be raised when a journal cannot be created.
constexpr int TRI_ERROR_ARANGO_NO_JOURNAL                                       = 1105;

/// 1106: ERROR_ARANGO_DATAFILE_ALREADY_EXISTS
/// "cannot create/rename datafile because it already exists"
/// Will be raised when the datafile cannot be created or renamed because a
/// file of the same name already exists.
constexpr int TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS                          = 1106;

/// 1107: ERROR_ARANGO_DATADIR_LOCKED
/// "database directory is locked"
/// Will be raised when the database directory is locked by a different process.
constexpr int TRI_ERROR_ARANGO_DATADIR_LOCKED                                   = 1107;

/// 1108: ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS
/// "cannot create/rename collection because directory already exists"
/// Will be raised when the collection cannot be created because a directory of
/// the same name already exists.
constexpr int TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS              = 1108;

/// 1109: ERROR_ARANGO_MSYNC_FAILED
/// "msync failed"
/// Will be raised when the system call msync failed.
constexpr int TRI_ERROR_ARANGO_MSYNC_FAILED                                     = 1109;

/// 1110: ERROR_ARANGO_DATADIR_UNLOCKABLE
/// "cannot lock database directory"
/// Will be raised when the server cannot lock the database directory on
/// startup.
constexpr int TRI_ERROR_ARANGO_DATADIR_UNLOCKABLE                               = 1110;

/// 1111: ERROR_ARANGO_SYNC_TIMEOUT
/// "sync timeout"
/// Will be raised when the server waited too long for a datafile to be synced
/// to disk.
constexpr int TRI_ERROR_ARANGO_SYNC_TIMEOUT                                     = 1111;

/// 1200: ERROR_ARANGO_CONFLICT
/// "conflict"
/// Will be raised when updating or deleting a document and a conflict has been
/// detected.
constexpr int TRI_ERROR_ARANGO_CONFLICT                                         = 1200;

/// 1201: ERROR_ARANGO_DATADIR_INVALID
/// "invalid database directory"
/// Will be raised when a non-existing database directory was specified when
/// starting the database.
constexpr int TRI_ERROR_ARANGO_DATADIR_INVALID                                  = 1201;

/// 1202: ERROR_ARANGO_DOCUMENT_NOT_FOUND
/// "document not found"
/// Will be raised when a document with a given identifier or handle is unknown.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND                               = 1202;

/// 1203: ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
/// "collection or view not found"
/// Will be raised when a collection with the given identifier or name is
/// unknown.
constexpr int TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND                            = 1203;

/// 1204: ERROR_ARANGO_COLLECTION_PARAMETER_MISSING
/// "parameter 'collection' not found"
/// Will be raised when the collection parameter is missing.
constexpr int TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING                     = 1204;

/// 1205: ERROR_ARANGO_DOCUMENT_HANDLE_BAD
/// "illegal document handle"
/// Will be raised when a document handle is corrupt.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD                              = 1205;

/// 1206: ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL
/// "maximal size of journal too small"
/// Will be raised when the maximal size of the journal is too small.
constexpr int TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL                           = 1206;

/// 1207: ERROR_ARANGO_DUPLICATE_NAME
/// "duplicate name"
/// Will be raised when a name duplicate is detected.
constexpr int TRI_ERROR_ARANGO_DUPLICATE_NAME                                   = 1207;

/// 1208: ERROR_ARANGO_ILLEGAL_NAME
/// "illegal name"
/// Will be raised when an illegal name is detected.
constexpr int TRI_ERROR_ARANGO_ILLEGAL_NAME                                     = 1208;

/// 1209: ERROR_ARANGO_NO_INDEX
/// "no suitable index known"
/// Will be raised when no suitable index for the query is known.
constexpr int TRI_ERROR_ARANGO_NO_INDEX                                         = 1209;

/// 1210: ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED
/// "unique constraint violated"
/// Will be raised when there is a unique constraint violation.
constexpr int TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED                       = 1210;

/// 1212: ERROR_ARANGO_INDEX_NOT_FOUND
/// "index not found"
/// Will be raised when an index with a given identifier is unknown.
constexpr int TRI_ERROR_ARANGO_INDEX_NOT_FOUND                                  = 1212;

/// 1213: ERROR_ARANGO_CROSS_COLLECTION_REQUEST
/// "cross collection request not allowed"
/// Will be raised when a cross-collection is requested.
constexpr int TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST                         = 1213;

/// 1214: ERROR_ARANGO_INDEX_HANDLE_BAD
/// "illegal index handle"
/// Will be raised when a index handle is corrupt.
constexpr int TRI_ERROR_ARANGO_INDEX_HANDLE_BAD                                 = 1214;

/// 1216: ERROR_ARANGO_DOCUMENT_TOO_LARGE
/// "document too large"
/// Will be raised when the document cannot fit into any datafile because of it
/// is too large.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE                               = 1216;

/// 1217: ERROR_ARANGO_COLLECTION_NOT_UNLOADED
/// "collection must be unloaded"
/// Will be raised when a collection should be unloaded, but has a different
/// status.
constexpr int TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED                          = 1217;

/// 1218: ERROR_ARANGO_COLLECTION_TYPE_INVALID
/// "collection type invalid"
/// Will be raised when an invalid collection type is used in a request.
constexpr int TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID                          = 1218;

/// 1219: ERROR_ARANGO_VALIDATION_FAILED
/// "validator failed"
/// Will be raised when the validation of an attribute of a structure failed.
constexpr int TRI_ERROR_ARANGO_VALIDATION_FAILED                                = 1219;

/// 1220: ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED
/// "parsing attribute name definition failed"
/// Will be raised when parsing an attribute name definition failed.
constexpr int TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED                          = 1220;

/// 1221: ERROR_ARANGO_DOCUMENT_KEY_BAD
/// "illegal document key"
/// Will be raised when a document key is corrupt.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD                                 = 1221;

/// 1222: ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED
/// "unexpected document key"
/// Will be raised when a user-defined document key is supplied for collections
/// with auto key generation.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED                          = 1222;

/// 1224: ERROR_ARANGO_DATADIR_NOT_WRITABLE
/// "server database directory not writable"
/// Will be raised when the server's database directory is not writable for the
/// current user.
constexpr int TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE                             = 1224;

/// 1225: ERROR_ARANGO_OUT_OF_KEYS
/// "out of keys"
/// Will be raised when a key generator runs out of keys.
constexpr int TRI_ERROR_ARANGO_OUT_OF_KEYS                                      = 1225;

/// 1226: ERROR_ARANGO_DOCUMENT_KEY_MISSING
/// "missing document key"
/// Will be raised when a document key is missing.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING                             = 1226;

/// 1227: ERROR_ARANGO_DOCUMENT_TYPE_INVALID
/// "invalid document type"
/// Will be raised when there is an attempt to create a document with an
/// invalid type.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID                            = 1227;

/// 1228: ERROR_ARANGO_DATABASE_NOT_FOUND
/// "database not found"
/// Will be raised when a non-existing database is accessed.
constexpr int TRI_ERROR_ARANGO_DATABASE_NOT_FOUND                               = 1228;

/// 1229: ERROR_ARANGO_DATABASE_NAME_INVALID
/// "database name invalid"
/// Will be raised when an invalid database name is used.
constexpr int TRI_ERROR_ARANGO_DATABASE_NAME_INVALID                            = 1229;

/// 1230: ERROR_ARANGO_USE_SYSTEM_DATABASE
/// "operation only allowed in system database"
/// Will be raised when an operation is requested in a database other than the
/// system database.
constexpr int TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE                              = 1230;

/// 1231: ERROR_ARANGO_ENDPOINT_NOT_FOUND
/// "endpoint not found"
/// Will be raised when there is an attempt to delete a non-existing endpoint.
constexpr int TRI_ERROR_ARANGO_ENDPOINT_NOT_FOUND                               = 1231;

/// 1232: ERROR_ARANGO_INVALID_KEY_GENERATOR
/// "invalid key generator"
/// Will be raised when an invalid key generator description is used.
constexpr int TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR                            = 1232;

/// 1233: ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE
/// "edge attribute missing or invalid"
/// will be raised when the _from or _to values of an edge are undefined or
/// contain an invalid value.
constexpr int TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE                           = 1233;

/// 1234: ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING
/// "index insertion warning - attribute missing in document"
/// Will be raised when an attempt to insert a document into an index is caused
/// by in the document not having one or more attributes which the index is
/// built on.
constexpr int TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING                 = 1234;

/// 1235: ERROR_ARANGO_INDEX_CREATION_FAILED
/// "index creation failed"
/// Will be raised when an attempt to create an index has failed.
constexpr int TRI_ERROR_ARANGO_INDEX_CREATION_FAILED                            = 1235;

/// 1236: ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT
/// "write-throttling timeout"
/// Will be raised when the server is write-throttled and a write operation has
/// waited too long for the server to process queued operations.
constexpr int TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT                           = 1236;

/// 1237: ERROR_ARANGO_COLLECTION_TYPE_MISMATCH
/// "collection type mismatch"
/// Will be raised when a collection has a different type from what has been
/// expected.
constexpr int TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH                         = 1237;

/// 1238: ERROR_ARANGO_COLLECTION_NOT_LOADED
/// "collection not loaded"
/// Will be raised when a collection is accessed that is not yet loaded.
constexpr int TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED                            = 1238;

/// 1239: ERROR_ARANGO_DOCUMENT_REV_BAD
/// "illegal document revision"
/// Will be raised when a document revision is corrupt or is missing where
/// needed.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_REV_BAD                                 = 1239;

/// 1300: ERROR_ARANGO_DATAFILE_FULL
/// "datafile full"
/// Will be raised when the datafile reaches its limit.
constexpr int TRI_ERROR_ARANGO_DATAFILE_FULL                                    = 1300;

/// 1301: ERROR_ARANGO_EMPTY_DATADIR
/// "server database directory is empty"
/// Will be raised when encountering an empty server database directory.
constexpr int TRI_ERROR_ARANGO_EMPTY_DATADIR                                    = 1301;

/// 1302: ERROR_ARANGO_TRY_AGAIN
/// "operation should be tried again"
/// Will be raised when an operation should be retried.
constexpr int TRI_ERROR_ARANGO_TRY_AGAIN                                        = 1302;

/// 1303: ERROR_ARANGO_BUSY
/// "engine is busy"
/// Will be raised when storage engine is busy.
constexpr int TRI_ERROR_ARANGO_BUSY                                             = 1303;

/// 1304: ERROR_ARANGO_MERGE_IN_PROGRESS
/// "merge in progress"
/// Will be raised when storage engine has a datafile merge in progress and
/// cannot complete the operation.
constexpr int TRI_ERROR_ARANGO_MERGE_IN_PROGRESS                                = 1304;

/// 1305: ERROR_ARANGO_IO_ERROR
/// "storage engine I/O error"
/// Will be raised when storage engine encounters an I/O error.
constexpr int TRI_ERROR_ARANGO_IO_ERROR                                         = 1305;

/// 1400: ERROR_REPLICATION_NO_RESPONSE
/// "no response"
/// Will be raised when the replication applier does not receive any or an
/// incomplete response from the master.
constexpr int TRI_ERROR_REPLICATION_NO_RESPONSE                                 = 1400;

/// 1401: ERROR_REPLICATION_INVALID_RESPONSE
/// "invalid response"
/// Will be raised when the replication applier receives an invalid response
/// from the master.
constexpr int TRI_ERROR_REPLICATION_INVALID_RESPONSE                            = 1401;

/// 1402: ERROR_REPLICATION_MASTER_ERROR
/// "master error"
/// Will be raised when the replication applier receives a server error from
/// the master.
constexpr int TRI_ERROR_REPLICATION_MASTER_ERROR                                = 1402;

/// 1403: ERROR_REPLICATION_MASTER_INCOMPATIBLE
/// "master incompatible"
/// Will be raised when the replication applier connects to a master that has
/// an incompatible version.
constexpr int TRI_ERROR_REPLICATION_MASTER_INCOMPATIBLE                         = 1403;

/// 1404: ERROR_REPLICATION_MASTER_CHANGE
/// "master change"
/// Will be raised when the replication applier connects to a different master
/// than before.
constexpr int TRI_ERROR_REPLICATION_MASTER_CHANGE                               = 1404;

/// 1405: ERROR_REPLICATION_LOOP
/// "loop detected"
/// Will be raised when the replication applier is asked to connect to itself
/// for replication.
constexpr int TRI_ERROR_REPLICATION_LOOP                                        = 1405;

/// 1406: ERROR_REPLICATION_UNEXPECTED_MARKER
/// "unexpected marker"
/// Will be raised when an unexpected marker is found in the replication log
/// stream.
constexpr int TRI_ERROR_REPLICATION_UNEXPECTED_MARKER                           = 1406;

/// 1407: ERROR_REPLICATION_INVALID_APPLIER_STATE
/// "invalid applier state"
/// Will be raised when an invalid replication applier state file is found.
constexpr int TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE                       = 1407;

/// 1408: ERROR_REPLICATION_UNEXPECTED_TRANSACTION
/// "invalid transaction"
/// Will be raised when an unexpected transaction id is found.
constexpr int TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION                      = 1408;

/// 1410: ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION
/// "invalid replication applier configuration"
/// Will be raised when the configuration for the replication applier is
/// invalid.
constexpr int TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION               = 1410;

/// 1411: ERROR_REPLICATION_RUNNING
/// "cannot perform operation while applier is running"
/// Will be raised when there is an attempt to perform an operation while the
/// replication applier is running.
constexpr int TRI_ERROR_REPLICATION_RUNNING                                     = 1411;

/// 1412: ERROR_REPLICATION_APPLIER_STOPPED
/// "replication stopped"
/// Special error code used to indicate the replication applier was stopped by
/// a user.
constexpr int TRI_ERROR_REPLICATION_APPLIER_STOPPED                             = 1412;

/// 1413: ERROR_REPLICATION_NO_START_TICK
/// "no start tick"
/// Will be raised when the replication applier is started without a known
/// start tick value.
constexpr int TRI_ERROR_REPLICATION_NO_START_TICK                               = 1413;

/// 1414: ERROR_REPLICATION_START_TICK_NOT_PRESENT
/// "start tick not present"
/// Will be raised when the replication applier fetches data using a start
/// tick, but that start tick is not present on the logger server anymore.
constexpr int TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT                      = 1414;

/// 1416: ERROR_REPLICATION_WRONG_CHECKSUM
/// "wrong checksum"
/// Will be raised when a new born follower submits a wrong checksum
constexpr int TRI_ERROR_REPLICATION_WRONG_CHECKSUM                              = 1416;

/// 1417: ERROR_REPLICATION_SHARD_NONEMPTY
/// "shard not empty"
/// Will be raised when a shard is not empty and the follower tries a shortcut
constexpr int TRI_ERROR_REPLICATION_SHARD_NONEMPTY                              = 1417;

/// 1450: ERROR_CLUSTER_NO_AGENCY
/// "could not connect to agency"
/// Will be raised when none of the agency servers can be connected to.
constexpr int TRI_ERROR_CLUSTER_NO_AGENCY                                       = 1450;

/// 1451: ERROR_CLUSTER_NO_COORDINATOR_HEADER
/// "missing coordinator header"
/// Will be raised when a DB server in a cluster receives a HTTP request
/// without a coordinator header.
constexpr int TRI_ERROR_CLUSTER_NO_COORDINATOR_HEADER                           = 1451;

/// 1452: ERROR_CLUSTER_COULD_NOT_LOCK_PLAN
/// "could not lock plan in agency"
/// Will be raised when a coordinator in a cluster cannot lock the Plan
/// hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_LOCK_PLAN                             = 1452;

/// 1453: ERROR_CLUSTER_COLLECTION_ID_EXISTS
/// "collection ID already exists"
/// Will be raised when a coordinator in a cluster tries to create a collection
/// and the collection ID already exists.
constexpr int TRI_ERROR_CLUSTER_COLLECTION_ID_EXISTS                            = 1453;

/// 1454: ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN
/// "could not create collection in plan"
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new collection in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN             = 1454;

/// 1455: ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION
/// "could not read version in current in agency"
/// Will be raised when a coordinator in a cluster cannot read the Version
/// entry in the Current hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_READ_CURRENT_VERSION                  = 1455;

/// 1456: ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION
/// "could not create collection"
/// Will be raised when a coordinator in a cluster notices that some DBServers
/// report problems when creating shards for a new collection.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION                     = 1456;

/// 1457: ERROR_CLUSTER_TIMEOUT
/// "timeout in cluster operation"
/// Will be raised when a coordinator in a cluster runs into a timeout for some
/// cluster wide operation.
constexpr int TRI_ERROR_CLUSTER_TIMEOUT                                         = 1457;

/// 1458: ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN
/// "could not remove collection from plan"
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// collection in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN             = 1458;

/// 1459: ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT
/// "could not remove collection from current"
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// collection in the Current hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_CURRENT          = 1459;

/// 1460: ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN
/// "could not create database in plan"
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new database in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN               = 1460;

/// 1461: ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE
/// "could not create database"
/// Will be raised when a coordinator in a cluster notices that some DBServers
/// report problems when creating databases for a new cluster wide database.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE                       = 1461;

/// 1462: ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN
/// "could not remove database from plan"
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// database in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN               = 1462;

/// 1463: ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT
/// "could not remove database from current"
/// Will be raised when a coordinator in a cluster cannot remove an entry for a
/// database in the Current hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT            = 1463;

/// 1464: ERROR_CLUSTER_SHARD_GONE
/// "no responsible shard found"
/// Will be raised when a coordinator in a cluster cannot determine the shard
/// that is responsible for a given document.
constexpr int TRI_ERROR_CLUSTER_SHARD_GONE                                      = 1464;

/// 1465: ERROR_CLUSTER_CONNECTION_LOST
/// "cluster internal HTTP connection broken"
/// Will be raised when a coordinator in a cluster loses an HTTP connection to
/// a DBserver in the cluster whilst transferring data.
constexpr int TRI_ERROR_CLUSTER_CONNECTION_LOST                                 = 1465;

/// 1466: ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY
/// "must not specify _key for this collection"
/// Will be raised when a coordinator in a cluster finds that the _key
/// attribute was specified in a sharded collection the uses not only _key as
/// sharding attribute.
constexpr int TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY                            = 1466;

/// 1467: ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS
/// "got contradicting answers from different shards"
/// Will be raised if a coordinator in a cluster gets conflicting results from
/// different shards, which should never happen.
constexpr int TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS                       = 1467;

/// 1468: ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN
/// "not all sharding attributes given"
/// Will be raised if a coordinator tries to find out which shard is
/// responsible for a partial document, but cannot do this because not all
/// sharding attributes are specified.
constexpr int TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN               = 1468;

/// 1469: ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES
/// "must not change the value of a shard key attribute"
/// Will be raised if there is an attempt to update the value of a shard
/// attribute.
constexpr int TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES             = 1469;

/// 1470: ERROR_CLUSTER_UNSUPPORTED
/// "unsupported operation or parameter"
/// Will be raised when there is an attempt to carry out an operation that is
/// not supported in the context of a sharded collection.
constexpr int TRI_ERROR_CLUSTER_UNSUPPORTED                                     = 1470;

/// 1471: ERROR_CLUSTER_ONLY_ON_COORDINATOR
/// "this operation is only valid on a coordinator in a cluster"
/// Will be raised if there is an attempt to run a coordinator-only operation
/// on a different type of node.
constexpr int TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR                             = 1471;

/// 1472: ERROR_CLUSTER_READING_PLAN_AGENCY
/// "error reading Plan in agency"
/// Will be raised if a coordinator or DBserver cannot read the Plan in the
/// agency.
constexpr int TRI_ERROR_CLUSTER_READING_PLAN_AGENCY                             = 1472;

/// 1473: ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION
/// "could not truncate collection"
/// Will be raised if a coordinator cannot truncate all shards of a cluster
/// collection.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION                   = 1473;

/// 1474: ERROR_CLUSTER_AQL_COMMUNICATION
/// "error in cluster internal communication for AQL"
/// Will be raised if the internal communication of the cluster for AQL
/// produces an error.
constexpr int TRI_ERROR_CLUSTER_AQL_COMMUNICATION                               = 1474;

/// 1475: ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED
/// "document not found or sharding attributes changed"
/// Will be raised when a document with a given identifier or handle is
/// unknown, or if the sharding attributes have been changed in a REPLACE
/// operation in the cluster.
constexpr int TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND_OR_SHARDING_ATTRIBUTES_CHANGED = 1475;

/// 1476: ERROR_CLUSTER_COULD_NOT_DETERMINE_ID
/// "could not determine my ID from my local info"
/// Will be raised if a cluster server at startup could not determine its own
/// ID from the local info provided.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_DETERMINE_ID                          = 1476;

/// 1477: ERROR_CLUSTER_ONLY_ON_DBSERVER
/// "this operation is only valid on a DBserver in a cluster"
/// Will be raised if there is an attempt to run a DBserver-only operation on a
/// different type of node.
constexpr int TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER                                = 1477;

/// 1478: ERROR_CLUSTER_BACKEND_UNAVAILABLE
/// "A cluster backend which was required for the operation could not be reached"
/// Will be raised if a required db server can't be reached.
constexpr int TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE                             = 1478;

/// 1479: ERROR_CLUSTER_UNKNOWN_CALLBACK_ENDPOINT
/// "An endpoint couldn't be found"
/// An endpoint couldn't be found
constexpr int TRI_ERROR_CLUSTER_UNKNOWN_CALLBACK_ENDPOINT                       = 1479;

/// 1480: ERROR_CLUSTER_AGENCY_STRUCTURE_INVALID
/// "Invalid agency structure"
/// The structure in the agency is invalid
constexpr int TRI_ERROR_CLUSTER_AGENCY_STRUCTURE_INVALID                        = 1480;

/// 1481: ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC
/// "collection is out of sync"
/// Will be raised if a collection needed during query execution is out of
/// sync. This currently can only happen when using satellite collections
constexpr int TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC                      = 1481;

/// 1482: ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN
/// "could not create index in plan"
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new index in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN                  = 1482;

/// 1483: ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN
/// "could not drop index in plan"
/// Will be raised when a coordinator in a cluster cannot remove an index from
/// the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN                    = 1483;

/// 1484: ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE
/// "chain of distributeShardsLike references"
/// Will be raised if one tries to create a collection with a
/// distributeShardsLike attribute which points to another collection that also
/// has one.
constexpr int TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE                   = 1484;

/// 1485: ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE
/// "must not drop collection while another has a distributeShardsLike
/// "attribute pointing to it"
/// Will be raised if one tries to drop a collection to which another
/// collection points with its distributeShardsLike attribute.
constexpr int TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE   = 1485;

/// 1486: ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE
/// "must not have a distributeShardsLike attribute pointing to an unknown
/// "collection"
/// Will be raised if one tries to create a collection which points to an
/// unknown collection in its distributeShardsLike attribute.
constexpr int TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE                    = 1486;

/// 1487: ERROR_CLUSTER_INSUFFICIENT_DBSERVERS
/// "the number of current dbservers is lower than the requested
/// "replicationFactor"
/// Will be raised if one tries to create a collection with a replicationFactor
/// greater than the available number of DBServers.
constexpr int TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS                          = 1487;

/// 1488: ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER
/// "a follower could not be dropped in agency"
/// Will be raised if a follower that ought to be dropped could not be dropped
/// in the agency (under Current).
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER                         = 1488;

/// 1489: ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION
/// "a shard leader refuses to perform a replication operation"
/// Will be raised if a replication operation is refused by a shard leader.
constexpr int TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION                = 1489;

/// 1490: ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION
/// "a shard follower refuses to perform an operation that is not a replication"
/// Will be raised if a non-replication operation is refused by a shard
/// follower.
constexpr int TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION                = 1490;

/// 1491: ERROR_CLUSTER_SHARD_LEADER_RESIGNED
/// "a (former) shard leader refuses to perform an operation, because it has
/// "resigned in the meantime"
/// Will be raised if a non-replication operation is refused by a former shard
/// leader that has found out that it is no longer the leader.
constexpr int TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED                           = 1491;

/// 1492: ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED
/// "some agency operation failed"
/// Will be raised if after various retries an agency operation could not be
/// performed successfully.
constexpr int TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED                     = 1492;

/// 1493: ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_REPLICATION_FACTOR
/// "conflicting replication factor with distributeShardsLike parameter
/// "assignment"
/// Will be raised if intended replication factor does not match that of the
/// prototype shard given in distributeShardsLike parameter.
constexpr int TRI_ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_REPLICATION_FACTOR       = 1493;

/// 1494: ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_NUMBER_OF_SHARDS
/// "conflicting shard number with distributeShardsLike parameter assignment"
/// Will be raised if intended number of shards does not match that of the
/// prototype shard given in distributeShardsLike parameter.
constexpr int TRI_ERROR_CLUSTER_DISTRIBUTE_SHARDS_LIKE_NUMBER_OF_SHARDS         = 1494;

/// 1495: ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING
/// "leadership challenge is ongoing"
/// Will be raised when servers are currently competing for leadership, and the
/// result is still unknown.
constexpr int TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING                    = 1495;

/// 1496: ERROR_CLUSTER_NOT_LEADER
/// "not a leader"
/// Will be raised when an operation is sent to a non-leading server.
constexpr int TRI_ERROR_CLUSTER_NOT_LEADER                                      = 1496;

/// 1497: ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN
/// "could not create view in plan"
/// Will be raised when a coordinator in a cluster cannot create an entry for a
/// new view in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN                   = 1497;

/// 1498: ERROR_CLUSTER_VIEW_ID_EXISTS
/// "view ID already exists"
/// Will be raised when a coordinator in a cluster tries to create a view and
/// the view ID already exists.
constexpr int TRI_ERROR_CLUSTER_VIEW_ID_EXISTS                                  = 1498;

/// 1499: ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION
/// "could not drop collection in plan"
/// Will be raised when a coordinator in a cluster cannot drop a collection
/// entry in the Plan hierarchy in the agency.
constexpr int TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION                       = 1499;

/// 1500: ERROR_QUERY_KILLED
/// "query killed"
/// Will be raised when a running query is killed by an explicit admin command.
constexpr int TRI_ERROR_QUERY_KILLED                                            = 1500;

/// 1501: ERROR_QUERY_PARSE
/// "%s"
/// Will be raised when query is parsed and is found to be syntactically
/// invalid.
constexpr int TRI_ERROR_QUERY_PARSE                                             = 1501;

/// 1502: ERROR_QUERY_EMPTY
/// "query is empty"
/// Will be raised when an empty query is specified.
constexpr int TRI_ERROR_QUERY_EMPTY                                             = 1502;

/// 1503: ERROR_QUERY_SCRIPT
/// "runtime error '%s'"
/// Will be raised when a runtime error is caused by the query.
constexpr int TRI_ERROR_QUERY_SCRIPT                                            = 1503;

/// 1504: ERROR_QUERY_NUMBER_OUT_OF_RANGE
/// "number out of range"
/// Will be raised when a number is outside the expected range.
constexpr int TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               = 1504;

/// 1505: ERROR_QUERY_INVALID_GEO_VALUE
/// "invalid geo coordinate value"
/// Will be raised when a geo index coordinate is invalid or out of range.
constexpr int TRI_ERROR_QUERY_INVALID_GEO_VALUE                                 = 1505;

/// 1510: ERROR_QUERY_VARIABLE_NAME_INVALID
/// "variable name '%s' has an invalid format"
/// Will be raised when an invalid variable name is used.
constexpr int TRI_ERROR_QUERY_VARIABLE_NAME_INVALID                             = 1510;

/// 1511: ERROR_QUERY_VARIABLE_REDECLARED
/// "variable '%s' is assigned multiple times"
/// Will be raised when a variable gets re-assigned in a query.
constexpr int TRI_ERROR_QUERY_VARIABLE_REDECLARED                               = 1511;

/// 1512: ERROR_QUERY_VARIABLE_NAME_UNKNOWN
/// "unknown variable '%s'"
/// Will be raised when an unknown variable is used or the variable is
/// undefined the context it is used.
constexpr int TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN                             = 1512;

/// 1521: ERROR_QUERY_COLLECTION_LOCK_FAILED
/// "unable to read-lock collection %s"
/// Will be raised when a read lock on the collection cannot be acquired.
constexpr int TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED                            = 1521;

/// 1522: ERROR_QUERY_TOO_MANY_COLLECTIONS
/// "too many collections/shards"
/// Will be raised when the number of collections or shards in a query is
/// beyond the allowed value.
constexpr int TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS                              = 1522;

/// 1530: ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED
/// "document attribute '%s' is assigned multiple times"
/// Will be raised when a document attribute is re-assigned.
constexpr int TRI_ERROR_QUERY_DOCUMENT_ATTRIBUTE_REDECLARED                     = 1530;

/// 1540: ERROR_QUERY_FUNCTION_NAME_UNKNOWN
/// "usage of unknown function '%s()'"
/// Will be raised when an undefined function is called.
constexpr int TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN                             = 1540;

/// 1541: ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH
/// "invalid number of arguments for function '%s()', expected number of
/// "arguments: minimum: %d, maximum: %d"
/// Will be raised when the number of arguments used in a function call does
/// not match the expected number of arguments for the function.
constexpr int TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH                 = 1541;

/// 1542: ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH
/// "invalid argument type in call to function '%s()'"
/// Will be raised when the type of an argument used in a function call does
/// not match the expected argument type.
constexpr int TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH                   = 1542;

/// 1543: ERROR_QUERY_INVALID_REGEX
/// "invalid regex value"
/// Will be raised when an invalid regex argument value is used in a call to a
/// function that expects a regex.
constexpr int TRI_ERROR_QUERY_INVALID_REGEX                                     = 1543;

/// 1550: ERROR_QUERY_BIND_PARAMETERS_INVALID
/// "invalid structure of bind parameters"
/// Will be raised when the structure of bind parameters passed has an
/// unexpected format.
constexpr int TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID                           = 1550;

/// 1551: ERROR_QUERY_BIND_PARAMETER_MISSING
/// "no value specified for declared bind parameter '%s'"
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
constexpr int TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            = 1551;

/// 1552: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
/// "bind parameter '%s' was not declared in the query"
/// Will be raised when a value gets specified for an undeclared bind parameter.
constexpr int TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         = 1552;

/// 1553: ERROR_QUERY_BIND_PARAMETER_TYPE
/// "bind parameter '%s' has an invalid value or type"
/// Will be raised when a bind parameter has an invalid value or type.
constexpr int TRI_ERROR_QUERY_BIND_PARAMETER_TYPE                               = 1553;

/// 1560: ERROR_QUERY_INVALID_LOGICAL_VALUE
/// "invalid logical value"
/// Will be raised when a non-boolean value is used in a logical operation.
constexpr int TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE                             = 1560;

/// 1561: ERROR_QUERY_INVALID_ARITHMETIC_VALUE
/// "invalid arithmetic value"
/// Will be raised when a non-numeric value is used in an arithmetic operation.
constexpr int TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE                          = 1561;

/// 1562: ERROR_QUERY_DIVISION_BY_ZERO
/// "division by zero"
/// Will be raised when there is an attempt to divide by zero.
constexpr int TRI_ERROR_QUERY_DIVISION_BY_ZERO                                  = 1562;

/// 1563: ERROR_QUERY_ARRAY_EXPECTED
/// "array expected"
/// Will be raised when a non-array operand is used for an operation that
/// expects an array argument operand.
constexpr int TRI_ERROR_QUERY_ARRAY_EXPECTED                                    = 1563;

/// 1569: ERROR_QUERY_FAIL_CALLED
/// "FAIL(%s) called"
/// Will be raised when the function FAIL() is called from inside a query.
constexpr int TRI_ERROR_QUERY_FAIL_CALLED                                       = 1569;

/// 1570: ERROR_QUERY_GEO_INDEX_MISSING
/// "no suitable geo index found for geo restriction on '%s'"
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
constexpr int TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 = 1570;

/// 1571: ERROR_QUERY_FULLTEXT_INDEX_MISSING
/// "no suitable fulltext index found for fulltext query on '%s'"
/// Will be raised when a fulltext query is performed on a collection without a
/// suitable fulltext index.
constexpr int TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING                            = 1571;

/// 1572: ERROR_QUERY_INVALID_DATE_VALUE
/// "invalid date value"
/// Will be raised when a value cannot be converted to a date.
constexpr int TRI_ERROR_QUERY_INVALID_DATE_VALUE                                = 1572;

/// 1573: ERROR_QUERY_MULTI_MODIFY
/// "multi-modify query"
/// Will be raised when an AQL query contains more than one data-modifying
/// operation.
constexpr int TRI_ERROR_QUERY_MULTI_MODIFY                                      = 1573;

/// 1574: ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION
/// "invalid aggregate expression"
/// Will be raised when an AQL query contains an invalid aggregate expression.
constexpr int TRI_ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION                      = 1574;

/// 1575: ERROR_QUERY_COMPILE_TIME_OPTIONS
/// "query options must be readable at query compile time"
/// Will be raised when an AQL data-modification query contains options that
/// cannot be figured out at query compile time.
constexpr int TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS                              = 1575;

/// 1576: ERROR_QUERY_EXCEPTION_OPTIONS
/// "query options expected"
/// Will be raised when an AQL data-modification query contains an invalid
/// options specification.
constexpr int TRI_ERROR_QUERY_EXCEPTION_OPTIONS                                 = 1576;

/// 1578: ERROR_QUERY_DISALLOWED_DYNAMIC_CALL
/// "disallowed dynamic call to '%s'"
/// Will be raised when a dynamic function call is made to a function that
/// cannot be called dynamically.
constexpr int TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL                           = 1578;

/// 1579: ERROR_QUERY_ACCESS_AFTER_MODIFICATION
/// "access after data-modification by %s"
/// Will be raised when collection data are accessed after a data-modification
/// operation.
constexpr int TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION                         = 1579;

/// 1580: ERROR_QUERY_FUNCTION_INVALID_NAME
/// "invalid user function name"
/// Will be raised when a user function with an invalid name is registered.
constexpr int TRI_ERROR_QUERY_FUNCTION_INVALID_NAME                             = 1580;

/// 1581: ERROR_QUERY_FUNCTION_INVALID_CODE
/// "invalid user function code"
/// Will be raised when a user function is registered with invalid code.
constexpr int TRI_ERROR_QUERY_FUNCTION_INVALID_CODE                             = 1581;

/// 1582: ERROR_QUERY_FUNCTION_NOT_FOUND
/// "user function '%s()' not found"
/// Will be raised when a user function is accessed but not found.
constexpr int TRI_ERROR_QUERY_FUNCTION_NOT_FOUND                                = 1582;

/// 1583: ERROR_QUERY_FUNCTION_RUNTIME_ERROR
/// "user function runtime error: %s"
/// Will be raised when a user function throws a runtime exception.
constexpr int TRI_ERROR_QUERY_FUNCTION_RUNTIME_ERROR                            = 1583;

/// 1590: ERROR_QUERY_BAD_JSON_PLAN
/// "bad execution plan JSON"
/// Will be raised when an HTTP API for a query got an invalid JSON object.
constexpr int TRI_ERROR_QUERY_BAD_JSON_PLAN                                     = 1590;

/// 1591: ERROR_QUERY_NOT_FOUND
/// "query ID not found"
/// Will be raised when an Id of a query is not found by the HTTP API.
constexpr int TRI_ERROR_QUERY_NOT_FOUND                                         = 1591;

/// 1592: ERROR_QUERY_IN_USE
/// "query with this ID is in use"
/// Will be raised when an Id of a query is found by the HTTP API but the query
/// is in use.
constexpr int TRI_ERROR_QUERY_IN_USE                                            = 1592;

/// 1593: ERROR_QUERY_USER_ASSERT
/// "%s"
/// Will be raised if and user provided expression fails to evalutate to true
constexpr int TRI_ERROR_QUERY_USER_ASSERT                                       = 1593;

/// 1594: ERROR_QUERY_USER_WARN
/// "%s"
/// Will be raised if and user provided expression fails to evalutate to true
constexpr int TRI_ERROR_QUERY_USER_WARN                                         = 1594;

/// 1600: ERROR_CURSOR_NOT_FOUND
/// "cursor not found"
/// Will be raised when a cursor is requested via its id but a cursor with that
/// id cannot be found.
constexpr int TRI_ERROR_CURSOR_NOT_FOUND                                        = 1600;

/// 1601: ERROR_CURSOR_BUSY
/// "cursor is busy"
/// Will be raised when a cursor is requested via its id but a concurrent
/// request is still using the cursor.
constexpr int TRI_ERROR_CURSOR_BUSY                                             = 1601;

/// 1650: ERROR_TRANSACTION_INTERNAL
/// "internal transaction error"
/// Will be raised when a wrong usage of transactions is detected. this is an
/// internal error and indicates a bug in ArangoDB.
constexpr int TRI_ERROR_TRANSACTION_INTERNAL                                    = 1650;

/// 1651: ERROR_TRANSACTION_NESTED
/// "nested transactions detected"
/// Will be raised when transactions are nested.
constexpr int TRI_ERROR_TRANSACTION_NESTED                                      = 1651;

/// 1652: ERROR_TRANSACTION_UNREGISTERED_COLLECTION
/// "unregistered collection used in transaction"
/// Will be raised when a collection is used in the middle of a transaction but
/// was not registered at transaction start.
constexpr int TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION                     = 1652;

/// 1653: ERROR_TRANSACTION_DISALLOWED_OPERATION
/// "disallowed operation inside transaction"
/// Will be raised when a disallowed operation is carried out in a transaction.
constexpr int TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION                        = 1653;

/// 1654: ERROR_TRANSACTION_ABORTED
/// "transaction aborted"
/// Will be raised when a transaction was aborted.
constexpr int TRI_ERROR_TRANSACTION_ABORTED                                     = 1654;

/// 1700: ERROR_USER_INVALID_NAME
/// "invalid user name"
/// Will be raised when an invalid user name is used.
constexpr int TRI_ERROR_USER_INVALID_NAME                                       = 1700;

/// 1701: ERROR_USER_INVALID_PASSWORD
/// "invalid password"
/// Will be raised when an invalid password is used.
constexpr int TRI_ERROR_USER_INVALID_PASSWORD                                   = 1701;

/// 1702: ERROR_USER_DUPLICATE
/// "duplicate user"
/// Will be raised when a user name already exists.
constexpr int TRI_ERROR_USER_DUPLICATE                                          = 1702;

/// 1703: ERROR_USER_NOT_FOUND
/// "user not found"
/// Will be raised when a user name is updated that does not exist.
constexpr int TRI_ERROR_USER_NOT_FOUND                                          = 1703;

/// 1704: ERROR_USER_CHANGE_PASSWORD
/// "user must change his password"
/// Will be raised when the user must change his password.
constexpr int TRI_ERROR_USER_CHANGE_PASSWORD                                    = 1704;

/// 1705: ERROR_USER_EXTERNAL
/// "user is external"
/// Will be raised when the user is authenicated by an external server.
constexpr int TRI_ERROR_USER_EXTERNAL                                           = 1705;

/// 1750: ERROR_SERVICE_INVALID_NAME
/// "invalid service name"
/// Will be raised when an invalid service name is specified.
constexpr int TRI_ERROR_SERVICE_INVALID_NAME                                    = 1750;

/// 1751: ERROR_SERVICE_INVALID_MOUNT
/// "invalid mount"
/// Will be raised when an invalid mount is specified.
constexpr int TRI_ERROR_SERVICE_INVALID_MOUNT                                   = 1751;

/// 1752: ERROR_SERVICE_DOWNLOAD_FAILED
/// "service download failed"
/// Will be raised when a service download from the central repository failed.
constexpr int TRI_ERROR_SERVICE_DOWNLOAD_FAILED                                 = 1752;

/// 1753: ERROR_SERVICE_UPLOAD_FAILED
/// "service upload failed"
/// Will be raised when a service upload from the client to the ArangoDB server
/// failed.
constexpr int TRI_ERROR_SERVICE_UPLOAD_FAILED                                   = 1753;

/// 1800: ERROR_LDAP_CANNOT_INIT
/// "cannot init a LDAP connection"
/// can not init a LDAP connection
constexpr int TRI_ERROR_LDAP_CANNOT_INIT                                        = 1800;

/// 1801: ERROR_LDAP_CANNOT_SET_OPTION
/// "cannot set a LDAP option"
/// can not set a LDAP option
constexpr int TRI_ERROR_LDAP_CANNOT_SET_OPTION                                  = 1801;

/// 1802: ERROR_LDAP_CANNOT_BIND
/// "cannot bind to a LDAP server"
/// can not bind to a LDAP server
constexpr int TRI_ERROR_LDAP_CANNOT_BIND                                        = 1802;

/// 1803: ERROR_LDAP_CANNOT_UNBIND
/// "cannot unbind from a LDAP server"
/// can not unbind from a LDAP server
constexpr int TRI_ERROR_LDAP_CANNOT_UNBIND                                      = 1803;

/// 1804: ERROR_LDAP_CANNOT_SEARCH
/// "cannot issue a LDAP search"
/// can not search the LDAP server
constexpr int TRI_ERROR_LDAP_CANNOT_SEARCH                                      = 1804;

/// 1805: ERROR_LDAP_CANNOT_START_TLS
/// "cannot start a TLS LDAP session"
/// can not star a TLS LDAP session
constexpr int TRI_ERROR_LDAP_CANNOT_START_TLS                                   = 1805;

/// 1806: ERROR_LDAP_FOUND_NO_OBJECTS
/// "LDAP didn't found any objects"
/// LDAP didn't found any objects with the specified search query
constexpr int TRI_ERROR_LDAP_FOUND_NO_OBJECTS                                   = 1806;

/// 1807: ERROR_LDAP_NOT_ONE_USER_FOUND
/// "LDAP found zero ore more than one user"
/// LDAP found zero ore more than one user
constexpr int TRI_ERROR_LDAP_NOT_ONE_USER_FOUND                                 = 1807;

/// 1808: ERROR_LDAP_USER_NOT_IDENTIFIED
/// "LDAP found a user, but its not the desired one"
/// LDAP found a user, but its not the desired one
constexpr int TRI_ERROR_LDAP_USER_NOT_IDENTIFIED                                = 1808;

/// 1820: ERROR_LDAP_INVALID_MODE
/// "invalid ldap mode"
/// cant distinguish a valid mode for provided ldap configuration
constexpr int TRI_ERROR_LDAP_INVALID_MODE                                       = 1820;

/// 1850: ERROR_TASK_INVALID_ID
/// "invalid task id"
/// Will be raised when a task is created with an invalid id.
constexpr int TRI_ERROR_TASK_INVALID_ID                                         = 1850;

/// 1851: ERROR_TASK_DUPLICATE_ID
/// "duplicate task id"
/// Will be raised when a task id is created with a duplicate id.
constexpr int TRI_ERROR_TASK_DUPLICATE_ID                                       = 1851;

/// 1852: ERROR_TASK_NOT_FOUND
/// "task not found"
/// Will be raised when a task with the specified id could not be found.
constexpr int TRI_ERROR_TASK_NOT_FOUND                                          = 1852;

/// 1901: ERROR_GRAPH_INVALID_GRAPH
/// "invalid graph"
/// Will be raised when an invalid name is passed to the server.
constexpr int TRI_ERROR_GRAPH_INVALID_GRAPH                                     = 1901;

/// 1902: ERROR_GRAPH_COULD_NOT_CREATE_GRAPH
/// "could not create graph"
/// Will be raised when an invalid name, vertices or edges is passed to the
/// server.
constexpr int TRI_ERROR_GRAPH_COULD_NOT_CREATE_GRAPH                            = 1902;

/// 1903: ERROR_GRAPH_INVALID_VERTEX
/// "invalid vertex"
/// Will be raised when an invalid vertex id is passed to the server.
constexpr int TRI_ERROR_GRAPH_INVALID_VERTEX                                    = 1903;

/// 1904: ERROR_GRAPH_COULD_NOT_CREATE_VERTEX
/// "could not create vertex"
/// Will be raised when the vertex could not be created.
constexpr int TRI_ERROR_GRAPH_COULD_NOT_CREATE_VERTEX                           = 1904;

/// 1905: ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX
/// "could not change vertex"
/// Will be raised when the vertex could not be changed.
constexpr int TRI_ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX                           = 1905;

/// 1906: ERROR_GRAPH_INVALID_EDGE
/// "invalid edge"
/// Will be raised when an invalid edge id is passed to the server.
constexpr int TRI_ERROR_GRAPH_INVALID_EDGE                                      = 1906;

/// 1907: ERROR_GRAPH_COULD_NOT_CREATE_EDGE
/// "could not create edge"
/// Will be raised when the edge could not be created.
constexpr int TRI_ERROR_GRAPH_COULD_NOT_CREATE_EDGE                             = 1907;

/// 1908: ERROR_GRAPH_COULD_NOT_CHANGE_EDGE
/// "could not change edge"
/// Will be raised when the edge could not be changed.
constexpr int TRI_ERROR_GRAPH_COULD_NOT_CHANGE_EDGE                             = 1908;

/// 1909: ERROR_GRAPH_TOO_MANY_ITERATIONS
/// "too many iterations - try increasing the value of 'maxIterations'"
/// Will be raised when too many iterations are done in a graph traversal.
constexpr int TRI_ERROR_GRAPH_TOO_MANY_ITERATIONS                               = 1909;

/// 1910: ERROR_GRAPH_INVALID_FILTER_RESULT
/// "invalid filter result"
/// Will be raised when an invalid filter result is returned in a graph
/// traversal.
constexpr int TRI_ERROR_GRAPH_INVALID_FILTER_RESULT                             = 1910;

/// 1920: ERROR_GRAPH_COLLECTION_MULTI_USE
/// "multi use of edge collection in edge def"
/// an edge collection may only be used once in one edge definition of a graph.
constexpr int TRI_ERROR_GRAPH_COLLECTION_MULTI_USE                              = 1920;

/// 1921: ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS
/// "edge collection already used in edge def"
///  is already used by another graph in a different edge definition.
constexpr int TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS                    = 1921;

/// 1922: ERROR_GRAPH_CREATE_MISSING_NAME
/// "missing graph name"
/// a graph name is required to create a graph.
constexpr int TRI_ERROR_GRAPH_CREATE_MISSING_NAME                               = 1922;

/// 1923: ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION
/// "malformed edge definition"
/// the edge definition is malformed. It has to be an array of objects.
constexpr int TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION                  = 1923;

/// 1924: ERROR_GRAPH_NOT_FOUND
/// "graph '%s' not found"
/// a graph with this name could not be found.
constexpr int TRI_ERROR_GRAPH_NOT_FOUND                                         = 1924;

/// 1925: ERROR_GRAPH_DUPLICATE
/// "graph already exists"
/// a graph with this name already exists.
constexpr int TRI_ERROR_GRAPH_DUPLICATE                                         = 1925;

/// 1926: ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST
/// "vertex collection does not exist or is not part of the graph"
/// the specified vertex collection does not exist or is not part of the graph.
constexpr int TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST                         = 1926;

/// 1927: ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX
/// "not a vertex collection"
/// the collection is not a vertex collection.
constexpr int TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX                      = 1927;

/// 1928: ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION
/// "not in orphan collection"
/// Vertex collection not in orphan collection of the graph.
constexpr int TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION                          = 1928;

/// 1929: ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF
/// "collection already used in edge def"
/// The collection is already used in an edge definition of the graph.
constexpr int TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF                       = 1929;

/// 1930: ERROR_GRAPH_EDGE_COLLECTION_NOT_USED
/// "edge collection not used in graph"
/// The edge collection is not used in any edge definition of the graph.
constexpr int TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED                          = 1930;

/// 1932: ERROR_GRAPH_NO_GRAPH_COLLECTION
/// "collection _graphs does not exist"
/// collection _graphs does not exist.
constexpr int TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION                               = 1932;

/// 1933: ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING
/// "Invalid example type. Has to be String, Array or Object"
/// Invalid example type. Has to be String, Array or Object.
constexpr int TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING               = 1933;

/// 1934: ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT
/// "Invalid example type. Has to be Array or Object"
/// Invalid example type. Has to be Array or Object.
constexpr int TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT                      = 1934;

/// 1935: ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS
/// "Invalid number of arguments. Expected: "
/// Invalid number of arguments. Expected: 
constexpr int TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS                       = 1935;

/// 1936: ERROR_GRAPH_INVALID_PARAMETER
/// "Invalid parameter type."
/// Invalid parameter type.
constexpr int TRI_ERROR_GRAPH_INVALID_PARAMETER                                 = 1936;

/// 1937: ERROR_GRAPH_INVALID_ID
/// "Invalid id"
/// Invalid id
constexpr int TRI_ERROR_GRAPH_INVALID_ID                                        = 1937;

/// 1938: ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS
/// "collection used in orphans"
/// The collection is already used in the orphans of the graph.
constexpr int TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS                        = 1938;

/// 1939: ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST
/// "edge collection does not exist or is not part of the graph"
/// the specified edge collection does not exist or is not part of the graph.
constexpr int TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST                           = 1939;

/// 1940: ERROR_GRAPH_EMPTY
/// "empty graph"
/// The requested graph has no edge collections.
constexpr int TRI_ERROR_GRAPH_EMPTY                                             = 1940;

/// 1941: ERROR_GRAPH_INTERNAL_DATA_CORRUPT
/// "internal graph data corrupt"
/// The _graphs collection contains invalid data.
constexpr int TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT                             = 1941;

/// 1942: ERROR_GRAPH_INTERNAL_EDGE_COLLECTION_ALREADY_SET
/// "edge collection already set"
/// Tried to add an edge collection which is already defined.
constexpr int TRI_ERROR_GRAPH_INTERNAL_EDGE_COLLECTION_ALREADY_SET              = 1942;

/// 1943: ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST
/// "malformed orphan list"
/// the orphan list argument is malformed. It has to be an array of strings.
constexpr int TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST                      = 1943;

/// 1944: ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT
/// "edge definition collection is a document collection"
/// the collection used as a relation is existing, but is a document
/// collection, it cannot be used here.
constexpr int TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT                       = 1944;

/// 1950: ERROR_SESSION_UNKNOWN
/// "unknown session"
/// Will be raised when an invalid/unknown session id is passed to the server.
constexpr int TRI_ERROR_SESSION_UNKNOWN                                         = 1950;

/// 1951: ERROR_SESSION_EXPIRED
/// "session expired"
/// Will be raised when a session is expired.
constexpr int TRI_ERROR_SESSION_EXPIRED                                         = 1951;

/// 2000: SIMPLE_CLIENT_UNKNOWN_ERROR
/// "unknown client error"
/// This error should not happen.
constexpr int TRI_SIMPLE_CLIENT_UNKNOWN_ERROR                                   = 2000;

/// 2001: SIMPLE_CLIENT_COULD_NOT_CONNECT
/// "could not connect to server"
/// Will be raised when the client could not connect to the server.
constexpr int TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT                               = 2001;

/// 2002: SIMPLE_CLIENT_COULD_NOT_WRITE
/// "could not write to server"
/// Will be raised when the client could not write data.
constexpr int TRI_SIMPLE_CLIENT_COULD_NOT_WRITE                                 = 2002;

/// 2003: SIMPLE_CLIENT_COULD_NOT_READ
/// "could not read from server"
/// Will be raised when the client could not read data.
constexpr int TRI_SIMPLE_CLIENT_COULD_NOT_READ                                  = 2003;

/// 2100: COMMUNICATOR_REQUEST_ABORTED
/// "Request aborted"
/// Request was aborted.
constexpr int TRI_COMMUNICATOR_REQUEST_ABORTED                                  = 2100;

/// 2101: COMMUNICATOR_DISABLED
/// "Communication was disabled"
/// Communication was disabled.
constexpr int TRI_COMMUNICATOR_DISABLED                                         = 2101;

/// 3000: ERROR_MALFORMED_MANIFEST_FILE
/// "failed to parse manifest file"
/// The service manifest file is not well-formed JSON.
constexpr int TRI_ERROR_MALFORMED_MANIFEST_FILE                                 = 3000;

/// 3001: ERROR_INVALID_SERVICE_MANIFEST
/// "manifest file is invalid"
/// The service manifest contains invalid values.
constexpr int TRI_ERROR_INVALID_SERVICE_MANIFEST                                = 3001;

/// 3002: ERROR_SERVICE_FILES_MISSING
/// "service files missing"
/// The service folder or bundle does not exist on this server.
constexpr int TRI_ERROR_SERVICE_FILES_MISSING                                   = 3002;

/// 3003: ERROR_SERVICE_FILES_OUTDATED
/// "service files outdated"
/// The local service bundle does not match the checksum in the database.
constexpr int TRI_ERROR_SERVICE_FILES_OUTDATED                                  = 3003;

/// 3004: ERROR_INVALID_FOXX_OPTIONS
/// "service options are invalid"
/// The service options contain invalid values.
constexpr int TRI_ERROR_INVALID_FOXX_OPTIONS                                    = 3004;

/// 3007: ERROR_INVALID_MOUNTPOINT
/// "invalid mountpath"
/// The service mountpath contains invalid characters.
constexpr int TRI_ERROR_INVALID_MOUNTPOINT                                      = 3007;

/// 3009: ERROR_SERVICE_NOT_FOUND
/// "service not found"
/// No service found at the given mountpath.
constexpr int TRI_ERROR_SERVICE_NOT_FOUND                                       = 3009;

/// 3010: ERROR_SERVICE_NEEDS_CONFIGURATION
/// "service needs configuration"
/// The service is missing configuration or dependencies.
constexpr int TRI_ERROR_SERVICE_NEEDS_CONFIGURATION                             = 3010;

/// 3011: ERROR_SERVICE_MOUNTPOINT_CONFLICT
/// "service already exists"
/// A service already exists at the given mountpath.
constexpr int TRI_ERROR_SERVICE_MOUNTPOINT_CONFLICT                             = 3011;

/// 3012: ERROR_SERVICE_MANIFEST_NOT_FOUND
/// "missing manifest file"
/// The service directory does not contain a manifest file.
constexpr int TRI_ERROR_SERVICE_MANIFEST_NOT_FOUND                              = 3012;

/// 3013: ERROR_SERVICE_OPTIONS_MALFORMED
/// "failed to parse service options"
/// The service options are not well-formed JSON.
constexpr int TRI_ERROR_SERVICE_OPTIONS_MALFORMED                               = 3013;

/// 3014: ERROR_SERVICE_SOURCE_NOT_FOUND
/// "source path not found"
/// The source path does not match a file or directory.
constexpr int TRI_ERROR_SERVICE_SOURCE_NOT_FOUND                                = 3014;

/// 3015: ERROR_SERVICE_SOURCE_ERROR
/// "error resolving source"
/// The source path could not be resolved.
constexpr int TRI_ERROR_SERVICE_SOURCE_ERROR                                    = 3015;

/// 3016: ERROR_SERVICE_UNKNOWN_SCRIPT
/// "unknown script"
/// The service does not have a script with this name.
constexpr int TRI_ERROR_SERVICE_UNKNOWN_SCRIPT                                  = 3016;

/// 3100: ERROR_MODULE_NOT_FOUND
/// "cannot locate module"
/// The module path could not be resolved.
constexpr int TRI_ERROR_MODULE_NOT_FOUND                                        = 3100;

/// 3101: ERROR_MODULE_SYNTAX_ERROR
/// "syntax error in module"
/// The module could not be parsed because of a syntax error.
constexpr int TRI_ERROR_MODULE_SYNTAX_ERROR                                     = 3101;

/// 3103: ERROR_MODULE_FAILURE
/// "failed to invoke module"
/// Failed to invoke the module in its context.
constexpr int TRI_ERROR_MODULE_FAILURE                                          = 3103;

/// 4000: ERROR_NO_SMART_COLLECTION
/// "collection is not smart"
/// The requested collection needs to be smart, but it ain't
constexpr int TRI_ERROR_NO_SMART_COLLECTION                                     = 4000;

/// 4001: ERROR_NO_SMART_GRAPH_ATTRIBUTE
/// "smart graph attribute not given"
/// The given document does not have the smart graph attribute set.
constexpr int TRI_ERROR_NO_SMART_GRAPH_ATTRIBUTE                                = 4001;

/// 4002: ERROR_CANNOT_DROP_SMART_COLLECTION
/// "cannot drop this smart collection"
/// This smart collection cannot be dropped, it dictates sharding in the graph.
constexpr int TRI_ERROR_CANNOT_DROP_SMART_COLLECTION                            = 4002;

/// 4003: ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE
/// "in smart vertex collections _key must be prefixed with the value of the
/// "smart graph attribute"
/// In a smart vertex collection _key must be prefixed with the value of the
/// smart graph attribute.
constexpr int TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE         = 4003;

/// 4004: ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE
/// "attribute cannot be used as smart graph attribute"
/// The given smartGraph attribute is illegal and connot be used for sharding.
/// All system attributes are forbidden.
constexpr int TRI_ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE                           = 4004;

/// 4005: ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH
/// "smart graph attribute mismatch"
/// The smart graph attribute of the given collection does not match the smart
/// graph attribute of the graph.
constexpr int TRI_ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH                          = 4005;

/// 5000: ERROR_CLUSTER_REPAIRS_FAILED
/// "error during cluster repairs"
/// General error during cluster repairs
constexpr int TRI_ERROR_CLUSTER_REPAIRS_FAILED                                  = 5000;

/// 5001: ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY
/// "not enough (healthy) db servers"
/// Will be raised when, during repairDistributeShardsLike, there must be a
/// free db server to move a shard, but there is no candidate or none is
/// healthy.
constexpr int TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY                      = 5001;

/// 5002: ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED
/// "replication factor violated during cluster repairs"
/// Will be raised on various inconsistencies regarding the replication factor
constexpr int TRI_ERROR_CLUSTER_REPAIRS_REPLICATION_FACTOR_VIOLATED             = 5002;

/// 5003: ERROR_CLUSTER_REPAIRS_NO_DBSERVERS
/// "no dbservers during cluster repairs"
/// Will be raised if a collection that is fixed has some shard without DB
/// Servers
constexpr int TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS                            = 5003;

/// 5004: ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS
/// "mismatching leaders during cluster repairs"
/// Will be raised if a shard in collection and its prototype in the
/// corresponding distributeShardsLike collection have mismatching leaders
/// (when they should already have been fixed)
constexpr int TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS                     = 5004;

/// 5005: ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS
/// "mismatching followers during cluster repairs"
/// Will be raised if a shard in collection and its prototype in the
/// corresponding distributeShardsLike collection don't have the same followers
/// (when they should already have been adjusted)
constexpr int TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS                   = 5005;

/// 5006: ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES
/// "inconsistent attributes during cluster repairs"
/// Will be raised if a collection that is fixed does (not) have
/// distributeShardsLike when it is expected, or does (not) have
/// repairingDistributeShardsLike when it is expected
constexpr int TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES                 = 5006;

/// 5007: ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS
/// "mismatching shards during cluster repairs"
/// Will be raised if in a collection and its distributeShardsLike prototype
/// collection some shard and its prototype have an unequal number of DB Servers
constexpr int TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_SHARDS                      = 5007;

/// 5008: ERROR_CLUSTER_REPAIRS_JOB_FAILED
/// "move shard job failed during cluster repairs"
/// Will be raised if a move shard job in the agency failed during cluster
/// repairs
constexpr int TRI_ERROR_CLUSTER_REPAIRS_JOB_FAILED                              = 5008;

/// 5009: ERROR_CLUSTER_REPAIRS_JOB_DISAPPEARED
/// "move shard job disappeared during cluster repairs"
/// Will be raised if a move shard job in the agency cannot be found anymore
/// before it finished
constexpr int TRI_ERROR_CLUSTER_REPAIRS_JOB_DISAPPEARED                         = 5009;

/// 5010: ERROR_CLUSTER_REPAIRS_OPERATION_FAILED
/// "agency transaction failed during cluster repairs"
/// Will be raised if an agency transaction failed during either sending or
/// executing it.
constexpr int TRI_ERROR_CLUSTER_REPAIRS_OPERATION_FAILED                        = 5010;

/// 20001: ERROR_AGENCY_INQUIRY_SYNTAX
/// "Illegal inquiry syntax"
/// Inquiry handles a list of string clientIds: [<clientId>,...].
constexpr int TRI_ERROR_AGENCY_INQUIRY_SYNTAX                                   = 20001;

/// 20011: ERROR_AGENCY_INFORM_MUST_BE_OBJECT
/// "Inform message must be an object."
/// The inform message in the agency must be an object.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT                            = 20011;

/// 20012: ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM
/// "Inform message must contain uint parameter 'term'"
/// The inform message in the agency must contain a uint parameter 'term'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM                         = 20012;

/// 20013: ERROR_AGENCY_INFORM_MUST_CONTAIN_ID
/// "Inform message must contain string parameter 'id'"
/// The inform message in the agency must contain a string parameter 'id'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ID                           = 20013;

/// 20014: ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE
/// "Inform message must contain array 'active'"
/// The inform message in the agency must contain an array 'active'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE                       = 20014;

/// 20015: ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL
/// "Inform message must contain object 'pool'"
/// The inform message in the agency must contain an object 'pool'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL                         = 20015;

/// 20016: ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING
/// "Inform message must contain object 'min ping'"
/// The inform message in the agency must contain an object 'min ping'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING                     = 20016;

/// 20017: ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING
/// "Inform message must contain object 'max ping'"
/// The inform message in the agency must contain an object 'max ping'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING                     = 20017;

/// 20018: ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT
/// "Inform message must contain object 'timeoutMult'"
/// The inform message in the agency must contain an object 'timeoutMult'.
constexpr int TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT                 = 20018;

/// 20020: ERROR_AGENCY_INQUIRE_CLIENT_ID_MUST_BE_STRING
/// "Inquiry failed"
/// Inquiry by clientId failed
constexpr int TRI_ERROR_AGENCY_INQUIRE_CLIENT_ID_MUST_BE_STRING                 = 20020;

/// 20021: ERROR_AGENCY_CANNOT_REBUILD_DBS
/// "Cannot rebuild readDB and spearHead"
/// Will be raised if the readDB or the spearHead cannot be rebuilt from the
/// replicated log.
constexpr int TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS                               = 20021;

/// 20501: ERROR_SUPERVISION_GENERAL_FAILURE
/// "general supervision failure"
/// General supervision failure.
constexpr int TRI_ERROR_SUPERVISION_GENERAL_FAILURE                             = 20501;

/// 21001: ERROR_DISPATCHER_IS_STOPPING
/// "dispatcher stopped"
/// Will be returned if a shutdown is in progress.
constexpr int TRI_ERROR_DISPATCHER_IS_STOPPING                                  = 21001;

/// 21002: ERROR_QUEUE_UNKNOWN
/// "named queue does not exist"
/// Will be returned if a queue with this name does not exist.
constexpr int TRI_ERROR_QUEUE_UNKNOWN                                           = 21002;

/// 21003: ERROR_QUEUE_FULL
/// "named queue is full"
/// Will be returned if a queue with this name is full.
constexpr int TRI_ERROR_QUEUE_FULL                                              = 21003;

/// 6001: ERROR_ACTION_ALREADY_REGISTERED
/// "maintenance action already registered"
/// Action with this description has been registered already
constexpr int TRI_ERROR_ACTION_ALREADY_REGISTERED                               = 6001;

/// 6002: ERROR_ACTION_OPERATION_UNABORTABLE
/// "this maintenance action cannot be stopped"
/// This maintenance action cannot be stopped once it is started
constexpr int TRI_ERROR_ACTION_OPERATION_UNABORTABLE                            = 6002;

/// 6003: ERROR_ACTION_UNFINISHED
/// "maintenance action still processing"
/// This maintenance action is still processing
constexpr int TRI_ERROR_ACTION_UNFINISHED                                       = 6003;

/// 6004: ERROR_NO_SUCH_ACTION
/// "no such maintenance action"
/// No such maintenance action exists
constexpr int TRI_ERROR_NO_SUCH_ACTION                                          = 6004;


/// register all errors for ArangoDB
void TRI_InitializeErrorMessages();

#endif
