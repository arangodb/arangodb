
#ifndef TRIAGENS_DURHAM_VOC_BASE_ERRORS_H
#define TRIAGENS_DURHAM_VOC_BASE_ERRORS_H 1

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @page AvocadoErrors Error codes and meanings
///
/// The following errors might be raised when running AvocadoDB:
///
/// - 0: @CODE{no error}
///   No error has occurred.
/// - 1: @CODE{failed}
///   Will be raised when a general error occurred.
/// - 2: @CODE{system error}
///   Will be raised when operating system error occurred.
/// - 3: @CODE{out of memory}
///   Will be raised when there is a memory shortage.
/// - 4: @CODE{internal error}
///   Will be raised when an internal error occurred.
/// - 5: @CODE{illegal number}
///   Will be raised when an illegal representation of a number was given.
/// - 6: @CODE{numeric overflow}
///   Will be raised when a numeric overflow occurred.
/// - 7: @CODE{illegal option}
///   Will be raised when an unknown option was supplied by the user.
/// - 8: @CODE{dead process identifier}
///   Will be raised when a PID without a living process was found.
/// - 9: @CODE{not implemented}
///   Will be raised when hitting an unimplemented feature.
/// - 400: @CODE{bad parameter}
///   Will be raised when the HTTP request does not fulfill the requirements.
/// - 405: @CODE{method not supported}
///   Will be raised when an unsupported HTTP method is used for an operation.
/// - 600: @CODE{invalid JSON object}
///   Will be raised when a string representation an JSON object is corrupt.
/// - 601: @CODE{superfluous URL suffices}
///   Will be raised when the URL contains superfluous suffices.
/// - 1000: @CODE{illegal state}
///   Internal error that will be raised when the datafile is not in the
///   required state.
/// - 1001: @CODE{illegal shaper}
///   Internal error that will be raised when the shaper encountered a porblem.
/// - 1002: @CODE{datafile sealed}
///   Internal error that will be raised when trying to write to a datafile.
/// - 1003: @CODE{unknown type}
///   Internal error that will be raised when an unknown collection type is
///   encountered.
/// - 1004: @CODE{ready only}
///   Internal error that will be raised when trying to write to a read-only
///   datafile or collection.
/// - 1100: @CODE{corrupted datafile}
///   Will be raised when a corruption is detected in a datafile.
/// - 1101: @CODE{illegal parameter file}
///   Will be raised if a parameter file is corrupted.
/// - 1102: @CODE{corrupted collection}
///   Will be raised when a collection contains one or more corrupted datafiles.
/// - 1103: @CODE{mmap failed}
///   Will be raised when the system call mmap failed.
/// - 1104: @CODE{msync failed}
///   Will be raised when the system call msync failed
/// - 1105: @CODE{no journal}
///   Will be raised when a journal cannot be created.
/// - 1106: @CODE{cannot rename because file ready exists}
///   Will be raised when the datafile cannot be renamed because a file of the
///   same name already exists.
/// - 1107: @CODE{filesystem full}
///   Will be raised when the filesystem is full.
/// - 1200: @CODE{conflict}
///   Will be raised when updating or deleting a document and a conflict has
///   been detected.
/// - 1201: @CODE{wrong path for database}
///   Will be raised when a non-existing directory was specified as path for
///   the database.
/// - 1202: @CODE{document not found}
///   Will be raised when a document with a given identifier or handle is
///   unknown.
/// - 1203: @CODE{collection not found}
///   Will be raised when a collection with a given identifier or name is
///   unknown.
/// - 1204: @CODE{parameter 'collection' not found}
///   Will be raised when the collection parameter is missing.
/// - 1205: @CODE{illegal document handle}
///   Will be raised when a document handle is corrupt.
/// - 1206: @CODE{maixaml size of journal too small}
///   Will be raised when the maximal size of the journal is too small.
/// - 1300: @CODE{datafile full}
///   Will be raised when the datafile reaches its limit.
/// - 1500: @CODE{query killed}
///   Will be raised when a running query is killed by an explicit admin
///   command.
/// - 1501: @CODE{parse error: \%s}
///   Will be raised when query is parsed and is found to be syntactially
///   invalid.
/// - 1502: @CODE{query is empty}
///   Will be raised when an empty query is specified.
/// - 1503: @CODE{query specification invalid}
///   Will be raised when a query is sent to the server with an incomplete or
///   invalid query specification structure.
/// - 1504: @CODE{number '\%s' is out of range}
///   Will be raised when a numeric value inside a query is out of the allowed
///   value range.
/// - 1521: @CODE{limit value '\%s' is out of range}
///   Will be raised when a limit value in the query is outside the allowed
///   range (e. g. when passing a negative skip value).
/// - 1505: @CODE{too many joins.}
///   Will be raised when the number of joins in a query is beyond the allowed
///   value.
/// - 1506: @CODE{collection name '\%s' is invalid}
///   Will be raised when an invalid collection name is used in the from clause
///   of a query.
/// - 1507: @CODE{collection alias '\%s' is invalid}
///   Will be raised when an invalid alias name is used for a collection.
/// - 1508: @CODE{collection alias '\%s' is declared multiple times in the same query}
///   Will be raised when the same alias name is declared multiple times in the
///   same query's from clause.
/// - 1509: @CODE{collection alias '\%s' is used but was not declared in the from clause}
///   Will be raised when an alias not declared in the from clause is used in
///   the query.
/// - 1510: @CODE{unable to open collection '\%s'}
///   Will be raised when one of the collections referenced in the query was
///   not found.
/// - 1511: @CODE{geo restriction for alias '\%s' is invalid}
///   Will be raised when a specified geo restriction is invalid.
/// - 1512: @CODE{no suitable geo index found for geo restriction on '\%s'}
///   Will be raised when a geo restriction was specified but no suitable geo
///   index is found to resolve it.
/// - 1513: @CODE{no value specified for declared bind parameter '\%s'}
///   Will be raised when a bind parameter was declared in the query but the
///   query is being executed with no value for that parameter.
/// - 1514: @CODE{value for bind parameter '\%s' is declared multiple times}
///   Will be raised when a value gets specified multiple times for the same
///   bind parameter.
/// - 1515: @CODE{bind parameter '\%s' was not declared in the query}
///   Will be raised when a value gets specified for an undeclared bind
///   parameter.
/// - 1516: @CODE{invalid value for bind parameter '\%s'}
///   Will be raised when an invalid value is specified for one of the bind
///   parameters.
/// - 1517: @CODE{bind parameter number '\%s' out of range}
///   Will be specified when the numeric index for a bind parameter of type @n
///   is out of the allowed range.
/// - 1600: @CODE{cursor not found}
///   Will be raised when a cursor is requested via its id but a cursor with
///   that id cannot be found.
/// - 1700: @CODE{expecting \<prefix\>/user/\<username\>}
///   TODO
/// - 1701: @CODE{cannot create user}
///   TODO
/// - 1702: @CODE{role not found}
///   TODO
/// - 1703: @CODE{no permission to create user with that role}
///   TODO
/// - 1704: @CODE{user not found}
///   TODO
/// - 1705: @CODE{cannot manage password for user}
///   TODO
/// - 1706: @CODE{expecting POST \<prefix\>/session}
///   TODO
/// - 1707: @CODE{expecting GET \<prefix\>/session/\<sid\>}
///   TODO
/// - 1708: @CODE{expecting PUT \<prefix\>/session/\<sid\>/\<method\>}
///   TODO
/// - 1709: @CODE{expecting DELETE \<prefix\>/session/\<sid\>}
///   TODO
/// - 1710: @CODE{unknown session}
///   TODO
/// - 1711: @CODE{session has not bound to user}
///   TODO
/// - 1712: @CODE{cannot login with session}
///   TODO
/// - 1713: @CODE{expecting GET \<prefix\>/users}
///   TODO
/// - 1714: @CODE{expecting /directory/sessionvoc/\<token\>}
///   TODO
/// - 1715: @CODE{directory server is not configured}
///   TODO
/// - 2000: @CODE{unknown client error}
///   This error should not happen.
/// - 2001: @CODE{could not connect to server}
///   Will be raised when the client could not connect to the server.
/// - 2002: @CODE{could not write to server}
///   Will be raised when the client could not write data.
/// - 2003: @CODE{could not read from server}
///   Will be raised when the client could not read data.
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
/// @brief register all errors for AvocadoDB
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
/// @brief 400: ERROR_HTTP_BAD_PARAMETER
///
/// bad parameter
///
/// Will be raised when the HTTP request does not fulfill the requirements.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_BAD_PARAMETER                                      (400)

////////////////////////////////////////////////////////////////////////////////
/// @brief 405: ERROR_HTTP_METHOD_NOT_ALLOWED
///
/// method not supported
///
/// Will be raised when an unsupported HTTP method is used for an operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_HTTP_METHOD_NOT_ALLOWED                                 (405)

////////////////////////////////////////////////////////////////////////////////
/// @brief 600: ERROR_HTTP_CORRUPTED_JSON
///
/// invalid JSON object
///
/// Will be raised when a string representation an JSON object is corrupt.
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
/// @brief 1000: ERROR_AVOCADO_ILLEGAL_STATE
///
/// illegal state
///
/// Internal error that will be raised when the datafile is not in the required
/// state.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_ILLEGAL_STATE                                   (1000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1001: ERROR_AVOCADO_SHAPER_FAILED
///
/// illegal shaper
///
/// Internal error that will be raised when the shaper encountered a porblem.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_SHAPER_FAILED                                   (1001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1002: ERROR_AVOCADO_DATAFILE_SEALED
///
/// datafile sealed
///
/// Internal error that will be raised when trying to write to a datafile.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_DATAFILE_SEALED                                 (1002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1003: ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE
///
/// unknown type
///
/// Internal error that will be raised when an unknown collection type is
/// encountered.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_UNKNOWN_COLLECTION_TYPE                         (1003)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1004: ERROR_AVOCADO_READ_ONLY
///
/// ready only
///
/// Internal error that will be raised when trying to write to a read-only
/// datafile or collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_READ_ONLY                                       (1004)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1100: ERROR_AVOCADO_CORRUPTED_DATAFILE
///
/// corrupted datafile
///
/// Will be raised when a corruption is detected in a datafile.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_CORRUPTED_DATAFILE                              (1100)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1101: ERROR_AVOCADO_ILLEGAL_PARAMETER_FILE
///
/// illegal parameter file
///
/// Will be raised if a parameter file is corrupted.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_ILLEGAL_PARAMETER_FILE                          (1101)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1102: ERROR_AVOCADO_CORRUPTED_COLLECTION
///
/// corrupted collection
///
/// Will be raised when a collection contains one or more corrupted datafiles.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_CORRUPTED_COLLECTION                            (1102)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1103: ERROR_AVOCADO_MMAP_FAILED
///
/// mmap failed
///
/// Will be raised when the system call mmap failed.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_MMAP_FAILED                                     (1103)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1104: ERROR_AVOCADO_MSYNC_FAILED
///
/// msync failed
///
/// Will be raised when the system call msync failed
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_MSYNC_FAILED                                    (1104)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1105: ERROR_AVOCADO_NO_JOURNAL
///
/// no journal
///
/// Will be raised when a journal cannot be created.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_NO_JOURNAL                                      (1105)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1106: ERROR_AVOCADO_DATAFILE_ALREADY_EXISTS
///
/// cannot rename because file ready exists
///
/// Will be raised when the datafile cannot be renamed because a file of the
/// same name already exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_DATAFILE_ALREADY_EXISTS                         (1106)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1107: ERROR_AVOCADO_FILESYSTEM_FULL
///
/// filesystem full
///
/// Will be raised when the filesystem is full.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_FILESYSTEM_FULL                                 (1107)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1200: ERROR_AVOCADO_CONFLICT
///
/// conflict
///
/// Will be raised when updating or deleting a document and a conflict has been
/// detected.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_CONFLICT                                        (1200)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1201: ERROR_AVOCADO_WRONG_VOCBASE_PATH
///
/// wrong path for database
///
/// Will be raised when a non-existing directory was specified as path for the
/// database.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_WRONG_VOCBASE_PATH                              (1201)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1202: ERROR_AVOCADO_DOCUMENT_NOT_FOUND
///
/// document not found
///
/// Will be raised when a document with a given identifier or handle is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_DOCUMENT_NOT_FOUND                              (1202)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1203: ERROR_AVOCADO_COLLECTION_NOT_FOUND
///
/// collection not found
///
/// Will be raised when a collection with a given identifier or name is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND                            (1203)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1204: ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING
///
/// parameter 'collection' not found
///
/// Will be raised when the collection parameter is missing.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING                    (1204)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1205: ERROR_AVOCADO_DOCUMENT_HANDLE_BAD
///
/// illegal document handle
///
/// Will be raised when a document handle is corrupt.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_DOCUMENT_HANDLE_BAD                             (1205)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1206: ERROR_AVOCADO_MAXIMAL_SIZE_TOO_SMALL
///
/// maixaml size of journal too small
///
/// Will be raised when the maximal size of the journal is too small.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_MAXIMAL_SIZE_TOO_SMALL                          (1206)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1300: ERROR_AVOCADO_DATAFILE_FULL
///
/// datafile full
///
/// Will be raised when the datafile reaches its limit.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_AVOCADO_DATAFILE_FULL                                   (1300)

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
/// parse error: %s
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
/// @brief 1503: ERROR_QUERY_SPECIFICATION_INVALID
///
/// query specification invalid
///
/// Will be raised when a query is sent to the server with an incomplete or
/// invalid query specification structure.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_SPECIFICATION_INVALID                             (1503)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1504: ERROR_QUERY_NUMBER_OUT_OF_RANGE
///
/// number '%s' is out of range
///
/// Will be raised when a numeric value inside a query is out of the allowed
/// value range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               (1504)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1521: ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE
///
/// limit value '%s' is out of range
///
/// Will be raised when a limit value in the query is outside the allowed range
/// (e. g. when passing a negative skip value).
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE                          (1521)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1505: ERROR_QUERY_TOO_MANY_JOINS
///
/// too many joins.
///
/// Will be raised when the number of joins in a query is beyond the allowed
/// value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_TOO_MANY_JOINS                                    (1505)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1506: ERROR_QUERY_COLLECTION_NAME_INVALID
///
/// collection name '%s' is invalid
///
/// Will be raised when an invalid collection name is used in the from clause
/// of a query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_NAME_INVALID                           (1506)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1507: ERROR_QUERY_COLLECTION_ALIAS_INVALID
///
/// collection alias '%s' is invalid
///
/// Will be raised when an invalid alias name is used for a collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID                          (1507)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1508: ERROR_QUERY_COLLECTION_ALIAS_REDECLARED
///
/// collection alias '%s' is declared multiple times in the same query
///
/// Will be raised when the same alias name is declared multiple times in the
/// same query's from clause.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED                       (1508)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1509: ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED
///
/// collection alias '%s' is used but was not declared in the from clause
///
/// Will be raised when an alias not declared in the from clause is used in the
/// query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED                       (1509)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1510: ERROR_QUERY_COLLECTION_NOT_FOUND
///
/// unable to open collection '%s'
///
/// Will be raised when one of the collections referenced in the query was not
/// found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_NOT_FOUND                              (1510)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1511: ERROR_QUERY_GEO_RESTRICTION_INVALID
///
/// geo restriction for alias '%s' is invalid
///
/// Will be raised when a specified geo restriction is invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID                           (1511)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1512: ERROR_QUERY_GEO_INDEX_MISSING
///
/// no suitable geo index found for geo restriction on '%s'
///
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 (1512)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1513: ERROR_QUERY_BIND_PARAMETER_MISSING
///
/// no value specified for declared bind parameter '%s'
///
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            (1513)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1514: ERROR_QUERY_BIND_PARAMETER_REDECLARED
///
/// value for bind parameter '%s' is declared multiple times
///
/// Will be raised when a value gets specified multiple times for the same bind
/// parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED                         (1514)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1515: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
///
/// bind parameter '%s' was not declared in the query
///
/// Will be raised when a value gets specified for an undeclared bind parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         (1515)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1516: ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID
///
/// invalid value for bind parameter '%s'
///
/// Will be raised when an invalid value is specified for one of the bind
/// parameters.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID                      (1516)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1517: ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE
///
/// bind parameter number '%s' out of range
///
/// Will be specified when the numeric index for a bind parameter of type @n is
/// out of the allowed range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE                (1517)

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
/// @brief 1700: ERROR_SESSION_USERHANDLER_URL_INVALID
///
/// expecting \<prefix\>/user/\<username\>
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERHANDLER_URL_INVALID                         (1700)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1701: ERROR_SESSION_USERHANDLER_CANNOT_CREATE_USER
///
/// cannot create user
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERHANDLER_CANNOT_CREATE_USER                  (1701)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1702: ERROR_SESSION_USERHANDLER_ROLE_NOT_FOUND
///
/// role not found
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERHANDLER_ROLE_NOT_FOUND                      (1702)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1703: ERROR_SESSION_USERHANDLER_NO_CREATE_PERMISSION
///
/// no permission to create user with that role
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERHANDLER_NO_CREATE_PERMISSION                (1703)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1704: ERROR_SESSION_USERHANDLER_USER_NOT_FOUND
///
/// user not found
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERHANDLER_USER_NOT_FOUND                      (1704)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1705: ERROR_SESSION_USERHANDLER_CANNOT_CHANGE_PW
///
/// cannot manage password for user
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERHANDLER_CANNOT_CHANGE_PW                    (1705)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1706: ERROR_SESSION_SESSIONHANDLER_URL_INVALID1
///
/// expecting POST \<prefix\>/session
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID1                     (1706)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1707: ERROR_SESSION_SESSIONHANDLER_URL_INVALID2
///
/// expecting GET \<prefix\>/session/\<sid\>
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID2                     (1707)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1708: ERROR_SESSION_SESSIONHANDLER_URL_INVALID3
///
/// expecting PUT \<prefix\>/session/\<sid\>/\<method\>
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID3                     (1708)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1709: ERROR_SESSION_SESSIONHANDLER_URL_INVALID4
///
/// expecting DELETE \<prefix\>/session/\<sid\>
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID4                     (1709)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1710: ERROR_SESSION_SESSIONHANDLER_SESSION_UNKNOWN
///
/// unknown session
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_SESSION_UNKNOWN                  (1710)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1711: ERROR_SESSION_SESSIONHANDLER_SESSION_NOT_BOUND
///
/// session has not bound to user
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_SESSION_NOT_BOUND                (1711)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1712: ERROR_SESSION_SESSIONHANDLER_CANNOT_LOGIN
///
/// cannot login with session
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_CANNOT_LOGIN                     (1712)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1713: ERROR_SESSION_USERSHANDLER_INVALID_URL
///
/// expecting GET \<prefix\>/users
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERSHANDLER_INVALID_URL                        (1713)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1714: ERROR_SESSION_DIRECTORYSERVER_INVALID_URL
///
/// expecting /directory/sessionvoc/\<token\>
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_DIRECTORYSERVER_INVALID_URL                     (1714)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1715: ERROR_SESSION_DIRECTORYSERVER_NOT_CONFIGURED
///
/// directory server is not configured
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_DIRECTORYSERVER_NOT_CONFIGURED                  (1715)

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
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

