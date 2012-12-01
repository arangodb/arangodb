/* avodoc.h - header file for FTS access to documents  */
/*   R. A. Parker    16.7.2012  */

#ifndef TRIAGENS_FULLTEXT_AVODOC_H
#define TRIAGENS_FULLTEXT_AVODOC_H 1

#include "BasicsC/common.h"

#include "FulltextIndex/FTS_index.h"

FTS_texts_t * FTS_GetTexts 
   (FTS_collection_id_t colid, FTS_document_id_t docid);

#endif
