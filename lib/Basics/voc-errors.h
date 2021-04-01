
#ifndef ARANGODB_BASICS_VOC_ERRORS_H
#define ARANGODB_BASICS_VOC_ERRORS_H 1

#include "Basics/ErrorCode.h"

/// Error codes and meanings
/// The following errors might be raised when running ArangoDB:


/// 0: ERROR_NO_ERROR
/// "no error"
/// No error has occurred.
constexpr auto TRI_ERROR_NO_ERROR                                                = ErrorCode{0};

/// 1: ERROR_FAILED
/// "failed"
/// Will be raised when a general error occurred.
constexpr auto TRI_ERROR_FAILED                                                  = ErrorCode{1};

/// 2: ERROR_SYS_ERROR
/// "system error"
/// Will be raised when operating system error occurred.
constexpr auto TRI_ERROR_SYS_ERROR                                               = ErrorCode{2};

/// 3: ERROR_OUT_OF_MEMORY
/// "out of memory"
/// Will be raised when there is a memory shortage.
constexpr auto TRI_ERROR_OUT_OF_MEMORY                                           = ErrorCode{3};

/// 4: ERROR_INTERNAL
/// "internal error"
/// Will be raised when an internal error occurred.
constexpr auto TRI_ERROR_INTERNAL                                                = ErrorCode{4};

/// 5: ERROR_ILLEGAL_NUMBER
/// "illegal number"
/// Will be raised when an illegal representation of a number was given.
constexpr auto TRI_ERROR_ILLEGAL_NUMBER                                          = ErrorCode{5};

/// 6: ERROR_NUMERIC_OVERFLOW
/// "numeric overflow"
/// Will be raised when a numeric overflow occurred.
constexpr auto TRI_ERROR_NUMERIC_OVERFLOW                                        = ErrorCode{6};

/// 7: ERROR_ILLEGAL_OPTION
/// "illegal option"
/// Will be raised when an unknown option was supplied by the user.
constexpr auto TRI_ERROR_ILLEGAL_OPTION                                          = ErrorCode{7};

/// 8: ERROR_DEAD_PID
/// "dead process identifier"
/// Will be raised when a PID without a living process was found.
constexpr auto TRI_ERROR_DEAD_PID                                                = ErrorCode{8};

/// 9: ERROR_NOT_IMPLEMENTED
/// "not implemented"
/// Will be raised when hitting an unimplemented feature.
constexpr auto TRI_ERROR_NOT_IMPLEMENTED                                         = ErrorCode{9};

/// 10: ERROR_BAD_PARAMETER
/// "bad parameter"
/// Will be raised when the parameter does not fulfill the requirements.
constexpr auto TRI_ERROR_BAD_PARAMETER                                           = ErrorCode{10};

/// 11: ERROR_FORBIDDEN
/// "forbidden"
/// Will be raised when you are missing permission for the operation.
constexpr auto TRI_ERROR_FORBIDDEN                                               = ErrorCode{11};

/// 12: ERROR_OUT_OF_MEMORY_MMAP
/// "out of memory in mmap"
/// Will be raised when there is a memory shortage.
constexpr auto TRI_ERROR_OUT_OF_MEMORY_MMAP                                      = ErrorCode{12};

/// 13: ERROR_CORRUPTED_CSV
/// "csv is corrupt"
/// Will be raised when encountering a corrupt csv line.
constexpr auto TRI_ERROR_CORRUPTED_CSV                                           = ErrorCode{13};

/// 14: ERROR_FILE_NOT_FOUND
/// "file not found"
/// Will be raised when a file is not found.
constexpr auto TRI_ERROR_FILE_NOT_FOUND                                          = ErrorCode{14};

/// 15: ERROR_CANNOT_WRITE_FILE
/// "cannot write file"
/// Will be raised when a file cannot be written.
constexpr auto TRI_ERROR_CANNOT_WRITE_FILE                                       = ErrorCode{15};

/// 16: ERROR_CANNOT_OVERWRITE_FILE
/// "cannot overwrite file"
/// Will be raised when an attempt is made to overwrite an existing file.
constexpr auto TRI_ERROR_CANNOT_OVERWRITE_FILE                                   = ErrorCode{16};

/// 17: ERROR_TYPE_ERROR
/// "type error"
/// Will be raised when a type error is encountered.
constexpr auto TRI_ERROR_TYPE_ERROR                                              = ErrorCode{17};

/// 18: ERROR_LOCK_TIMEOUT
/// "lock timeout"
/// Will be raised when there's a timeout waiting for a lock.
constexpr auto TRI_ERROR_LOCK_TIMEOUT                                            = ErrorCode{18};

/// 19: ERROR_CANNOT_CREATE_DIRECTORY
/// "cannot create directory"
/// Will be raised when an attempt to create a directory fails.
constexpr auto TRI_ERROR_CANNOT_CREATE_DIRECTORY                                 = ErrorCode{19};

/// 20: ERROR_CANNOT_CREATE_TEMP_FILE
/// "cannot create temporary file"
/// Will be raised when an attempt to create a temporary file fails.
constexpr auto TRI_ERROR_CANNOT_CREATE_TEMP_FILE                                 = ErrorCode{20};

/// 21: ERROR_REQUEST_CANCELED
/// "canceled request"
/// Will be raised when a request is canceled by the user.
constexpr auto TRI_ERROR_REQUEST_CANCELED                                        = ErrorCode{21};

/// 22: ERROR_DEBUG
/// "intentional debug error"
/// Will be raised intentionally during debugging.
constexpr auto TRI_ERROR_DEBUG                                                   = ErrorCode{22};

/// 25: ERROR_IP_ADDRESS_INVALID
/// "IP address is invalid"
/// Will be raised when the structure of an IP address is invalid.
constexpr auto TRI_ERROR_IP_ADDRESS_INVALID                                      = ErrorCode{25};

/// 27: ERROR_FILE_EXISTS
/// "file exists"
/// Will be raised when a file already exists.
constexpr auto TRI_ERROR_FILE_EXISTS                                             = ErrorCode{27};

/// 28: ERROR_LOCKED
/// "locked"
/// Will be raised when a resource or an operation is locked.
constexpr auto TRI_ERROR_LOCKED                                                  = ErrorCode{28};

/// 29: ERROR_DEADLOCK
/// "deadlock detected"
/// Will be raised when a deadlock is detected when accessing collections.
constexpr auto TRI_ERROR_DEADLOCK                                                = ErrorCode{29};

/// 30: ERROR_SHUTTING_DOWN
/// "shutdown in progress"
/// Will be raised when a call cannot succeed because a server shutdown is
/// already in progress.
constexpr auto TRI_ERROR_SHUTTING_DOWN                                           = ErrorCode{30};

/// 31: ERROR_ONLY_ENTERPRISE
/// "only enterprise version"
/// Will be raised when an Enterprise Edition feature is requested from the
/// Community Edition.
constexpr auto TRI_ERROR_ONLY_ENTERPRISE                                         = ErrorCode{31};

/// 32: ERROR_RESOURCE_LIMIT
/// "resource limit exceeded"
/// Will be raised when the resources used by an operation exceed the
/// configured maximum value.
constexpr auto TRI_ERROR_RESOURCE_LIMIT                                          = ErrorCode{32};

/// 33: ERROR_ARANGO_ICU_ERROR
/// "icu error: %s"
/// will be raised if icu operations failed
constexpr auto TRI_ERROR_ARANGO_ICU_ERROR                                        = ErrorCode{33};

/// 34: ERROR_CANNOT_READ_FILE
/// "cannot read file"
/// Will be raised when a file cannot be read.
constexpr auto TRI_ERROR_CANNOT_READ_FILE                                        = ErrorCode{34};

/// 35: ERROR_INCOMPATIBLE_VERSION
/// "incompatible server version"
/// Will be raised when a server is running an incompatible version of ArangoDB.
constexpr auto TRI_ERROR_INCOMPATIBLE_VERSION                                    = ErrorCode{35};

/// 36: ERROR_DISABLED
/// "disabled"
/// Will be raised when a requested resource is not enabled.
constexpr auto TRI_ERROR_DISABLED                                                = ErrorCode{36};

/// 37: ERROR_MALFORMED_JSON
/// "malformed json"
/// Will be raised when a JSON string could not be parsed.
constexpr auto TRI_ERROR_MALFORMED_JSON                                          = ErrorCode{37};

/// 400: ERROR_HTTP_BAD_PARAMETER
/// "bad parameter"
/// Will be raised when the HTTP request does not fulfill the requirements.
constexpr auto TRI_ERROR_HTTP_BAD_PARAMETER                                      = ErrorCode{400};

/// 401: ERROR_HTTP_UNAUTHORIZED
/// "unauthorized"
/// Will be raised when authorization is required but the user is not
/// authorized.
constexpr auto TRI_ERROR_HTTP_UNAUTHORIZED                                       = ErrorCode{401};

/// 403: ERROR_HTTP_FORBIDDEN
/// "forbidden"
/// Will be raised when the operation is forbidden.
constexpr auto TRI_ERROR_HTTP_FORBIDDEN                                          = ErrorCode{403};

/// 404: ERROR_HTTP_NOT_FOUND
/// "not found"
/// Will be raised when an URI is unknown.
constexpr auto TRI_ERROR_HTTP_NOT_FOUND                                          = ErrorCode{404};

/// 405: ERROR_HTTP_METHOD_NOT_ALLOWED
/// "method not supported"
/// Will be raised when an unsupported HTTP method is used for an operation.
constexpr auto TRI_ERROR_HTTP_METHOD_NOT_ALLOWED                                 = ErrorCode{405};

/// 406: ERROR_HTTP_NOT_ACCEPTABLE
/// "request not acceptable"
/// Will be raised when an unsupported HTTP content type is used for an
/// operation, or if a request is not acceptable for a leader or follower.
constexpr auto TRI_ERROR_HTTP_NOT_ACCEPTABLE                                     = ErrorCode{406};

/// 408: ERROR_HTTP_REQUEST_TIMEOUT
/// "request timeout"
/// Will be raised when a timeout occured.
constexpr auto TRI_ERROR_HTTP_REQUEST_TIMEOUT                                    = ErrorCode{408};

/// 409: ERROR_HTTP_CONFLICT
/// "conflict"
/// Will be raised when a conflict occurs in an HTTP operation.
constexpr auto TRI_ERROR_HTTP_CONFLICT                                           = ErrorCode{409};

/// 410: ERROR_HTTP_GONE
/// "content permanently deleted"
/// Will be raised when the requested content has been permanently deleted.
constexpr auto TRI_ERROR_HTTP_GONE                                               = ErrorCode{410};

/// 412: ERROR_HTTP_PRECONDITION_FAILED
/// "precondition failed"
/// Will be raised when a precondition for an HTTP request is not met.
constexpr auto TRI_ERROR_HTTP_PRECONDITION_FAILED                                = ErrorCode{412};

/// 500: ERROR_HTTP_SERVER_ERROR
/// "internal server error"
/// Will be raised when an internal server is encountered.
constexpr auto TRI_ERROR_HTTP_SERVER_ERROR                                       = ErrorCode{500};

/// 501: ERROR_HTTP_NOT_IMPLEMENTED
/// "not implemented"
/// Will be raised when an API is called this is not implemented in general, or
/// not implemented for the current setup.
constexpr auto TRI_ERROR_HTTP_NOT_IMPLEMENTED                                    = ErrorCode{501};

/// 503: ERROR_HTTP_SERVICE_UNAVAILABLE
/// "service unavailable"
/// Will be raised when a service is temporarily unavailable.
constexpr auto TRI_ERROR_HTTP_SERVICE_UNAVAILABLE                                = ErrorCode{503};

/// 504: ERROR_HTTP_GATEWAY_TIMEOUT
/// "gateway timeout"
/// Will be raised when a service contacted by ArangoDB does not respond in a
/// timely manner.
constexpr auto TRI_ERROR_HTTP_GATEWAY_TIMEOUT                                    = ErrorCode{504};

/// 600: ERROR_HTTP_CORRUPTED_JSON
/// "invalid JSON object"
/// Will be raised when a string representation of a JSON object is corrupt.
constexpr auto TRI_ERROR_HTTP_CORRUPTED_JSON                                     = ErrorCode{600};

/// 601: ERROR_HTTP_SUPERFLUOUS_SUFFICES
/// "superfluous URL suffices"
/// Will be raised when the URL contains superfluous suffices.
constexpr auto TRI_ERROR_HTTP_SUPERFLUOUS_SUFFICES                               = ErrorCode{601};

/// 1000: ERROR_ARANGO_ILLEGAL_STATE
/// "illegal state"
/// Internal error that will be raised when the datafile is not in the required
/// state.
constexpr auto TRI_ERROR_ARANGO_ILLEGAL_STATE                                    = ErrorCode{1000};

/// 1002: ERROR_ARANGO_DATAFILE_SEALED
/// "datafile sealed"
/// Internal error that will be raised when trying to write to a datafile.
constexpr auto TRI_ERROR_ARANGO_DATAFILE_SEALED                                  = ErrorCode{1002};

/// 1004: ERROR_ARANGO_READ_ONLY
/// "read only"
/// Internal error that will be raised when trying to write to a read-only
/// datafile or collection.
constexpr auto TRI_ERROR_ARANGO_READ_ONLY                                        = ErrorCode{1004};

/// 1005: ERROR_ARANGO_DUPLICATE_IDENTIFIER
/// "duplicate identifier"
/// Internal error that will be raised when a identifier duplicate is detected.
constexpr auto TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER                             = ErrorCode{1005};

/// 1006: ERROR_ARANGO_DATAFILE_UNREADABLE
/// "datafile unreadable"
/// Internal error that will be raised when a datafile is unreadable.
constexpr auto TRI_ERROR_ARANGO_DATAFILE_UNREADABLE                              = ErrorCode{1006};

/// 1007: ERROR_ARANGO_DATAFILE_EMPTY
/// "datafile empty"
/// Internal error that will be raised when a datafile is empty.
constexpr auto TRI_ERROR_ARANGO_DATAFILE_EMPTY                                   = ErrorCode{1007};

/// 1008: ERROR_ARANGO_RECOVERY
/// "logfile recovery error"
/// Will be raised when an error occurred during WAL log file recovery.
constexpr auto TRI_ERROR_ARANGO_RECOVERY                                         = ErrorCode{1008};

/// 1009: ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND
/// "datafile statistics not found"
/// Will be raised when a required datafile statistics object was not found.
constexpr auto TRI_ERROR_ARANGO_DATAFILE_STATISTICS_NOT_FOUND                    = ErrorCode{1009};

/// 1100: ERROR_ARANGO_CORRUPTED_DATAFILE
/// "corrupted datafile"
/// Will be raised when a corruption is detected in a datafile.
constexpr auto TRI_ERROR_ARANGO_CORRUPTED_DATAFILE                               = ErrorCode{1100};

/// 1101: ERROR_ARANGO_ILLEGAL_PARAMETER_FILE
/// "illegal or unreadable parameter file"
/// Will be raised if a parameter file is corrupted or cannot be read.
constexpr auto TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE                           = ErrorCode{1101};

/// 1102: ERROR_ARANGO_CORRUPTED_COLLECTION
/// "corrupted collection"
/// Will be raised when a collection contains one or more corrupted data files.
constexpr auto TRI_ERROR_ARANGO_CORRUPTED_COLLECTION                             = ErrorCode{1102};

/// 1103: ERROR_ARANGO_MMAP_FAILED
/// "mmap failed"
/// Will be raised when the system call mmap failed.
constexpr auto TRI_ERROR_ARANGO_MMAP_FAILED                                      = ErrorCode{1103};

/// 1104: ERROR_ARANGO_FILESYSTEM_FULL
/// "filesystem full"
/// Will be raised when the filesystem is full.
constexpr auto TRI_ERROR_ARANGO_FILESYSTEM_FULL                                  = ErrorCode{1104};

/// 1105: ERROR_ARANGO_NO_JOURNAL
/// "no journal"
/// Will be raised when a journal cannot be created.
constexpr auto TRI_ERROR_ARANGO_NO_JOURNAL                                       = ErrorCode{1105};

/// 1106: ERROR_ARANGO_DATAFILE_ALREADY_EXISTS
/// "cannot create/rename datafile because it already exists"
/// Will be raised when the datafile cannot be created or renamed because a
/// file of the same name already exists.
constexpr auto TRI_ERROR_ARANGO_DATAFILE_ALREADY_EXISTS                          = ErrorCode{1106};

/// 1107: ERROR_ARANGO_DATADIR_LOCKED
/// "database directory is locked"
/// Will be raised when the database directory is locked by a different process.
constexpr auto TRI_ERROR_ARANGO_DATADIR_LOCKED                                   = ErrorCode{1107};

/// 1108: ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS
/// "cannot create/rename collection because directory already exists"
/// Will be raised when the collection cannot be created because a directory of
/// the same name already exists.
constexpr auto TRI_ERROR_ARANGO_COLLECTION_DIRECTORY_ALREADY_EXISTS              = ErrorCode{1108};

/// 1109: ERROR_ARANGO_MSYNC_FAILED
/// "msync failed"
/// Will be raised when the system call msync failed.
constexpr auto TRI_ERROR_ARANGO_MSYNC_FAILED                                     = ErrorCode{1109};

/// 1110: ERROR_ARANGO_DATADIR_UNLOCKABLE
/// "cannot lock database directory"
/// Will be raised when the server cannot lock the database directory on
/// startup.
constexpr auto TRI_ERROR_ARANGO_DATADIR_UNLOCKABLE                               = ErrorCode{1110};

/// 1111: ERROR_ARANGO_SYNC_TIMEOUT
/// "sync timeout"
/// Will be raised when the server waited too long for a datafile to be synced
/// to disk.
constexpr auto TRI_ERROR_ARANGO_SYNC_TIMEOUT                                     = ErrorCode{1111};

/// 1200: ERROR_ARANGO_CONFLICT
/// "conflict"
/// Will be raised when updating or deleting a document and a conflict has been
/// detected.
constexpr auto TRI_ERROR_ARANGO_CONFLICT                                         = ErrorCode{1200};

/// 1201: ERROR_ARANGO_DATADIR_INVALID
/// "invalid database directory"
/// Will be raised when a non-existing database directory was specified when
/// starting the database.
constexpr auto TRI_ERROR_ARANGO_DATADIR_INVALID                                  = ErrorCode{1201};

/// 1202: ERROR_ARANGO_DOCUMENT_NOT_FOUND
/// "document not found"
/// Will be raised when a document with a given identifier is unknown.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND                               = ErrorCode{1202};

/// 1203: ERROR_ARANGO_DATA_SOURCE_NOT_FOUND
/// "collection or view not found"
/// Will be raised when a collection or View with the given identifier or name
/// is unknown.
constexpr auto TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND                            = ErrorCode{1203};

/// 1204: ERROR_ARANGO_COLLECTION_PARAMETER_MISSING
/// "parameter 'collection' not found"
/// Will be raised when the collection parameter is missing.
constexpr auto TRI_ERROR_ARANGO_COLLECTION_PARAMETER_MISSING                     = ErrorCode{1204};

/// 1205: ERROR_ARANGO_DOCUMENT_HANDLE_BAD
/// "illegal document identifier"
/// Will be raised when a document identifier is corrupt.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD                              = ErrorCode{1205};

/// 1206: ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL
/// "maximal size of journal too small"
/// Will be raised when the maximal size of the journal is too small.
constexpr auto TRI_ERROR_ARANGO_MAXIMAL_SIZE_TOO_SMALL                           = ErrorCode{1206};

/// 1207: ERROR_ARANGO_DUPLICATE_NAME
/// "duplicate name"
/// Will be raised when a name duplicate is detected.
constexpr auto TRI_ERROR_ARANGO_DUPLICATE_NAME                                   = ErrorCode{1207};

/// 1208: ERROR_ARANGO_ILLEGAL_NAME
/// "illegal name"
/// Will be raised when an illegal name is detected.
constexpr auto TRI_ERROR_ARANGO_ILLEGAL_NAME                                     = ErrorCode{1208};

/// 1209: ERROR_ARANGO_NO_INDEX
/// "no suitable index known"
/// Will be raised when no suitable index for the query is known.
constexpr auto TRI_ERROR_ARANGO_NO_INDEX                                         = ErrorCode{1209};

/// 1210: ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED
/// "unique constraint violated"
/// Will be raised when there is a unique constraint violation.
constexpr auto TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED                       = ErrorCode{1210};

/// 1212: ERROR_ARANGO_INDEX_NOT_FOUND
/// "index not found"
/// Will be raised when an index with a given identifier is unknown.
constexpr auto TRI_ERROR_ARANGO_INDEX_NOT_FOUND                                  = ErrorCode{1212};

/// 1213: ERROR_ARANGO_CROSS_COLLECTION_REQUEST
/// "cross collection request not allowed"
/// Will be raised when a cross-collection is requested.
constexpr auto TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST                         = ErrorCode{1213};

/// 1214: ERROR_ARANGO_INDEX_HANDLE_BAD
/// "illegal index identifier"
/// Will be raised when a index identifier is corrupt.
constexpr auto TRI_ERROR_ARANGO_INDEX_HANDLE_BAD                                 = ErrorCode{1214};

/// 1216: ERROR_ARANGO_DOCUMENT_TOO_LARGE
/// "document too large"
/// Will be raised when the document cannot fit into any datafile because of it
/// is too large.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE                               = ErrorCode{1216};

/// 1217: ERROR_ARANGO_COLLECTION_NOT_UNLOADED
/// "collection must be unloaded"
/// Will be raised when a collection should be unloaded, but has a different
/// status.
constexpr auto TRI_ERROR_ARANGO_COLLECTION_NOT_UNLOADED                          = ErrorCode{1217};

/// 1218: ERROR_ARANGO_COLLECTION_TYPE_INVALID
/// "collection type invalid"
/// Will be raised when an invalid collection type is used in a request.
constexpr auto TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID                          = ErrorCode{1218};

/// 1220: ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED
/// "parsing attribute name definition failed"
/// Will be raised when parsing an attribute name definition failed.
constexpr auto TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED                          = ErrorCode{1220};

/// 1221: ERROR_ARANGO_DOCUMENT_KEY_BAD
/// "illegal document key"
/// Will be raised when a document key is corrupt.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD                                 = ErrorCode{1221};

/// 1222: ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED
/// "unexpected document key"
/// Will be raised when a user-defined document key is supplied for collections
/// with auto key generation.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED                          = ErrorCode{1222};

/// 1224: ERROR_ARANGO_DATADIR_NOT_WRITABLE
/// "server database directory not writable"
/// Will be raised when the server's database directory is not writable for the
/// current user.
constexpr auto TRI_ERROR_ARANGO_DATADIR_NOT_WRITABLE                             = ErrorCode{1224};

/// 1225: ERROR_ARANGO_OUT_OF_KEYS
/// "out of keys"
/// Will be raised when a key generator runs out of keys.
constexpr auto TRI_ERROR_ARANGO_OUT_OF_KEYS                                      = ErrorCode{1225};

/// 1226: ERROR_ARANGO_DOCUMENT_KEY_MISSING
/// "missing document key"
/// Will be raised when a document key is missing.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING                             = ErrorCode{1226};

/// 1227: ERROR_ARANGO_DOCUMENT_TYPE_INVALID
/// "invalid document type"
/// Will be raised when there is an attempt to create a document with an
/// invalid type.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID                            = ErrorCode{1227};

/// 1228: ERROR_ARANGO_DATABASE_NOT_FOUND
/// "database not found"
/// Will be raised when a non-existing database is accessed.
constexpr auto TRI_ERROR_ARANGO_DATABASE_NOT_FOUND                               = ErrorCode{1228};

/// 1229: ERROR_ARANGO_DATABASE_NAME_INVALID
/// "database name invalid"
/// Will be raised when an invalid database name is used.
constexpr auto TRI_ERROR_ARANGO_DATABASE_NAME_INVALID                            = ErrorCode{1229};

/// 1230: ERROR_ARANGO_USE_SYSTEM_DATABASE
/// "operation only allowed in system database"
/// Will be raised when an operation is requested in a database other than the
/// system database.
constexpr auto TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE                              = ErrorCode{1230};

/// 1232: ERROR_ARANGO_INVALID_KEY_GENERATOR
/// "invalid key generator"
/// Will be raised when an invalid key generator description is used.
constexpr auto TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR                            = ErrorCode{1232};

/// 1233: ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE
/// "edge attribute missing or invalid"
/// will be raised when the _from or _to values of an edge are undefined or
/// contain an invalid value.
constexpr auto TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE                           = ErrorCode{1233};

/// 1235: ERROR_ARANGO_INDEX_CREATION_FAILED
/// "index creation failed"
/// Will be raised when an attempt to create an index has failed.
constexpr auto TRI_ERROR_ARANGO_INDEX_CREATION_FAILED                            = ErrorCode{1235};

/// 1236: ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT
/// "write-throttling timeout"
/// Will be raised when the server is write-throttled and a write operation has
/// waited too long for the server to process queued operations.
constexpr auto TRI_ERROR_ARANGO_WRITE_THROTTLE_TIMEOUT                           = ErrorCode{1236};

/// 1237: ERROR_ARANGO_COLLECTION_TYPE_MISMATCH
/// "collection type mismatch"
/// Will be raised when a collection has a different type from what has been
/// expected.
constexpr auto TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH                         = ErrorCode{1237};

/// 1238: ERROR_ARANGO_COLLECTION_NOT_LOADED
/// "collection not loaded"
/// Will be raised when a collection is accessed that is not yet loaded.
constexpr auto TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED                            = ErrorCode{1238};

/// 1239: ERROR_ARANGO_DOCUMENT_REV_BAD
/// "illegal document revision"
/// Will be raised when a document revision is corrupt or is missing where
/// needed.
constexpr auto TRI_ERROR_ARANGO_DOCUMENT_REV_BAD                                 = ErrorCode{1239};

/// 1240: ERROR_ARANGO_INCOMPLETE_READ
/// "incomplete read"
/// Will be raised by the storage engine when a read cannot be completed.
constexpr auto TRI_ERROR_ARANGO_INCOMPLETE_READ                                  = ErrorCode{1240};

/// 1300: ERROR_ARANGO_DATAFILE_FULL
/// "datafile full"
/// Will be raised when the datafile reaches its limit.
constexpr auto TRI_ERROR_ARANGO_DATAFILE_FULL                                    = ErrorCode{1300};

/// 1301: ERROR_ARANGO_EMPTY_DATADIR
/// "server database directory is empty"
/// Will be raised when encountering an empty server database directory.
constexpr auto TRI_ERROR_ARANGO_EMPTY_DATADIR                                    = ErrorCode{1301};

/// 1302: ERROR_ARANGO_TRY_AGAIN
/// "operation should be tried again"
/// Will be raised when an operation should be retried.
constexpr auto TRI_ERROR_ARANGO_TRY_AGAIN                                        = ErrorCode{1302};

/// 1303: ERROR_ARANGO_BUSY
/// "engine is busy"
/// Will be raised when storage engine is busy.
constexpr auto TRI_ERROR_ARANGO_BUSY                                             = ErrorCode{1303};

/// 1304: ERROR_ARANGO_MERGE_IN_PROGRESS
/// "merge in progress"
/// Will be raised when storage engine has a datafile merge in progress and
/// cannot complete the operation.
constexpr auto TRI_ERROR_ARANGO_MERGE_IN_PROGRESS                                = ErrorCode{1304};

/// 1305: ERROR_ARANGO_IO_ERROR
/// "storage engine I/O error"
/// Will be raised when storage engine encounters an I/O error.
constexpr auto TRI_ERROR_ARANGO_IO_ERROR                                         = ErrorCode{1305};

/// 1400: ERROR_REPLICATION_NO_RESPONSE
/// "no response"
/// Will be raised when the replication applier does not receive any or an
/// incomplete response from the leader.
constexpr auto TRI_ERROR_REPLICATION_NO_RESPONSE                                 = ErrorCode{1400};

/// 1401: ERROR_REPLICATION_INVALID_RESPONSE
/// "invalid response"
/// Will be raised when the replication applier receives an invalid response
/// from the leader.
constexpr auto TRI_ERROR_REPLICATION_INVALID_RESPONSE                            = ErrorCode{1401};

/// 1402: ERROR_REPLICATION_LEADER_ERROR
/// "leader error"
/// Will be raised when the replication applier receives a server error from
/// the leader.
constexpr auto TRI_ERROR_REPLICATION_LEADER_ERROR                                = ErrorCode{1402};

/// 1403: ERROR_REPLICATION_LEADER_INCOMPATIBLE
/// "leader incompatible"
/// Will be raised when the replication applier connects to a leader that has
/// an incompatible version.
constexpr auto TRI_ERROR_REPLICATION_LEADER_INCOMPATIBLE                         = ErrorCode{1403};

/// 1404: ERROR_REPLICATION_LEADER_CHANGE
/// "leader change"
/// Will be raised when the replication applier connects to a different leader
/// than before.
constexpr auto TRI_ERROR_REPLICATION_LEADER_CHANGE                               = ErrorCode{1404};

/// 1405: ERROR_REPLICATION_LOOP
/// "loop detected"
/// Will be raised when the replication applier is asked to connect to itself
/// for replication.
constexpr auto TRI_ERROR_REPLICATION_LOOP                                        = ErrorCode{1405};

/// 1406: ERROR_REPLICATION_UNEXPECTED_MARKER
/// "unexpected marker"
/// Will be raised when an unexpected marker is found in the replication log
/// stream.
constexpr auto TRI_ERROR_REPLICATION_UNEXPECTED_MARKER                           = ErrorCode{1406};

/// 1407: ERROR_REPLICATION_INVALID_APPLIER_STATE
/// "invalid applier state"
/// Will be raised when an invalid replication applier state file is found.
constexpr auto TRI_ERROR_REPLICATION_INVALID_APPLIER_STATE                       = ErrorCode{1407};

/// 1408: ERROR_REPLICATION_UNEXPECTED_TRANSACTION
/// "invalid transaction"
/// Will be raised when an unexpected transaction id is found.
constexpr auto TRI_ERROR_REPLICATION_UNEXPECTED_TRANSACTION                      = ErrorCode{1408};

/// 1410: ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION
/// "invalid replication applier configuration"
/// Will be raised when the configuration for the replication applier is
/// invalid.
constexpr auto TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION               = ErrorCode{1410};

/// 1411: ERROR_REPLICATION_RUNNING
/// "cannot perform operation while applier is running"
/// Will be raised when there is an attempt to perform an operation while the
/// replication applier is running.
constexpr auto TRI_ERROR_REPLICATION_RUNNING                                     = ErrorCode{1411};

/// 1412: ERROR_REPLICATION_APPLIER_STOPPED
/// "replication stopped"
/// Special error code used to indicate the replication applier was stopped by
/// a user.
constexpr auto TRI_ERROR_REPLICATION_APPLIER_STOPPED                             = ErrorCode{1412};

/// 1413: ERROR_REPLICATION_NO_START_TICK
/// "no start tick"
/// Will be raised when the replication applier is started without a known
/// start tick value.
constexpr auto TRI_ERROR_REPLICATION_NO_START_TICK                               = ErrorCode{1413};

/// 1414: ERROR_REPLICATION_START_TICK_NOT_PRESENT
/// "start tick not present"
/// Will be raised when the replication applier fetches data using a start
/// tick, but that start tick is not present on the logger server anymore.
constexpr auto TRI_ERROR_REPLICATION_START_TICK_NOT_PRESENT                      = ErrorCode{1414};

/// 1416: ERROR_REPLICATION_WRONG_CHECKSUM
/// "wrong checksum"
/// Will be raised when a new born follower submits a wrong checksum
constexpr auto TRI_ERROR_REPLICATION_WRONG_CHECKSUM                              = ErrorCode{1416};

/// 1417: ERROR_REPLICATION_SHARD_NONEMPTY
/// "shard not empty"
/// Will be raised when a shard is not empty and the follower tries a shortcut
constexpr auto TRI_ERROR_REPLICATION_SHARD_NONEMPTY                              = ErrorCode{1417};

/// 1447: ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED
/// "follower transaction intermediate commit already performed"
/// Will be raised when a follower transaction has already performed an
/// intermediate commit and must be rolled back.
constexpr auto TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED           = ErrorCode{1447};

/// 1448: ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED
/// "creating collection failed due to precondition"
/// Will be raised when updating the plan on collection creation failed.
constexpr auto TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED           = ErrorCode{1448};

/// 1449: ERROR_CLUSTER_SERVER_UNKNOWN
/// "got a request from an unknown server"
/// Will be raised on some occasions when one server gets a request from
/// another, which has not (yet?) been made known via the Agency.
constexpr auto TRI_ERROR_CLUSTER_SERVER_UNKNOWN                                  = ErrorCode{1449};

/// 1450: ERROR_CLUSTER_TOO_MANY_SHARDS
/// "too many shards"
/// Will be raised when the number of shards for a collection is higher than
/// allowed.
constexpr auto TRI_ERROR_CLUSTER_TOO_MANY_SHARDS                                 = ErrorCode{1450};

/// 1454: ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN
/// "could not create collection in plan"
/// Will be raised when a Coordinator in a cluster cannot create an entry for a
/// new collection in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION_IN_PLAN             = ErrorCode{1454};

/// 1456: ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION
/// "could not create collection"
/// Will be raised when a Coordinator in a cluster notices that some DB-Servers
/// report problems when creating shards for a new collection.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_CREATE_COLLECTION                     = ErrorCode{1456};

/// 1457: ERROR_CLUSTER_TIMEOUT
/// "timeout in cluster operation"
/// Will be raised when a Coordinator in a cluster runs into a timeout for some
/// cluster wide operation.
constexpr auto TRI_ERROR_CLUSTER_TIMEOUT                                         = ErrorCode{1457};

/// 1458: ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN
/// "could not remove collection from plan"
/// Will be raised when a Coordinator in a cluster cannot remove an entry for a
/// collection in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_COLLECTION_IN_PLAN             = ErrorCode{1458};

/// 1460: ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN
/// "could not create database in plan"
/// Will be raised when a Coordinator in a cluster cannot create an entry for a
/// new database in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE_IN_PLAN               = ErrorCode{1460};

/// 1461: ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE
/// "could not create database"
/// Will be raised when a Coordinator in a cluster notices that some DB-Servers
/// report problems when creating databases for a new cluster wide database.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_CREATE_DATABASE                       = ErrorCode{1461};

/// 1462: ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN
/// "could not remove database from plan"
/// Will be raised when a Coordinator in a cluster cannot remove an entry for a
/// database in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_PLAN               = ErrorCode{1462};

/// 1463: ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT
/// "could not remove database from current"
/// Will be raised when a Coordinator in a cluster cannot remove an entry for a
/// database in the Current hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_REMOVE_DATABASE_IN_CURRENT            = ErrorCode{1463};

/// 1464: ERROR_CLUSTER_SHARD_GONE
/// "no responsible shard found"
/// Will be raised when a Coordinator in a cluster cannot determine the shard
/// that is responsible for a given document.
constexpr auto TRI_ERROR_CLUSTER_SHARD_GONE                                      = ErrorCode{1464};

/// 1465: ERROR_CLUSTER_CONNECTION_LOST
/// "cluster internal HTTP connection broken"
/// Will be raised when a Coordinator in a cluster loses an HTTP connection to
/// a DB-Server in the cluster whilst transferring data.
constexpr auto TRI_ERROR_CLUSTER_CONNECTION_LOST                                 = ErrorCode{1465};

/// 1466: ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY
/// "must not specify _key for this collection"
/// Will be raised when a Coordinator in a cluster finds that the _key
/// attribute was specified in a sharded collection the uses not only _key as
/// sharding attribute.
constexpr auto TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY                            = ErrorCode{1466};

/// 1467: ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS
/// "got contradicting answers from different shards"
/// Will be raised if a Coordinator in a cluster gets conflicting results from
/// different shards, which should never happen.
constexpr auto TRI_ERROR_CLUSTER_GOT_CONTRADICTING_ANSWERS                       = ErrorCode{1467};

/// 1468: ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN
/// "not all sharding attributes given"
/// Will be raised if a Coordinator tries to find out which shard is
/// responsible for a partial document, but cannot do this because not all
/// sharding attributes are specified.
constexpr auto TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN               = ErrorCode{1468};

/// 1469: ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES
/// "must not change the value of a shard key attribute"
/// Will be raised if there is an attempt to update the value of a shard
/// attribute.
constexpr auto TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES             = ErrorCode{1469};

/// 1470: ERROR_CLUSTER_UNSUPPORTED
/// "unsupported operation or parameter for clusters"
/// Will be raised when there is an attempt to carry out an operation that is
/// not supported in the context of a sharded collection.
constexpr auto TRI_ERROR_CLUSTER_UNSUPPORTED                                     = ErrorCode{1470};

/// 1471: ERROR_CLUSTER_ONLY_ON_COORDINATOR
/// "this operation is only valid on a coordinator in a cluster"
/// Will be raised if there is an attempt to run a Coordinator-only operation
/// on a different type of node.
constexpr auto TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR                             = ErrorCode{1471};

/// 1472: ERROR_CLUSTER_READING_PLAN_AGENCY
/// "error reading Plan in agency"
/// Will be raised if a Coordinator or DB-Server cannot read the Plan in the
/// Agency.
constexpr auto TRI_ERROR_CLUSTER_READING_PLAN_AGENCY                             = ErrorCode{1472};

/// 1473: ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION
/// "could not truncate collection"
/// Will be raised if a Coordinator cannot truncate all shards of a cluster
/// collection.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_TRUNCATE_COLLECTION                   = ErrorCode{1473};

/// 1474: ERROR_CLUSTER_AQL_COMMUNICATION
/// "error in cluster internal communication for AQL"
/// Will be raised if the internal communication of the cluster for AQL
/// produces an error.
constexpr auto TRI_ERROR_CLUSTER_AQL_COMMUNICATION                               = ErrorCode{1474};

/// 1477: ERROR_CLUSTER_ONLY_ON_DBSERVER
/// "this operation is only valid on a DBserver in a cluster"
/// Will be raised if there is an attempt to run a DB-Server-only operation on
/// a different type of node.
constexpr auto TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER                                = ErrorCode{1477};

/// 1478: ERROR_CLUSTER_BACKEND_UNAVAILABLE
/// "A cluster backend which was required for the operation could not be reached"
/// Will be raised if a required DB-Server can't be reached.
constexpr auto TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE                             = ErrorCode{1478};

/// 1481: ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC
/// "collection is out of sync"
/// Will be raised if a collection needed during query execution is out of
/// sync. This currently can only happen when using SatelliteCollections
constexpr auto TRI_ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC                      = ErrorCode{1481};

/// 1482: ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN
/// "could not create index in plan"
/// Will be raised when a Coordinator in a cluster cannot create an entry for a
/// new index in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_CREATE_INDEX_IN_PLAN                  = ErrorCode{1482};

/// 1483: ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN
/// "could not drop index in plan"
/// Will be raised when a Coordinator in a cluster cannot remove an index from
/// the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_DROP_INDEX_IN_PLAN                    = ErrorCode{1483};

/// 1484: ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE
/// "chain of distributeShardsLike references"
/// Will be raised if one tries to create a collection with a
/// distributeShardsLike attribute which points to another collection that also
/// has one.
constexpr auto TRI_ERROR_CLUSTER_CHAIN_OF_DISTRIBUTESHARDSLIKE                   = ErrorCode{1484};

/// 1485: ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE
/// "must not drop collection while another has a distributeShardsLike
/// "attribute pointing to it"
/// Will be raised if one tries to drop a collection to which another
/// collection points with its distributeShardsLike attribute.
constexpr auto TRI_ERROR_CLUSTER_MUST_NOT_DROP_COLL_OTHER_DISTRIBUTESHARDSLIKE   = ErrorCode{1485};

/// 1486: ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE
/// "must not have a distributeShardsLike attribute pointing to an unknown
/// "collection"
/// Will be raised if one tries to create a collection which points to an
/// unknown collection in its distributeShardsLike attribute.
constexpr auto TRI_ERROR_CLUSTER_UNKNOWN_DISTRIBUTESHARDSLIKE                    = ErrorCode{1486};

/// 1487: ERROR_CLUSTER_INSUFFICIENT_DBSERVERS
/// "the number of current dbservers is lower than the requested
/// "replicationFactor"
/// Will be raised if one tries to create a collection with a replicationFactor
/// greater than the available number of DB-Servers.
constexpr auto TRI_ERROR_CLUSTER_INSUFFICIENT_DBSERVERS                          = ErrorCode{1487};

/// 1488: ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER
/// "a follower could not be dropped in agency"
/// Will be raised if a follower that ought to be dropped could not be dropped
/// in the Agency (under Current).
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER                         = ErrorCode{1488};

/// 1489: ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION
/// "a shard leader refuses to perform a replication operation"
/// Will be raised if a replication operation is refused by a shard leader.
constexpr auto TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION                = ErrorCode{1489};

/// 1490: ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION
/// "a shard follower refuses to perform an operation that is not a replication"
/// Will be raised if a non-replication operation is refused by a shard
/// follower.
constexpr auto TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION                = ErrorCode{1490};

/// 1491: ERROR_CLUSTER_SHARD_LEADER_RESIGNED
/// "a (former) shard leader refuses to perform an operation, because it has
/// "resigned in the meantime"
/// Will be raised if a non-replication operation is refused by a former shard
/// leader that has found out that it is no longer the leader.
constexpr auto TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED                           = ErrorCode{1491};

/// 1492: ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED
/// "some agency operation failed"
/// Will be raised if after various retries an Agency operation could not be
/// performed successfully.
constexpr auto TRI_ERROR_CLUSTER_AGENCY_COMMUNICATION_FAILED                     = ErrorCode{1492};

/// 1495: ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING
/// "leadership challenge is ongoing"
/// Will be raised when servers are currently competing for leadership, and the
/// result is still unknown.
constexpr auto TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING                    = ErrorCode{1495};

/// 1496: ERROR_CLUSTER_NOT_LEADER
/// "not a leader"
/// Will be raised when an operation is sent to a non-leading server.
constexpr auto TRI_ERROR_CLUSTER_NOT_LEADER                                      = ErrorCode{1496};

/// 1497: ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN
/// "could not create view in plan"
/// Will be raised when a Coordinator in a cluster cannot create an entry for a
/// new View in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_CREATE_VIEW_IN_PLAN                   = ErrorCode{1497};

/// 1498: ERROR_CLUSTER_VIEW_ID_EXISTS
/// "view ID already exists"
/// Will be raised when a Coordinator in a cluster tries to create a View and
/// the View ID already exists.
constexpr auto TRI_ERROR_CLUSTER_VIEW_ID_EXISTS                                  = ErrorCode{1498};

/// 1499: ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION
/// "could not drop collection in plan"
/// Will be raised when a Coordinator in a cluster cannot drop a collection
/// entry in the Plan hierarchy in the Agency.
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_DROP_COLLECTION                       = ErrorCode{1499};

/// 1500: ERROR_QUERY_KILLED
/// "query killed"
/// Will be raised when a running query is killed by an explicit admin command.
constexpr auto TRI_ERROR_QUERY_KILLED                                            = ErrorCode{1500};

/// 1501: ERROR_QUERY_PARSE
/// "%s"
/// Will be raised when query is parsed and is found to be syntactically
/// invalid.
constexpr auto TRI_ERROR_QUERY_PARSE                                             = ErrorCode{1501};

/// 1502: ERROR_QUERY_EMPTY
/// "query is empty"
/// Will be raised when an empty query is specified.
constexpr auto TRI_ERROR_QUERY_EMPTY                                             = ErrorCode{1502};

/// 1503: ERROR_QUERY_SCRIPT
/// "runtime error '%s'"
/// Will be raised when a runtime error is caused by the query.
constexpr auto TRI_ERROR_QUERY_SCRIPT                                            = ErrorCode{1503};

/// 1504: ERROR_QUERY_NUMBER_OUT_OF_RANGE
/// "number out of range"
/// Will be raised when a number is outside the expected range.
constexpr auto TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               = ErrorCode{1504};

/// 1505: ERROR_QUERY_INVALID_GEO_VALUE
/// "invalid geo coordinate value"
/// Will be raised when a geo index coordinate is invalid or out of range.
constexpr auto TRI_ERROR_QUERY_INVALID_GEO_VALUE                                 = ErrorCode{1505};

/// 1510: ERROR_QUERY_VARIABLE_NAME_INVALID
/// "variable name '%s' has an invalid format"
/// Will be raised when an invalid variable name is used.
constexpr auto TRI_ERROR_QUERY_VARIABLE_NAME_INVALID                             = ErrorCode{1510};

/// 1511: ERROR_QUERY_VARIABLE_REDECLARED
/// "variable '%s' is assigned multiple times"
/// Will be raised when a variable gets re-assigned in a query.
constexpr auto TRI_ERROR_QUERY_VARIABLE_REDECLARED                               = ErrorCode{1511};

/// 1512: ERROR_QUERY_VARIABLE_NAME_UNKNOWN
/// "unknown variable '%s'"
/// Will be raised when an unknown variable is used or the variable is
/// undefined the context it is used.
constexpr auto TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN                             = ErrorCode{1512};

/// 1521: ERROR_QUERY_COLLECTION_LOCK_FAILED
/// "unable to read-lock collection %s"
/// Will be raised when a read lock on the collection cannot be acquired.
constexpr auto TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED                            = ErrorCode{1521};

/// 1522: ERROR_QUERY_TOO_MANY_COLLECTIONS
/// "too many collections/shards"
/// Will be raised when the number of collections or shards in a query is
/// beyond the allowed value.
constexpr auto TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS                              = ErrorCode{1522};

/// 1540: ERROR_QUERY_FUNCTION_NAME_UNKNOWN
/// "usage of unknown function '%s()'"
/// Will be raised when an undefined function is called.
constexpr auto TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN                             = ErrorCode{1540};

/// 1541: ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH
/// "invalid number of arguments for function '%s()', expected number of
/// "arguments: minimum: %d, maximum: %d"
/// Will be raised when the number of arguments used in a function call does
/// not match the expected number of arguments for the function.
constexpr auto TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH                 = ErrorCode{1541};

/// 1542: ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH
/// "invalid argument type in call to function '%s()'"
/// Will be raised when the type of an argument used in a function call does
/// not match the expected argument type.
constexpr auto TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH                   = ErrorCode{1542};

/// 1543: ERROR_QUERY_INVALID_REGEX
/// "invalid regex value"
/// Will be raised when an invalid regex argument value is used in a call to a
/// function that expects a regex.
constexpr auto TRI_ERROR_QUERY_INVALID_REGEX                                     = ErrorCode{1543};

/// 1550: ERROR_QUERY_BIND_PARAMETERS_INVALID
/// "invalid structure of bind parameters"
/// Will be raised when the structure of bind parameters passed has an
/// unexpected format.
constexpr auto TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID                           = ErrorCode{1550};

/// 1551: ERROR_QUERY_BIND_PARAMETER_MISSING
/// "no value specified for declared bind parameter '%s'"
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
constexpr auto TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            = ErrorCode{1551};

/// 1552: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
/// "bind parameter '%s' was not declared in the query"
/// Will be raised when a value gets specified for an undeclared bind parameter.
constexpr auto TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         = ErrorCode{1552};

/// 1553: ERROR_QUERY_BIND_PARAMETER_TYPE
/// "bind parameter '%s' has an invalid value or type"
/// Will be raised when a bind parameter has an invalid value or type.
constexpr auto TRI_ERROR_QUERY_BIND_PARAMETER_TYPE                               = ErrorCode{1553};

/// 1561: ERROR_QUERY_INVALID_ARITHMETIC_VALUE
/// "invalid arithmetic value"
/// Will be raised when a non-numeric value is used in an arithmetic operation.
constexpr auto TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE                          = ErrorCode{1561};

/// 1562: ERROR_QUERY_DIVISION_BY_ZERO
/// "division by zero"
/// Will be raised when there is an attempt to divide by zero.
constexpr auto TRI_ERROR_QUERY_DIVISION_BY_ZERO                                  = ErrorCode{1562};

/// 1563: ERROR_QUERY_ARRAY_EXPECTED
/// "array expected"
/// Will be raised when a non-array operand is used for an operation that
/// expects an array argument operand.
constexpr auto TRI_ERROR_QUERY_ARRAY_EXPECTED                                    = ErrorCode{1563};

/// 1568: ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION
/// "collection '%s' used as expression operand"
/// Will be raised when a collection is used as an operand in an AQL expression.
constexpr auto TRI_ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION                     = ErrorCode{1568};

/// 1569: ERROR_QUERY_FAIL_CALLED
/// "FAIL(%s) called"
/// Will be raised when the function FAIL() is called from inside a query.
constexpr auto TRI_ERROR_QUERY_FAIL_CALLED                                       = ErrorCode{1569};

/// 1570: ERROR_QUERY_GEO_INDEX_MISSING
/// "no suitable geo index found for geo restriction on '%s'"
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
constexpr auto TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 = ErrorCode{1570};

/// 1571: ERROR_QUERY_FULLTEXT_INDEX_MISSING
/// "no suitable fulltext index found for fulltext query on '%s'"
/// Will be raised when a fulltext query is performed on a collection without a
/// suitable fulltext index.
constexpr auto TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING                            = ErrorCode{1571};

/// 1572: ERROR_QUERY_INVALID_DATE_VALUE
/// "invalid date value"
/// Will be raised when a value cannot be converted to a date.
constexpr auto TRI_ERROR_QUERY_INVALID_DATE_VALUE                                = ErrorCode{1572};

/// 1573: ERROR_QUERY_MULTI_MODIFY
/// "multi-modify query"
/// Will be raised when an AQL query contains more than one data-modifying
/// operation.
constexpr auto TRI_ERROR_QUERY_MULTI_MODIFY                                      = ErrorCode{1573};

/// 1574: ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION
/// "invalid aggregate expression"
/// Will be raised when an AQL query contains an invalid aggregate expression.
constexpr auto TRI_ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION                      = ErrorCode{1574};

/// 1575: ERROR_QUERY_COMPILE_TIME_OPTIONS
/// "query options must be readable at query compile time"
/// Will be raised when an AQL data-modification query contains options that
/// cannot be figured out at query compile time.
constexpr auto TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS                              = ErrorCode{1575};

/// 1577: ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE
/// "could not use forced index hint"
/// Will be raised when forceIndexHint is specified, and the hint cannot be
/// used to serve the query.
constexpr auto TRI_ERROR_QUERY_FORCED_INDEX_HINT_UNUSABLE                        = ErrorCode{1577};

/// 1578: ERROR_QUERY_DISALLOWED_DYNAMIC_CALL
/// "disallowed dynamic call to '%s'"
/// Will be raised when a dynamic function call is made to a function that
/// cannot be called dynamically.
constexpr auto TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL                           = ErrorCode{1578};

/// 1579: ERROR_QUERY_ACCESS_AFTER_MODIFICATION
/// "access after data-modification by %s"
/// Will be raised when collection data are accessed after a data-modification
/// operation.
constexpr auto TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION                         = ErrorCode{1579};

/// 1580: ERROR_QUERY_FUNCTION_INVALID_NAME
/// "invalid user function name"
/// Will be raised when a user function with an invalid name is registered.
constexpr auto TRI_ERROR_QUERY_FUNCTION_INVALID_NAME                             = ErrorCode{1580};

/// 1581: ERROR_QUERY_FUNCTION_INVALID_CODE
/// "invalid user function code"
/// Will be raised when a user function is registered with invalid code.
constexpr auto TRI_ERROR_QUERY_FUNCTION_INVALID_CODE                             = ErrorCode{1581};

/// 1582: ERROR_QUERY_FUNCTION_NOT_FOUND
/// "user function '%s()' not found"
/// Will be raised when a user function is accessed but not found.
constexpr auto TRI_ERROR_QUERY_FUNCTION_NOT_FOUND                                = ErrorCode{1582};

/// 1583: ERROR_QUERY_FUNCTION_RUNTIME_ERROR
/// "user function runtime error: %s"
/// Will be raised when a user function throws a runtime exception.
constexpr auto TRI_ERROR_QUERY_FUNCTION_RUNTIME_ERROR                            = ErrorCode{1583};

/// 1590: ERROR_QUERY_BAD_JSON_PLAN
/// "bad execution plan JSON"
/// Will be raised when an HTTP API for a query got an invalid JSON object.
constexpr auto TRI_ERROR_QUERY_BAD_JSON_PLAN                                     = ErrorCode{1590};

/// 1591: ERROR_QUERY_NOT_FOUND
/// "query ID not found"
/// Will be raised when an Id of a query is not found by the HTTP API.
constexpr auto TRI_ERROR_QUERY_NOT_FOUND                                         = ErrorCode{1591};

/// 1593: ERROR_QUERY_USER_ASSERT
/// "%s"
/// Will be raised if and user provided expression fails to evaluate to true
constexpr auto TRI_ERROR_QUERY_USER_ASSERT                                       = ErrorCode{1593};

/// 1594: ERROR_QUERY_USER_WARN
/// "%s"
/// Will be raised if and user provided expression fails to evaluate to true
constexpr auto TRI_ERROR_QUERY_USER_WARN                                         = ErrorCode{1594};

/// 1595: ERROR_QUERY_WINDOW_AFTER_MODIFICATION
/// "window operation after data-modification"
/// Will be raised when a window node is created after a data-modification
/// operation.
constexpr auto TRI_ERROR_QUERY_WINDOW_AFTER_MODIFICATION                         = ErrorCode{1595};

/// 1600: ERROR_CURSOR_NOT_FOUND
/// "cursor not found"
/// Will be raised when a cursor is requested via its id but a cursor with that
/// id cannot be found.
constexpr auto TRI_ERROR_CURSOR_NOT_FOUND                                        = ErrorCode{1600};

/// 1601: ERROR_CURSOR_BUSY
/// "cursor is busy"
/// Will be raised when a cursor is requested via its id but a concurrent
/// request is still using the cursor.
constexpr auto TRI_ERROR_CURSOR_BUSY                                             = ErrorCode{1601};

/// 1620: ERROR_VALIDATION_FAILED
/// "schema validation failed"
/// Will be raised when a document does not pass schema validation.
constexpr auto TRI_ERROR_VALIDATION_FAILED                                       = ErrorCode{1620};

/// 1621: ERROR_VALIDATION_BAD_PARAMETER
/// "invalid schema validation parameter"
/// Will be raised when the schema description is invalid.
constexpr auto TRI_ERROR_VALIDATION_BAD_PARAMETER                                = ErrorCode{1621};

/// 1650: ERROR_TRANSACTION_INTERNAL
/// "internal transaction error"
/// Will be raised when a wrong usage of transactions is detected. this is an
/// internal error and indicates a bug in ArangoDB.
constexpr auto TRI_ERROR_TRANSACTION_INTERNAL                                    = ErrorCode{1650};

/// 1651: ERROR_TRANSACTION_NESTED
/// "nested transactions detected"
/// Will be raised when transactions are nested.
constexpr auto TRI_ERROR_TRANSACTION_NESTED                                      = ErrorCode{1651};

/// 1652: ERROR_TRANSACTION_UNREGISTERED_COLLECTION
/// "unregistered collection used in transaction"
/// Will be raised when a collection is used in the middle of a transaction but
/// was not registered at transaction start.
constexpr auto TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION                     = ErrorCode{1652};

/// 1653: ERROR_TRANSACTION_DISALLOWED_OPERATION
/// "disallowed operation inside transaction"
/// Will be raised when a disallowed operation is carried out in a transaction.
constexpr auto TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION                        = ErrorCode{1653};

/// 1654: ERROR_TRANSACTION_ABORTED
/// "transaction aborted"
/// Will be raised when a transaction was aborted.
constexpr auto TRI_ERROR_TRANSACTION_ABORTED                                     = ErrorCode{1654};

/// 1655: ERROR_TRANSACTION_NOT_FOUND
/// "transaction not found"
/// Will be raised when a transaction was not found.
constexpr auto TRI_ERROR_TRANSACTION_NOT_FOUND                                   = ErrorCode{1655};

/// 1700: ERROR_USER_INVALID_NAME
/// "invalid user name"
/// Will be raised when an invalid user name is used.
constexpr auto TRI_ERROR_USER_INVALID_NAME                                       = ErrorCode{1700};

/// 1702: ERROR_USER_DUPLICATE
/// "duplicate user"
/// Will be raised when a user name already exists.
constexpr auto TRI_ERROR_USER_DUPLICATE                                          = ErrorCode{1702};

/// 1703: ERROR_USER_NOT_FOUND
/// "user not found"
/// Will be raised when a user name is updated that does not exist.
constexpr auto TRI_ERROR_USER_NOT_FOUND                                          = ErrorCode{1703};

/// 1705: ERROR_USER_EXTERNAL
/// "user is external"
/// Will be raised when the user is authenticated by an external server.
constexpr auto TRI_ERROR_USER_EXTERNAL                                           = ErrorCode{1705};

/// 1752: ERROR_SERVICE_DOWNLOAD_FAILED
/// "service download failed"
/// Will be raised when a service download from the central repository failed.
constexpr auto TRI_ERROR_SERVICE_DOWNLOAD_FAILED                                 = ErrorCode{1752};

/// 1753: ERROR_SERVICE_UPLOAD_FAILED
/// "service upload failed"
/// Will be raised when a service upload from the client to the ArangoDB server
/// failed.
constexpr auto TRI_ERROR_SERVICE_UPLOAD_FAILED                                   = ErrorCode{1753};

/// 1800: ERROR_LDAP_CANNOT_INIT
/// "cannot init a LDAP connection"
/// can not init a LDAP connection
constexpr auto TRI_ERROR_LDAP_CANNOT_INIT                                        = ErrorCode{1800};

/// 1801: ERROR_LDAP_CANNOT_SET_OPTION
/// "cannot set a LDAP option"
/// can not set a LDAP option
constexpr auto TRI_ERROR_LDAP_CANNOT_SET_OPTION                                  = ErrorCode{1801};

/// 1802: ERROR_LDAP_CANNOT_BIND
/// "cannot bind to a LDAP server"
/// can not bind to a LDAP server
constexpr auto TRI_ERROR_LDAP_CANNOT_BIND                                        = ErrorCode{1802};

/// 1803: ERROR_LDAP_CANNOT_UNBIND
/// "cannot unbind from a LDAP server"
/// can not unbind from a LDAP server
constexpr auto TRI_ERROR_LDAP_CANNOT_UNBIND                                      = ErrorCode{1803};

/// 1804: ERROR_LDAP_CANNOT_SEARCH
/// "cannot issue a LDAP search"
/// can not search the LDAP server
constexpr auto TRI_ERROR_LDAP_CANNOT_SEARCH                                      = ErrorCode{1804};

/// 1805: ERROR_LDAP_CANNOT_START_TLS
/// "cannot start a TLS LDAP session"
/// can not star a TLS LDAP session
constexpr auto TRI_ERROR_LDAP_CANNOT_START_TLS                                   = ErrorCode{1805};

/// 1806: ERROR_LDAP_FOUND_NO_OBJECTS
/// "LDAP didn't found any objects"
/// LDAP didn't found any objects with the specified search query
constexpr auto TRI_ERROR_LDAP_FOUND_NO_OBJECTS                                   = ErrorCode{1806};

/// 1807: ERROR_LDAP_NOT_ONE_USER_FOUND
/// "LDAP found zero ore more than one user"
/// LDAP found zero ore more than one user
constexpr auto TRI_ERROR_LDAP_NOT_ONE_USER_FOUND                                 = ErrorCode{1807};

/// 1808: ERROR_LDAP_USER_NOT_IDENTIFIED
/// "LDAP found a user, but its not the desired one"
/// LDAP found a user, but its not the desired one
constexpr auto TRI_ERROR_LDAP_USER_NOT_IDENTIFIED                                = ErrorCode{1808};

/// 1809: ERROR_LDAP_OPERATIONS_ERROR
/// "LDAP returned an operations error"
/// LDAP returned an operations error
constexpr auto TRI_ERROR_LDAP_OPERATIONS_ERROR                                   = ErrorCode{1809};

/// 1820: ERROR_LDAP_INVALID_MODE
/// "invalid ldap mode"
/// cant distinguish a valid mode for provided LDAP configuration
constexpr auto TRI_ERROR_LDAP_INVALID_MODE                                       = ErrorCode{1820};

/// 1850: ERROR_TASK_INVALID_ID
/// "invalid task id"
/// Will be raised when a task is created with an invalid id.
constexpr auto TRI_ERROR_TASK_INVALID_ID                                         = ErrorCode{1850};

/// 1851: ERROR_TASK_DUPLICATE_ID
/// "duplicate task id"
/// Will be raised when a task id is created with a duplicate id.
constexpr auto TRI_ERROR_TASK_DUPLICATE_ID                                       = ErrorCode{1851};

/// 1852: ERROR_TASK_NOT_FOUND
/// "task not found"
/// Will be raised when a task with the specified id could not be found.
constexpr auto TRI_ERROR_TASK_NOT_FOUND                                          = ErrorCode{1852};

/// 1901: ERROR_GRAPH_INVALID_GRAPH
/// "invalid graph"
/// Will be raised when an invalid name is passed to the server.
constexpr auto TRI_ERROR_GRAPH_INVALID_GRAPH                                     = ErrorCode{1901};

/// 1906: ERROR_GRAPH_INVALID_EDGE
/// "invalid edge"
/// Will be raised when an invalid edge id is passed to the server.
constexpr auto TRI_ERROR_GRAPH_INVALID_EDGE                                      = ErrorCode{1906};

/// 1909: ERROR_GRAPH_TOO_MANY_ITERATIONS
/// "too many iterations - try increasing the value of 'maxIterations'"
/// Will be raised when too many iterations are done in a graph traversal.
constexpr auto TRI_ERROR_GRAPH_TOO_MANY_ITERATIONS                               = ErrorCode{1909};

/// 1910: ERROR_GRAPH_INVALID_FILTER_RESULT
/// "invalid filter result"
/// Will be raised when an invalid filter result is returned in a graph
/// traversal.
constexpr auto TRI_ERROR_GRAPH_INVALID_FILTER_RESULT                             = ErrorCode{1910};

/// 1920: ERROR_GRAPH_COLLECTION_MULTI_USE
/// "multi use of edge collection in edge def"
/// an edge collection may only be used once in one edge definition of a graph.
constexpr auto TRI_ERROR_GRAPH_COLLECTION_MULTI_USE                              = ErrorCode{1920};

/// 1921: ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS
/// "edge collection already used in edge def"
///  is already used by another graph in a different edge definition.
constexpr auto TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS                    = ErrorCode{1921};

/// 1922: ERROR_GRAPH_CREATE_MISSING_NAME
/// "missing graph name"
/// a graph name is required to create or drop a graph.
constexpr auto TRI_ERROR_GRAPH_CREATE_MISSING_NAME                               = ErrorCode{1922};

/// 1923: ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION
/// "malformed edge definition"
/// the edge definition is malformed. It has to be an array of objects.
constexpr auto TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION                  = ErrorCode{1923};

/// 1924: ERROR_GRAPH_NOT_FOUND
/// "graph '%s' not found"
/// a graph with this name could not be found.
constexpr auto TRI_ERROR_GRAPH_NOT_FOUND                                         = ErrorCode{1924};

/// 1925: ERROR_GRAPH_DUPLICATE
/// "graph already exists"
/// a graph with this name already exists.
constexpr auto TRI_ERROR_GRAPH_DUPLICATE                                         = ErrorCode{1925};

/// 1926: ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST
/// "vertex collection does not exist or is not part of the graph"
/// the specified vertex collection does not exist or is not part of the graph.
constexpr auto TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST                         = ErrorCode{1926};

/// 1927: ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX
/// "collection not a vertex collection"
/// the collection is not a vertex collection.
constexpr auto TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX                      = ErrorCode{1927};

/// 1928: ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION
/// "collection is not in list of orphan collections"
/// Vertex collection not in list of orphan collections of the graph.
constexpr auto TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION                          = ErrorCode{1928};

/// 1929: ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF
/// "collection already used in edge def"
/// The collection is already used in an edge definition of the graph.
constexpr auto TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF                       = ErrorCode{1929};

/// 1930: ERROR_GRAPH_EDGE_COLLECTION_NOT_USED
/// "edge collection not used in graph"
/// The edge collection is not used in any edge definition of the graph.
constexpr auto TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED                          = ErrorCode{1930};

/// 1932: ERROR_GRAPH_NO_GRAPH_COLLECTION
/// "collection _graphs does not exist"
/// collection _graphs does not exist.
constexpr auto TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION                               = ErrorCode{1932};

/// 1935: ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS
/// "Invalid number of arguments. Expected: "
/// Invalid number of arguments. Expected: 
constexpr auto TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS                       = ErrorCode{1935};

/// 1936: ERROR_GRAPH_INVALID_PARAMETER
/// "Invalid parameter type."
/// Invalid parameter type.
constexpr auto TRI_ERROR_GRAPH_INVALID_PARAMETER                                 = ErrorCode{1936};

/// 1938: ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS
/// "collection used in orphans"
/// The collection is already used in the orphans of the graph.
constexpr auto TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS                        = ErrorCode{1938};

/// 1939: ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST
/// "edge collection does not exist or is not part of the graph"
/// the specified edge collection does not exist or is not part of the graph.
constexpr auto TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST                           = ErrorCode{1939};

/// 1940: ERROR_GRAPH_EMPTY
/// "empty graph"
/// The requested graph has no edge collections.
constexpr auto TRI_ERROR_GRAPH_EMPTY                                             = ErrorCode{1940};

/// 1941: ERROR_GRAPH_INTERNAL_DATA_CORRUPT
/// "internal graph data corrupt"
/// The _graphs collection contains invalid data.
constexpr auto TRI_ERROR_GRAPH_INTERNAL_DATA_CORRUPT                             = ErrorCode{1941};

/// 1943: ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST
/// "malformed orphan list"
/// the orphan list argument is malformed. It has to be an array of strings.
constexpr auto TRI_ERROR_GRAPH_CREATE_MALFORMED_ORPHAN_LIST                      = ErrorCode{1943};

/// 1944: ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT
/// "edge definition collection is a document collection"
/// the collection used as a relation is existing, but is a document
/// collection, it cannot be used here.
constexpr auto TRI_ERROR_GRAPH_EDGE_DEFINITION_IS_DOCUMENT                       = ErrorCode{1944};

/// 1945: ERROR_GRAPH_COLLECTION_IS_INITIAL
/// "initial collection is not allowed to be removed manually"
/// the collection is used as the initial collection of this graph and is not
/// allowed to be removed manually.
constexpr auto TRI_ERROR_GRAPH_COLLECTION_IS_INITIAL                             = ErrorCode{1945};

/// 1946: ERROR_GRAPH_NO_INITIAL_COLLECTION
/// "no valid initial collection found"
/// during the graph creation process no collection could be selected as the
/// needed initial collection. Happens if a distributeShardsLike or
/// replicationFactor mismatch was found.
constexpr auto TRI_ERROR_GRAPH_NO_INITIAL_COLLECTION                             = ErrorCode{1946};

/// 1947: ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED
/// "referenced vertex collection is not part of the graph"
/// the _from or _to collection specified for the edge refers to a vertex
/// collection which is not used in any edge definition of the graph.
constexpr auto TRI_ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED             = ErrorCode{1947};

/// 1950: ERROR_SESSION_UNKNOWN
/// "unknown session"
/// Will be raised when an invalid/unknown session id is passed to the server.
constexpr auto TRI_ERROR_SESSION_UNKNOWN                                         = ErrorCode{1950};

/// 1951: ERROR_SESSION_EXPIRED
/// "session expired"
/// Will be raised when a session is expired.
constexpr auto TRI_ERROR_SESSION_EXPIRED                                         = ErrorCode{1951};

/// 2000: ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR
/// "unknown client error"
/// This error should not happen.
constexpr auto TRI_ERROR_SIMPLE_CLIENT_UNKNOWN_ERROR                             = ErrorCode{2000};

/// 2001: ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT
/// "could not connect to server"
/// Will be raised when the client could not connect to the server.
constexpr auto TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_CONNECT                         = ErrorCode{2001};

/// 2002: ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE
/// "could not write to server"
/// Will be raised when the client could not write data.
constexpr auto TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_WRITE                           = ErrorCode{2002};

/// 2003: ERROR_SIMPLE_CLIENT_COULD_NOT_READ
/// "could not read from server"
/// Will be raised when the client could not read data.
constexpr auto TRI_ERROR_SIMPLE_CLIENT_COULD_NOT_READ                            = ErrorCode{2003};

/// 2019: ERROR_WAS_ERLAUBE
/// "was erlaube?!"
/// Will be raised if was erlaube?!
constexpr auto TRI_ERROR_WAS_ERLAUBE                                             = ErrorCode{2019};

/// 2200: ERROR_INTERNAL_AQL
/// "General internal AQL error"
/// Internal error during AQL execution
constexpr auto TRI_ERROR_INTERNAL_AQL                                            = ErrorCode{2200};

/// 2201: ERROR_WROTE_TOO_FEW_OUTPUT_REGISTERS
/// "An AQL block wrote too few output registers"
/// An AQL block wrote too few output registers
constexpr auto TRI_ERROR_WROTE_TOO_FEW_OUTPUT_REGISTERS                          = ErrorCode{2201};

/// 2202: ERROR_WROTE_TOO_MANY_OUTPUT_REGISTERS
/// "An AQL block wrote too many output registers"
/// An AQL block wrote too many output registers
constexpr auto TRI_ERROR_WROTE_TOO_MANY_OUTPUT_REGISTERS                         = ErrorCode{2202};

/// 2203: ERROR_WROTE_OUTPUT_REGISTER_TWICE
/// "An AQL block wrote an output register twice"
/// An AQL block wrote an output register twice
constexpr auto TRI_ERROR_WROTE_OUTPUT_REGISTER_TWICE                             = ErrorCode{2203};

/// 2204: ERROR_WROTE_IN_WRONG_REGISTER
/// "An AQL block wrote in a register that is not its output"
/// An AQL block wrote in a register that is not its output
constexpr auto TRI_ERROR_WROTE_IN_WRONG_REGISTER                                 = ErrorCode{2204};

/// 2205: ERROR_INPUT_REGISTERS_NOT_COPIED
/// "An AQL block did not copy its input registers"
/// An AQL block did not copy its input registers
constexpr auto TRI_ERROR_INPUT_REGISTERS_NOT_COPIED                              = ErrorCode{2205};

/// 3000: ERROR_MALFORMED_MANIFEST_FILE
/// "failed to parse manifest file"
/// The service manifest file is not well-formed JSON.
constexpr auto TRI_ERROR_MALFORMED_MANIFEST_FILE                                 = ErrorCode{3000};

/// 3001: ERROR_INVALID_SERVICE_MANIFEST
/// "manifest file is invalid"
/// The service manifest contains invalid values.
constexpr auto TRI_ERROR_INVALID_SERVICE_MANIFEST                                = ErrorCode{3001};

/// 3002: ERROR_SERVICE_FILES_MISSING
/// "service files missing"
/// The service folder or bundle does not exist on this server.
constexpr auto TRI_ERROR_SERVICE_FILES_MISSING                                   = ErrorCode{3002};

/// 3003: ERROR_SERVICE_FILES_OUTDATED
/// "service files outdated"
/// The local service bundle does not match the checksum in the database.
constexpr auto TRI_ERROR_SERVICE_FILES_OUTDATED                                  = ErrorCode{3003};

/// 3004: ERROR_INVALID_FOXX_OPTIONS
/// "service options are invalid"
/// The service options contain invalid values.
constexpr auto TRI_ERROR_INVALID_FOXX_OPTIONS                                    = ErrorCode{3004};

/// 3007: ERROR_INVALID_MOUNTPOINT
/// "invalid mountpath"
/// The service mountpath contains invalid characters.
constexpr auto TRI_ERROR_INVALID_MOUNTPOINT                                      = ErrorCode{3007};

/// 3009: ERROR_SERVICE_NOT_FOUND
/// "service not found"
/// No service found at the given mountpath.
constexpr auto TRI_ERROR_SERVICE_NOT_FOUND                                       = ErrorCode{3009};

/// 3010: ERROR_SERVICE_NEEDS_CONFIGURATION
/// "service needs configuration"
/// The service is missing configuration or dependencies.
constexpr auto TRI_ERROR_SERVICE_NEEDS_CONFIGURATION                             = ErrorCode{3010};

/// 3011: ERROR_SERVICE_MOUNTPOINT_CONFLICT
/// "service already exists"
/// A service already exists at the given mountpath.
constexpr auto TRI_ERROR_SERVICE_MOUNTPOINT_CONFLICT                             = ErrorCode{3011};

/// 3012: ERROR_SERVICE_MANIFEST_NOT_FOUND
/// "missing manifest file"
/// The service directory does not contain a manifest file.
constexpr auto TRI_ERROR_SERVICE_MANIFEST_NOT_FOUND                              = ErrorCode{3012};

/// 3013: ERROR_SERVICE_OPTIONS_MALFORMED
/// "failed to parse service options"
/// The service options are not well-formed JSON.
constexpr auto TRI_ERROR_SERVICE_OPTIONS_MALFORMED                               = ErrorCode{3013};

/// 3014: ERROR_SERVICE_SOURCE_NOT_FOUND
/// "source path not found"
/// The source path does not match a file or directory.
constexpr auto TRI_ERROR_SERVICE_SOURCE_NOT_FOUND                                = ErrorCode{3014};

/// 3015: ERROR_SERVICE_SOURCE_ERROR
/// "error resolving source"
/// The source path could not be resolved.
constexpr auto TRI_ERROR_SERVICE_SOURCE_ERROR                                    = ErrorCode{3015};

/// 3016: ERROR_SERVICE_UNKNOWN_SCRIPT
/// "unknown script"
/// The service does not have a script with this name.
constexpr auto TRI_ERROR_SERVICE_UNKNOWN_SCRIPT                                  = ErrorCode{3016};

/// 3099: ERROR_SERVICE_API_DISABLED
/// "service api disabled"
/// The API for managing Foxx services has been disabled on this server.
constexpr auto TRI_ERROR_SERVICE_API_DISABLED                                    = ErrorCode{3099};

/// 3100: ERROR_MODULE_NOT_FOUND
/// "cannot locate module"
/// The module path could not be resolved.
constexpr auto TRI_ERROR_MODULE_NOT_FOUND                                        = ErrorCode{3100};

/// 3101: ERROR_MODULE_SYNTAX_ERROR
/// "syntax error in module"
/// The module could not be parsed because of a syntax error.
constexpr auto TRI_ERROR_MODULE_SYNTAX_ERROR                                     = ErrorCode{3101};

/// 3103: ERROR_MODULE_FAILURE
/// "failed to invoke module"
/// Failed to invoke the module in its context.
constexpr auto TRI_ERROR_MODULE_FAILURE                                          = ErrorCode{3103};

/// 4000: ERROR_NO_SMART_COLLECTION
/// "collection is not smart"
/// The requested collection needs to be smart, but it ain't
constexpr auto TRI_ERROR_NO_SMART_COLLECTION                                     = ErrorCode{4000};

/// 4001: ERROR_NO_SMART_GRAPH_ATTRIBUTE
/// "smart graph attribute not given"
/// The given document does not have the SmartGraph attribute set.
constexpr auto TRI_ERROR_NO_SMART_GRAPH_ATTRIBUTE                                = ErrorCode{4001};

/// 4002: ERROR_CANNOT_DROP_SMART_COLLECTION
/// "cannot drop this smart collection"
/// This smart collection cannot be dropped, it dictates sharding in the graph.
constexpr auto TRI_ERROR_CANNOT_DROP_SMART_COLLECTION                            = ErrorCode{4002};

/// 4003: ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE
/// "in smart vertex collections _key must be a string and prefixed with the
/// "value of the smart graph attribute"
/// In a smart vertex collection _key must be prefixed with the value of the
/// SmartGraph attribute.
constexpr auto TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE         = ErrorCode{4003};

/// 4004: ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE
/// "attribute cannot be used as smart graph attribute"
/// The given smartGraph attribute is illegal and cannot be used for sharding.
/// All system attributes are forbidden.
constexpr auto TRI_ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE                           = ErrorCode{4004};

/// 4005: ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH
/// "smart graph attribute mismatch"
/// The SmartGraph attribute of the given collection does not match the
/// SmartGraph attribute of the graph.
constexpr auto TRI_ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH                          = ErrorCode{4005};

/// 4006: ERROR_INVALID_SMART_JOIN_ATTRIBUTE
/// "invalid smart join attribute declaration"
/// Will be raised when the smartJoinAttribute declaration is invalid.
constexpr auto TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE                            = ErrorCode{4006};

/// 4007: ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_JOIN_ATTRIBUTE
/// "shard key value must be prefixed with the value of the smart join attribute"
/// when using smartJoinAttribute for a collection, the shard key value must be
/// prefixed with the value of the SmartJoin attribute.
constexpr auto TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_JOIN_ATTRIBUTE          = ErrorCode{4007};

/// 4008: ERROR_NO_SMART_JOIN_ATTRIBUTE
/// "smart join attribute not given or invalid"
/// The given document does not have the required SmartJoin attribute set or it
/// has an invalid value.
constexpr auto TRI_ERROR_NO_SMART_JOIN_ATTRIBUTE                                 = ErrorCode{4008};

/// 4009: ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE
/// "must not change the value of the smartJoinAttribute"
/// Will be raised if there is an attempt to update the value of the
/// smartJoinAttribute.
constexpr auto TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE            = ErrorCode{4009};

/// 4010: ERROR_INVALID_DISJOINT_SMART_EDGE
/// "non disjoint edge found"
/// Will be raised if there is an attempt to create an edge between separated
/// graph components.
constexpr auto TRI_ERROR_INVALID_DISJOINT_SMART_EDGE                             = ErrorCode{4010};

/// 20001: ERROR_AGENCY_MALFORMED_GOSSIP_MESSAGE
/// "malformed gossip message"
/// Malformed gossip message.
constexpr auto TRI_ERROR_AGENCY_MALFORMED_GOSSIP_MESSAGE                         = ErrorCode{20001};

/// 20002: ERROR_AGENCY_MALFORMED_INQUIRE_REQUEST
/// "malformed inquire request"
/// Malformed inquire request.
constexpr auto TRI_ERROR_AGENCY_MALFORMED_INQUIRE_REQUEST                        = ErrorCode{20002};

/// 20011: ERROR_AGENCY_INFORM_MUST_BE_OBJECT
/// "Inform message must be an object."
/// The inform message in the Agency must be an object.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_BE_OBJECT                            = ErrorCode{20011};

/// 20012: ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM
/// "Inform message must contain uint parameter 'term'"
/// The inform message in the Agency must contain a uint parameter 'term'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TERM                         = ErrorCode{20012};

/// 20013: ERROR_AGENCY_INFORM_MUST_CONTAIN_ID
/// "Inform message must contain string parameter 'id'"
/// The inform message in the Agency must contain a string parameter 'id'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ID                           = ErrorCode{20013};

/// 20014: ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE
/// "Inform message must contain array 'active'"
/// The inform message in the Agency must contain an array 'active'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_ACTIVE                       = ErrorCode{20014};

/// 20015: ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL
/// "Inform message must contain object 'pool'"
/// The inform message in the Agency must contain an object 'pool'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_POOL                         = ErrorCode{20015};

/// 20016: ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING
/// "Inform message must contain object 'min ping'"
/// The inform message in the Agency must contain an object 'min ping'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MIN_PING                     = ErrorCode{20016};

/// 20017: ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING
/// "Inform message must contain object 'max ping'"
/// The inform message in the Agency must contain an object 'max ping'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_MAX_PING                     = ErrorCode{20017};

/// 20018: ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT
/// "Inform message must contain object 'timeoutMult'"
/// The inform message in the Agency must contain an object 'timeoutMult'.
constexpr auto TRI_ERROR_AGENCY_INFORM_MUST_CONTAIN_TIMEOUT_MULT                 = ErrorCode{20018};

/// 20021: ERROR_AGENCY_CANNOT_REBUILD_DBS
/// "Cannot rebuild readDB and spearHead"
/// Will be raised if the readDB or the spearHead cannot be rebuilt from the
/// replicated log.
constexpr auto TRI_ERROR_AGENCY_CANNOT_REBUILD_DBS                               = ErrorCode{20021};

/// 20030: ERROR_AGENCY_MALFORMED_TRANSACTION
/// "malformed agency transaction"
/// Malformed agency transaction.
constexpr auto TRI_ERROR_AGENCY_MALFORMED_TRANSACTION                            = ErrorCode{20030};

/// 20501: ERROR_SUPERVISION_GENERAL_FAILURE
/// "general supervision failure"
/// General supervision failure.
constexpr auto TRI_ERROR_SUPERVISION_GENERAL_FAILURE                             = ErrorCode{20501};

/// 21003: ERROR_QUEUE_FULL
/// "named queue is full"
/// Will be returned if a queue with this name is full.
constexpr auto TRI_ERROR_QUEUE_FULL                                              = ErrorCode{21003};

/// 6002: ERROR_ACTION_OPERATION_UNABORTABLE
/// "this maintenance action cannot be stopped"
/// This maintenance action cannot be stopped once it is started
constexpr auto TRI_ERROR_ACTION_OPERATION_UNABORTABLE                            = ErrorCode{6002};

/// 6003: ERROR_ACTION_UNFINISHED
/// "maintenance action still processing"
/// This maintenance action is still processing
constexpr auto TRI_ERROR_ACTION_UNFINISHED                                       = ErrorCode{6003};

/// 6004: ERROR_NO_SUCH_ACTION
/// "no such maintenance action"
/// No such maintenance action exists
constexpr auto TRI_ERROR_NO_SUCH_ACTION                                          = ErrorCode{6004};

/// 7001: ERROR_HOT_BACKUP_INTERNAL
/// "internal hot backup error"
/// Failed to create hot backup set
constexpr auto TRI_ERROR_HOT_BACKUP_INTERNAL                                     = ErrorCode{7001};

/// 7002: ERROR_HOT_RESTORE_INTERNAL
/// "internal hot restore error"
/// Failed to restore to hot backup set
constexpr auto TRI_ERROR_HOT_RESTORE_INTERNAL                                    = ErrorCode{7002};

/// 7003: ERROR_BACKUP_TOPOLOGY
/// "backup does not match this topology"
/// The hot backup set cannot be restored on non matching cluster topology
constexpr auto TRI_ERROR_BACKUP_TOPOLOGY                                         = ErrorCode{7003};

/// 7004: ERROR_NO_SPACE_LEFT_ON_DEVICE
/// "no space left on device"
/// No space left on device
constexpr auto TRI_ERROR_NO_SPACE_LEFT_ON_DEVICE                                 = ErrorCode{7004};

/// 7005: ERROR_FAILED_TO_UPLOAD_BACKUP
/// "failed to upload hot backup set to remote target"
/// Failed to upload hot backup set to remote target
constexpr auto TRI_ERROR_FAILED_TO_UPLOAD_BACKUP                                 = ErrorCode{7005};

/// 7006: ERROR_FAILED_TO_DOWNLOAD_BACKUP
/// "failed to download hot backup set from remote source"
/// Failed to download hot backup set from remote source
constexpr auto TRI_ERROR_FAILED_TO_DOWNLOAD_BACKUP                               = ErrorCode{7006};

/// 7007: ERROR_NO_SUCH_HOT_BACKUP
/// "no such hot backup set can be found"
/// Cannot find a hot backup set with this Id
constexpr auto TRI_ERROR_NO_SUCH_HOT_BACKUP                                      = ErrorCode{7007};

/// 7008: ERROR_REMOTE_REPOSITORY_CONFIG_BAD
/// "remote hotback repository configuration error"
/// The configuration given for upload or download operation to/from remote hot
/// backup repositories is wrong.
constexpr auto TRI_ERROR_REMOTE_REPOSITORY_CONFIG_BAD                            = ErrorCode{7008};

/// 7009: ERROR_LOCAL_LOCK_FAILED
/// "some db servers cannot be reached for transaction locks"
/// Some of the DB-Servers cannot be reached for transaction locks.
constexpr auto TRI_ERROR_LOCAL_LOCK_FAILED                                       = ErrorCode{7009};

/// 7010: ERROR_LOCAL_LOCK_RETRY
/// "some db servers cannot be reached for transaction locks"
/// Some of the DB-Servers cannot be reached for transaction locks.
constexpr auto TRI_ERROR_LOCAL_LOCK_RETRY                                        = ErrorCode{7010};

/// 7011: ERROR_HOT_BACKUP_CONFLICT
/// "hot backup conflict"
/// Conflict of multiple hot backup processes.
constexpr auto TRI_ERROR_HOT_BACKUP_CONFLICT                                     = ErrorCode{7011};

/// 7012: ERROR_HOT_BACKUP_DBSERVERS_AWOL
/// "hot backup not all db servers reachable"
/// One or more DB-Servers could not be reached for hot backup inquiry
constexpr auto TRI_ERROR_HOT_BACKUP_DBSERVERS_AWOL                               = ErrorCode{7012};

/// 7021: ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN
/// "analyzers in plan could not be modified"
/// Plan could not be modified while creating or deleting Analyzers revision
constexpr auto TRI_ERROR_CLUSTER_COULD_NOT_MODIFY_ANALYZERS_IN_PLAN              = ErrorCode{7021};

/// 8001: ERROR_AIR_EXECUTION_ERROR
/// "error during AIR execution"
/// During the execution of an AIR program an error occurred
constexpr auto TRI_ERROR_AIR_EXECUTION_ERROR                                     = ErrorCode{8001};

#endif
