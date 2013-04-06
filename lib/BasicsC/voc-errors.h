
#ifndef TRIAGENS_BASICS_C_VOC_ERRORS_H
#define TRIAGENS_BASICS_C_VOC_ERRORS_H 1

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page ArangoErrors Error codes and meanings
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
/// - 400: @LIT{bad parameter}
///   Will be raised when the HTTP request does not fulfill the requirements.
/// - 403: @LIT{forbidden}
///   Will be raised when the operation is forbidden.
/// - 404: @LIT{not found}
///   Will be raised when an URI is unknown.
/// - 405: @LIT{method not supported}
///   Will be raised when an unsupported HTTP method is used for an operation.
/// - 500: @LIT{internal server error}
///   Will be raised when an internal server is encountered.
/// - 600: @LIT{invalid JSON object}
///   Will be raised when a string representation of a JSON object is corrupt.
/// - 601: @LIT{superfluous URL suffices}
///   Will be raised when the URL contains superfluous suffices.
/// - 1000: @LIT{illegal state}
///   Internal error that will be raised when the datafile is not in the
///   required state.
/// - 1001: @LIT{illegal shaper}
///   Internal error that will be raised when the shaper encountered a porblem.
/// - 1002: @LIT{datafile sealed}
///   Internal error that will be raised when trying to write to a datafile.
/// - 1003: @LIT{unknown type}
///   Internal error that will be raised when an unknown collection type is
///   encountered.
/// - 1004: @LIT{ready only}
///   Internal error that will be raised when trying to write to a read-only
///   datafile or collection.
/// - 1005: @LIT{duplicate identifier}
///   Internal error that will be raised when a identifier duplicate is
///   detected.
/// - 1006: @LIT{datafile unreadable}
///   Internal error that will be raised when the datafile is unreadable.
/// - 1100: @LIT{corrupted datafile}
///   Will be raised when a corruption is detected in a datafile.
/// - 1101: @LIT{illegal parameter file}
///   Will be raised if a parameter file is corrupted.
/// - 1102: @LIT{corrupted collection}
///   Will be raised when a collection contains one or more corrupted datafiles.
/// - 1103: @LIT{mmap failed}
///   Will be raised when the system call mmap failed.
/// - 1104: @LIT{filesystem full}
///   Will be raised when the filesystem is full.
/// - 1105: @LIT{no journal}
///   Will be raised when a journal cannot be created.
/// - 1106: @LIT{cannot create/rename datafile because it already exists}
///   Will be raised when the datafile cannot be created or renamed because a
///   file of the same name already exists.
/// - 1107: @LIT{database is locked}
///   Will be raised when the database is locked by a different process.
/// - 1108: @LIT{cannot create/rename collection because directory already exists}
///   Will be raised when the collection cannot be created because a directory
///   of the same name already exists.
/// - 1109: @LIT{msync failed}
///   Will be raised when the system call msync failed.
/// - 1200: @LIT{conflict}
///   Will be raised when updating or deleting a document and a conflict has
///   been detected.
/// - 1201: @LIT{wrong path for database}
///   Will be raised when a non-existing directory was specified as path for
///   the database.
/// - 1202: @LIT{document not found}
///   Will be raised when a document with a given identifier or handle is
///   unknown.
/// - 1203: @LIT{collection not found}
///   Will be raised when a collection with a given identifier or name is
///   unknown.
/// - 1204: @LIT{parameter 'collection' not found}
///   Will be raised when the collection parameter is missing.
/// - 1205: @LIT{illegal document handle}
///   Will be raised when a document handle is corrupt.
/// - 1206: @LIT{maixmal size of journal too small}
///   Will be raised when the maximal size of the journal is too small.
/// - 1207: @LIT{duplicate name}
///   Will be raised when a name duplicate is detected.
/// - 1208: @LIT{illegal name}
///   Will be raised when an illegal name is detected.
/// - 1209: @LIT{no suitable index known}
///   Will be raised when no suitable index for the query is known.
/// - 1210: @LIT{unique constraint violated}
///   Will be raised when there is a unique constraint violation.
/// - 1211: @LIT{geo index violated}
///   Will be raised when a illegale coordinate is used.
/// - 1212: @LIT{index not found}
///   Will be raised when an index with a given identifier is unknown.
/// - 1213: @LIT{cross collection request not allowed}
///   Will be raised when a cross-collection is requested.
/// - 1214: @LIT{illegal index handle}
///   Will be raised when a index handle is corrupt.
/// - 1215: @LIT{cap constraint already defined}
///   Will be raised when a cap constraint was already defined.
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
/// - 1220: @LIT{parser failed}
///   Will be raised when the parsing of an attribute of a structure failed.
/// - 1221: @LIT{illegal document key}
///   Will be raised when a document key is corrupt.
/// - 1222: @LIT{unexpected document key}
///   Will be raised when a user-defined document key is supplied for
///   collections with auto key generation.
/// - 1223: @LIT{index needs resizing}
///   Will be raised when an index is full and should be resized to contain
///   more data.
/// - 1224: @LIT{database directory not writable}
///   Will be raised when the database directory is not writable for the
///   current user.
/// - 1225: @LIT{out of keys}
///   Will be raised when a key generator runs out of keys.
/// - 1226: @LIT{missing document key}
///   Will be raised when a document key is missing.
/// - 1300: @LIT{datafile full}
///   Will be raised when the datafile reaches its limit.
/// - 1500: @LIT{query killed}
///   Will be raised when a running query is killed by an explicit admin
///   command.
/// - 1501: @LIT{\%s}
///   Will be raised when query is parsed and is found to be syntactially
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
/// - 1540: @LIT{usage of unknown function '\%s'}
///   Will be raised when an undefined function is called.
/// - 1541: @LIT{invalid number of arguments for function '\%s'}
///   Will be raised when the number of arguments used in a function call does
///   not match the expected number of arguments for the function.
/// - 1542: @LIT{invalid argument type used in call to function '\%s()'}
///   Will be raised when the type of an argument used in a function call does
///   not match the expected argument type.
/// - 1543: @LIT{invalid regex argument value used in call to function '\%s()'}
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
/// - 1563: @LIT{list expected}
///   Will be raised when a non-list operand is used for an operation that
///   expects a list argument operand.
/// - 1569: @LIT{FAIL(\%s) called}
///   Will be raised when the function FAIL() is called from inside a query.
/// - 1570: @LIT{no suitable geo index found for geo restriction on '\%s'}
///   Will be raised when a geo restriction was specified but no suitable geo
///   index is found to resolve it.
/// - 1571: @LIT{no suitable fulltext index found for fulltext query on '\%s'}
///   Will be raised when a fulltext query is performed on a collection without
///   a suitable fulltext index.
/// - 1580: @LIT{invalid user function name}
///   Will be raised when a user function with an invalid name is registered.
/// - 1581: @LIT{invalid user function code}
///   Will be raised when a user function is registered with invalid code.
/// - 1582: @LIT{user function not found}
///   Will be raised when a user function is accessed but not found.
/// - 1600: @LIT{cursor not found}
///   Will be raised when a cursor is requested via its id but a cursor with
///   that id cannot be found.
/// - 1650: @LIT{transaction definition is incomplete}
///   Will be raised when the transaction definition is incomplete (e.g. lacks
///   collections to use).
/// - 1651: @LIT{invalid transaction state}
///   Will be raised when an operation is requested on a transaction that has
///   an incompatible state.
/// - 1652: @LIT{nested transactions detected}
///   Will be raised when transactions are nested.
/// - 1653: @LIT{internal transaction error}
///   Will be raised when a wrong usage of transactions is detected. this is an
///   internal error and indicates a bug in ArangoDB.
/// - 1654: @LIT{unregistered collection used in transaction}
///   Will be raised when a collection is used in the middle of a transaction
///   but was not registered at transaction start.
/// - 1655: @LIT{disallowed operation inside a transaction}
///   Will be raised when a disallowed operation is carried out in a
///   transaction.
/// - 1700: @LIT{invalid user name}
///   Will be raised when an invalid user name is used
/// - 1701: @LIT{invalid password}
///   Will be raised when an invalid password is used
/// - 1702: @LIT{duplicate user}
///   Will be raised when a user name already exists
/// - 1703: @LIT{user not found}
///   Will be raised when a user name is updated that does not exist
/// - 1750: @LIT{application not found}
///   Will be raised when an application is not found or not present in the
///   specified version.
/// - 1751: @LIT{invalid application name}
///   Will be raised when an invalid application name is specified.
/// - 1752: @LIT{invalid mount}
///   Will be raised when an invalid mount is specified.
/// - 1753: @LIT{application download failed}
///   Will be raised when an application download from the central repository
///   failed.
/// - 1800: @LIT{invalid key declaration}
///   Will be raised when an invalid key specification is passed to the server
/// - 1801: @LIT{key already exists}
///   Will be raised when a key is to be created that already exists
/// - 1802: @LIT{key not found}
///   Will be raised when the specified key is not found
/// - 1803: @LIT{key is not unique}
///   Will be raised when the specified key is not unique
/// - 1804: @LIT{key value not changed}
///   Will be raised when updating the value for a key does not work
/// - 1805: @LIT{key value not removed}
///   Will be raised when deleting a key/value pair does not work
/// - 1806: @LIT{missing value}
///   Will be raised when the value is missing
/// - 1901: @LIT{invalid graph}
///   Will be raised when an invalid name is passed to the server
/// - 1902: @LIT{could not create graph}
///   Will be raised when an invalid name, vertices or edges is passed to the
///   server
/// - 1903: @LIT{invalid vertex}
///   Will be raised when an invalid vertex id is passed to the server
/// - 1904: @LIT{could not create vertex}
///   Will be raised when the vertex could not be created
/// - 1905: @LIT{could not change vertex}
///   Will be raised when the vertex could not be changed
/// - 1906: @LIT{invalid edge}
///   Will be raised when an invalid edge id is passed to the server
/// - 1907: @LIT{could not create edge}
///   Will be raised when the edge could not be created
/// - 1908: @LIT{could not change edge}
///   Will be raised when the edge could not be changed
/// - 1951: @LIT{invalid session}
///   Will be raised when an invalid session id is passed to the server
/// - 1952: @LIT{could not create session}
///   Will be raised when the session could not be created
/// - 1953: @LIT{could not change session}
///   Will be raised when session data could not be changed
/// - 1961: @LIT{invalid form}
///   Will be raised when an invalid form id is passed to the server
/// - 1962: @LIT{could not create form}
///   Will be raised when the form could not be created
/// - 1963: @LIT{could not change form}
///   Will be raised when form data could not be changed
/// - 2000: @LIT{unknown client error}
///   This error should not happen.
/// - 2001: @LIT{could not connect to server}
///   Will be raised when the client could not connect to the server.
/// - 2002: @LIT{could not write to server}
///   Will be raised when the client could not write data.
/// - 2003: @LIT{could not read from server}
///   Will be raised when the client could not read data.
/// - 3100: @LIT{priority queue insert failure}
///   Will be raised when an attempt to insert a document into a priority queue
///   index fails for some reason.
/// - 3110: @LIT{priority queue remove failure}
///   Will be raised when an attempt to remove a document from a priority queue
///   index fails for some reason.
/// - 3111: @LIT{priority queue remove failure - item missing in index}
///   Will be raised when an attempt to remove a document from a priority queue
///   index fails when document can not be located within the index.
/// - 3312: @LIT{(non-unique) hash index insert failure - document duplicated in index}
///   Will be raised when an attempt to insert a document into a non-unique
///   hash index fails due to the fact that document is duplicated within that
///   index.
/// - 3313: @LIT{(non-unique) skiplist index insert failure - document duplicated in index}
///   Will be raised when an attempt to insert a document into a non-unique
///   skiplist index fails due to the fact that document is duplicated within
///   that index.
/// - 3200: @LIT{hash index insertion warning - attribute missing in document}
///   Will be raised when an attempt to insert a document into a hash index is
///   caused by the document not having one or more attributes which are
///   required by the hash index.
/// - 3202: @LIT{hash index update warning - attribute missing in revised document}
///   Will be raised when an attempt to update a document results in the
///   revised document not having one or more attributes which are required by
///   the hash index.
/// - 3211: @LIT{hash index remove failure - item missing in index}
///   Will be raised when an attempt to remove a document from a hash index
///   fails when document can not be located within that index.
/// - 3300: @LIT{skiplist index insertion warning - attribute missing in document}
///   Will be raised when an attempt to insert a document into a skiplist index
///   is caused by in the document not having one or more attributes which are
///   required by the skiplist index.
/// - 3302: @LIT{skiplist index update warning - attribute missing in revised document}
///   Will be raised when an attempt to update a document results in the
///   revised document not having one or more attributes which are required by
///   the skiplist index.
/// - 3311: @LIT{skiplist index remove failure - item missing in index}
///   Will be raised when an attempt to remove a document from a skiplist index
///   fails when document can not be located within that index.
/// - 3400: @LIT{bitarray index insertion warning - attribute missing in document}
///   Will be raised when an attempt to insert a document into a bitarray index
///   is caused by in the document not having one or more attributes which are
///   required by the bitarray index.
/// - 3402: @LIT{bitarray index update warning - attribute missing in revised document}
///   Will be raised when an attempt to update a document results in the
///   revised document not having one or more attributes which are required by
///   the bitarray index.
/// - 3411: @LIT{bitarray index remove failure - item missing in index}
///   Will be raised when an attempt to remove a document from a bitarray index
///   fails when document can not be located within that index.
/// - 3413: @LIT{bitarray index insert failure - document attribute value unsupported in index}
///   Will be raised when an attempt to insert a document into a bitarray index
///   fails due to the fact that one or more values for an index attribute is
///   not supported within that index.
/// - 3415: @LIT{bitarray index creation failure - one or more index attributes are duplicated.}
///   Will be raised when an attempt to create an index with two or more index
///   attributes repeated.
/// - 3417: @LIT{bitarray index creation failure - one or more index attribute values are duplicated.}
///   Will be raised when an attempt to create an index with two or more index
///   attribute values repeated.
/// - 10000: @LIT{element not inserted into structure, because key already exists}
///   Will be returned if the element was not insert because the key already
///   exists.
/// - 10001: @LIT{element not inserted into structure, because it already exists}
///   Will be returned if the element was not insert because it already exists.
/// - 10002: @LIT{key not found in structure}
///   Will be returned if the key was not found in the structure.
/// - 10003: @LIT{element not found in structure}
///   Will be returned if the element was not found in the structure.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocError
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief helper macro to define an error string
////////////////////////////////////////////////////////////////////////////////

#define REG_ERROR(id, label) TRI_set_errno_string(TRI_ ## id, label);

////////////////////////////////////////////////////////////////////////////////
/// @brief register all errors for ArangoDB
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseErrorMessages (void);

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
/// @brief 400: ERROR_HTTP_BAD_PARAMETER
///
/// bad parameter
///
/// Will be raised when the HTTP request does not fulfill the requirements.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_BAD_PARAMETER                                      (400)

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
/// @brief 500: ERROR_HTTP_SERVER_ERROR
///
/// internal server error
///
/// Will be raised when an internal server is encountered.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_SERVER_ERROR                                       (500)

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
/// @brief 1001: ERROR_ARANGO_SHAPER_FAILED
///
/// illegal shaper
///
/// Internal error that will be raised when the shaper encountered a porblem.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_SHAPER_FAILED                                    (1001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1002: ERROR_ARANGO_DATAFILE_SEALED
///
/// datafile sealed
///
/// Internal error that will be raised when trying to write to a datafile.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_SEALED                                  (1002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1003: ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE
///
/// unknown type
///
/// Internal error that will be raised when an unknown collection type is
/// encountered.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_UNKNOWN_COLLECTION_TYPE                          (1003)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1004: ERROR_ARANGO_READ_ONLY
///
/// ready only
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
/// Internal error that will be raised when the datafile is unreadable.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_UNREADABLE                              (1006)

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
/// illegal parameter file
///
/// Will be raised if a parameter file is corrupted.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_ILLEGAL_PARAMETER_FILE                           (1101)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1102: ERROR_ARANGO_CORRUPTED_COLLECTION
///
/// corrupted collection
///
/// Will be raised when a collection contains one or more corrupted datafiles.
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
/// @brief 1107: ERROR_ARANGO_DATABASE_LOCKED
///
/// database is locked
///
/// Will be raised when the database is locked by a different process.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATABASE_LOCKED                                  (1107)

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
/// @brief 1200: ERROR_ARANGO_CONFLICT
///
/// conflict
///
/// Will be raised when updating or deleting a document and a conflict has been
/// detected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_CONFLICT                                         (1200)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1201: ERROR_ARANGO_WRONG_VOCBASE_PATH
///
/// wrong path for database
///
/// Will be raised when a non-existing directory was specified as path for the
/// database.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_WRONG_VOCBASE_PATH                               (1201)

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
/// Will be raised when a collection with a given identifier or name is unknown.
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
/// maixmal size of journal too small
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
/// @brief 1211: ERROR_ARANGO_GEO_INDEX_VIOLATED
///
/// geo index violated
///
/// Will be raised when a illegale coordinate is used.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_GEO_INDEX_VIOLATED                               (1211)

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
/// @brief 1215: ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED
///
/// cap constraint already defined
///
/// Will be raised when a cap constraint was already defined.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED                   (1215)

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
/// @brief 1220: ERROR_ARANGO_PARSER_FAILED
///
/// parser failed
///
/// Will be raised when the parsing of an attribute of a structure failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_PARSER_FAILED                                    (1220)

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
/// @brief 1223: ERROR_ARANGO_INDEX_NEEDS_RESIZE
///
/// index needs resizing
///
/// Will be raised when an index is full and should be resized to contain more
/// data.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE                               (1223)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1224: ERROR_ARANGO_DATADIR_NOT_WRITABLE
///
/// database directory not writable
///
/// Will be raised when the database directory is not writable for the current
/// user.
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
/// @brief 1300: ERROR_ARANGO_DATAFILE_FULL
///
/// datafile full
///
/// Will be raised when the datafile reaches its limit.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_DATAFILE_FULL                                    (1300)

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
/// Will be raised when query is parsed and is found to be syntactially invalid.
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
/// usage of unknown function '%s'
///
/// Will be raised when an undefined function is called.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN                             (1540)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1541: ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH
///
/// invalid number of arguments for function '%s'
///
/// Will be raised when the number of arguments used in a function call does
/// not match the expected number of arguments for the function.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH                 (1541)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1542: ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH
///
/// invalid argument type used in call to function '%s()'
///
/// Will be raised when the type of an argument used in a function call does
/// not match the expected argument type.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH                   (1542)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1543: ERROR_QUERY_INVALID_REGEX
///
/// invalid regex argument value used in call to function '%s()'
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
/// @brief 1563: ERROR_QUERY_LIST_EXPECTED
///
/// list expected
///
/// Will be raised when a non-list operand is used for an operation that
/// expects a list argument operand.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_LIST_EXPECTED                                     (1563)

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
/// user function not found
///
/// Will be raised when a user function is accessed but not found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_FUNCTION_NOT_FOUND                                (1582)

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
/// @brief 1650: ERROR_TRANSACTION_INCOMPLETE
///
/// transaction definition is incomplete
///
/// Will be raised when the transaction definition is incomplete (e.g. lacks
/// collections to use).
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_INCOMPLETE                                  (1650)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1651: ERROR_TRANSACTION_INVALID_STATE
///
/// invalid transaction state
///
/// Will be raised when an operation is requested on a transaction that has an
/// incompatible state.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_INVALID_STATE                               (1651)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1652: ERROR_TRANSACTION_NESTED
///
/// nested transactions detected
///
/// Will be raised when transactions are nested.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_NESTED                                      (1652)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1653: ERROR_TRANSACTION_INTERNAL
///
/// internal transaction error
///
/// Will be raised when a wrong usage of transactions is detected. this is an
/// internal error and indicates a bug in ArangoDB.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_INTERNAL                                    (1653)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1654: ERROR_TRANSACTION_UNREGISTERED_COLLECTION
///
/// unregistered collection used in transaction
///
/// Will be raised when a collection is used in the middle of a transaction but
/// was not registered at transaction start.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION                     (1654)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1655: ERROR_TRANSACTION_DISALLOWED_OPERATION
///
/// disallowed operation inside a transaction
///
/// Will be raised when a disallowed operation is carried out in a transaction.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION                        (1655)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1700: ERROR_USER_INVALID_NAME
///
/// invalid user name
///
/// Will be raised when an invalid user name is used
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_INVALID_NAME                                       (1700)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1701: ERROR_USER_INVALID_PASSWORD
///
/// invalid password
///
/// Will be raised when an invalid password is used
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_INVALID_PASSWORD                                   (1701)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1702: ERROR_USER_DUPLICATE
///
/// duplicate user
///
/// Will be raised when a user name already exists
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_DUPLICATE                                          (1702)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1703: ERROR_USER_NOT_FOUND
///
/// user not found
///
/// Will be raised when a user name is updated that does not exist
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_USER_NOT_FOUND                                          (1703)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1750: ERROR_APPLICATION_NOT_FOUND
///
/// application not found
///
/// Will be raised when an application is not found or not present in the
/// specified version.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_APPLICATION_NOT_FOUND                                   (1750)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1751: ERROR_APPLICATION_INVALID_NAME
///
/// invalid application name
///
/// Will be raised when an invalid application name is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_APPLICATION_INVALID_NAME                                (1751)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1752: ERROR_APPLICATION_INVALID_MOUNT
///
/// invalid mount
///
/// Will be raised when an invalid mount is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_APPLICATION_INVALID_MOUNT                               (1752)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1753: ERROR_APPLICATION_DOWNLOAD_FAILED
///
/// application download failed
///
/// Will be raised when an application download from the central repository
/// failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_APPLICATION_DOWNLOAD_FAILED                             (1753)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1800: ERROR_KEYVALUE_INVALID_KEY
///
/// invalid key declaration
///
/// Will be raised when an invalid key specification is passed to the server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_INVALID_KEY                                    (1800)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1801: ERROR_KEYVALUE_KEY_EXISTS
///
/// key already exists
///
/// Will be raised when a key is to be created that already exists
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_KEY_EXISTS                                     (1801)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1802: ERROR_KEYVALUE_KEY_NOT_FOUND
///
/// key not found
///
/// Will be raised when the specified key is not found
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_KEY_NOT_FOUND                                  (1802)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1803: ERROR_KEYVALUE_KEY_NOT_UNIQUE
///
/// key is not unique
///
/// Will be raised when the specified key is not unique
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_KEY_NOT_UNIQUE                                 (1803)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1804: ERROR_KEYVALUE_KEY_NOT_CHANGED
///
/// key value not changed
///
/// Will be raised when updating the value for a key does not work
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_KEY_NOT_CHANGED                                (1804)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1805: ERROR_KEYVALUE_KEY_NOT_REMOVED
///
/// key value not removed
///
/// Will be raised when deleting a key/value pair does not work
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_KEY_NOT_REMOVED                                (1805)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1806: ERROR_KEYVALUE_NO_VALUE
///
/// missing value
///
/// Will be raised when the value is missing
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_KEYVALUE_NO_VALUE                                       (1806)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1901: ERROR_GRAPH_INVALID_GRAPH
///
/// invalid graph
///
/// Will be raised when an invalid name is passed to the server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_GRAPH                                     (1901)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1902: ERROR_GRAPH_COULD_NOT_CREATE_GRAPH
///
/// could not create graph
///
/// Will be raised when an invalid name, vertices or edges is passed to the
/// server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CREATE_GRAPH                            (1902)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1903: ERROR_GRAPH_INVALID_VERTEX
///
/// invalid vertex
///
/// Will be raised when an invalid vertex id is passed to the server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_VERTEX                                    (1903)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1904: ERROR_GRAPH_COULD_NOT_CREATE_VERTEX
///
/// could not create vertex
///
/// Will be raised when the vertex could not be created
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CREATE_VERTEX                           (1904)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1905: ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX
///
/// could not change vertex
///
/// Will be raised when the vertex could not be changed
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX                           (1905)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1906: ERROR_GRAPH_INVALID_EDGE
///
/// invalid edge
///
/// Will be raised when an invalid edge id is passed to the server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_INVALID_EDGE                                      (1906)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1907: ERROR_GRAPH_COULD_NOT_CREATE_EDGE
///
/// could not create edge
///
/// Will be raised when the edge could not be created
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CREATE_EDGE                             (1907)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1908: ERROR_GRAPH_COULD_NOT_CHANGE_EDGE
///
/// could not change edge
///
/// Will be raised when the edge could not be changed
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_GRAPH_COULD_NOT_CHANGE_EDGE                             (1908)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1951: ERROR_SESSION_INVALID_SESSION
///
/// invalid session
///
/// Will be raised when an invalid session id is passed to the server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_INVALID_SESSION                                 (1951)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1952: ERROR_SESSION_COULD_NOT_CREATE_SESSION
///
/// could not create session
///
/// Will be raised when the session could not be created
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_COULD_NOT_CREATE_SESSION                        (1952)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1953: ERROR_SESSION_COULD_NOT_CHANGE_SESSION
///
/// could not change session
///
/// Will be raised when session data could not be changed
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_COULD_NOT_CHANGE_SESSION                        (1953)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1961: ERROR_SESSION_INVALID_FORM
///
/// invalid form
///
/// Will be raised when an invalid form id is passed to the server
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_INVALID_FORM                                    (1961)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1962: ERROR_SESSION_COULD_NOT_CREATE_FORM
///
/// could not create form
///
/// Will be raised when the form could not be created
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_COULD_NOT_CREATE_FORM                           (1962)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1963: ERROR_SESSION_COULD_NOT_CHANGE_FORM
///
/// could not change form
///
/// Will be raised when form data could not be changed
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_COULD_NOT_CHANGE_FORM                           (1963)

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
/// @brief 3100: ERROR_ARANGO_INDEX_PQ_INSERT_FAILED
///
/// priority queue insert failure
///
/// Will be raised when an attempt to insert a document into a priority queue
/// index fails for some reason.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_PQ_INSERT_FAILED                           (3100)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3110: ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED
///
/// priority queue remove failure
///
/// Will be raised when an attempt to remove a document from a priority queue
/// index fails for some reason.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_FAILED                           (3110)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3111: ERROR_ARANGO_INDEX_PQ_REMOVE_ITEM_MISSING
///
/// priority queue remove failure - item missing in index
///
/// Will be raised when an attempt to remove a document from a priority queue
/// index fails when document can not be located within the index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_PQ_REMOVE_ITEM_MISSING                     (3111)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3312: ERROR_ARANGO_INDEX_HASH_INSERT_ITEM_DUPLICATED
///
/// (non-unique) hash index insert failure - document duplicated in index
///
/// Will be raised when an attempt to insert a document into a non-unique hash
/// index fails due to the fact that document is duplicated within that index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_HASH_INSERT_ITEM_DUPLICATED                (3312)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3313: ERROR_ARANGO_INDEX_SKIPLIST_INSERT_ITEM_DUPLICATED
///
/// (non-unique) skiplist index insert failure - document duplicated in index
///
/// Will be raised when an attempt to insert a document into a non-unique
/// skiplist index fails due to the fact that document is duplicated within
/// that index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_SKIPLIST_INSERT_ITEM_DUPLICATED            (3313)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3200: WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING
///
/// hash index insertion warning - attribute missing in document
///
/// Will be raised when an attempt to insert a document into a hash index is
/// caused by the document not having one or more attributes which are required
/// by the hash index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_HASH_DOCUMENT_ATTRIBUTE_MISSING          (3200)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3202: WARNING_ARANGO_INDEX_HASH_UPDATE_ATTRIBUTE_MISSING
///
/// hash index update warning - attribute missing in revised document
///
/// Will be raised when an attempt to update a document results in the revised
/// document not having one or more attributes which are required by the hash
/// index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_HASH_UPDATE_ATTRIBUTE_MISSING            (3202)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3211: WARNING_ARANGO_INDEX_HASH_REMOVE_ITEM_MISSING
///
/// hash index remove failure - item missing in index
///
/// Will be raised when an attempt to remove a document from a hash index fails
/// when document can not be located within that index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_HASH_REMOVE_ITEM_MISSING                 (3211)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3300: WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING
///
/// skiplist index insertion warning - attribute missing in document
///
/// Will be raised when an attempt to insert a document into a skiplist index
/// is caused by in the document not having one or more attributes which are
/// required by the skiplist index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_SKIPLIST_DOCUMENT_ATTRIBUTE_MISSING      (3300)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3302: WARNING_ARANGO_INDEX_SKIPLIST_UPDATE_ATTRIBUTE_MISSING
///
/// skiplist index update warning - attribute missing in revised document
///
/// Will be raised when an attempt to update a document results in the revised
/// document not having one or more attributes which are required by the
/// skiplist index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_SKIPLIST_UPDATE_ATTRIBUTE_MISSING        (3302)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3311: WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING
///
/// skiplist index remove failure - item missing in index
///
/// Will be raised when an attempt to remove a document from a skiplist index
/// fails when document can not be located within that index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_SKIPLIST_REMOVE_ITEM_MISSING             (3311)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3400: WARNING_ARANGO_INDEX_BITARRAY_DOCUMENT_ATTRIBUTE_MISSING
///
/// bitarray index insertion warning - attribute missing in document
///
/// Will be raised when an attempt to insert a document into a bitarray index
/// is caused by in the document not having one or more attributes which are
/// required by the bitarray index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_BITARRAY_DOCUMENT_ATTRIBUTE_MISSING      (3400)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3402: WARNING_ARANGO_INDEX_BITARRAY_UPDATE_ATTRIBUTE_MISSING
///
/// bitarray index update warning - attribute missing in revised document
///
/// Will be raised when an attempt to update a document results in the revised
/// document not having one or more attributes which are required by the
/// bitarray index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_BITARRAY_UPDATE_ATTRIBUTE_MISSING        (3402)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3411: WARNING_ARANGO_INDEX_BITARRAY_REMOVE_ITEM_MISSING
///
/// bitarray index remove failure - item missing in index
///
/// Will be raised when an attempt to remove a document from a bitarray index
/// fails when document can not be located within that index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_WARNING_ARANGO_INDEX_BITARRAY_REMOVE_ITEM_MISSING             (3411)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3413: ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE
///
/// bitarray index insert failure - document attribute value unsupported in
/// index
///
/// Will be raised when an attempt to insert a document into a bitarray index
/// fails due to the fact that one or more values for an index attribute is not
/// supported within that index.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_BITARRAY_INSERT_ITEM_UNSUPPORTED_VALUE     (3413)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3415: ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_ATTRIBUTES
///
/// bitarray index creation failure - one or more index attributes are
/// duplicated.
///
/// Will be raised when an attempt to create an index with two or more index
/// attributes repeated.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_ATTRIBUTES (3415)

////////////////////////////////////////////////////////////////////////////////
/// @brief 3417: ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_VALUES
///
/// bitarray index creation failure - one or more index attribute values are
/// duplicated.
///
/// Will be raised when an attempt to create an index with two or more index
/// attribute values repeated.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_ARANGO_INDEX_BITARRAY_CREATION_FAILURE_DUPLICATE_VALUES (3417)

////////////////////////////////////////////////////////////////////////////////
/// @brief 10000: RESULT_KEY_EXISTS
///
/// element not inserted into structure, because key already exists
///
/// Will be returned if the element was not insert because the key already
/// exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_RESULT_KEY_EXISTS                                             (10000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 10001: RESULT_ELEMENT_EXISTS
///
/// element not inserted into structure, because it already exists
///
/// Will be returned if the element was not insert because it already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_RESULT_ELEMENT_EXISTS                                         (10001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 10002: RESULT_KEY_NOT_FOUND
///
/// key not found in structure
///
/// Will be returned if the key was not found in the structure.
////////////////////////////////////////////////////////////////////////////////

#define TRI_RESULT_KEY_NOT_FOUND                                          (10002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 10003: RESULT_ELEMENT_NOT_FOUND
///
/// element not found in structure
///
/// Will be returned if the element was not found in the structure.
////////////////////////////////////////////////////////////////////////////////

#define TRI_RESULT_ELEMENT_NOT_FOUND                                      (10003)


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

