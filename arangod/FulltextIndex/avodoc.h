/* avodoc.h - header file for FTS access to documents  */
/*   R. A. Parker    16.7.2012  */

typedef uint64_t FTS_collection_id_t;
typedef uint64_t FTS_document_id_t;

typedef struct
{
    size_t _len;
    uint8_t * * _texts;
    void (*free)(void *);
}   FTS_texts_t;

FTS_texts_t * FTS_GetTexts 
   (FTS_collection_id_t colid, FTS_document_id_t docid);

/* end of avodoc.h */


