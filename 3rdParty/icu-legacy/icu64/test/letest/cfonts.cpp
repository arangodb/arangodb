// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *
 * (C) Copyright IBM Corp. 1998-2014 - All Rights Reserved
 *
 */

#ifndef USING_ICULEHB /* C API not available under HB */

#include "layout/LETypes.h"
#include "loengine.h"
#include "PortableFontInstance.h"
#include "SimpleFontInstance.h"

U_CDECL_BEGIN

le_font *le_portableFontOpen(const char *fileName,
					float pointSize,
					LEErrorCode *status)
{
	return (le_font *) new PortableFontInstance(fileName, pointSize, *status);
}

le_font *le_simpleFontOpen(float pointSize,
				  LEErrorCode *status)
{
	return (le_font *) new SimpleFontInstance(pointSize, *status);
}

void le_fontClose(le_font *font)
{
	LEFontInstance *fontInstance = (LEFontInstance *) font;

	delete fontInstance;
}

const char *le_getNameString(le_font *font, le_uint16 nameID, le_uint16 platform, le_uint16 encoding, le_uint16 language)
{
	PortableFontInstance *pfi = (PortableFontInstance *) font;

	return pfi->getNameString(nameID, platform, encoding, language);
}

const LEUnicode16 *le_getUnicodeNameString(le_font *font, le_uint16 nameID, le_uint16 platform, le_uint16 encoding, le_uint16 language)
{
	PortableFontInstance *pfi = (PortableFontInstance *) font;

	return pfi->getUnicodeNameString(nameID, platform, encoding, language);
}

void le_deleteNameString(le_font *font, const char *name)
{
	PortableFontInstance *pfi = (PortableFontInstance *) font;

	pfi->deleteNameString(name);
}

void le_deleteUnicodeNameString(le_font *font, const LEUnicode16 *name)
{
	PortableFontInstance *pfi = (PortableFontInstance *) font;

	pfi->deleteNameString(name);
}

le_uint32 le_getFontChecksum(le_font *font)
{
	PortableFontInstance *pfi = (PortableFontInstance *) font;

	return pfi->getFontChecksum();
}

U_CDECL_END
#endif
