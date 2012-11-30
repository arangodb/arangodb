/*   ftsindex.c - The Full Text Search */
/*   R. A. Parker    24.10.2012  */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "avodoc.h"
#include "zstr.h"
#include "FTS_index.h"

/* codes - in zcode.c so need externs here  */
extern ZCOD zcutf;
extern ZCOD zcbky;
extern ZCOD zcdelt;
extern ZCOD zcdoc;
extern ZCOD zckk;
extern ZCOD zcdh;

typedef struct 
{
/* first the read/write lock for the index - needs to go here  */
    int readwritelock;     /* certainly NOT an int!  */
    FTS_collection_id_t colid;   /* collection ID for this index  */
    FTS_document_id_t *handles;    /* array converting handles to docid */
    uint8_t * handsfree;
    FTS_document_id_t firstfree;   /* Start of handle free chain.  */
    FTS_document_id_t lastslot;
    int options;
    TUBER * index1;
    TUBER * index2;
    TUBER * index3;
} FTS_real_index;

/* Get a unicode character from utf-8  */

uint64_t getunicode(uint8_t ** ptr)
{
    uint64_t c1;
    c1=**ptr;
    if(c1<128)
    {
        (*ptr)++;
        return c1;
    }
    if(c1<224)
    {
        c1=((c1-192)<<6)+(*((*ptr)+1)-128);
        (*ptr)+=2;
        return c1;
    }
    if(c1<240)
    {
        c1=((c1-224)<<12)+((*((*ptr)+1)-128)<<6)
                          +(*((*ptr)+2)-128);
        (*ptr)+=3;
        return c1;
    }
    if(c1<248)
    {
        c1=((c1-240)<<18)+((*((*ptr)+1)-128)<<12)
                         +((*((*ptr)+2)-128)<<6)
                          +(*((*ptr)+3)-128);
        (*ptr)+=4;
        return c1;
    }
    return 0;
}

FTS_index_t * FTS_CreateIndex(FTS_collection_id_t coll,
    uint64_t options, uint64_t sizes[10])
/* sizes[0] = size of handles table to start with  */
/* sizes[1] = number of bytes for index 1 */
/* sizes[2] = number of bytes for index 2 */
/* sizes[3] = number of bytes for index 3 */

{
    FTS_real_index * ix;
    uint64_t bk;
    int i;
    ix=malloc(sizeof(FTS_real_index));
    if(ix==NULL) return NULL;
    ix->colid=coll;
/* TBD initialize readwritelock */
    ix->handles=malloc((sizes[0]+2)*sizeof(FTS_document_id_t));
    ix->handsfree=malloc((sizes[0]+2)*sizeof(uint8_t));
/* set up free chain of document handles  */
    for(i=1;i<sizes[0];i++)
    {
        ix->handles[i]=i+1;
        ix->handsfree[i]=1;
    }
    ix->handles[sizes[0]]=0;  /* end of free chain  */
    ix->handsfree[sizes[0]]=1;
    ix->firstfree=1;
    ix->lastslot=sizes[0];
/*     create index 2 */
    ix->index2 = ZStrTuberCons(sizes[2],TUBER_BITS_8);
    bk=ZStrTuberIns(ix->index2,0,0);
    if(bk!=0) printf("Help - Can't insert root of index 2\n");
/*     create index 3 */
    ix->index3 = ZStrTuberCons(sizes[3],TUBER_BITS_32);
    ix->options=options;
/*     create index 1 if needed */
    if(options==FTS_INDEX_SUBSTRINGS)
    {
        ix->index1 = ZStrTuberCons(sizes[1],TUBER_BITS_8);
        bk=ZStrTuberIns(ix->index1,0,0);
        if(bk!=0) printf("Help - Can't insert root of index 1\n");
    }
    return (FTS_index_t *) ix;
}

void FTS_FreeIndex ( FTS_index_t * ftx)
{
    FTS_real_index * ix;
    ix = (FTS_real_index *) ftx;
    if(ix->options==FTS_INDEX_SUBSTRINGS) ZStrTuberDest(ix->index1);
    ZStrTuberDest(ix->index2);
    ZStrTuberDest(ix->index3);
    free(ix->handsfree);
    free(ix->handles);
    free(ix);
}
int xxlet[100];
void index2dump(FTS_real_index * ix, uint64_t kkey, int lev)
{
    CTX ctx, dctx,x3ctx;
    ZSTR *zstr, *x3zstr;
    int i,temp,md;
    uint64_t x64,oldlet,newlet,bkey,newkkey;
    uint64_t docb,dock,han,oldhan;
    zstr=ZStrCons(30);
    x3zstr=ZStrCons(35);
    ZStrCxClear(&zcutf,&ctx);
    ZStrCxClear(&zcdelt,&dctx);
    ZStrCxClear(&zcdoc,&x3ctx);
    for(i=1;i<lev;i++) printf(" %c",xxlet[i]);
    i=ZStrTuberRead(ix->index2,kkey,zstr);
    temp=kkey;
    if(i!=0)
    {
        printf("cannot read kkey = %d from TUBER\n",temp);
        return;
    }
    md=ZStrBitsOut(zstr,1);
    temp=kkey;
    printf("...kkey %d ",temp);
    temp=md;
    printf("Md=%d ",temp);
    temp=zstr->dat[0];
    printf(" zstr %x",temp);
    if(md==1)
    {
        docb=ZStrCxDec(zstr,&zcbky,&ctx);
        temp=docb;
        printf(" doc-b = %d",temp);
        dock=ZStrTuberK(ix->index3,kkey,0,docb);
        temp=dock;
        printf(" doc-k = %d",temp);
    }
    oldlet=0;

    while(1)
    {
        newlet=ZStrCxDec(zstr,&zcdelt,&dctx);
        if(newlet==oldlet) break;
        bkey=ZStrCxDec(zstr,&zcbky,&ctx);
        x64=ZStrUnXl(&zcutf,newlet);
        temp=x64;
        if(temp<128)
            printf(" %c",temp);
        else
            printf(" %x",temp);
        temp=bkey;
        printf(" %d",temp);
        oldlet=newlet;
    }
    if(md==1)
    {
        printf("\n --- Docs ---");
        i=ZStrTuberRead(ix->index3,dock,x3zstr);
        oldhan=0;
        while(1)
        {
            han=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
            if(han==oldhan) break;
            temp=han;
            printf("h= %d ",temp);
            temp=ix->handles[han];
            printf("id= %d; ",temp);
            oldhan=han;
        }
    }
    printf("\n");
    i=ZStrTuberRead(ix->index2,kkey,zstr);
    x64=ZStrBitsOut(zstr,1);
    if(x64==1)
        bkey=ZStrCxDec(zstr,&zcbky,&ctx);
    oldlet=0;
    ZStrCxClear(&zcdelt,&dctx);
    while(1)
    {
        newlet=ZStrCxDec(zstr,&zcdelt,&dctx);
        if(newlet==oldlet) return;
        bkey=ZStrCxDec(zstr,&zcbky,&ctx);
        newkkey=ZStrTuberK(ix->index2,kkey,newlet,bkey);
        xxlet[lev]=ZStrUnXl(&zcutf,newlet);
        index2dump(ix,newkkey,lev+1);
        oldlet=newlet;
    }
}

void indexd(FTS_index_t * ftx)
{
    FTS_real_index * ix;
    int i;
    uint64_t kroot;
int temp;
    ix = (FTS_real_index *)ftx;
    printf("\n\nDump of Index\n");
temp=ix->firstfree;
    printf("Free-chain starts at handle %d\n",temp);
    printf("======= First ten handles======\n");
    for(i=1;i<11;i++)
    {
temp=ix->handles[i];
        printf("Handle %d is docid %d\n", i,temp);
    }
    printf("======= Index 2 ===============\n");
    kroot=ZStrTuberK(ix->index2,0,0,0);
    index2dump(ix,kroot,1);
}

void RealAddDocument(FTS_index_t * ftx, FTS_document_id_t docid)
{
    FTS_real_index * ix;
    FTS_texts_t *rawwords;
    CTX ctx2a, ctx2b, x3ctx, x3ctxb;
    STEX * stex;
    ZSTR *zstrwl, *zstr2a, *zstr2b, *x3zstr, *x3zstrb;
    uint64_t letters[42];
    uint64_t ixlet[42];
    uint64_t kkey[42];    /* for word *without* this letter */
    uint64_t kkey1[42];   /* ix1 word whose last letter is this */
    int ixlen;
    uint16_t * wpt;
    uint64_t handle, newhan, oldhan;
    uint64_t kroot,kroot1;
    int nowords,wdx;
    int i,j,len,j1,j2;
    uint8_t * utf;
    uint64_t unicode;
    uint64_t tran,x64,oldlet, newlet, bkey;
    uint64_t docb,dock;
   
    ix = (FTS_real_index *)ftx;
    kroot=ZStrTuberK(ix->index2,0,0,0);
    if(ix->options==FTS_INDEX_SUBSTRINGS)
        kroot1=ZStrTuberK(ix->index1,0,0,0);
    kkey[0]=kroot;     /* origin of index 2 */

/*     allocate the document handle  */
    handle = ix->firstfree;
/* TBD what to do if no more handles  */
    if(handle==0)
    {
        printf("Run out of document handles!\n");
        return;
    }
    ix->firstfree = ix->handles[handle];
    ix->handles[handle]=docid;
    ix->handsfree[handle]=0;
    
/*     Get the actual words from the caller  */
    rawwords = FTS_GetTexts(ix->colid, docid);
    nowords=rawwords->_len;
/*     Put the words into a STEX */

    stex=ZStrSTCons(2);  /* format 2=uint16 is all that there is! */
    zstrwl=ZStrCons(25);  /* 25 enough for word list  */
    zstr2a=ZStrCons(30);  /* 30 uint64's is always enough for ix2  */
    zstr2b=ZStrCons(30);
    x3zstr=ZStrCons(35);
    x3zstrb=ZStrCons(35);
    for(i=0;i<nowords;i++)
    {
        utf=rawwords->_texts[i];
        j=0;
        ZStrClear(zstrwl);
        unicode=getunicode(&utf);
        while(unicode!=0)
        {
            ZStrEnc(zstrwl,&zcutf,unicode);
            unicode=getunicode(&utf);
            j++;
            if(j>40) break;
        }
/* terminate the word and insert into STEX  */
        ZStrEnc(zstrwl,&zcutf,0);
        ZStrNormalize(zstrwl);
        ZStrSTAppend(stex,zstrwl);
    }
/*     Sort them */
    ZStrSTSort(stex);
/*     Set current length of word = 0 */
    ixlen=0;
/*     For each word in the STEX */
    nowords=stex->cnt;
    wpt=(uint16_t *) stex->list;
    for(wdx=0;wdx<nowords;wdx++)
    {
/*         get it out as a word  */
        ZStrInsert(zstrwl,wpt,2);
        len=0;
        while(1)
        {
            letters[len]=ZStrDec(zstrwl,&zcutf);
            if(letters[len]==0) break;
            len++;
        }

        wpt+=ZStrExtLen(wpt,2);
/*         find out where it first differs from previous one */
        for(j=0;j<ixlen;j++)
        {
            if(letters[j]!=ixlet[j]) break;
        }
/* For every new letter in the word, get its K-key into array */
        while(j<len)
        {
/*     obtain the translation of the letter  */
            tran=ZStrXlate(&zcutf,letters[j]);
/*     Get the Z-string for the index-2 entry before this letter */
            i=ZStrTuberRead(ix->index2,kkey[j],zstr2a);
            if(i==1)
            {
                printf("Kkey not found - we're buggered\n");
            }

            x64=ZStrBitsOut(zstr2a,1);
            if(x64==1) 
            {
/*     skip over the B-key into index 3  */
                docb=ZStrDec(zstr2a,&zcbky);
            }
/*     look to see if the letter is there  */
            ZStrCxClear(&zcdelt, &ctx2a);
            newlet=0;
            while(1)
            {
                oldlet=newlet;
                newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                if(newlet==oldlet) break;
                bkey=ZStrDec(zstr2a,&zcbky);
                if(newlet>=tran) break;
            }
            if(newlet != tran)
            {
/*       if not there, create a new index-2 entry for it  */
                bkey=ZStrTuberIns(ix->index2,kkey[j],tran);
                kkey[j+1]=ZStrTuberK(ix->index2,kkey[j],tran,bkey); 
/*     update old index-2 entry to insert new letter  */
                ZStrCxClear(&zcdelt, &ctx2a);
                ZStrCxClear(&zcdelt, &ctx2b);
                i=ZStrTuberRead(ix->index2,kkey[j],zstr2a);
                ZStrClear(zstr2b);
                x64=ZStrBitsOut(zstr2a,1);
                ZStrBitsIn(x64,1,zstr2b);
                if(x64==1) 
                {
/*     copy over the B-key into index 3  */
                    docb=ZStrDec(zstr2a,&zcbky);
                    ZStrEnc(zstr2b,&zcbky,docb);
                }
                newlet=0;
                while(1)
                {
                    oldlet=newlet;
                    newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                    if(newlet==oldlet) break;
                    if(newlet>tran) break;
                    ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                    x64=ZStrDec(zstr2a,&zcbky);
                    ZStrEnc(zstr2b,&zcbky,x64);
                }
                ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                ZStrEnc(zstr2b,&zcbky,bkey);
                if(newlet==oldlet)
                {
                    ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                }
                else
                {
                    while(newlet!=oldlet)
                    {
                        oldlet=newlet;
                        ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                        x64=ZStrDec(zstr2a,&zcbky);
                        ZStrEnc(zstr2b,&zcbky,x64);
                        newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                    }
                    ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                }
                ZStrNormalize(zstr2b);
                ZStrTuberUpdate(ix->index2,kkey[j],zstr2b);
            }
            else
            {
/*     - if it is, get its KKey and put in (next) slot */
                kkey[j+1]=ZStrTuberK(ix->index2,kkey[j],tran,bkey);
            }
            j++;
        }
/*      kkey[j] is kkey of whole word.     */
/*      so read the zstr from index2       */
        i=ZStrTuberRead(ix->index2,kkey[j],zstr2a);
        if(i==1)
        {
            printf("Kkey not found - we're running for cover\n");
        }
/*  is there already an index-3 entry available?  */
        x64=ZStrBitsOut(zstr2a,1);
/*     If so, get its b-key  */
        if(x64==1)
        {
            docb=ZStrDec(zstr2a,&zcbky);
        }
        else
        {
            docb=ZStrTuberIns(ix->index3,kkey[j],0);
/*     put it into index 2 */
            ZStrCxClear(&zcdelt, &ctx2a);
            ZStrCxClear(&zcdelt, &ctx2b);
            i=ZStrTuberRead(ix->index2,kkey[j],zstr2a);
            ZStrClear(zstr2b);
            x64=ZStrBitsOut(zstr2a,1);
            ZStrBitsIn(1,1,zstr2b);
            ZStrEnc(zstr2b,&zcbky,docb);
            newlet=0;
            while(1)
            {
                oldlet=newlet;
                newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                if(newlet==oldlet) break;
                ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                x64=ZStrDec(zstr2a,&zcbky);
                ZStrEnc(zstr2b,&zcbky,x64);
            }
            ZStrNormalize(zstr2b);
            ZStrTuberUpdate(ix->index2,kkey[j],zstr2b); 
        }
        dock=ZStrTuberK(ix->index3,kkey[j],0,docb);
/*     insert doc handle into index 3      */
        i=ZStrTuberRead(ix->index3,dock,x3zstr);
        ZStrClear(x3zstrb);
        if(i==1)
        {
            printf("Kkey not found in ix3 - we're doomed\n");
        }
        ZStrCxClear(&zcdoc, &x3ctx);
        ZStrCxClear(&zcdoc, &x3ctxb);
        newhan=0;
        while(1)
        {
            oldhan=newhan;
            newhan=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
            if(newhan==oldhan) break;
            if(newhan>handle) break;
            ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,newhan);
        }
        ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,handle);
        if(newhan==oldhan)
            ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,handle);
        else
        {
            ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,newhan);
            while(newhan!=oldhan)
            {
                oldhan=newhan;
                newhan=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
                ZStrCxEnc(x3zstrb,&zcdoc,&x3ctxb,newhan);
            }
        }
        ZStrNormalize(x3zstrb);
        ZStrTuberUpdate(ix->index3,dock,x3zstrb); 
/*      copy the word into ix                */
        ixlen=len;
        for(j=0;j<len;j++) ixlet[j]=letters[j];
        
        if(ix->options==FTS_INDEX_SUBSTRINGS)
        {
            for(j1=0;j1<len;j1++)
            {
                kkey1[j1+1]=kroot1;
                for(j2=j1;j2>=0;j2--)
                {
                    tran=ZStrXlate(&zcutf,ixlet[j2]);
                    i=ZStrTuberRead(ix->index1,kkey1[j2+1],zstr2a);
                    if(i==1)
                    {
          printf("Kkey not found - we're in trouble!\n");
                    }
/*     look to see if the letter is there  */
                    ZStrCxClear(&zcdelt, &ctx2a);
                    newlet=0;
                    while(1)
                    {
                        oldlet=newlet;
                        newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                        if(newlet==oldlet) break;
                        bkey=ZStrDec(zstr2a,&zcbky);
                        if(newlet>=tran) break;
                    }
                    if(newlet != tran)
                    {

/*       if not there, create a new index-1 entry for it  */
                        bkey=ZStrTuberIns(ix->index1,kkey1[j2+1],tran);
                        kkey1[j2]=ZStrTuberK(ix->index1,kkey1[j2+1],tran,bkey); 
/*     update old index-1 entry to insert new letter  */
                        ZStrCxClear(&zcdelt, &ctx2a);
                        ZStrCxClear(&zcdelt, &ctx2b);
                        i=ZStrTuberRead(ix->index1,kkey1[j2+1],zstr2a);
                        ZStrClear(zstr2b);
                        newlet=0;
                        while(1)
                        {
                            oldlet=newlet;
                            newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                            if(newlet==oldlet) break;
                            if(newlet>tran) break;
                            ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                            x64=ZStrDec(zstr2a,&zcbky);
                            ZStrEnc(zstr2b,&zcbky,x64);
                        }
                        ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                        ZStrEnc(zstr2b,&zcbky,bkey);
                        if(newlet==oldlet)
                        {
                            ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,tran);
                        }
                        else
                        {
                            while(newlet!=oldlet)
                            {
                                oldlet=newlet;
                                ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                                x64=ZStrDec(zstr2a,&zcbky);
                                ZStrEnc(zstr2b,&zcbky,x64);
                                newlet=ZStrCxDec(zstr2a,&zcdelt,&ctx2a);
                            }
                            ZStrCxEnc(zstr2b,&zcdelt,&ctx2b,newlet);
                        }
                        ZStrNormalize(zstr2b);
                        ZStrTuberUpdate(ix->index1,kkey1[j2+1],zstr2b);
                    }
                    else
                    {
                        kkey1[j2]=ZStrTuberK(ix->index1,kkey1[j2+1],tran,bkey);
                    }
                }
            }
        }
    }
    ZStrSTDest(stex);
    ZStrDest(zstrwl);
    ZStrDest(zstr2a);
    ZStrDest(zstr2b);
    ZStrDest(x3zstr);
    ZStrDest(x3zstrb);
}

void RealDeleteDocument(FTS_index_t * ftx, FTS_document_id_t docid)
{
    FTS_real_index * ix;
    FTS_document_id_t i;
    ix=(FTS_real_index *) ftx;
    for(i=0;i<=ix->lastslot;i++)
    {
        if(ix->handsfree[i]==1) continue;
        if(ix->handles[i]==docid) break;
    }
    if(i>ix->lastslot)
    {
/* TBD - what to do if a document is deleted that isn't there?  */
        printf("tried to delete nonexistent document\n");
    }
    ix->handsfree[i]=1;
}

/* now the customer-facing routines                     */
/* These are needed so that the lock is held in Update  */
/* preventing a query getting a result with neither the */
/* old version nor the new one                          */

void FTS_AddDocument(FTS_index_t * ftx, FTS_document_id_t docid)
{
/* TBD obtain write lock  */
    RealAddDocument(ftx,docid);
/* TBD release write lock */
}

void FTS_DeleteDocument(FTS_index_t * ftx, FTS_document_id_t docid)
{
/* TBD obtain write lock  */
    RealDeleteDocument(ftx,docid);
/* TBD release write lock */
}

void FTS_UpdateDocument(FTS_index_t * ftx, FTS_document_id_t docid)
{
/* TBD obtain write lock  */
    RealDeleteDocument(ftx,docid);
    RealAddDocument(ftx,docid);
/* TBD release write lock */
}

void FTS_BackgroundTask(FTS_index_t * ftx)
{
/* obtain LOCKMAIN */
/* remove deleted handles from index3 not done QQQ  */
/* release LOCKMAIN */
}
/* not a valid kkey - 52 bits long!*/
#define NOTFOUND 0xF777777777777


uint64_t findkkey1(FTS_real_index * ix, uint64_t * word)
{
    ZSTR *zstr;
    CTX ctx;
    uint64_t * wd;
    uint64_t tran,newlet,oldlet,bkey,kk1;
    int j;
    zstr = ZStrCons(10);
    wd=word;
    while(*wd != 0) wd++;
    kk1=ZStrTuberK(ix->index2,0,0,0);
    while(1)
    {
        if(wd==word) break;
        tran=*(--wd);
/*     Get the Z-string for the index-1 entry of this key */
        j=ZStrTuberRead(ix->index1,kk1,zstr);
        if(j==1)
        {
            kk1=NOTFOUND;
            break;
        }
        ZStrCxClear(&zcdelt, &ctx);
        newlet=0;
        while(1)
        {
            oldlet=newlet;
            newlet=ZStrCxDec(zstr,&zcdelt,&ctx);
            if(newlet==oldlet)
            {
                kk1=NOTFOUND;
                break;
            }
            bkey=ZStrDec(zstr,&zcbky);
            if(newlet>tran)
            {
                kk1=NOTFOUND;
                break;
            }
            if(newlet==tran) break;
        }
        if(kk1==NOTFOUND) break;
        kk1=ZStrTuberK(ix->index1,kk1,tran,bkey);
    }
    ZStrDest(zstr);
    return kk1;
}

uint64_t findkkey2(FTS_real_index * ix, uint64_t * word)
{
    ZSTR *zstr;
    CTX ctx;
    uint64_t tran,x64,docb,newlet,oldlet,bkey,kk2;
    int j;
    zstr = ZStrCons(10);
    kk2=ZStrTuberK(ix->index2,0,0,0);
    while(1)
    {
        tran=*(word++);
        if(tran==0) break;
/*     Get the Z-string for the index-2 entry of this key */
        j=ZStrTuberRead(ix->index2,kk2,zstr);
        if(j==1)
        {
            kk2=NOTFOUND;
            break;
        }
        x64=ZStrBitsOut(zstr,1);
        if(x64==1) 
        {
/*     skip over the B-key into index 3  */
            docb=ZStrDec(zstr,&zcbky);
/* silly use of docb to get rid of compiler warning  */
          if(docb==0xffffff) printf("impossible\n");
        }
        ZStrCxClear(&zcdelt, &ctx);
        newlet=0;
        while(1)
        {
            oldlet=newlet;
            newlet=ZStrCxDec(zstr,&zcdelt,&ctx);
            if(newlet==oldlet)
            {
                kk2=NOTFOUND;
                break;
            }
            bkey=ZStrDec(zstr,&zcbky);
            if(newlet>tran)
            {
                kk2=NOTFOUND;
                break;
            }
            if(newlet==tran) break;
        }
        if(kk2==NOTFOUND) break;
        kk2=ZStrTuberK(ix->index2,kk2,tran,bkey);
    }
    ZStrDest(zstr);
    return kk2;
}
/*  QUERY  */
/* for each query term  */
/*  update zstra2 to only contain handles matching that also */


/* recursive index 2 handles kk2 to dochan STEX using zcdh */

void ix2recurs(STEX * dochan, FTS_real_index * ix, uint64_t kk2)
{
    ZSTR *zstr2, *zstr3, *zstr;
    CTX ctx2, ctx3;
    uint64_t docb,newlet,oldlet,newkk2,bkey;
    uint64_t x64, dock, oldhan,newhan; 
    int i,j;
    zstr2=ZStrCons(10);  /* index 2 entry for this prefix */
    zstr3=ZStrCons(10);  /* index 3 entry for this prefix */
                               /* if any  */
    zstr = ZStrCons(2);  /* single doc handle work area */
    j=ZStrTuberRead(ix->index2,kk2,zstr2);
    if(j==1)
    {
        printf("recursion failed to read kk2\n");
        exit(1);
    }
    x64=ZStrBitsOut(zstr2,1);
    if(x64==1) 
    {
/* process the documents into the STEX  */
/* uses zcdh not LastEnc because it must sort into */
/* numerical order                                 */
        docb=ZStrDec(zstr2,&zcbky);
        dock=ZStrTuberK(ix->index3,kk2,0,docb);
        i=ZStrTuberRead(ix->index3,dock,zstr3);
        if(i==1)
        {
            printf("Kkey not in ix3 - we're doomed\n");
        }
        ZStrCxClear(&zcdoc, &ctx3);
        newhan=0;
        while(1)
        {
            oldhan=newhan;
            newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
            if(newhan==oldhan) break;
            if(ix->handsfree[newhan]==0)
            {
                ZStrClear(zstr);
                ZStrEnc(zstr,&zcdh,newhan);
                ZStrSTAppend(dochan,zstr);
            }
        }
    }
    ZStrCxClear(&zcdelt, &ctx2);
    newlet=0;
    while(1)
    {
        oldlet=newlet;
        newlet=ZStrCxDec(zstr2,&zcdelt,&ctx2);
        if(newlet==oldlet) break;
        bkey=ZStrDec(zstr2,&zcbky);
        newkk2=ZStrTuberK(ix->index2,kk2,newlet,bkey);
        ix2recurs(dochan,ix,newkk2);
    }
    ZStrDest(zstr2);
    ZStrDest(zstr3);
    ZStrDest(zstr);    
    return;
}

void ix1recurs(STEX * dochan, FTS_real_index * ix, uint64_t kk1, uint64_t * wd)
{

    ZSTR *zstr;
    CTX ctx;
    int j;
    uint64_t newlet,oldlet,bkey,newkk1,kk2;
    kk2=findkkey2(ix,wd);
    if(kk2!=NOTFOUND) ix2recurs(dochan,ix,kk2);
    zstr=ZStrCons(10);  /* index 1 entry for this prefix */
    j=ZStrTuberRead(ix->index1,kk1,zstr);
    if(j==1)
    {
        printf("recursion failed to read kk1\n");
        exit(1);
    }
    ZStrCxClear(&zcdelt, &ctx);
    newlet=0;
    while(1)
    {
        oldlet=newlet;
        newlet=ZStrCxDec(zstr,&zcdelt,&ctx);
        if(newlet==oldlet) break;
        bkey=ZStrDec(zstr,&zcbky);
        newkk1=ZStrTuberK(ix->index1,kk1,newlet,bkey);
        *(wd-1)=newlet;
        ix1recurs(dochan,ix,newkk1,wd-1);
    }
    ZStrDest(zstr);
    return;
}

FTS_document_ids_t * FTS_FindDocuments (FTS_index_t * ftx,
                                FTS_query_t * query)
{
    FTS_document_ids_t * dc;
    FTS_real_index * ix;
    size_t queryterm;
    ZSTR *zstr2,*zstr3;
    ZSTR *zstra1, *zstra2, *ztemp;
    ZSTR *zstr;
    STEX * dochan;
    CTX ctxa1, ctxa2;
    CTX ctx3;
    uint64_t word1[100];
    int i,j;
    uint64_t kk2,kk1,x64,docb,dock;
    uint64_t oldhan,newhan,ndocs,lasthan,odocs;
    uint64_t nhand1,ohand1;
    uint8_t * utf;
    uint64_t unicode;
    uint16_t *docpt;
/* TBD obtain read lock */

    ix=(FTS_real_index *) ftx;
    dc=malloc(sizeof(FTS_document_ids_t));
    dc->_len=0;     /* no docids so far  */
    dc->_docs=NULL;
    zstr2=ZStrCons(10);  /* from index-2 tuber */
    zstr3=ZStrCons(10);  /* from index-3 tuber  */
    zstra1=ZStrCons(10); /* current list of documents */
    zstra2=ZStrCons(10); /* new list of documents  */
    zstr  =ZStrCons(4);  /* work zstr from stex  */
/*     - for each term in the query */
    for(queryterm=0;queryterm<query->_len;queryterm++)
    {
/*  Depending on the query type, the objective is do   */
/*  populate or "and" zstra1 with the sorted list      */
/*  of document handles that match that term           */
/*  TBD - what to do if it is not a legal option?  */
/* TBD combine this with otheer options - no need to use zstring  */
        if(query->_localOptions[queryterm] == FTS_MATCH_COMPLETE)
        {
            j=0;
            utf= query->_texts[queryterm];
            while(1)
            {
                unicode=getunicode(&utf);
                word1[j++]=ZStrXlate(&zcutf,unicode);
                if(unicode==0) break;
            }
            kk2=findkkey2(ix,word1);
            if(kk2==NOTFOUND) break;
            j=ZStrTuberRead(ix->index2,kk2,zstr2);
            x64=ZStrBitsOut(zstr2,1);
            if(x64!=1) break;
            docb=ZStrDec(zstr2,&zcbky);
            dock=ZStrTuberK(ix->index3,kk2,0,docb);
            i=ZStrTuberRead(ix->index3,dock,zstr3);
            if(i==1)
            {
                printf("Kkey not in ix3 - we're terrified\n");
            }
            ZStrCxClear(&zcdoc, &ctx3);
            ZStrCxClear(&zcdoc, &ctxa2);
            ZStrClear(zstra2);
            newhan=0;
            lasthan=0;
            ndocs=0;
            if(queryterm==0)
            {
                while(1)
                {
                    oldhan=newhan;
                    newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
                    if(newhan==oldhan) break;
                    if(ix->handsfree[newhan]==0)
                    {
                        ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                        lasthan=newhan;
                        ndocs++;
                    }
                }

            }
            else
            {
                ZStrCxClear(&zcdoc, &ctxa1);
                ohand1=0;
                nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                oldhan=0;
                newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
/*     zstra1 = zstra1 & zstra2  */
                while(1)
                {
                    if(nhand1==ohand1) break;
                    if(oldhan==newhan) break;
                    if(newhan==nhand1)
                    {
                        if(ix->handsfree[newhan]==0)
                        {
                            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                            lasthan=newhan;
                            ndocs++;
                        }
                        oldhan=newhan;
                        newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else if(newhan>nhand1)
                    {
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else
                    {
                        oldhan=newhan;
                        newhan=ZStrCxDec(zstr3,&zcdoc,&ctx3);
                    }
                }
            }
            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,lasthan);
            ZStrNormalize(zstra2);
            ztemp=zstra1;
            zstra1=zstra2;
            zstra2=ztemp;
        }  /* end of match-complete code  */
        if (  (query->_localOptions[queryterm] == FTS_MATCH_PREFIX)  ||
              (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING)  )
        {
/*  Make STEX to contain new list of handles  */
            dochan=ZStrSTCons(2);
            j=50;
            utf= query->_texts[queryterm];
/* TBD protect against query string greater than 40?  */
            while(1)
            {
                unicode=getunicode(&utf);
                word1[j++]=ZStrXlate(&zcutf,unicode);
                if(unicode==0) break;
            }
            if (query->_localOptions[queryterm] == FTS_MATCH_PREFIX)
            {
                kk2=findkkey2(ix,word1+50);
                if(kk2==NOTFOUND) break;
/*  call routine to recursively put handles to STEX  */
                ix2recurs(dochan,ix,kk2);
            }
            if (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING)
            {
                kk1=findkkey1(ix,word1+50);
                if(kk1==NOTFOUND) break;
/*  call routine to recursively put handles to STEX  */
                ix1recurs(dochan,ix,kk1,word1+50);
            }
            ZStrSTSort(dochan);
            odocs=dochan->cnt;
            docpt=dochan->list;
            ZStrCxClear(&zcdoc, &ctxa2);
            ZStrClear(zstra2);
            lasthan=0;
            ndocs=0;
            if(queryterm==0)
            {
                for(i=0;i<odocs;i++)
                {
                    ZStrInsert(zstr,docpt,2);
                    newhan=ZStrDec(zstr,&zcdh);
                    docpt+=ZStrExtLen(docpt,2);
                    if(ix->handsfree[newhan]==0)
                    {
                        ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                        lasthan=newhan;
                        ndocs++;
                    }
                }
            }
            else
            {
/*  merge prefix stex with zstra1  */
                ZStrCxClear(&zcdoc, &ctxa1);
                ohand1=0;
                if(odocs==0) continue;
                nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                ZStrInsert(zstr,docpt,2);
                newhan=ZStrDec(zstr,&zcdh);
                docpt+=ZStrExtLen(docpt,2);
/*     zstra1 = zstra1 & zstra2  */
                while(1)
                {
                    if(nhand1==ohand1) break;
                    if(newhan==nhand1)
                    {
                        if(ix->handsfree[newhan]==0)
                        {
                            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,newhan);
                            lasthan=newhan;
                            ndocs++;
                        }
                        ZStrInsert(zstr,docpt,2);
                        newhan=ZStrDec(zstr,&zcdh);
                        docpt+=ZStrExtLen(docpt,2);
                        odocs--;
                        if(odocs==0) break;
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else if(newhan>nhand1)
                    {
                        ohand1=nhand1;
                        nhand1=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
                    }
                    else
                    {
                        ZStrInsert(zstr,docpt,2);
                        newhan=ZStrDec(zstr,&zcdh);
                        docpt+=ZStrExtLen(docpt,2);
                        odocs--;
                        if(odocs==0) break;
                    }
                }
            }
            ZStrCxEnc(zstra2,&zcdoc,&ctxa2,lasthan);
            ZStrNormalize(zstra2);
            ztemp=zstra1;
            zstra1=zstra2;
            zstra2=ztemp;         
        }   /* end of match-prefix code  */
    }
    ZStrCxClear(&zcdoc, &ctxa1);
    newhan=0;
    dc->_docs=malloc(ndocs*sizeof(FTS_document_id_t));
    ndocs=0;
    while(1)
    {
        oldhan=newhan;
        newhan=ZStrCxDec(zstra1,&zcdoc,&ctxa1);
        if(newhan==oldhan) break;
        if(ix->handsfree[newhan]==0)
            dc->_docs[ndocs++]=ix->handles[newhan];
    }
    dc->_len=ndocs;
    ZStrDest(zstra1);
    ZStrDest(zstra2);
/* TBD relinquish read lock  */
    return dc;
}

void FTS_Free_Documents(FTS_document_ids_t * doclist)
{
    if(doclist->_docs!=NULL) free (doclist->_docs);
    free(doclist);
}

/* end of ftsindex.c */
