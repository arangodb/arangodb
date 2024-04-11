// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 **********************************************************************
 *   Copyright (C) 2003-2013, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "layout/LETypes.h"

//#include "letest.h"
#include "FontTableCache.h"

#define TABLE_CACHE_INIT 5
#define TABLE_CACHE_GROW 5

struct FontTableCacheEntry
{
  LETag tag;
  const void *table;
  size_t length;
};

FontTableCache::FontTableCache()
    : fTableCacheCurr(0), fTableCacheSize(TABLE_CACHE_INIT)
{
    fTableCache = LE_NEW_ARRAY(FontTableCacheEntry, fTableCacheSize);

    if (fTableCache == nullptr) {
        fTableCacheSize = 0;
        return;
    }

    for (int i = 0; i < fTableCacheSize; i += 1) {
        fTableCache[i].tag   = 0;
        fTableCache[i].table = nullptr;
        fTableCache[i].length = 0;
    }
}

FontTableCache::~FontTableCache()
{
    for (int i = fTableCacheCurr - 1; i >= 0; i -= 1) {
      LE_DELETE_ARRAY(fTableCache[i].table);

        fTableCache[i].tag   = 0;
        fTableCache[i].table = nullptr;
        fTableCache[i].length = 0;
    }

    fTableCacheCurr = 0;

    LE_DELETE_ARRAY(fTableCache);
}

void FontTableCache::freeFontTable(const void *table) const
{
  LE_DELETE_ARRAY(table);
}

const void *FontTableCache::find(LETag tableTag, size_t &length) const
{
    for (int i = 0; i < fTableCacheCurr; i += 1) {
        if (fTableCache[i].tag == tableTag) {
          length = fTableCache[i].length;
          return fTableCache[i].table;
        }
    }

    const void *table = readFontTable(tableTag, length);

    ((FontTableCache *) this)->add(tableTag, table, length);

    return table;
}

void FontTableCache::add(LETag tableTag, const void *table, size_t length)
{
    if (fTableCacheCurr >= fTableCacheSize) {
        le_int32 newSize = fTableCacheSize + TABLE_CACHE_GROW;

        fTableCache = (FontTableCacheEntry *) LE_GROW_ARRAY(fTableCache, newSize);

        for (le_int32 i = fTableCacheSize; i < newSize; i += 1) {
            fTableCache[i].tag   = 0;
            fTableCache[i].table = nullptr;
            fTableCache[i].length = 0;
        }

        fTableCacheSize = newSize;
    }

    fTableCache[fTableCacheCurr].tag   = tableTag;
    fTableCache[fTableCacheCurr].table = table;
    fTableCache[fTableCacheCurr].length = length;

    fTableCacheCurr += 1;
}
