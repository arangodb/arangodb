/*
 * %W% %E%
 *
 * (C) Copyright IBM Corp. 2008-2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "OpenTypeTables.h"
#include "GlyphSubstitutionTables.h"
#include "LookupProcessor.h"
#include "ExtensionSubtables.h"
#include "GlyphIterator.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

// read a 32-bit value that might only be 16-bit-aligned in memory
static inline le_uint32 READ_LONG(le_uint32 code) {
    le_uint16* first = ((le_uint16*)&code);
    le_uint16* second = (((le_uint16*)&code) + 1);
    return (le_uint32)((SWAPW(*first) << 16) + SWAPW(*second));
}

// FIXME: should look at the format too... maybe have a sub-class for it?
le_uint32 ExtensionSubtable::process(const LookupProcessor *lookupProcessor, le_uint16 lookupType,
                                      GlyphIterator *glyphIterator, const LEFontInstance *fontInstance, LEErrorCode& success) const
{
    const LEReferenceTo<ExtensionSubtable> thisRef(lookupProcessor->getReference(), success); // create a reference to this

    if (LE_FAILURE(success)) {
        return 0;
    }

    le_uint16 elt = SWAPW(extensionLookupType);

    if (elt != lookupType) {      
        le_uint32 extOffset = READ_LONG(extensionOffset);
        LEReferenceTo<LookupSubtable> subtable(thisRef, success, extOffset);

        if(LE_SUCCESS(success)) {
          return lookupProcessor->applySubtable(subtable, elt, glyphIterator, fontInstance, success);
        }
    }

    return 0;
}

U_NAMESPACE_END
