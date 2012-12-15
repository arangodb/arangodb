////////////////////////////////////////////////////////////////////////////////
/// @brief full text search
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author R. A. Parker  
/// @author Copyright 2012, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "fulltext-index.h"

#include "BasicsC/locks.h"
#include "BasicsC/logging.h"

#include "FulltextIndex/zstr-include.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                           externs
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief codes, defined in zcode.c
////////////////////////////////////////////////////////////////////////////////

extern ZCOD zcutf;
extern ZCOD zcbky;
extern ZCOD zcdelt;
extern ZCOD zcdoc;
extern ZCOD zckk;
extern ZCOD zcdh;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief not a valid kkey - 52 bits long!
////////////////////////////////////////////////////////////////////////////////

#define NOTFOUND 0xF777777777777

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of Unicode characters for an indexed word
////////////////////////////////////////////////////////////////////////////////

#define MAX_WORD_LENGTH      (40)

////////////////////////////////////////////////////////////////////////////////
/// @brief gap between two words in a temporary search buffer
////////////////////////////////////////////////////////////////////////////////

#define SPACING              (10)

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum tolerable occupancy of the index (e.g. 60 %)
////////////////////////////////////////////////////////////////////////////////

#define HEALTH_THRESHOLD     (75)

////////////////////////////////////////////////////////////////////////////////
/// @brief index extra growth factor
/// if 1.0, the index will be resized to the values originally suggested. As
/// resizing is expensive, one might want to decrease the overall number of
/// resizings. This can be done by setting this number to a value bigger than 
/// 1.0
////////////////////////////////////////////////////////////////////////////////

#define EXTRA_GROWTH_FACTOR  (1.5)

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual index struct used
////////////////////////////////////////////////////////////////////////////////

typedef struct { 
  void*                 _context; // arbitrary context info the index passed to getTexts
  int                   _options;

  FTS_document_id_t*    _handles; // array converting handles to docid 
  uint8_t*              _handlesFree;
  FTS_document_id_t     _firstFree; // start of handle free chain
  FTS_document_id_t     _lastSlot;
  TUBER*                _index1;
  TUBER*                _index2;
  TUBER*                _index3;
  uint64_t              _ix3KKey; // current key in background cleanup iteration

  uint64_t              _maxDocuments;
  uint64_t              _numDocuments;
  uint64_t              _numDeletions;

  FTS_texts_t* (*getTexts)(FTS_document_id_t, void*);
  void (*freeWordlist)(FTS_texts_t*);
} 
FTS_real_index;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief get a unicode character number from a UTF-8 string
////////////////////////////////////////////////////////////////////////////////

static uint64_t GetUnicode (uint8_t** ptr) {
  uint64_t c1;

  c1 = **ptr;
  if (c1 < 128) {
    // single byte
    (*ptr)++;
    return c1;
  }

  // multi-byte
  if (c1 < 224) {
    c1 = ((c1 - 192) << 6) + 
         (*((*ptr) + 1) - 128);
    (*ptr) += 2;
    return c1;
  }

  if (c1 < 240) {
    c1 = ((c1 - 224) << 12) + 
         ((*((*ptr) + 1) - 128) << 6) + 
         (*((*ptr) + 2) - 128);
    (*ptr) += 3;
    return c1;
  }

  if (c1 < 248) {
    c1 = ((c1 - 240) << 18) + 
         ((*((*ptr) + 1) - 128) << 12) + 
         ((*((*ptr) + 2) - 128) << 6) + 
         (*((*ptr) + 3) - 128);
    (*ptr) += 4;
    return c1;
  }

  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief translate zstr error code into TRI_error code
////////////////////////////////////////////////////////////////////////////////

static int TranslateZStrErrorCode (int zstrErrorCode) {
  assert(zstrErrorCode != 0);

  if (zstrErrorCode == 2) {
    return TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
  }

  return TRI_ERROR_OUT_OF_MEMORY;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a document to the index
////////////////////////////////////////////////////////////////////////////////

int RealAddDocument (FTS_index_t* ftx, FTS_document_id_t docid, FTS_texts_t* rawwords) {
  FTS_real_index* ix;
  CTX ctx2a, ctx2b, x3ctx, x3ctxb;
  STEX* stex;
  ZSTR* zstrwl;
  ZSTR* zstr2a;
  ZSTR* zstr2b;
  ZSTR* x3zstr;
  ZSTR* x3zstrb;
  uint64_t letters[MAX_WORD_LENGTH + 2];
  uint64_t ixlet[MAX_WORD_LENGTH + 2];
  uint64_t kkey[MAX_WORD_LENGTH + 2];    /* for word *without* this letter */
  uint64_t kkey1[MAX_WORD_LENGTH + 2];   /* ix1 word whose last letter is this */
  int ixlen;
  uint16_t* wpt;
  uint64_t handle, newhan, oldhan;
  uint64_t kroot1 = 0; /* initialise even if unused. this will prevent compiler warnings */
  int nowords, wdx;
  int i, j, len;
  uint64_t tran, x64, oldlet, newlet;
  uint64_t bkey = 0;
  uint64_t docb, dock;
  int res;
  int res2;

  ix = (FTS_real_index*) ftx;
  
  // allocate the document handle 
  handle = ix->_firstFree;
  if (handle == 0) {
    // no more document handles free
    LOG_ERROR("fail on %d", __LINE__);
    return TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
  }
  
  stex = ZStrSTCons(2);  /* format 2=uint16 is all that there is! */
  if (stex == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }
  
  // origin of index 2 
  kkey[0] = ZStrTuberK(ix->_index2, 0, 0, 0);

  if (ix->_options == FTS_INDEX_SUBSTRINGS) {
    kroot1 = ZStrTuberK(ix->_index1, 0, 0, 0);
  }

  res = TRI_ERROR_NO_ERROR;

  zstrwl  = ZStrCons(25);  /* 25 enough for word list  */
  zstr2a  = ZStrCons(30);  /* 30 uint64's is always enough for ix2  */
  zstr2b  = ZStrCons(30);
  x3zstr  = ZStrCons(35);
  x3zstrb = ZStrCons(35);

  // check for out of memory
  if (zstrwl == NULL || zstr2a == NULL || zstr2b == NULL || x3zstr == NULL || x3zstrb == NULL) {
    res = TRI_ERROR_OUT_OF_MEMORY;
    goto oom;
  }
  
  // put all words into a STEX 
  nowords = rawwords->_len;
  for (i = 0; i < nowords; i++) {
    uint64_t unicode;
    uint8_t* utf;

    utf = rawwords->_texts[i];
    j = 0;
    ZStrClear(zstrwl);
    unicode = GetUnicode(&utf);
    while (unicode != 0) {
      if (ZStrEnc(zstrwl, &zcutf, unicode) != 0) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }

      unicode = GetUnicode(&utf);
      j++;
      if (j > MAX_WORD_LENGTH) {
        break;
      }
    }

    // terminate the word and insert into STEX
    if (ZStrEnc(zstrwl, &zcutf, 0) != 0) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      goto oom;
    }

    ZStrNormalize(zstrwl);
    if (ZStrSTAppend(stex, zstrwl) != 0) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      goto oom;
    }
  }

  // sort them
  ZStrSTSort(stex);

  // set current length of word = 0 
  ixlen = 0;

  // for each word in the STEX
  nowords = stex->cnt;
  wpt = (uint16_t*) stex->list;
  for (wdx = 0; wdx < nowords; wdx++) {
    // get it out as a word  
    if (ZStrInsert(zstrwl, wpt, 2) != 0) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      goto oom;
    }

    len = 0;
    while (1) {
      letters[len] = ZStrDec(zstrwl, &zcutf);
      if (letters[len] == 0) {
        break;
      }
      len++;
    }

    wpt += ZStrExtLen(wpt, 2);
    // find out where it first differs from previous one 
    for (j = 0; j < ixlen; j++) {
      if (letters[j] != ixlet[j]) {
        break;
      }
    }

    // for every new letter in the word, get its K-key into array 
    while (j < len) {
      // obtain the translation of the letter  
      tran = ZStrXlate(&zcutf, letters[j]);
      // get the Z-string for the index-2 entry before this letter 
      i = ZStrTuberRead(ix->_index2, kkey[j], zstr2a);
      if (i == 1) {
        res = TRI_ERROR_INTERNAL; 
        goto oom;
      }

      x64 = ZStrBitsOut(zstr2a, 1);
      if (x64 == 1) {
        // skip over the B-key into index 3  
        docb = ZStrDec(zstr2a, &zcbky);
      }
      // look to see if the letter is there 
      ZStrCxClear(&zcdelt, &ctx2a);
      newlet = 0;
      while (1) {
        oldlet = newlet;
        newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
        if (newlet == oldlet) {
          break;
        }

        bkey = ZStrDec(zstr2a, &zcbky);
        if (newlet >= tran) {
          break;
        }
      }

      if (newlet != tran) {
        // if not there, create a new index-2 entry for it  
        bkey = ZStrTuberIns(ix->_index2, kkey[j], tran);
        if (bkey == INSFAIL) {
          res = TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
          goto oom;
        }
        kkey[j + 1] = ZStrTuberK(ix->_index2, kkey[j], tran, bkey); 
        // update old index-2 entry to insert new letter
        ZStrCxClear(&zcdelt, &ctx2a);
        ZStrCxClear(&zcdelt, &ctx2b);
        i = ZStrTuberRead(ix->_index2, kkey[j], zstr2a);
        if (i == 1) {
          res = TRI_ERROR_INTERNAL;
          goto oom;
        }
        ZStrClear(zstr2b);
        x64 = ZStrBitsOut(zstr2a, 1);
        if (ZStrBitsIn(x64, 1, zstr2b) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
        if (x64 == 1) { 
          // copy over the B-key into index 3 
          docb = ZStrDec(zstr2a, &zcbky);
          if (ZStrEnc(zstr2b, &zcbky, docb) != 0) {
            res = TRI_ERROR_OUT_OF_MEMORY;
            goto oom;
          }
        }

        newlet = 0;
        while (1) {
          oldlet = newlet;
          newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
          if (newlet == oldlet || newlet > tran) {
            break;
          }
          if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
            res = TRI_ERROR_OUT_OF_MEMORY;
            goto oom;
          }
          x64 = ZStrDec(zstr2a, &zcbky);
          if (ZStrEnc(zstr2b, &zcbky, x64) != 0) {
            res = TRI_ERROR_OUT_OF_MEMORY;
            goto oom;
          }
        }
        if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, tran) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }

        if (ZStrEnc(zstr2b, &zcbky, bkey) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
        if (newlet == oldlet) {
          if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, tran) != 0) {
            res = TRI_ERROR_OUT_OF_MEMORY;
            goto oom;
          }
        }
        else {
          while (newlet != oldlet) {
            oldlet = newlet;
            if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
              res = TRI_ERROR_OUT_OF_MEMORY;
              goto oom;
            }
            x64 = ZStrDec(zstr2a, &zcbky);
            if (ZStrEnc(zstr2b, &zcbky, x64) != 0) {
              res = TRI_ERROR_OUT_OF_MEMORY;
              goto oom;
            }
            newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
          }
          if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
            res = TRI_ERROR_OUT_OF_MEMORY;
            goto oom;
          }
        }
        ZStrNormalize(zstr2b);
        res2 =  ZStrTuberUpdate(ix->_index2, kkey[j], zstr2b);
        if (res2 != 0) {
          res = TranslateZStrErrorCode(res2);
          goto oom;
        }
      }
      else {
        // if it is, get its KKey and put in (next) slot 
        kkey[j + 1] = ZStrTuberK(ix->_index2, kkey[j], tran, bkey);
      }
      j++;
    }
    
    // kkey[j] is kkey of whole word.
    // so read the zstr from index2 
    i = ZStrTuberRead(ix->_index2, kkey[j], zstr2a);
    if (i == 1) {
      res = TRI_ERROR_INTERNAL;
      goto oom;
    }
    // is there already an index-3 entry available? 
    x64 = ZStrBitsOut(zstr2a, 1);
    // If so, get its b-key  
    if(x64 == 1) {
      docb = ZStrDec(zstr2a, &zcbky);
    }
    else {
      docb = ZStrTuberIns(ix->_index3, kkey[j], 0);
      if (docb == INSFAIL) {
        res = TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
        goto oom;
      }
      // put it into index 2 
      ZStrCxClear(&zcdelt, &ctx2a);
      ZStrCxClear(&zcdelt, &ctx2b);
      i = ZStrTuberRead(ix->_index2, kkey[j], zstr2a);
      if (i == 1) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }
      ZStrClear(zstr2b);
      x64 = ZStrBitsOut(zstr2a, 1);
      if (ZStrBitsIn(1, 1, zstr2b) != 0) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }

      if (ZStrEnc(zstr2b, &zcbky, docb) != 0) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }

      newlet = 0;
      while (1) {
        oldlet = newlet;
        newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
        if (newlet == oldlet) {
          break;
        }

        if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
        x64 = ZStrDec(zstr2a, &zcbky);
        if (ZStrEnc(zstr2b,&zcbky, x64) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
      }
      ZStrNormalize(zstr2b);
      res2 = ZStrTuberUpdate(ix->_index2, kkey[j], zstr2b);
      if (res2 != 0) {
        res = TranslateZStrErrorCode(res2);
        goto oom;
      } 
    }
    dock = ZStrTuberK(ix->_index3, kkey[j], 0, docb);
    // insert doc handle into index 3
    i = ZStrTuberRead(ix->_index3, dock, x3zstr);
    ZStrClear(x3zstrb);
    if (i == 1) {
      res = TRI_ERROR_INTERNAL; 
      goto oom;
    }

    ZStrCxClear(&zcdoc, &x3ctx);
    ZStrCxClear(&zcdoc, &x3ctxb);
    newhan = 0;
    while (1) {
      oldhan = newhan;
      newhan = ZStrCxDec(x3zstr, &zcdoc, &x3ctx);
      if (newhan == oldhan || newhan > handle) {
        break;
      }
      
      if (ZStrCxEnc(x3zstrb, &zcdoc, &x3ctxb, newhan) != 0) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }
    }
    if (ZStrCxEnc(x3zstrb, &zcdoc, &x3ctxb, handle) != 0) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      goto oom;
    }
    if (newhan == oldhan) {
      if (ZStrCxEnc(x3zstrb, &zcdoc, &x3ctxb, handle) != 0) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }
    }
    else {
      if (ZStrCxEnc(x3zstrb, &zcdoc, &x3ctxb, newhan) != 0) {
        res = TRI_ERROR_OUT_OF_MEMORY;
        goto oom;
      }
      while (newhan != oldhan) {
        oldhan = newhan;
        newhan = ZStrCxDec(x3zstr, &zcdoc, &x3ctx);
        if (ZStrCxEnc(x3zstrb, &zcdoc, &x3ctxb, newhan) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
      }
    }
    ZStrNormalize(x3zstrb);
    res2 = ZStrTuberUpdate(ix->_index3, dock, x3zstrb);
    if (res2 != 0) {
      res = TranslateZStrErrorCode(res2);
      goto oom;
    }

    // copy the word into ix
    ixlen = len;
    for (j = 0; j < len; j++) {
      ixlet[j] = letters[j];
    }
        
    if (ix->_options == FTS_INDEX_SUBSTRINGS) {
      int j1, j2;

      for (j1 = 0; j1 < len; j1++) {
        kkey1[j1 + 1] = kroot1;
        for (j2 = j1; j2 >= 0; j2--) {
          tran = ZStrXlate(&zcutf, ixlet[j2]);
          i = ZStrTuberRead(ix->_index1, kkey1[j2 + 1], zstr2a);
          if (i == 1) {
            res = TRI_ERROR_INTERNAL; 
            goto oom;
          }
          // look to see if the letter is there  
          ZStrCxClear(&zcdelt, &ctx2a);
          newlet = 0;
          while (1) {
            oldlet = newlet;
            newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
            if (newlet == oldlet) {
              break;
            }
            bkey = ZStrDec(zstr2a, &zcbky);
            if (newlet >= tran) {
              break;
            }
          }
          if (newlet != tran) {
            // if not there, create a new index-1 entry for it  
            bkey = ZStrTuberIns(ix->_index1, kkey1[j2 + 1], tran);
            if (bkey == INSFAIL) {
              res = TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
              goto oom;
            }
            kkey1[j2] = ZStrTuberK(ix->_index1, kkey1[j2 + 1], tran, bkey); 
            // update old index-1 entry to insert new letter
            ZStrCxClear(&zcdelt, &ctx2a);
            ZStrCxClear(&zcdelt, &ctx2b);
            i = ZStrTuberRead(ix->_index1, kkey1[j2 + 1], zstr2a);
            if (i == 1) {
              res = TRI_ERROR_INTERNAL;
              goto oom;
            }
            ZStrClear(zstr2b);
            newlet = 0;
            while (1) {
              oldlet = newlet;
              newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
              if (newlet == oldlet || newlet > tran) {
                break;
              }
              if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
                res = TRI_ERROR_OUT_OF_MEMORY;
                goto oom;
              }
              x64 = ZStrDec(zstr2a, &zcbky);
              if (ZStrEnc(zstr2b, &zcbky, x64) != 0) {
                res = TRI_ERROR_OUT_OF_MEMORY;
                goto oom;
              }
            }
            if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, tran) != 0) {
              res = TRI_ERROR_OUT_OF_MEMORY;
              goto oom;
            }
            if (ZStrEnc(zstr2b, &zcbky, bkey) != 0) {
              res = TRI_ERROR_OUT_OF_MEMORY;
              goto oom;
            }
            if (newlet == oldlet) {
              if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, tran) != 0) {
                res = TRI_ERROR_OUT_OF_MEMORY;
                goto oom;
              }
            }
            else {
              while (newlet != oldlet) {
                oldlet = newlet;
                if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
                  res = TRI_ERROR_OUT_OF_MEMORY;
                  goto oom;
                }
                x64 = ZStrDec(zstr2a, &zcbky);
                if (ZStrEnc(zstr2b, &zcbky, x64) != 0) {
                  res = TRI_ERROR_OUT_OF_MEMORY;
                  goto oom;
                }
                newlet = ZStrCxDec(zstr2a, &zcdelt, &ctx2a);
              }
              if (ZStrCxEnc(zstr2b, &zcdelt, &ctx2b, newlet) != 0) {
                res = TRI_ERROR_OUT_OF_MEMORY;
                goto oom;
              }
            }
            ZStrNormalize(zstr2b);
            res2 = ZStrTuberUpdate(ix->_index1, kkey1[j2 + 1], zstr2b);
            if (res2 != 0) {
              res = TranslateZStrErrorCode(res2);
              goto oom;
            }
          }
          else {
            kkey1[j2] = ZStrTuberK(ix->_index1, kkey1[j2 + 1], tran, bkey);
          }
        }
      }
    }
  }
  
  ix->_numDocuments++;
  
  // insert the handle
  ix->_firstFree           = ix->_handles[handle];
  ix->_handles[handle]     = docid;
  ix->_handlesFree[handle] = 0;

oom:
  ZStrSTDest(stex);

  if (zstrwl != NULL) {
    ZStrDest(zstrwl);
  }
  if (zstr2a != NULL) {
    ZStrDest(zstr2a);
  }
  if (zstr2b != NULL) {
    ZStrDest(zstr2b);
  }
  if (x3zstr != NULL) {
    ZStrDest(x3zstr);
  }
  if (x3zstrb != NULL) {
    ZStrDest(x3zstrb);
  }

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document from the index
////////////////////////////////////////////////////////////////////////////////

static int RealDeleteDocument (FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix;
  FTS_document_id_t i;

  ix = (FTS_real_index*) ftx;
  for (i = 1; i <= ix->_lastSlot; i++) {
    if (ix->_handlesFree[i] == 1) {
      continue;
    }

    if (ix->_handles[i] == docid) {
      break;
    }
  }

  if (i > ix->_lastSlot) {
    LOG_ERROR("fail on %d", __LINE__);
    return TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
  }

  ix->_handlesFree[i] = 1;
  if (ix->_numDocuments > 0) {
    // should never underflow
    ix->_numDocuments--;
  }

  ix->_numDeletions++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a key - prefix or substring matching
////////////////////////////////////////////////////////////////////////////////

static uint64_t FindKKey1 (FTS_real_index* ix, uint64_t* word) {
  ZSTR* zstr;
  CTX ctx;
  uint64_t* wd;
  uint64_t bkey, kk1;

  zstr = ZStrCons(10);
  if (zstr == NULL) {
    // actually an out-of-memory error would be more appropriate here
    return NOTFOUND;
  }

  wd = word;
  while (*wd != 0) {
    wd++;
  }

  kk1 = ZStrTuberK(ix->_index2, 0, 0, 0);

  while (1) {
    uint64_t tran;
    uint64_t newlet;

    if (wd == word) {
      break;
    }

    tran = *(--wd);
    // get the Z-string for the index-1 entry of this key 
    if (ZStrTuberRead(ix->_index1, kk1, zstr) == 1) {
      kk1 = NOTFOUND;
      break;
    }

    ZStrCxClear(&zcdelt, &ctx);
    newlet = 0;
    while (1) {
      uint64_t oldlet;

      oldlet = newlet;
      newlet = ZStrCxDec(zstr, &zcdelt, &ctx);
      if (newlet == oldlet) {
        kk1 = NOTFOUND;
        break;
      }

      bkey = ZStrDec(zstr, &zcbky);
      if (newlet > tran) {
        kk1 = NOTFOUND;
        break;
      }
      if (newlet == tran) {
        break;
      }
    }

    if (kk1 == NOTFOUND) {
      break;
    }

    kk1 = ZStrTuberK(ix->_index1, kk1, tran, bkey);
  }

  ZStrDest(zstr);
  return kk1;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a key - complete matching
////////////////////////////////////////////////////////////////////////////////

static uint64_t FindKKey2 (FTS_real_index* ix, uint64_t* word) {
  ZSTR* zstr;
  CTX ctx;
  uint64_t kk2;

  zstr = ZStrCons(10);
  if (zstr == NULL) {
    // actually an out-of-memory error would be more appropriate here
    return NOTFOUND;
  }

  kk2 = ZStrTuberK(ix->_index2, 0, 0, 0);

  while (1) {
    uint64_t tran;
    uint64_t newlet;
    uint64_t bkey;

    tran = *(word++);
    if (tran == 0) {
      break;
    }
    // get the Z-string for the index-2 entry of this key
    if (ZStrTuberRead(ix->_index2, kk2, zstr) == 1) {
      kk2 = NOTFOUND;
      break;
    }

    if (ZStrBitsOut(zstr, 1) == 1) {
      uint64_t docb;

      // skip over the B-key into index 3
      docb = ZStrDec(zstr, &zcbky);
      // silly use of docb to get rid of compiler warning  
      if (docb == 0xffffff) {
        // actually some "internal error" code would be more appropriate here
        ZStrDest(zstr);
        return NOTFOUND;
      }
    }
    ZStrCxClear(&zcdelt, &ctx);
    
    newlet = 0;
    while (1) {
      uint64_t oldlet;

      oldlet = newlet;
      newlet = ZStrCxDec(zstr, &zcdelt, &ctx);
      if (newlet == oldlet) {
        kk2 = NOTFOUND;
        break;
      }

      bkey = ZStrDec(zstr, &zcbky);
      if (newlet > tran) {
        kk2 = NOTFOUND;
        break;
      }
      if (newlet == tran) {
        break;
      }
    }

    if (kk2 == NOTFOUND) {
      break;
    }

    kk2 = ZStrTuberK(ix->_index2, kk2, tran, bkey);
  }

  ZStrDest(zstr);
  return kk2;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index recursion, complete matching
/// for each query term, update zstra2 to only contain handles matching that
/// also recursive index 2 handles kk2 to dochan STEX using zcdh 
////////////////////////////////////////////////////////////////////////////////

static int Ix2Recurs (STEX* dochan, FTS_real_index* ix, uint64_t kk2) {
  ZSTR* zstr2;
  ZSTR* zstr3;
  ZSTR* zstr;
  CTX ctx2, ctx3;
  uint64_t newlet;
  int res;

  // index 2 entry for this prefix 
  zstr2 = ZStrCons(10); 
  if (zstr2 == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // index 3 entry for this prefix (if any)
  zstr3 = ZStrCons(10); 
  if (zstr3 == NULL) {
    ZStrDest(zstr2);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // single doc handle work area 
  zstr = ZStrCons(2);  
  if (zstr == NULL) {
    ZStrDest(zstr3);
    ZStrDest(zstr2);
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (ZStrTuberRead(ix->_index2, kk2, zstr2) == 1) {
    ZStrDest(zstr);
    ZStrDest(zstr3);
    ZStrDest(zstr2);
    return TRI_ERROR_INTERNAL;
  }

  res = TRI_ERROR_NO_ERROR;

  if (ZStrBitsOut(zstr2, 1) == 1) {
    // process the documents into the STEX  
    // uses zcdh not LastEnc because it must sort into 
    // numerical order                                 
    uint64_t docb;
    uint64_t dock;
    uint64_t newhan;
    int i;

    docb = ZStrDec(zstr2, &zcbky);
    dock = ZStrTuberK(ix->_index3, kk2, 0, docb);
    i = ZStrTuberRead(ix->_index3, dock, zstr3);
    if (i == 1) {
      res = TRI_ERROR_OUT_OF_MEMORY;
      goto oom;
    }
    ZStrCxClear(&zcdoc, &ctx3);

    newhan = 0;
    while (1) {
      uint64_t oldhan;

      oldhan = newhan;
      newhan = ZStrCxDec(zstr3, &zcdoc, &ctx3);
      if (newhan == oldhan) {
        break;
      }

      if (ix->_handlesFree[newhan] == 0) {
        ZStrClear(zstr);
        if (ZStrEnc(zstr, &zcdh, newhan) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
        if (ZStrSTAppend(dochan, zstr) != 0) {
          res = TRI_ERROR_OUT_OF_MEMORY;
          goto oom;
        }
      }
    }
  }
  ZStrCxClear(&zcdelt, &ctx2);

  newlet = 0;
  while (1) {
    uint64_t oldlet;
    uint64_t newkk2;
    uint64_t bkey;

    oldlet = newlet;
    newlet = ZStrCxDec(zstr2, &zcdelt, &ctx2);
    if (newlet == oldlet) {
      break;
    }

    bkey = ZStrDec(zstr2, &zcbky);
    newkk2 = ZStrTuberK(ix->_index2, kk2, newlet, bkey);
    res = Ix2Recurs(dochan, ix, newkk2);
    if (res != TRI_ERROR_NO_ERROR) {
      break;
    }
  }

oom:
  ZStrDest(zstr2);
  ZStrDest(zstr3);
  ZStrDest(zstr);    

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief index recursion, prefix matching
////////////////////////////////////////////////////////////////////////////////

static int Ix1Recurs (STEX* dochan, 
                      FTS_real_index* ix, 
                      uint64_t kk1, 
                      uint64_t* wd) {
  ZSTR* zstr;
  CTX ctx;
  uint64_t newlet;
  uint64_t kk2;
  int res;

  res = TRI_ERROR_NO_ERROR;

  kk2 = FindKKey2(ix,wd);

  if (kk2 != NOTFOUND) {
    res = Ix2Recurs(dochan, ix, kk2);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  // index 1 entry for this prefix 
  zstr = ZStrCons(10);  
  if (zstr == NULL) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  if (ZStrTuberRead(ix->_index1, kk1, zstr) == 1) {
    return TRI_ERROR_INTERNAL;
  }

  ZStrCxClear(&zcdelt, &ctx);
  newlet = 0;

  while (1) {
    uint64_t oldlet;
    uint64_t bkey;
    uint64_t newkk1;

    oldlet = newlet;
    newlet = ZStrCxDec(zstr, &zcdelt, &ctx);
    if (newlet == oldlet) {
      break;
    }
    bkey = ZStrDec(zstr, &zcbky);
    newkk1 = ZStrTuberK(ix->_index1, kk1, newlet, bkey);
    *(wd - 1) = newlet;

    res = Ix1Recurs(dochan, ix, newkk1, wd - 1);
    if (res != TRI_ERROR_NO_ERROR) {
      return res;
    }
  }

  ZStrDest(zstr);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief read a unicode word into a buffer of uint64_ts
////////////////////////////////////////////////////////////////////////////////

static void FillWordBuffer (uint64_t* target, const uint8_t* source) {
  uint8_t* current;
  int i;

  current = (uint8_t*) source;
  i = 0;
  while (1) {
    uint64_t unicode = GetUnicode(&current);
    
    target[i++] = ZStrXlate(&zcutf, unicode);
    if (unicode == 0 || i > MAX_WORD_LENGTH) {
      break;
    }
  }
  target[i] = 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add the found documents to the result 
////////////////////////////////////////////////////////////////////////////////

static void AddResultDocuments (FTS_document_ids_t* result, 
                                FTS_real_index* ftx, 
                                ZSTR* zstr, 
                                CTX* ctx) {
  uint64_t newHandle;
  uint64_t numDocs;
 
  newHandle = 0;
  numDocs = 0;

  while (1) {
    uint64_t oldHandle;

    oldHandle = newHandle;
    newHandle = ZStrCxDec(zstr, &zcdoc, ctx);
    if (newHandle == oldHandle) {
      break;
    }
    if (ftx->_handlesFree[newHandle] == 0) {
      result->_docs[numDocs++] = ftx->_handles[newHandle];
    }
  }
  result->_len = numDocs;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the health of the index
/// the health will be returned as an integer with range 0..100 
/// 0 means the index is 0% full and 100 means the index is 100% full
/// values above 60 should trigger an index resize elsewhere
/// the stats array will be populated with appropriate index sizes when the
/// index is going to be resized
////////////////////////////////////////////////////////////////////////////////

int FTS_HealthIndex (FTS_index_t* ftx, uint64_t* stats) {
  FTS_real_index* ix;
  uint64_t st[2];
  uint64_t health;

  ix = (FTS_real_index*) ftx;

  health = (ix->_numDocuments * 100) / ix->_maxDocuments;

  if (ix->_options == FTS_INDEX_SUBSTRINGS) {
    ZStrTuberStats(ix->_index1, st);
    stats[1] = st[1];
    if (health < st[0]) {
      health = st[0];
    }
  }
  else {
    stats[1] = 0;
  }

  ZStrTuberStats(ix->_index2, st);
  stats[2] = st[1];
  if (health < st[0]) {
    health = st[0];
  }

  ZStrTuberStats(ix->_index3, st);
  stats[3] = st[1];
  if (health < st[0]) {
    health = st[0];
  }

  stats[0] = (health * (ix->_numDocuments + 5)) / 50; 
  if (stats[0] < (ix->_numDocuments + 5)) {
    stats[0] = (ix->_numDocuments + 5);
  }

  if (EXTRA_GROWTH_FACTOR > 1.0) {
    size_t i;

    for (i = 0; i < 4; ++i) {
      stats[i] = (uint64_t) ((double) stats[i] * (double) EXTRA_GROWTH_FACTOR);
    }
  }

  return (int) health;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clone an existing index
/// this will copy the properties of the old index, but will take different
/// sizes. This function is called when the index is resized
/// It will also copy the documents from the old index into the new one
////////////////////////////////////////////////////////////////////////////////

FTS_index_t* FTS_CloneIndex (FTS_index_t* ftx,
                             FTS_document_id_t excludeDocument,
                             uint64_t sizes[4]) {
  FTS_real_index* old;
  FTS_index_t* clone;

  old = (FTS_real_index*) ftx;

  // create new index
  clone = FTS_CreateIndex(old->_context, old->getTexts, old->freeWordlist, old->_options, sizes);
  if (clone != NULL) {
    // copy documents
    FTS_document_id_t i;
    uint64_t count = 0;

    for (i = 1; i <= old->_lastSlot; i++) {
      FTS_document_id_t found;
      int res;

      if (old->_handlesFree[i] == 1) {
        // document is marked as deleted
        continue;
      }

      found = old->_handles[i];
      if (found == excludeDocument) {
        // do not insert this document, because the caller will insert it later
        continue;
      }

      res = FTS_AddDocument(clone, found);
      if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE) {
        // if resize fails, everything's ruined
        LOG_ERROR("resizing the fulltext index failed with %d, sizes were: %llu %llu %llu %llu",
          res,
          (unsigned long long) sizes[0],
          (unsigned long long) sizes[1],
          (unsigned long long) sizes[2],
          (unsigned long long) sizes[3]);

        FTS_FreeIndex(clone);
        return NULL;
      }

      ++count;
    }

    LOG_DEBUG("cloned %llu documents", (unsigned long long) count);
  }
  
  return clone;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new fulltext index
///
/// sizes[0] = size of handles table to start with
/// sizes[1] = number of bytes for index 1
/// sizes[2] = number of bytes for index 2
/// sizes[3] = number of bytes for index 3
////////////////////////////////////////////////////////////////////////////////

FTS_index_t* FTS_CreateIndex (void* context,
                              FTS_texts_t* (*getTexts)(FTS_document_id_t, void*),
                              void (*freeWordlist)(FTS_texts_t*),
                              int options, 
                              uint64_t sizes[4]) {
  FTS_real_index* ix;
  uint64_t i;
    
  LOG_TRACE("creating fulltext index with sizes %llu %llu %llu %llu", 
            (unsigned long long) sizes[0],
            (unsigned long long) sizes[1],
            (unsigned long long) sizes[2],
            (unsigned long long) sizes[3]);
  
  ix = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(FTS_real_index), false);
  if (ix == NULL) {
    return NULL;
  }

  ix->_handles = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (sizes[0] + 2) * sizeof(FTS_document_id_t), false);
  if (ix->_handles == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);
    return NULL;
  }

  ix->_handlesFree = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, (sizes[0] + 2) * sizeof(uint8_t), false);
  if (ix->_handlesFree == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);
    return NULL;
  }
 
  ix->_maxDocuments = sizes[0]; 
  ix->_numDocuments = 0;
  ix->_numDeletions = 0;
  ix->_context      = context;
  ix->_options      = options;
  ix->_ix3KKey      = 0;

  // wordlists retrieval function
  ix->getTexts      = getTexts;
  // free function for wordlists
  ix->freeWordlist  = freeWordlist;

  // set up free chain of document handles
  for (i = 1; i < sizes[0]; i++) {
    ix->_handles[i]          = i + 1;
    ix->_handlesFree[i]      = 1;
  }

  // end of free chain  
  ix->_handles[sizes[0]]     = 0;  
  ix->_handlesFree[sizes[0]] = 1;
  ix->_firstFree             = 1;
  ix->_lastSlot              = sizes[0];

  // create index 2 
  // ---------------------------------------------------

  ix->_index2 = ZStrTuberCons(sizes[2], TUBER_BITS_8);
  if (ix->_index2 == NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handlesFree);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);

    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  if (ZStrTuberIns(ix->_index2, 0, 0) != 0) {
    ZStrTuberDest(ix->_index2);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handlesFree);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);

    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // create index 3
  // ---------------------------------------------------

  ix->_index3 = ZStrTuberCons(sizes[3], TUBER_BITS_64);
  if (ix->_index3 == NULL) {
    ZStrTuberDest(ix->_index2);

    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handlesFree);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);

    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }
  
  // create index 1
  // ---------------------------------------------------

  if (ix->_options == FTS_INDEX_SUBSTRINGS) {
    ix->_index1 = ZStrTuberCons(sizes[1], TUBER_BITS_8);
    if (ix->_index1 == NULL) {
      ZStrTuberDest(ix->_index3);
      ZStrTuberDest(ix->_index2);
    
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handlesFree);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);

      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    if (ZStrTuberIns(ix->_index1, 0, 0) != 0) {
      ZStrTuberDest(ix->_index1);
      ZStrTuberDest(ix->_index3);
      ZStrTuberDest(ix->_index2);

      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handlesFree);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
      TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);

      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }
  }
  
  return (FTS_index_t*) ix;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free an existing fulltext index
////////////////////////////////////////////////////////////////////////////////

void FTS_FreeIndex (FTS_index_t* ftx) {
  FTS_real_index* ix;

  ix = (FTS_real_index*) ftx;

  if (ix->_options == FTS_INDEX_SUBSTRINGS) {
    ZStrTuberDest(ix->_index1);
  }

  ZStrTuberDest(ix->_index2);
  ZStrTuberDest(ix->_index3);

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handlesFree);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix->_handles);
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, ix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a document to the index
/// the caller must have write-locked the index
////////////////////////////////////////////////////////////////////////////////

int FTS_AddDocument (FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix;
  FTS_texts_t* rawwords;
  uint64_t sizes[4];
  int health;
  int res;
   
  ix = (FTS_real_index*) ftx;

  // get the actual words from the caller 
  rawwords = ix->getTexts(docid, ix->_context);
  if (rawwords == NULL || rawwords->_len == 0) {
    // document does not contain words
    return TRI_ERROR_NO_ERROR;
  }

  res = RealAddDocument(ftx, docid, rawwords);

  health = FTS_HealthIndex(ftx, sizes);
  if (health > HEALTH_THRESHOLD || res == TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE) {
    LOG_TRACE("fulltext index health threshold exceeded. new suggested sizes are: %llu %llu %llu %llu",
              (unsigned long long) sizes[0],
              (unsigned long long) sizes[1],
              (unsigned long long) sizes[2],
              (unsigned long long) sizes[3]);
    res = TRI_ERROR_ARANGO_INDEX_NEEDS_RESIZE;
  }

  ix->freeWordlist(rawwords);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document from the index
/// the caller must have write-locked the index
////////////////////////////////////////////////////////////////////////////////

int FTS_DeleteDocument (FTS_index_t* ftx, FTS_document_id_t docid) {
  int res;
  
  res = RealDeleteDocument(ftx, docid);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief update an existing document in the index
/// the caller must have write-locked the index
////////////////////////////////////////////////////////////////////////////////

int FTS_UpdateDocument (FTS_index_t* ftx, FTS_document_id_t docid) {
  FTS_real_index* ix;
  FTS_texts_t* rawwords;
  int res;

  ix = (FTS_real_index*) ftx;

  // get the actual words from the caller 
  rawwords = ix->getTexts(docid, ix->_context);
  if (rawwords == NULL || rawwords->_len == 0) {
    // document does not contain words
    return TRI_ERROR_NO_ERROR;
  }

  RealDeleteDocument(ftx, docid);
  res = RealAddDocument(ftx, docid, rawwords);
  
  ix->freeWordlist(rawwords);

  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the index should be cleaned up
////////////////////////////////////////////////////////////////////////////////

bool FTS_ShouldCleanupIndex (FTS_index_t* ftx) {
  FTS_real_index* ix;

  ix = (FTS_real_index*) ftx;

  return (ix->_numDeletions > FTS_CLEANUP_THRESHOLD);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Incremental scan and cleanup routine, called from a background task
/// This reads index3 and removes handles of unused documents. Will stop after
/// stop after scanning <docs> document/word pair scans. 
/// The caller must have write-locked the index
/// 
/// The function may return the following values:
/// 0 = cleanup done, but not finished
/// 1 = out of memory
/// 2 = index needs a resize
/// 3 = cleanup finished
////////////////////////////////////////////////////////////////////////////////

int FTS_BackgroundTask (FTS_index_t* ftx, int docs) {
  FTS_real_index* ix;
  int dleft, i;
  CTX cold;
  CTX cnew;
  uint64_t newterm;
  uint64_t oldhan;
  uint64_t han;
  ZSTR* zold;
  ZSTR* znew;
  int result;

  znew = ZStrCons(100);
  if (znew == NULL) {
    return 1;
  }

  zold = ZStrCons(100);
  if (zold == NULL) {
    ZStrDest(znew);
    return 1;
  }
  
  dleft = docs;
  result = 0;
  ix = (FTS_real_index*) ftx;

  while (dleft > 0) {
    uint64_t numDeletions;

    assert(ix->_ix3KKey < (ix->_index3)->kmax);

    numDeletions = 0;
    i = ZStrTuberRead(ix->_index3, ix->_ix3KKey, zold);
    if (i == 2) {
      result = 1;
      break;
    }

    if (i == 0) {
      ZStrCxClear(&zcdoc, &cold);
      ZStrCxClear(&zcdoc, &cnew);
      ZStrClear(znew);
      oldhan = 0;
      newterm =0;
      while (1) {
        han = ZStrCxDec(zold, &zcdoc, &cold);
        if (han == oldhan) {
          break;
        }

        oldhan = han;
        dleft--;

        if (ix->_handlesFree[han] == 0) {
          i = ZStrCxEnc(znew, &zcdoc, &cnew, han);
          if (i != 0) {
            ix->_ix3KKey = 0;
            ZStrDest(znew);
            ZStrDest(zold);
            return 1;
          }
          newterm = han;
        }
        else {
          // something was deleted
          ++numDeletions;
        }
      }

      if (numDeletions > 0) {
        // update existing entry in tuber
        // but only if there's something to update

        i = ZStrCxEnc(znew, &zcdoc, &cnew, newterm);
        if (i != 0) {
          ix->_ix3KKey = 0;
          ZStrDest(znew);
          ZStrDest(zold);
          return 1;
        }
  
        if (ix->_numDeletions >= numDeletions) {
          ix->_numDeletions -= numDeletions;
        }

        ZStrNormalize(znew);
        i = ZStrTuberUpdate(ix->_index3, ix->_ix3KKey, znew);
      }

      if (i != 0) {
        ix->_ix3KKey = 0;
        ZStrDest(znew);
        ZStrDest(zold);
        return i;
      }
    }

    // next
    ix->_ix3KKey++;

    if (ix->_ix3KKey >= (ix->_index3)->kmax) {
      ix->_ix3KKey = 0;
      result = 3; // finished iterating over all document handles
      break;
    }
  }

  ZStrDest(znew);
  ZStrDest(zold);

  return result;  
}

////////////////////////////////////////////////////////////////////////////////
/// @brief perform a search in the index
/// The caller must have read-locked the index
////////////////////////////////////////////////////////////////////////////////

FTS_document_ids_t* FTS_FindDocuments (FTS_index_t* ftx,
                                       FTS_query_t* query) {
  FTS_document_ids_t* dc;
  FTS_real_index* ix;
  ZSTR* zstr2;
  ZSTR* zstr3;
  ZSTR* zstra1;
  ZSTR* zstra2;
  ZSTR* ztemp;
  ZSTR* zstr;
  CTX ctxa1;
  CTX ctxa2;
  CTX ctx3;
  size_t queryterm;
  uint64_t word[2 * (MAX_WORD_LENGTH + SPACING)];
  uint64_t ndocs = 0;

  // initialise 
  dc = NULL;
  TRI_set_errno(TRI_ERROR_NO_ERROR);

  zstr2 = ZStrCons(10);  /* from index-2 tuber */
  if (zstr2 == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  zstr3 = ZStrCons(10);  /* from index-3 tuber  */
  if (zstr3 == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    ZStrDest(zstr2);
    return NULL;
  }

  zstra1 = ZStrCons(10); /* current list of documents */
  if (zstra1 == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    ZStrDest(zstr3);
    ZStrDest(zstr2);
    return NULL;
  }

  zstra2 = ZStrCons(10); /* new list of documents  */
  if (zstra2 == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    ZStrDest(zstra1);
    ZStrDest(zstr3);
    ZStrDest(zstr2);
    return NULL;
  }

  zstr = ZStrCons(4);  /* work zstr from stex  */
  if (zstr == NULL) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    ZStrDest(zstra2);
    ZStrDest(zstra1);
    ZStrDest(zstr3);
    ZStrDest(zstr2);
    return NULL;
  }

  ix = (FTS_real_index*) ftx;

  // for each term in the query 
  for (queryterm = 0; queryterm < query->_len; queryterm++) {
    if (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING &&
        ix->_options != FTS_INDEX_SUBSTRINGS) {
      // substring search but index does not contain substrings
      ZStrDest(zstra1);
      ZStrDest(zstra2);
      ZStrDest(zstr); 
      ZStrDest(zstr2); 
      ZStrDest(zstr3);  
      return NULL;
    }

/*  Depending on the query type, the objective is do   */
/*  populate or "and" zstra1 with the sorted list      */
/*  of document handles that match that term           */
/*  TBD - what to do if it is not a legal option?  */
/* TBD combine this with other options - no need to use zstring  */
    ndocs = 0;

    if (query->_localOptions[queryterm] == FTS_MATCH_COMPLETE) {
      uint64_t docb;
      uint64_t dock;
      uint64_t kkey;
      uint64_t lasthan;

      FillWordBuffer(&word[0], query->_texts[queryterm]);

      kkey = FindKKey2(ix, word);
      if (kkey == NOTFOUND) {
        break;
      }

      ZStrTuberRead(ix->_index2, kkey, zstr2);
      if (ZStrBitsOut(zstr2, 1) != 1) {
        break;
      }

      docb = ZStrDec(zstr2, &zcbky);
      dock = ZStrTuberK(ix->_index3, kkey, 0, docb);
      if (ZStrTuberRead(ix->_index3, dock, zstr3) == 1) {
        printf("Kkey not in ix3 - we're terrified\n");
      }

      ZStrCxClear(&zcdoc, &ctx3);
      ZStrCxClear(&zcdoc, &ctxa2);
      ZStrClear(zstra2);
      lasthan = 0;
      
      if (queryterm == 0) {
        uint64_t newhan = 0;

        while (1) {
          uint64_t oldhan;

          oldhan = newhan;
          newhan = ZStrCxDec(zstr3, &zcdoc, &ctx3);
          if (newhan == oldhan) {
            break;
          }
          if (ix->_handlesFree[newhan] == 0) {
            if (ZStrCxEnc(zstra2, &zcdoc, &ctxa2, newhan) != 0) {
              TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
              goto oom;
            }
            lasthan = newhan;
            ndocs++;
          }
        }
      }
      else {
        uint64_t nhand1;
        uint64_t ohand1;
        uint64_t oldhan;
        uint64_t newhan;

        ZStrCxClear(&zcdoc, &ctxa1);
        ohand1 = 0;
        nhand1 = ZStrCxDec(zstra1, &zcdoc, &ctxa1);
        oldhan = 0;
        newhan = ZStrCxDec(zstr3, &zcdoc, &ctx3);
        // zstra1 = zstra1 & zstra2  
        while (1) {
          if (nhand1 == ohand1) {
            break;
          }
          if (oldhan == newhan) {
            break;
          }
          if (newhan == nhand1) {
            if (ix->_handlesFree[newhan] == 0) {
              if (ZStrCxEnc(zstra2, &zcdoc, &ctxa2, newhan) != 0) {
                TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
                goto oom;
              }
              lasthan = newhan;
              ndocs++;
            }
            oldhan = newhan;
            newhan = ZStrCxDec(zstr3, &zcdoc, &ctx3);
            ohand1 = nhand1;
            nhand1 = ZStrCxDec(zstra1, &zcdoc, &ctxa1);
          }
          else if (newhan > nhand1) {
            ohand1 = nhand1;
            nhand1 = ZStrCxDec(zstra1, &zcdoc, &ctxa1);
          }
          else {
            oldhan = newhan;
            newhan = ZStrCxDec(zstr3, &zcdoc, &ctx3);
          }
        }
      }

      if (ZStrCxEnc(zstra2, &zcdoc, &ctxa2, lasthan) != 0) {
        TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
        goto oom;
      }

      ZStrNormalize(zstra2);
      ztemp = zstra1;
      zstra1 = zstra2;
      zstra2 = ztemp;
    }  /* end of match-complete code  */
    else if ((query->_localOptions[queryterm] == FTS_MATCH_PREFIX) ||
             (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING)) {
      uint16_t* docpt;
      STEX* dochan;
      uint64_t odocs;
      uint64_t lasthan;

      // make STEX to contain new list of handles 
      dochan = ZStrSTCons(2);
      if (dochan == NULL) {
        TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
        goto oom;
      }

      FillWordBuffer(&word[MAX_WORD_LENGTH + SPACING], query->_texts[queryterm]);
      
      if (query->_localOptions[queryterm] == FTS_MATCH_PREFIX) {
        // prefix matching
        uint64_t kkey;

        kkey = FindKKey2(ix, word + MAX_WORD_LENGTH + SPACING);
        if (kkey == NOTFOUND) {
          ZStrSTDest(dochan); 
          break;
        }

        // call routine to recursively put handles to STEX
        if (Ix2Recurs(dochan, ix, kkey) != TRI_ERROR_NO_ERROR) {
          ZStrSTDest(dochan); 
          TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
          goto oom;
        }
      }
      else if (query->_localOptions[queryterm] == FTS_MATCH_SUBSTRING) {
        // substring matching
        uint64_t kkey;

        kkey = FindKKey1(ix, word + MAX_WORD_LENGTH + SPACING);
        if (kkey == NOTFOUND) {
          ZStrSTDest(dochan); 
          break;
        }
        // call routine to recursively put handles to STEX
        if (Ix1Recurs(dochan, ix, kkey, word + MAX_WORD_LENGTH + SPACING) != TRI_ERROR_NO_ERROR) {
          ZStrSTDest(dochan); 
          TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
          goto oom;
        }
      }

      ZStrSTSort(dochan);

      odocs = dochan->cnt;
      docpt = dochan->list;
      ZStrCxClear(&zcdoc, &ctxa2);
      ZStrClear(zstra2);
      lasthan = 0;
      
      if (queryterm == 0) {
        uint64_t i;

        for (i = 0; i < odocs; i++) {
          uint64_t newhan;

          if (ZStrInsert(zstr, docpt, 2) != 0) {
            TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
            ZStrSTDest(dochan); 
            goto oom;
          }
          newhan = ZStrDec(zstr, &zcdh);
          docpt += ZStrExtLen(docpt, 2);
          if (ix->_handlesFree[newhan] == 0) {
            if (ZStrCxEnc(zstra2, &zcdoc, &ctxa2, newhan) != 0) {
              TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
              ZStrSTDest(dochan); 
              goto oom;
            }
            lasthan = newhan;
            ndocs++;
          }
        }
      }
      else {
        // merge prefix stex with zstra1
        uint64_t newhan;
        uint64_t nhand1;
        uint64_t ohand1;

        ZStrCxClear(&zcdoc, &ctxa1);
        if (odocs == 0) {
          ZStrSTDest(dochan); 
          continue;
        }

        nhand1 = ZStrCxDec(zstra1, &zcdoc, &ctxa1);
        if (ZStrInsert(zstr, docpt, 2) != 0) {
          TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
          ZStrSTDest(dochan); 
          goto oom;
        }
        newhan = ZStrDec(zstr, &zcdh);
        docpt += ZStrExtLen(docpt, 2);
        odocs--;
        ohand1 = 0;

        // zstra1 = zstra1 & zstra2  
        while (1) {
          if (nhand1 == ohand1) {
            break;
          }
          if (newhan == nhand1) {
            if (ix->_handlesFree[newhan] == 0) {
              if (ZStrCxEnc(zstra2, &zcdoc, &ctxa2, newhan) != 0) {
                TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
                ZStrSTDest(dochan); 
                goto oom;
              }

              lasthan = newhan;
              ndocs++;
            }
            if (odocs == 0) {
              break;
            }
            if (ZStrInsert(zstr, docpt, 2) != 0) {
              TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
              ZStrSTDest(dochan); 
              goto oom;
            }

            newhan = ZStrDec(zstr, &zcdh);
            docpt += ZStrExtLen(docpt, 2);
            odocs--;
            ohand1 = nhand1;
            nhand1 = ZStrCxDec(zstra1, &zcdoc, &ctxa1);
          }
          else if (newhan > nhand1) {
            ohand1 = nhand1;
            nhand1 = ZStrCxDec(zstra1, &zcdoc, &ctxa1);
          }
          else {
            if (odocs == 0) {
              break;
            }
            if (ZStrInsert(zstr, docpt, 2) != 0) {
              TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
              ZStrSTDest(dochan); 
              goto oom;
            }
            newhan = ZStrDec(zstr, &zcdh);
            docpt += ZStrExtLen(docpt, 2);
            odocs--;
          }
        }
      }
      if (ZStrCxEnc(zstra2, &zcdoc, &ctxa2, lasthan) != 0) {
        TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
        goto oom;
      }
      ZStrNormalize(zstra2);
      ztemp = zstra1;
      zstra1 = zstra2;
      zstra2 = ztemp; 
      ZStrSTDest(dochan); 
    }   /* end of match-prefix code  */
  }


  // prepare the result set
  dc = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(FTS_document_ids_t), false);
  if (dc == NULL) {
    // out of memory
  }
  else {
    // init result set
    dc->_len = 0;
    dc->_docs = NULL;
    
    if (ndocs > 0) {
      // we found some results
      dc->_docs = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, ndocs * sizeof(FTS_document_id_t), false);
      if (dc->_docs != NULL) {
        ZStrCxClear(&zcdoc, &ctxa1);
        AddResultDocuments(dc, ix, zstra1, &ctxa1);
      }
      else {
        // this will trigger an out of memory error at the call size
        TRI_Free(TRI_UNKNOWN_MEM_ZONE, dc);
        dc = NULL;
      }
    }
  }

oom:
    
  ZStrDest(zstra1);
  ZStrDest(zstra2);
  ZStrDest(zstr); 
  ZStrDest(zstr2); 
  ZStrDest(zstr3);  

  return dc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free results of a search
////////////////////////////////////////////////////////////////////////////////

void FTS_Free_Documents (FTS_document_ids_t* doclist) {
  if (doclist->_docs != NULL) {
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, doclist->_docs);
  }

  TRI_Free(TRI_UNKNOWN_MEM_ZONE, doclist);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#if 0
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
    i=ZStrTuberRead(ix->_index2,kkey,zstr);
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
        dock=ZStrTuberK(ix->_index3,kkey,0,docb);
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
        i=ZStrTuberRead(ix->_index3,dock,x3zstr);
        oldhan=0;
        while(1)
        {
            han=ZStrCxDec(x3zstr,&zcdoc,&x3ctx);
            if(han==oldhan) break;
            temp=han;
            printf("h= %d ",temp);
            temp=ix->_handles[han];
            printf("id= %d; ",temp);
            oldhan=han;
        }
    }
    printf("\n");
    i=ZStrTuberRead(ix->_index2,kkey,zstr);
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
        newkkey=ZStrTuberK(ix->_index2,kkey,newlet,bkey);
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
temp=ix->_firstFree;
    printf("Free-chain starts at handle %d\n",temp);
    printf("======= First ten handles======\n");
    for(i=1;i<11;i++)
    {
temp=ix->_handles[i];
        printf("Handle %d is docid %d\n", i,temp);
    }
    printf("======= Index 2 ===============\n");
    kroot=ZStrTuberK(ix->_index2,0,0,0);
    index2dump(ix,kroot,1);
}
#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
