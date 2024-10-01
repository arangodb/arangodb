// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 *
 *   Copyright (C) 1999-2015, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 *******************************************************************************
 *   file name:  SimpleFontInstance.cpp
 *
 *   created on: 03/30/2006
 *   created by: Eric R. Mader
 */

#include "unicode/utypes.h"
#include "unicode/uchar.h"

#include "layout/LETypes.h"
#include "layout/LEFontInstance.h"

#ifndef USING_ICULEHB
#include "CanonShaping.h"
#endif

#include "SimpleFontInstance.h"

SimpleFontInstance::SimpleFontInstance(float pointSize, LEErrorCode &status)
    : fPointSize(pointSize), fAscent(0), fDescent(0)
{
    if (LE_FAILURE(status)) {
        return;
    }

    fAscent  = (le_int32) yUnitsToPoints(2000.0);
    fDescent = (le_int32) yUnitsToPoints(600.0);

    return;
}

SimpleFontInstance::~SimpleFontInstance()
{
    // nothing to do...
}

const void *SimpleFontInstance::getFontTable(LETag tableTag, size_t &length) const
{
  length = -1; // unknown for this test.
#ifndef USING_ICULEHB
    if (tableTag == LE_GSUB_TABLE_TAG) {
        return CanonShaping::glyphSubstitutionTable;
    }
    
    if (tableTag == LE_GDEF_TABLE_TAG) {
        return CanonShaping::glyphDefinitionTable;
    }
#endif
    return NULL;
}

void SimpleFontInstance::getGlyphAdvance(LEGlyphID glyph, LEPoint &advance) const
{
#if 0
    if (u_getCombiningClass((UChar32) glyph) == 0) {
        advance.fX = xUnitsToPoints(2048);
    } else {
        advance.fX = 0;
    }
#else
    (void)glyph;  // Suppress unused parameter compiler warning.
    advance.fX = xUnitsToPoints(2048);
#endif

    advance.fY = 0;
}

le_int32 SimpleFontInstance::getUnitsPerEM() const
{
    return 2048;
}

le_int32 SimpleFontInstance::getAscent() const
{
    return fAscent;
}

le_int32 SimpleFontInstance::getDescent() const
{
    return fDescent;
}

le_int32 SimpleFontInstance::getLeading() const
{
    return 0;
}

// We really want to inherit this method from the superclass, but some compilers
// issue a warning if we don't implement it...
LEGlyphID SimpleFontInstance::mapCharToGlyph(LEUnicode32 ch, const LECharMapper *mapper, le_bool filterZeroWidth) const
{
    return LEFontInstance::mapCharToGlyph(ch, mapper, filterZeroWidth);
}

// We really want to inherit this method from the superclass, but some compilers
// issue a warning if we don't implement it...
LEGlyphID SimpleFontInstance::mapCharToGlyph(LEUnicode32 ch, const LECharMapper *mapper) const
{
    return LEFontInstance::mapCharToGlyph(ch, mapper);
}

LEGlyphID SimpleFontInstance::mapCharToGlyph(LEUnicode32 ch) const
{
    return (LEGlyphID) ch;
}

float SimpleFontInstance::getXPixelsPerEm() const
{
    return fPointSize;
}

float SimpleFontInstance::getYPixelsPerEm() const
{
    return fPointSize;
}

float SimpleFontInstance::getScaleFactorX() const
{
    return 1.0;
}

float SimpleFontInstance::getScaleFactorY() const
{
    return 1.0;
}

le_bool SimpleFontInstance::getGlyphPoint(LEGlyphID /*glyph*/, le_int32 /*pointNumber*/, LEPoint &/*point*/) const
{
    return FALSE;
}

