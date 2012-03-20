////////////////////////////////////////////////////////////////////////////////
/// @brief auto-generated file generated from errors.dat
////////////////////////////////////////////////////////////////////////////////

#include <BasicsC/common.h>
#include "VocBase/voc-errors.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup VocError
/// @{
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseAvocadoErrors (void) {
  REG_ERROR(VOC_ERROR_ILLEGAL_STATE, "illegal state");
  REG_ERROR(VOC_ERROR_SHAPER_FAILED, "illegal shaper");
  REG_ERROR(VOC_ERROR_CORRUPTED_DATAFILE, "corrupted datafile");
  REG_ERROR(VOC_ERROR_MMAP_FAILED, "mmap failed");
  REG_ERROR(VOC_ERROR_MSYNC_FAILED, "msync failed");
  REG_ERROR(VOC_ERROR_NO_JOURNAL, "no journal");
  REG_ERROR(VOC_ERROR_DATAFILE_SEALED, "datafile sealed");
  REG_ERROR(VOC_ERROR_CORRUPTED_COLLECTION, "corrupted collection");
  REG_ERROR(VOC_ERROR_UNKNOWN_TYPE, "unknown type");
  REG_ERROR(VOC_ERROR_ILLEGAL_PARAMETER, "illegal parameter");
  REG_ERROR(VOC_ERROR_INDEX_EXISTS, "index exists");
  REG_ERROR(VOC_ERROR_CONFLICT, "conflict");
  REG_ERROR(VOC_ERROR_WRONG_PATH, "wrong path");
  REG_ERROR(VOC_ERROR_CANNOT_RENAME, "cannot rename");
  REG_ERROR(VOC_ERROR_WRITE_FAILED, "write failed");
  REG_ERROR(VOC_ERROR_READ_ONLY, "ready only");
  REG_ERROR(VOC_ERROR_DATAFILE_FULL, "datafile full");
  REG_ERROR(VOC_ERROR_FILESYSTEM_FULL, "filesystem full");
  REG_ERROR(VOC_ERROR_READ_FAILED, "read failed");
  REG_ERROR(VOC_ERROR_FILE_NOT_FOUND, "file not found");
  REG_ERROR(VOC_ERROR_FILE_NOT_ACCESSIBLE, "file not accessible");
  REG_ERROR(VOC_ERROR_DOCUMENT_NOT_FOUND, "document not found");
  REG_ERROR(ERROR_QUERY_OOM, "out of memory");
  REG_ERROR(ERROR_QUERY_KILLED, "query killed");
  REG_ERROR(ERROR_QUERY_PARSE, "parse error: %s");
  REG_ERROR(ERROR_QUERY_EMPTY, "query is empty");
  REG_ERROR(ERROR_QUERY_SPECIFICATION_INVALID, "query specification invalid");
  REG_ERROR(ERROR_QUERY_NUMBER_OUT_OF_RANGE, "number '%s' is out of range");
  REG_ERROR(ERROR_QUERY_LIMIT_VALUE_OUT_OF_RANGE, "limit value '%s' is out of range");
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
  REG_ERROR(ERROR_CURSOR_NOT_FOUND, "cursor not found");
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

