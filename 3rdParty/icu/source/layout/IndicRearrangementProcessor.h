/*
 *
 * (C) Copyright IBM Corp. 1998-2013 - All Rights Reserved
 *
 */

#ifndef __INDICREARRANGEMENTPROCESSOR_H
#define __INDICREARRANGEMENTPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "MorphTables.h"
#include "SubtableProcessor.h"
#include "StateTableProcessor.h"
#include "IndicRearrangement.h"

U_NAMESPACE_BEGIN

class LEGlyphStorage;

class IndicRearrangementProcessor : public StateTableProcessor
{
public:
    virtual void beginStateTable();

    virtual ByteOffset processStateEntry(LEGlyphStorage &glyphStorage, le_int32 &currGlyph, EntryTableIndex index);

    virtual void endStateTable();

    void doRearrangementAction(LEGlyphStorage &glyphStorage, IndicRearrangementVerb verb) const;

    IndicRearrangementProcessor(const LEReferenceTo<MorphSubtableHeader> &morphSubtableHeader, LEErrorCode &success);
    virtual ~IndicRearrangementProcessor();

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

protected:
    le_int32 firstGlyph;
    le_int32 lastGlyph;

    LEReferenceTo<IndicRearrangementSubtableHeader> indicRearrangementSubtableHeader;
    LEReferenceToArrayOf<IndicRearrangementStateEntry> entryTable;
    LEReferenceToArrayOf<le_int16> int16Table;

};

U_NAMESPACE_END
#endif
