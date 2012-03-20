
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
///   TODO
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
///   Will be raised when a numeric value inside a query is out of the allowed
///   value range.
/// - 1521: @CODE{limit value '\%s' is out of range}
///   Will be raised when a limit value in the query is outside the allowed
///   range (e. g. when passing a negative skip value).
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
///   Will be specified when the numeric index for a bind parameter of type @n
///   is out of the allowed range.
/// - 1600: @CODE{cursor not found}
///   Will be raised when a cursor is requested via its id but a cursor with
///   that id cannot be found.
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

void TRI_InitialiseAvocadoErrors (void);

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
/// TODO
////////////////////////////////////////////////////////////////////////////////

#define TRI_VOC_ERROR_DOCUMENT_NOT_FOUND                                  (1200)

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
///
/// query killed
///
/// Will be raised when a running query is killed by an explicit admin command.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_KILLED                                            (1501)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1510: ERROR_QUERY_PARSE
///
/// parse error: %s
///
/// Will be raised when query is parsed and is found to be syntactially invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_PARSE                                             (1510)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1511: ERROR_QUERY_EMPTY
///
/// query is empty
///
/// Will be raised when an empty query is specified.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_EMPTY                                             (1511)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1512: ERROR_QUERY_SPECIFICATION_INVALID
///
/// query specification invalid
///
/// Will be raised when a query is sent to the server with an incomplete or
/// invalid query specification structure.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_SPECIFICATION_INVALID                             (1512)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1520: ERROR_QUERY_NUMBER_OUT_OF_RANGE
///
/// number '%s' is out of range
///
/// Will be raised when a numeric value inside a query is out of the allowed
/// value range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE                               (1520)

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
/// @brief 1540: ERROR_QUERY_TOO_MANY_JOINS
///
/// too many joins.
///
/// Will be raised when the number of joins in a query is beyond the allowed
/// value.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_TOO_MANY_JOINS                                    (1540)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1550: ERROR_QUERY_COLLECTION_NAME_INVALID
///
/// collection name '%s' is invalid
///
/// Will be raised when an invalid collection name is used in the from clause
/// of a query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_NAME_INVALID                           (1550)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1551: ERROR_QUERY_COLLECTION_ALIAS_INVALID
///
/// collection alias '%s' is invalid
///
/// Will be raised when an invalid alias name is used for a collection.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_INVALID                          (1551)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1552: ERROR_QUERY_COLLECTION_ALIAS_REDECLARED
///
/// collection alias '%s' is declared multiple times in the same query
///
/// Will be raised when the same alias name is declared multiple times in the
/// same query's from clause.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_REDECLARED                       (1552)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1553: ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED
///
/// collection alias '%s' is used but was not declared in the from clause
///
/// Will be raised when an alias not declared in the from clause is used in the
/// query.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_ALIAS_UNDECLARED                       (1553)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1560: ERROR_QUERY_COLLECTION_NOT_FOUND
///
/// unable to open collection '%s'
///
/// Will be raised when one of the collections referenced in the query was not
/// found.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_COLLECTION_NOT_FOUND                              (1560)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1570: ERROR_QUERY_GEO_RESTRICTION_INVALID
///
/// geo restriction for alias '%s' is invalid
///
/// Will be raised when a specified geo restriction is invalid.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_RESTRICTION_INVALID                           (1570)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1571: ERROR_QUERY_GEO_INDEX_MISSING
///
/// no suitable geo index found for geo restriction on '%s'
///
/// Will be raised when a geo restriction was specified but no suitable geo
/// index is found to resolve it.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_GEO_INDEX_MISSING                                 (1571)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1590: ERROR_QUERY_BIND_PARAMETER_MISSING
///
/// no value specified for declared bind parameter '%s'
///
/// Will be raised when a bind parameter was declared in the query but the
/// query is being executed with no value for that parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_MISSING                            (1590)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1591: ERROR_QUERY_BIND_PARAMETER_REDECLARED
///
/// value for bind parameter '%s' is declared multiple times
///
/// Will be raised when a value gets specified multiple times for the same bind
/// parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_REDECLARED                         (1591)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1592: ERROR_QUERY_BIND_PARAMETER_UNDECLARED
///
/// bind parameter '%s' was not declared in the query
///
/// Will be raised when a value gets specified for an undeclared bind parameter.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED                         (1592)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1593: ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID
///
/// invalid value for bind parameter '%s'
///
/// Will be raised when an invalid value is specified for one of the bind
/// parameters.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_VALUE_INVALID                      (1593)

////////////////////////////////////////////////////////////////////////////////
/// @brief 1594: ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE
///
/// bind parameter number '%s' out of range
///
/// Will be specified when the numeric index for a bind parameter of type @n is
/// out of the allowed range.
////////////////////////////////////////////////////////////////////////////////

#define TRI_ERROR_QUERY_BIND_PARAMETER_NUMBER_OUT_OF_RANGE                (1594)

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
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

