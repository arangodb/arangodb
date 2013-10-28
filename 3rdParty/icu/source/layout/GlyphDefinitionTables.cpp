/*
 *
 * (C) Copyright IBM Corp. 1998 - 2013 - All Rights Reserved
 *
 */

#include "LETypes.h"
#include "OpenTypeTables.h"
#include "GlyphDefinitionTables.h"
#include "LESwaps.h"

U_NAMESPACE_BEGIN

const LEReferenceTo<GlyphClassDefinitionTable>
GlyphDefinitionTableHeader::getGlyphClassDefinitionTable(const LEReferenceTo<GlyphDefinitionTableHeader>& base,
                                                         LEErrorCode &success) const
{
  if(LE_FAILURE(success)) return LEReferenceTo<GlyphClassDefinitionTable>();
  return LEReferenceTo<GlyphClassDefinitionTable>(base, success, SWAPW(glyphClassDefOffset));
}

const LEReferenceTo<AttachmentListTable>
GlyphDefinitionTableHeader::getAttachmentListTable(const LEReferenceTo<GlyphDefinitionTableHeader>& base,
                                                         LEErrorCode &success) const
{
    if(LE_FAILURE(success)) return LEReferenceTo<AttachmentListTable>();
    return LEReferenceTo<AttachmentListTable>(base, success, SWAPW(attachListOffset));
}

const LEReferenceTo<LigatureCaretListTable>
GlyphDefinitionTableHeader::getLigatureCaretListTable(const LEReferenceTo<GlyphDefinitionTableHeader>& base,
                                                         LEErrorCode &success) const
{
    if(LE_FAILURE(success)) return LEReferenceTo<LigatureCaretListTable>();
    return LEReferenceTo<LigatureCaretListTable>(base, success, SWAPW(ligCaretListOffset));
}

const LEReferenceTo<MarkAttachClassDefinitionTable>
GlyphDefinitionTableHeader::getMarkAttachClassDefinitionTable(const LEReferenceTo<GlyphDefinitionTableHeader>& base,
                                                         LEErrorCode &success) const
{
    if(LE_FAILURE(success)) return LEReferenceTo<MarkAttachClassDefinitionTable>();
    return LEReferenceTo<MarkAttachClassDefinitionTable>(base, success, SWAPW(MarkAttachClassDefOffset));
}

U_NAMESPACE_END
