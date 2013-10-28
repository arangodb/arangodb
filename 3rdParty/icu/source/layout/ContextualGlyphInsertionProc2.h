/*
 *
 * (C) Copyright IBM Corp.  and others 2013 - All Rights Reserved
 *
 */

#ifndef __CONTEXTUALGLYPHINSERTIONPROCESSOR2_H
#define __CONTEXTUALGLYPHINSERTIONPROCESSOR2_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor2.h"
#include "StateTableProcessor2.h"
#include "ContextualGlyphInsertionProc2.h"
#include "ContextualGlyphInsertion.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class ContextualGlyphInsertionProcessor2 : public StateTableProcessor2
{
public:
    virtual void beginStateTable();

    virtual le_uint16 processStateEntry(LEGlyphStorage &glyphStorage, 
                                        le_int32 &currGlyph, EntryTableIndex2 index, LEErrorCode &success);

    virtual void endStateTable();

    ContextualGlyphInsertionProcessor2(const LEReferenceTo<MorphSubtableHeader2> &morphSubtableHeader, LEErrorCode &success);
    virtual ~ContextualGlyphInsertionProcessor2();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @stable ICU 2.8
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @stable ICU 2.8
     */
    static UClassID getStaticClassID();

private:
    ContextualGlyphInsertionProcessor2();

    /**
     * Perform the actual insertion
     * @param atGlyph index of glyph to insert at
     * @param index index into the insertionTable (in/out)
     * @param count number of insertions
     * @param isKashidaLike Kashida like (vs Split Vowel like). No effect currently.
     * @param isBefore if true, insert extra glyphs before the marked glyph
     */
    void doInsertion(LEGlyphStorage &glyphStorage,
                              le_int16 atGlyph,
                              le_int16 &index,
                              le_int16 count,
                              le_bool isKashidaLike,
                              le_bool isBefore,
                              LEErrorCode &success);


protected:
    le_int32 markGlyph;
    LEReferenceToArrayOf<le_uint16> insertionTable;
    LEReferenceToArrayOf<ContextualGlyphInsertionStateEntry2> entryTable;
    LEReferenceTo<ContextualGlyphInsertionHeader2> contextualGlyphHeader;
};

U_NAMESPACE_END
#endif
