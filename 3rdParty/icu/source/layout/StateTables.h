/*
 *
 * (C) Copyright IBM Corp. and others 1998-2013 - All Rights Reserved
 *
 */

#ifndef __STATETABLES_H
#define __STATETABLES_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LayoutTables.h"

U_NAMESPACE_BEGIN




/* 
 * State table loop detection. 
 * Detects if too many ( LE_STATE_PATIENCE_COUNT ) state changes occur without moving the glyph index 'g'.
 * 
 * Usage (pseudocode):
 *
 * {
 *   LE_STATE_PATIENCE_INIT();
 *
 *   int g=0; // the glyph index - expect it to be moving
 * 
 *   for(;;) {
 *     if(LE_STATE_PATIENCE_DECR()) { // decrements the patience counter
 *        // ran out of patience, get out.
 *        break;
 *     }
 *     
 *     LE_STATE_PATIENCE_CURR(int, g); // store the 'current'
 *     state = newState(state,g);
 *     g+= <something, could be zero>;
 *     LE_STATE_PATIENCE_INCR(g);  // if g has moved, increment the patience counter. Otherwise leave it.
 *   }
 * 
 */

#define LE_STATE_PATIENCE_COUNT 4096 /**< give up if a state table doesn't move the glyph after this many iterations */
#define LE_STATE_PATIENCE_INIT()  le_uint32 le_patience_count = LE_STATE_PATIENCE_COUNT
#define LE_STATE_PATIENCE_DECR()  --le_patience_count==0
#define LE_STATE_PATIENCE_CURR(type,x)  type le_patience_curr=(x)
#define LE_STATE_PATIENCE_INCR(x)    if((x)!=le_patience_curr) ++le_patience_count;


struct StateTableHeader
{
    le_int16 stateSize;
    ByteOffset classTableOffset;
    ByteOffset stateArrayOffset;
    ByteOffset entryTableOffset;
};

struct StateTableHeader2 
{
    le_uint32 nClasses;
    le_uint32 classTableOffset;
    le_uint32 stateArrayOffset;
    le_uint32 entryTableOffset;
};

enum ClassCodes
{
    classCodeEOT = 0,
    classCodeOOB = 1,
    classCodeDEL = 2,
    classCodeEOL = 3,
    classCodeFirstFree = 4,
    classCodeMAX = 0xFF
};

typedef le_uint8 ClassCode;

struct ClassTable
{
    TTGlyphID firstGlyph;
    le_uint16 nGlyphs;
    ClassCode classArray[ANY_NUMBER];
};
LE_VAR_ARRAY(ClassTable, classArray)

enum StateNumber
{
    stateSOT        = 0,
    stateSOL        = 1,
    stateFirstFree  = 2,
    stateMAX        = 0xFF
};

typedef le_uint8 EntryTableIndex;

struct StateEntry
{
    ByteOffset  newStateOffset;
    le_int16    flags;
};

typedef le_uint16 EntryTableIndex2;

struct StateEntry2 // same struct different interpretation
{
    le_uint16    newStateIndex;
    le_uint16    flags;
};

U_NAMESPACE_END
#endif

