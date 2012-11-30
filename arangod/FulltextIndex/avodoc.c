/* avodoc.c - My imitation of Avocado  */
/*   R. A. Parker    26.11.2012  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "avodoc.h"

#include "FulltextIndex/FTS_index.h"


static FTS_texts_t * cons(void)
{
    FTS_texts_t * tx;
    tx=malloc(sizeof(FTS_texts_t));
    tx->_texts=malloc(10*sizeof(uint8_t *));
    return tx;
}

uint8_t w1[]="trinket";
uint8_t w2[]="fred";
uint8_t w3[]="zebra";
uint8_t w4[]="aardvark";
uint8_t w5[]="freed";
uint8_t w6[]="fredp";
uint8_t w7[]="fredq";
uint8_t w8[]="fredr";
uint8_t wp[]="fre";
uint8_t wf[]="red";

void freg(void * doc)
{
    printf("tried to free the document!\n");
}

FTS_texts_t * FTS_GetTexts 
   (FTS_collection_id_t colid, FTS_document_id_t docid)

{
    FTS_texts_t * tx;
    tx=cons();
    if( (colid==2) && (docid==2) )
    {
        tx->_len=9;
        tx->_texts[0]=w1;
        tx->_texts[1]=w2;
        tx->_texts[2]=w3;
        tx->_texts[3]=w4;
        tx->_texts[4]=w5;
        tx->_texts[5]=w6;
        tx->_texts[6]=w1;
        tx->_texts[7]=w2;
        tx->_texts[8]=w7;
    }
    if( (colid==2) && (docid==3) )
    {
        tx->_len=7;
        tx->_texts[0]=w4;
        tx->_texts[1]=w4;
        tx->_texts[2]=w4;
        tx->_texts[3]=w4;
        tx->_texts[4]=w5;
        tx->_texts[5]=w6;
        tx->_texts[6]=w4;
    }
    if( (colid==2) && (docid==5) )
    {
        tx->_len=8;
        tx->_texts[0]=w1;
        tx->_texts[1]=w1;
        tx->_texts[2]=w3;
        tx->_texts[3]=w5;
        tx->_texts[4]=w5;
        tx->_texts[5]=w7;
        tx->_texts[6]=w7;
        tx->_texts[7]=w1;
    }
    if( (colid==2) && (docid==8) )
    {
        tx->_len=10;
        tx->_texts[0]=w1;
        tx->_texts[1]=w2;
        tx->_texts[2]=w3;
        tx->_texts[3]=w4;
        tx->_texts[4]=w1;
        tx->_texts[5]=w2;
        tx->_texts[6]=w3;
        tx->_texts[7]=w4;
        tx->_texts[8]=w1;
        tx->_texts[9]=w2;
    }
    if( (colid==2) && (docid==11) )
    {
        tx->_len=6;
        tx->_texts[0]=w2;
        tx->_texts[1]=w3;
        tx->_texts[2]=w4;
        tx->_texts[3]=w4;
        tx->_texts[4]=w7;
        tx->_texts[5]=w4;
    }
    if( (colid==1) && (docid==2) )
    {
        tx->_len=9;
        tx->_texts[0]=w1;
        tx->_texts[1]=w2;
        tx->_texts[2]=w3;
        tx->_texts[3]=w4;
        tx->_texts[4]=w5;
        tx->_texts[5]=w6;
        tx->_texts[6]=w1;
        tx->_texts[7]=w2;
        tx->_texts[8]=w7;
    }
    if( (colid==1) && (docid==3) )
    {
        tx->_len=7;
        tx->_texts[0]=w4;
        tx->_texts[1]=w4;
        tx->_texts[2]=w4;
        tx->_texts[3]=w4;
        tx->_texts[4]=w5;
        tx->_texts[5]=w6;
        tx->_texts[6]=w4;
    }
    if( (colid==1) && (docid==5) )
    {
        tx->_len=8;
        tx->_texts[0]=w1;
        tx->_texts[1]=w1;
        tx->_texts[2]=w3;
        tx->_texts[3]=w5;
        tx->_texts[4]=w5;
        tx->_texts[5]=w7;
        tx->_texts[6]=w7;
        tx->_texts[7]=w1;
    }
    if( (colid==1) && (docid==8) )
    {
        tx->_len=10;
        tx->_texts[0]=w1;
        tx->_texts[1]=w2;
        tx->_texts[2]=w3;
        tx->_texts[3]=w4;
        tx->_texts[4]=w1;
        tx->_texts[5]=w2;
        tx->_texts[6]=w3;
        tx->_texts[7]=w4;
        tx->_texts[8]=w1;
        tx->_texts[9]=w2;
    }
    if( (colid==1) && (docid==11) )
    {
        tx->_len=6;
        tx->_texts[0]=w2;
        tx->_texts[1]=w3;
        tx->_texts[2]=w4;
        tx->_texts[3]=w4;
        tx->_texts[4]=w7;
        tx->_texts[5]=w4;
    }
    tx->free=freg;
    return tx;
}

#if 0
int main(int argc, char ** argv)
{
    long long x1;
    int i;
int temp;
    FTS_collection_id_t colid1;
    FTS_document_id_t docid;
    FTS_index_t * ftx, *ftx2;
    FTS_query_t query;
    FTS_document_ids_t * queryres;
    uint64_t def[10]=FTS_SIZES_DEFAULT;
    printf("Minature FTS-test program started\n");
    query._localOptions = malloc(5*sizeof(uint64_t));
    query._texts = malloc(5*sizeof(uint8_t *));
    colid1=1;

    ftx=FTS_CreateIndex(colid1,0,def);
    if(ftx==NULL)
    {
        printf("Create returned NULL, so giving up\n");
        return 1;
    }
    printf("Managed to create an index . . . so far so good\n");
    docid=11;
    FTS_AddDocument(ftx,docid);
    printf("Added document 11\n");
    docid=2;
    FTS_AddDocument(ftx,docid);
    printf("Added document 2\n");
    docid=3;
    FTS_AddDocument(ftx,docid);
    printf("Added document 3\n");
    docid=5;
    FTS_AddDocument(ftx,docid);
    printf("Added document 5\n");
    docid=8;
    FTS_AddDocument(ftx,docid);
    printf("Added document 8\n");
    FTS_BackgroundTask(ftx);
    printf("Came out of background task\n");
    FTS_BackgroundTask(ftx);
    printf("Came out of background task again\n");
/*    indexd(ftx);   */
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[0] = w1;  
    queryres = FTS_FindDocuments(ftx,&query);
    x1=queryres->_len;
    printf("Resulted in %lld documents\n",x1);
    for(i=0;i<x1;i++)
    {
        temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);

    query._globalOptions = 0;
    query._len = 2;
    query._localOptions[0]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[0] = w4;  
    query._localOptions[1]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[1] = w2; 
    queryres = FTS_FindDocuments(ftx,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);

    docid=8;
    FTS_DeleteDocument(ftx,docid);
    printf("Deleted document 8\n");
/* first query */
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[0] = w1;  
    queryres = FTS_FindDocuments(ftx,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);
/* second query */
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_PREFIX;   /* whole word */  
    query._texts[0] = wp;  
    queryres = FTS_FindDocuments(ftx,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);
/* third query */
    query._globalOptions = 0;
    query._len = 2;
    query._localOptions[0]=FTS_MATCH_COMPLETE;
    query._localOptions[1]=FTS_MATCH_PREFIX;   /* whole word */  
    query._texts[0] = w1;
    query._texts[1] = wp;  
    queryres = FTS_FindDocuments(ftx,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);
/* end of queries */
/* now create an index with partial words allowed */

    colid1=2;
    ftx2=FTS_CreateIndex(colid1,FTS_INDEX_SUBSTRINGS,def);
    if(ftx2==NULL)
    {
        printf("Create returned NULL, so giving up\n");
        return 1;
    }
    printf("Managed to create an index . . . so far so good\n");
    docid=11;
    FTS_AddDocument(ftx2,docid);
    printf("Added document 11\n");
    docid=2;
    FTS_AddDocument(ftx2,docid);
    printf("Added document 2\n");
    docid=3;
    FTS_AddDocument(ftx2,docid);
    printf("Added document 3\n");
    docid=5;
    FTS_AddDocument(ftx2,docid);
    printf("Added document 5\n");
    docid=8;
    FTS_AddDocument(ftx2,docid);
    printf("Added document 8\n");
    FTS_BackgroundTask(ftx2);
    printf("Came out of background task\n");
    FTS_BackgroundTask(ftx2);
    printf("Came out of background task again\n");
/*    indexd(ftx2);   */
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[0] = w1;  
    queryres = FTS_FindDocuments(ftx2,&query);
    x1=queryres->_len;
    printf("Resulted in %lld documents\n",x1);
    for(i=0;i<x1;i++)
    {
        temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);

    query._globalOptions = 0;
    query._len = 2;
    query._localOptions[0]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[0] = w4;  
    query._localOptions[1]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[1] = w2; 
    queryres = FTS_FindDocuments(ftx2,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);

    docid=2;
    FTS_DeleteDocument(ftx2,docid);
    printf("Deleted document 2\n");
    docid=8;
    FTS_DeleteDocument(ftx2,docid);
    printf("Deleted document 8\n");
/* first query */
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_COMPLETE;   /* whole word */  
    query._texts[0] = w1;  
    queryres = FTS_FindDocuments(ftx2,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);
/* second query */
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_PREFIX;   /* whole word */  
    query._texts[0] = wp;  
    queryres = FTS_FindDocuments(ftx2,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);
/* third query */
    query._globalOptions = 0;
    query._len = 2;
    query._localOptions[0]=FTS_MATCH_COMPLETE;
    query._localOptions[1]=FTS_MATCH_PREFIX;   /* whole word */  
    query._texts[0] = w1;
    query._texts[1] = wp;  
    queryres = FTS_FindDocuments(ftx2,&query);
    x1=queryres->_len;
temp=x1;
    printf("Resulted in %d documents\n",temp);
    for(i=0;i<x1;i++)
    {
temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);
    query._globalOptions = 0;
    query._len = 1;
    query._localOptions[0]=FTS_MATCH_SUBSTRING;   /* whole word */  
    query._texts[0] = wf;  
    queryres = FTS_FindDocuments(ftx2,&query);
    x1=queryres->_len;
    printf("Substring - Resulted in %lld documents\n",x1);
    for(i=0;i<x1;i++)
    {
        temp=queryres->_docs[i];
        printf(" %d",temp);
    }
    printf("\n");
    FTS_Free_Documents(queryres);

/* end of queries */
    FTS_FreeIndex(ftx2);
    FTS_FreeIndex(ftx);
    printf("First simple test completed - free'd the index again\n");
    return 0;
}
#endif

/* end of avodoc.c */


