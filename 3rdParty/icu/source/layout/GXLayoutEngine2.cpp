/*
 *
 * (C) Copyright IBM Corp.  and others 1998-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "LayoutEngine.h"
#include "GXLayoutEngine2.h"
#include "LEGlyphStorage.h"
#include "MorphTables.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(GXLayoutEngine2)

GXLayoutEngine2::GXLayoutEngine2(const LEFontInstance *fontInstance, le_int32 scriptCode, le_int32 languageCode, const LEReferenceTo<MorphTableHeader2> &morphTable, le_int32 typoFlags, LEErrorCode &success) 
  : LayoutEngine(fontInstance, scriptCode, languageCode, typoFlags, success), fMorphTable(morphTable)
{
  // nothing else to do?
}

GXLayoutEngine2::~GXLayoutEngine2()
{
    reset();
}

// apply 'morx' table
le_int32 GXLayoutEngine2::computeGlyphs(const LEUnicode chars[], le_int32 offset, le_int32 count, le_int32 max, le_bool rightToLeft, LEGlyphStorage &glyphStorage, LEErrorCode &success)
{
    if (LE_FAILURE(success)) {
        return 0;
    }

    if (chars == NULL || offset < 0 || count < 0 || max < 0 || offset >= max || offset + count > max) {
        success = LE_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }
    
    mapCharsToGlyphs(chars, offset, count, rightToLeft, rightToLeft, glyphStorage, success);

    if (LE_FAILURE(success)) {
        return 0;
    }

    fMorphTable->process(fMorphTable, glyphStorage, fTypoFlags, success);
    return count;
}

// apply positional tables
void GXLayoutEngine2::adjustGlyphPositions(const LEUnicode chars[], le_int32 offset, le_int32 count, le_bool /*reverse*/,
                                          LEGlyphStorage &/*glyphStorage*/, LEErrorCode &success)
{
    if (LE_FAILURE(success)) {
        return;
    }

    if (chars == NULL || offset < 0 || count < 0) {
        success = LE_ILLEGAL_ARGUMENT_ERROR;
        return;
    }

    // FIXME: no positional processing yet...
}

U_NAMESPACE_END
