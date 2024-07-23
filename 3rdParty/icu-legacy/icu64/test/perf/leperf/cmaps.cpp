/**************************************************************************
*
*   © 2016 and later: Unicode, Inc. and others.
*   License & terms of use: http://www.unicode.org/copyright.html#License
*
***************************************************************************
***************************************************************************
*
*   Copyright (C) 1998-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
************************************************************************/

#include "layout/LETypes.h"
#include "layout/LESwaps.h"

#include "sfnt.h"
#include "cmaps.h"
#include <stdio.h>

#define SWAPU16(code) ((LEUnicode16) SWAPW(code))
#define SWAPU32(code) ((LEUnicode32) SWAPL(code))

//
// Finds the high bit by binary searching
// through the bits in value.
//
le_int8 highBit(le_uint32 value)
{
    le_uint8 bit = 0;

    if (value >= 1 << 16) {
        value >>= 16;
        bit += 16;
    }

    if (value >= 1 << 8) {
        value >>= 8;
        bit += 8;
    }

    if (value >= 1 << 4) {
        value >>= 4;
        bit += 4;
    }

    if (value >= 1 << 2) {
        value >>= 2;
        bit += 2;
    }

    if (value >= 1 << 1) {
        value >>= 1;
        bit += 1;
    }

    return bit;
}

CMAPMapper *CMAPMapper::createUnicodeMapper(const CMAPTable *cmap)
{
    le_uint16 i;
    le_uint16 nSubtables = SWAPW(cmap->numberSubtables);
    const CMAPEncodingSubtable *subtable = NULL;
    le_bool found = FALSE;
    le_uint16 foundPlatformID = 0xFFFF;
    le_uint16 foundPlatformSpecificID = 0xFFFF;
    le_uint32 foundOffset = 0;
    le_uint16 foundTable = 0xFFFF;
    // first pass, look for MS table. (preferred?)
    for (i = 0; i < nSubtables && !found; i += 1) {
        const CMAPEncodingSubtableHeader *esh = &cmap->encodingSubtableHeaders[i];

        le_uint16 platformID = SWAPW(esh->platformID);
        le_uint16 platformSpecificID = SWAPW(esh->platformSpecificID);
        if (platformID == 3) { // microsoft
          switch (platformSpecificID) {
            case 1: // Unicode BMP (UCS-2)
            case 10: // Unicode UCS-4
                foundOffset = SWAPL(esh->encodingOffset);
                foundPlatformID = platformID;
                foundPlatformSpecificID = platformSpecificID;
                found = TRUE;
                foundTable = i;
                break;

                //default:
              //              printf("%s:%d: microsoft (3) platform specific ID %d (wanted 1 or 10) for subtable %d/%d\n", __FILE__, __LINE__, (SWAPW(esh->platformSpecificID)), i, nSubtables);
            }
        } else {
          //printf("%s:%d: platform  ID %d (wanted 3, microsoft) for subtable %d/%d\n", __FILE__, __LINE__, (SWAPW(esh->platformID)), i, nSubtables);
        }
    }

    // second pass, allow non MS table
    // first pass, look for MS table. (preferred?)
      for (i = 0; i < nSubtables && !found; i += 1) {
        const CMAPEncodingSubtableHeader *esh = &cmap->encodingSubtableHeaders[i];
        le_uint16 platformID = SWAPW(esh->platformID);
        le_uint16 platformSpecificID = SWAPW(esh->platformSpecificID);
        //printf("%s:%d: table %d/%d has platform:specific %d:%d\n", __FILE__, __LINE__, i, nSubtables, platformID, platformSpecificID);
        switch(platformID) {
        case 0: // Unicode platform
          switch(platformSpecificID) {
          case 0:
          case 1:
          case 2:
          case 3:
            foundOffset = SWAPL(esh->encodingOffset);
            foundPlatformID = platformID;
            foundPlatformSpecificID = platformSpecificID;
            foundTable = i;
            found = TRUE;
            break;

          default: printf("Error: table %d (psid %d) is unknown. Skipping.\n", i, platformSpecificID); break;
          }
          break;
        
          //default:
          //printf("Skipping platform id %d\n", platformID);
        }
    }


    if (found)
    {
      subtable = (const CMAPEncodingSubtable *) ((const char *) cmap + foundOffset);
      //printf("%s:%d: using subtable #%d/%d type %d:%d\n", __FILE__, __LINE__, foundTable, nSubtables, foundPlatformID, foundPlatformSpecificID);
    } else {
      printf("%s:%d: could not find subtable.\n", __FILE__, __LINE__);
      return NULL;
    }

    le_uint16 tableFormat = SWAPW(subtable->format);
    //printf("%s:%d: table format %d\n", __FILE__, __LINE__, tableFormat);

    switch (tableFormat) {
    case 4:
        return new CMAPFormat4Mapper(cmap, (const CMAPFormat4Encoding *) subtable);

    case 12:
    {
        const CMAPFormat12Encoding *encoding = (const CMAPFormat12Encoding *) subtable;

        return new CMAPGroupMapper(cmap, encoding->groups, SWAPL(encoding->nGroups));
    }

    default:
        break;
    }

    printf("%s:%d: Unknown format %x.\n", __FILE__, __LINE__, (SWAPW(subtable->format)));
    return NULL;
}

CMAPFormat4Mapper::CMAPFormat4Mapper(const CMAPTable *cmap, const CMAPFormat4Encoding *header)
    : CMAPMapper(cmap)
{
    le_uint16 segCount = SWAPW(header->segCountX2) / 2;

    fEntrySelector = SWAPW(header->entrySelector);
    fRangeShift = SWAPW(header->rangeShift) / 2;
    fEndCodes = &header->endCodes[0];
    fStartCodes = &header->endCodes[segCount + 1]; // + 1 for reservedPad...
    fIdDelta = &fStartCodes[segCount];
    fIdRangeOffset = &fIdDelta[segCount];
}

LEGlyphID CMAPFormat4Mapper::unicodeToGlyph(LEUnicode32 unicode32) const
{
    if (unicode32 >= 0x10000) {
        return 0;
    }

    LEUnicode16 unicode = (LEUnicode16) unicode32;
    le_uint16 index = 0;
    le_uint16 probe = 1 << fEntrySelector;
    TTGlyphID result = 0;

    if (SWAPU16(fStartCodes[fRangeShift]) <= unicode) {
        index = fRangeShift;
    }

    while (probe > (1 << 0)) {
        probe >>= 1;

        if (SWAPU16(fStartCodes[index + probe]) <= unicode) {
            index += probe;
        }
    }

    if (unicode >= SWAPU16(fStartCodes[index]) && unicode <= SWAPU16(fEndCodes[index])) {
        if (fIdRangeOffset[index] == 0) {
            result = (TTGlyphID) unicode;
        } else {
            le_uint16 offset = unicode - SWAPU16(fStartCodes[index]);
            le_uint16 rangeOffset = SWAPW(fIdRangeOffset[index]);
            le_uint16 *glyphIndexTable = (le_uint16 *) ((char *) &fIdRangeOffset[index] + rangeOffset);

            result = SWAPW(glyphIndexTable[offset]);
        }

        result += SWAPW(fIdDelta[index]);
    } else {
        result = 0;
    }

    return LE_SET_GLYPH(0, result);
}

CMAPFormat4Mapper::~CMAPFormat4Mapper()
{
    // parent destructor does it all
}

CMAPGroupMapper::CMAPGroupMapper(const CMAPTable *cmap, const CMAPGroup *groups, le_uint32 nGroups)
    : CMAPMapper(cmap), fGroups(groups)
{
    le_uint8 bit = highBit(nGroups);
    fPower = 1 << bit;
    fRangeOffset = nGroups - fPower;
}

LEGlyphID CMAPGroupMapper::unicodeToGlyph(LEUnicode32 unicode32) const
{
    le_int32 probe = fPower;
    le_int32 range = 0;

    if (SWAPU32(fGroups[fRangeOffset].startCharCode) <= unicode32) {
        range = fRangeOffset;
    }

    while (probe > (1 << 0)) {
        probe >>= 1;

        if (SWAPU32(fGroups[range + probe].startCharCode) <= unicode32) {
            range += probe;
        }
    }

    if (SWAPU32(fGroups[range].startCharCode) <= unicode32 && SWAPU32(fGroups[range].endCharCode) >= unicode32) {
        return (LEGlyphID) (SWAPU32(fGroups[range].startGlyphCode) + unicode32 - SWAPU32(fGroups[range].startCharCode));
    }

    return 0;
}

CMAPGroupMapper::~CMAPGroupMapper()
{
    // parent destructor does it all
}

