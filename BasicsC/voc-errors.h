
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
<<<<<<< HEAD
///   TODO
/// - 1: @CODE{failed}
///   TODO
/// - 2: @CODE{system error}
///   TODO
/// - 3: @CODE{out of memory}
///   TODO
/// - 4: @CODE{internal error}
///   TODO
/// - 5: @CODE{illegal number}
///   TODO
/// - 6: @CODE{numeric overflow}
///   TODO
/// - 7: @CODE{illegal option}
///   TODO
/// - 8: @CODE{dead process identifier}
///   TODO
/// - 9: @CODE{open/create file failed}
///   TODO
/// - 10: @CODE{write failed}
///   TODO
/// - 11: @CODE{lock failed}
///   TODO
/// - 12: @CODE{unlock failed}
///   TODO
/// - 1000: @CODE{illegal state}
///   TODO
/// - 1001: @CODE{illegal shaper}
///   TODO
/// - 1002: @CODE{corrupted datafile}
///   TODO
/// - 1003: @CODE{mmap failed}
///   TODO
/// - 1004: @CODE{msync failed}
///   TODO
/// - 1005: @CODE{no journal}
///   TODO
/// - 1006: @CODE{datafile sealed}
///   TODO
/// - 1007: @CODE{corrupted collection}
///   TODO
/// - 1008: @CODE{unknown type}
///   TODO
/// - 1009: @CODE{illegal parameter}
///   TODO
/// - 1010: @CODE{index exists}
///   TODO
/// - 1011: @CODE{conflict}
///   TODO
/// - 1100: @CODE{wrong path}
///   TODO
/// - 1101: @CODE{cannot rename}
///   TODO
/// - 1102: @CODE{write failed}
///   TODO
/// - 1103: @CODE{ready only}
///   TODO
/// - 1104: @CODE{datafile full}
///   TODO
/// - 1105: @CODE{filesystem full}
///   TODO
/// - 1106: @CODE{read failed}
///   TODO
/// - 1107: @CODE{file not found}
///   TODO
/// - 1108: @CODE{file not accessible}
///   TODO
/// - 1200: @CODE{document not found}
///   Will be raised when a doucment with a given identifier or handle is
///   unknown.
/// - 1201: @CODE{collection not found}
///   Will be raised when a collection with a given identifier or name is
///   unknown.
/// - 1202: @CODE{parameter collection not found}
///   Will be raised when the collection parameter is missing.
/// - 1203: @CODE{document altered}
///   Will be raised when a document has been altered and a change would result
///   in a conflict.
/// - 1204: @CODE{illegal document handle}
///   Will be raised when a document handle is corrupt.
/// - 1205: @CODE{collection already exists}
///   Will be raised when a collection with a given identifier or name already
///   exists.
/// - 1500: @CODE{out of memory}
///   Will be raised during query execution when a memory allocation request
///   can not be satisfied.
/// - 1501: @CODE{query killed}
///   Will be raised when a running query is killed by an explicit admin
///   command.
/// - 1510: @CODE{parse error: \%s}
///   Will be raised when query is parsed and is found to be syntactially
///   invalid.
/// - 1511: @CODE{query is empty}
///   Will be raised when an empty query is specified.
/// - 1512: @CODE{query specification invalid}
///   Will be raised when a query is sent to the server with an incomplete or
///   invalid query specification structure.
/// - 1520: @CODE{number '\%s' is out of range}
=======
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
///   Will be raised when the a bad does not fulfill the requirements.
/// - 405: @CODE{method not supported}
///   Will be raised when an unsupported HTTP method is used for an operation.
/// - 600: @CODE{invalid JSON object}
///   Will be raised when a string representation an JSON object is corrupt."
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
/// - 1106: @CODE{cannot rename, file ready exists}
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
/// - 1540: @CODE{too many joins.}
///   Will be raised when the number of joins in a query is beyond the allowed
///   value.
/// - 1550: @CODE{collection name '\%s' is invalid}
///   Will be raised when an invalid collection name is used in the from clause
///   of a query.
/// - 1551: @CODE{collection alias '\%s' is invalid}
///   Will be raised when an invalid alias name is used for a collection.
/// - 1552: @CODE{collection alias '\%s' is declared multiple times in the same query}
///   Will be raised when the same alias name is declared multiple times in the
///   same query's from clause.
/// - 1553: @CODE{collection alias '\%s' is used but was not declared in the from clause}
///   Will be raised when an alias not declared in the from clause is used in
///   the query.
/// - 1560: @CODE{unable to open collection '\%s'}
///   Will be raised when one of the collections referenced in the query was
///   not found.
/// - 1570: @CODE{geo restriction for alias '\%s' is invalid}
///   Will be raised when a specified geo restriction is invalid.
/// - 1571: @CODE{no suitable geo index found for geo restriction on '\%s'}
///   Will be raised when a geo restriction was specified but no suitable geo
///   index is found to resolve it.
/// - 1590: @CODE{no value specified for declared bind parameter '\%s'}
///   Will be raised when a bind parameter was declared in the query but the
///   query is being executed with no value for that parameter.
/// - 1591: @CODE{value for bind parameter '\%s' is declared multiple times}
///   Will be raised when a value gets specified multiple times for the same
///   bind parameter.
/// - 1592: @CODE{bind parameter '\%s' was not declared in the query}
///   Will be raised when a value gets specified for an undeclared bind
///   parameter.
/// - 1593: @CODE{invalid value for bind parameter '\%s'}
///   Will be raised when an invalid value is specified for one of the bind
///   parameters.
/// - 1594: @CODE{bind parameter number '\%s' out of range}
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
/// - 1700: @CODE{expecting <prefix>/user/<username>}
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
/// - 1706: @CODE{expecting POST <prefix>/session}
///   TODO
/// - 1707: @CODE{expecting GET <prefix>/session/<sid>}
///   TODO
/// - 1708: @CODE{expecting PUT <prefix>/session/<sid>/<method>}
///   TODO
/// - 1709: @CODE{expecting DELETE <prefix>/session/<sid>}
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
/// - 1713: @CODE{expecting GET <prefix>/users}
///   TODO
/// - 1714: @CODE{expecting /directory/sessionvoc/<token>}
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
/// - 3000: @CODE{method not supported}
///   Will be raised when an unsupported HTTP method is used for an operation.

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
/// TODO
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
<<<<<<< HEAD
/// @brief 9: ERROR_OPEN_ERROR
///
/// open/create file failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_OPEN_ERROR                                              (9)

////////////////////////////////////////////////////////////////////////////////
/// @brief 10: ERROR_WRITE_ERROR
///
/// write failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_WRITE_ERROR                                             (10)

////////////////////////////////////////////////////////////////////////////////
/// @brief 11: ERROR_LOCK_ERROR
///
/// lock failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_LOCK_ERROR                                              (11)

////////////////////////////////////////////////////////////////////////////////
/// @brief 12: ERROR_UNLOCKED_FILE
///
/// unlock failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_UNLOCKED_FILE                                           (12)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1000: VOC_ERROR_ILLEGAL_STATE
///
/// illegal state
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_ILLEGAL_STATE                                       (1000)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1001: VOC_ERROR_SHAPER_FAILED
///
/// illegal shaper
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_SHAPER_FAILED                                       (1001)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1002: VOC_ERROR_CORRUPTED_DATAFILE
///
/// corrupted datafile
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_CORRUPTED_DATAFILE                                  (1002)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1003: VOC_ERROR_MMAP_FAILED
///
/// mmap failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_MMAP_FAILED                                         (1003)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1004: VOC_ERROR_MSYNC_FAILED
///
/// msync failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_MSYNC_FAILED                                        (1004)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1005: VOC_ERROR_NO_JOURNAL
///
/// no journal
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_NO_JOURNAL                                          (1005)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1006: VOC_ERROR_DATAFILE_SEALED
///
/// datafile sealed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_DATAFILE_SEALED                                     (1006)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1007: VOC_ERROR_CORRUPTED_COLLECTION
///
/// corrupted collection
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_CORRUPTED_COLLECTION                                (1007)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1008: VOC_ERROR_UNKNOWN_TYPE
///
/// unknown type
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_UNKNOWN_TYPE                                        (1008)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1009: VOC_ERROR_ILLEGAL_PARAMETER
///
/// illegal parameter
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_ILLEGAL_PARAMETER                                   (1009)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1010: VOC_ERROR_INDEX_EXISTS
///
/// index exists
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_INDEX_EXISTS                                        (1010)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1011: VOC_ERROR_CONFLICT
///
/// conflict
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_CONFLICT                                            (1011)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1100: VOC_ERROR_WRONG_PATH
///
/// wrong path
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_WRONG_PATH                                          (1100)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1101: VOC_ERROR_CANNOT_RENAME
///
/// cannot rename
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_CANNOT_RENAME                                       (1101)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1102: VOC_ERROR_WRITE_FAILED
///
/// write failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_WRITE_FAILED                                        (1102)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1103: VOC_ERROR_READ_ONLY
///
/// ready only
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_READ_ONLY                                           (1103)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1104: VOC_ERROR_DATAFILE_FULL
///
/// datafile full
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_DATAFILE_FULL                                       (1104)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1105: VOC_ERROR_FILESYSTEM_FULL
///
/// filesystem full
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_FILESYSTEM_FULL                                     (1105)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1106: VOC_ERROR_READ_FAILED
///
/// read failed
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_READ_FAILED                                         (1106)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1107: VOC_ERROR_FILE_NOT_FOUND
///
/// file not found
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_FILE_NOT_FOUND                                      (1107)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1108: VOC_ERROR_FILE_NOT_ACCESSIBLE
///
/// file not accessible
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_FILE_NOT_ACCESSIBLE                                 (1108)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1200: VOC_ERROR_DOCUMENT_NOT_FOUND
///
/// document not found
///
/// Will be raised when a doucment with a given identifier or handle is unknown.
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_DOCUMENT_NOT_FOUND                                  (1200)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1201: VOC_ERROR_COLLECTION_NOT_FOUND
=======
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
/// Will be raised when the a bad does not fulfill the requirements.
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
/// Will be raised when a string representation an JSON object is corrupt."
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
/// cannot rename, file ready exists
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
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// collection not found
///
/// Will be raised when a collection with a given identifier or name is unknown.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_VOC_ERROR_COLLECTION_NOT_FOUND                                (1201)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1202: VOC_ERROR_COLLECTION_PARAMETER_MISSING
///
/// parameter collection not found
=======
#define TRI_ERROR_AVOCADO_COLLECTION_NOT_FOUND                            (1203)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1204: ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING
///
/// parameter 'collection' not found
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// Will be raised when the collection parameter is missing.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_VOC_ERROR_COLLECTION_PARAMETER_MISSING                        (1202)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1203: VOC_ERROR_DOCUMENT_ALTERED
///
/// document altered
///
/// Will be raised when a document has been altered and a change would result
/// in a conflict.
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_DOCUMENT_ALTERED                                    (1203)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1204: VOC_ERROR_DOCUMENT_HANDLE_BAD
=======
#define TRI_ERROR_AVOCADO_COLLECTION_PARAMETER_MISSING                    (1204)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1205: ERROR_AVOCADO_DOCUMENT_HANDLE_BAD
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// illegal document handle
///
/// Will be raised when a document handle is corrupt.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_VOC_ERROR_DOCUMENT_HANDLE_BAD                                 (1204)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1205: VOC_ERROR_COLLECTION_EXISTS
///
/// collection already exists
///
/// Will be raised when a collection with a given identifier or name already
/// exists.
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_COLLECTION_EXISTS                                   (1205)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1500: ERROR_QUERY_OOM
///
/// out of memory
///
/// Will be raised during query execution when a memory allocation request can
/// not be satisfied.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_OOM                                               (1500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1501: ERROR_QUERY_KILLED
=======
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
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// query killed
///
/// Will be raised when a running query is killed by an explicit admin command.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_KILLED                                            (1501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1510: ERROR_QUERY_PARSE
=======
#define TRI_ERROR_QUERY_KILLED                                            (1500)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1501: ERROR_QUERY_PARSE
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// parse error: %s
///
/// Will be raised when query is parsed and is found to be syntactially invalid.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_PARSE                                             (1510)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1511: ERROR_QUERY_EMPTY
=======
#define TRI_ERROR_QUERY_PARSE                                             (1501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1502: ERROR_QUERY_EMPTY
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// query is empty
///
/// Will be raised when an empty query is specified.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_EMPTY                                             (1511)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1512: ERROR_QUERY_SPECIFICATION_INVALID
=======
#define TRI_ERROR_QUERY_EMPTY                                             (1502)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1503: ERROR_QUERY_SPECIFICATION_INVALID
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// query specification invalid
///
/// Will be raised when a query is sent to the server with an incomplete or
/// invalid query specification structure.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_SPECIFICATION_INVALID                             (1512)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1520: ERROR_QUERY_NUMBER_OUT_OF_RANGE
=======
#define TRI_ERROR_QUERY_SPECIFICATION_INVALID                             (1503)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1504: ERROR_QUERY_NUMBER_OUT_OF_RANGE
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// number '%s' is out of range
///
/// Will be raised when a numeric value inside a query is out of the allowed
/// value range.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               (1520)
=======
#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               (1504)
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf

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
<<<<<<< HEAD
/// @brief 1540: ERROR_QUERY_TOO_MANY_JOINS
=======
/// @brief 1505: ERROR_QUERY_TOO_MANY_JOINS
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// too many joins.
///
/// Will be raised when the number of joins in a query is beyond the allowed
/// value.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_TOO_MANY_JOINS                                    (1540)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1550: ERROR_QUERY_COLLECTION_NAME_INVALID
=======
#define TRI_ERROR_QUERY_TOO_MANY_JOINS                                    (1505)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1506: ERROR_QUERY_COLLECTION_NAME_INVALID
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// collection name '%s' is invalid
///
/// Will be raised when an invalid collection name is used in the from clause
/// of a query.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_COLLECTION_NAME_INVALID                           (1550)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1551: ERROR_QUERY_COLLECTION_ALIAS_INVALID
=======
#define TRI_ERROR_QUERY_COLLECTION_NAME_INVALID                           (1506)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1507: ERROR_QUERY_COLLECTION_ALIAS_INVALID
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// collection alias '%s' is invalid
///
/// Will be raised when an invalid alias name is used for a collection.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID                          (1551)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1552: ERROR_QUERY_COLLECTION_ALIAS_REDECLARED
=======
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID                          (1507)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1508: ERROR_QUERY_COLLECTION_ALIAS_REDECLARED
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// collection alias '%s' is declared multiple times in the same query
///
/// Will be raised when the same alias name is declared multiple times in the
/// same query's from clause.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED                       (1552)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1553: ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED
=======
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED                       (1508)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1509: ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// collection alias '%s' is used but was not declared in the from clause
///
/// Will be raised when an alias not declared in the from clause is used in the
/// query.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED                       (1553)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1560: ERROR_QUERY_COLLECTION_NOT_FOUND
=======
#define TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED                       (1509)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1510: ERROR_QUERY_COLLECTION_NOT_FOUND
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// unable to open collection '%s'
///
/// Will be raised when one of the collections referenced in the query was not
/// found.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_COLLECTION_NOT_FOUND                              (1560)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1570: ERROR_QUERY_GEO_RESTRICTION_INVALID
=======
#define TRI_ERROR_QUERY_COLLECTION_NOT_FOUND                              (1510)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1511: ERROR_QUERY_GEO_RESTRICTION_INVALID
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// geo restriction for alias '%s' is invalid
///
/// Will be raised when a specified geo restriction is invalid.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID                           (1570)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1571: ERROR_QUERY_GEO_INDEX_MISSING
=======
#define TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID                           (1511)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1512: ERROR_QUERY_GEO_INDEX_MISSING
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// no suitable geo index found for geo restriction on '%s'
///
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 (1571)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1590: ERROR_QUERY_BIND_PARAMETER_MISSING
=======
#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 (1512)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1513: ERROR_QUERY_BIND_PARAMETER_MISSING
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// no value specified for declared bind parameter '%s'
///
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            (1590)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1591: ERROR_QUERY_BIND_PARAMETER_REDECLARED
=======
#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            (1513)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1514: ERROR_QUERY_BIND_PARAMETER_REDECLARED
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// value for bind parameter '%s' is declared multiple times
///
/// Will be raised when a value gets specified multiple times for the same bind
/// parameter.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED                         (1591)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1592: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
=======
#define TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED                         (1514)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1515: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// bind parameter '%s' was not declared in the query
///
/// Will be raised when a value gets specified for an undeclared bind parameter.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         (1592)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1593: ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID
=======
#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         (1515)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1516: ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// invalid value for bind parameter '%s'
///
/// Will be raised when an invalid value is specified for one of the bind
/// parameters.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID                      (1593)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1594: ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE
=======
#define TRI_ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID                      (1516)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1517: ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// bind parameter number '%s' out of range
///
/// Will be specified when the numeric index for a bind parameter of type @n is
/// out of the allowed range.
////////////////////////////////////////////////////////////////////////////////

<<<<<<< HEAD
#define TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE                (1594)
=======
#define TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE                (1517)
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf

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
<<<<<<< HEAD
/// expecting <prefix>/user/<username>
=======
/// expecting \<prefix\>/user/\<username\>
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
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
<<<<<<< HEAD
/// expecting POST <prefix>/session
=======
/// expecting POST \<prefix\>/session
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID1                     (1706)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1707: ERROR_SESSION_SESSIONHANDLER_URL_INVALID2
///
<<<<<<< HEAD
/// expecting GET <prefix>/session/<sid>
=======
/// expecting GET \<prefix\>/session/\<sid\>
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID2                     (1707)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1708: ERROR_SESSION_SESSIONHANDLER_URL_INVALID3
///
<<<<<<< HEAD
/// expecting PUT <prefix>/session/<sid>/<method>
=======
/// expecting PUT \<prefix\>/session/\<sid\>/\<method\>
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_SESSIONHANDLER_URL_INVALID3                     (1708)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1709: ERROR_SESSION_SESSIONHANDLER_URL_INVALID4
///
<<<<<<< HEAD
/// expecting DELETE <prefix>/session/<sid>
=======
/// expecting DELETE \<prefix\>/session/\<sid\>
>>>>>>> 5803d27052780853bde39b1c784cc3867fd3b6cf
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
/// expecting GET <prefix>/users
///
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_SESSION_USERSHANDLER_INVALID_URL                        (1713)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1714: ERROR_SESSION_DIRECTORYSERVER_INVALID_URL
///
/// expecting /directory/sessionvoc/<token>
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
/// @brief 3000: ERROR_PROTOCOL_UNSUPPORTED_METHOD
///
/// method not supported
///
/// Will be raised when an unsupported HTTP method is used for an operation.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_PROTOCOL_UNSUPPORTED_METHOD                             (3000)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

