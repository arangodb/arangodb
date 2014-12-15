/***************************************************************************
*
*   Copyright (C) 2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
************************************************************************/

#include "unicode/utimer.h"
#include "unicode/ustdio.h"
#include "layout/LETypes.h"
#include "layout/LayoutEngine.h"
#include "layout/LEScripts.h"
#include "SimpleFontInstance.h"
#include "PortableFontInstance.h"

class Params {
public:
  LEFontInstance *font;
  LEUnicode *chars;
  le_int32 charLen;
  ScriptCodes script;
  le_int32 glyphCount;
};

LEUnicode     ArabChars[] = {
        0x0045, 0x006E, 0x0067, 0x006C, 0x0069, 0x0073, 0x0068, 0x0020, // "English "
        0x0645, 0x0627, 0x0646, 0x062A, 0x0648, 0x0634,                 // MEM ALIF KAF NOON TEH WAW SHEEN
        0x0020, 0x0074, 0x0065, 0x0078, 0x0074, 0x02E, 0                   // " text."
    };

void iterate(void * p) {
  Params* params = (Params*) p;

    LEErrorCode status = LE_NO_ERROR;
    LEFontInstance *font = params->font;
    LayoutEngine *engine = LayoutEngine::layoutEngineFactory(font, params->script, -1, status);
    LEGlyphID *glyphs    = NULL;
    le_int32  *indices   = NULL;
    float     *positions = NULL;
    le_int32   glyphCount = 0;
    LEUnicode *chars = params->chars;
    glyphCount = engine->layoutChars(chars, 0, params->charLen, params->charLen, TRUE, 0.0, 0.0, status);
    glyphs    = LE_NEW_ARRAY(LEGlyphID, glyphCount + 10);
    indices   = LE_NEW_ARRAY(le_int32, glyphCount + 10);
    positions = LE_NEW_ARRAY(float, glyphCount + 10);
    engine->getGlyphs(glyphs, status);
    params->glyphCount = glyphCount;
    
    
    delete glyphs;
    delete indices;
    delete positions;
    delete engine;
    //delete font;
}

int main(int argc, const char *argv[]) {
  double len=10.0;
  for(int i=1;i<argc;i++) {
    puts("arg:");
    puts(argv[i]);
    if(argv[i][0]=='p') {
      printf("hit enter-pid=%d", getpid());
      getchar();
    } else if(argv[i][0]>='0' && argv[i][0]<='9') {
      len = (1.0)*(argv[i][0]-'0');
    }
  }
  u_printf("leperf: Testing %s for %.fs...\n", U_ICU_VERSION, len);
  LEErrorCode status = LE_NO_ERROR;
  //uloc_setDefault("en_US", &status);
  Params p;

#if 0  
  p.script = arabScriptCode;
  p.chars = ArabChars;
  p.charLen = sizeof(ArabChars)/sizeof(ArabChars[0]);
#else
  p.script = latnScriptCode;
  p.chars = new LEUnicode[257];
  for(int i=0;i<256;i++) {
    p.chars[i] = i+1;
  }
  p.chars[256] = 0;
  p.charLen = 256;
#endif
  
  int32_t loopCount;
  double timeTaken;
  double timeNs;
#if 0
  p.font = new SimpleFontInstance(12, status);
  u_printf("leperf: Running SFI...\r");
  timeTaken = utimer_loopUntilDone(len, &loopCount, iterate, &p);
  u_printf("leperf: SFI .. took %.fs %.2fns/ea\nleperf: .. iter= %d\n", timeTaken, 1000000000.0*(timeTaken/(double)loopCount), (int32_t)loopCount);
  delete p.font;
#endif
    PortableFontInstance *font;
    LEErrorCode fontStatus = LE_NO_ERROR;
    const char *fontPath = "myfont.ttf";

    font = new PortableFontInstance(fontPath, 12, fontStatus);

    p.font = font;
    loopCount=0;
    u_printf("leperf: testing %s\n", fontPath);
    u_printf("leperf: Running ...\r");
  timeTaken = utimer_loopUntilDone(len, &loopCount, iterate, &p);
  timeNs = 1000000000.0*(timeTaken/(double)loopCount);
  u_printf("leperf: PFI .. took %.fs %.2fns/ea\nleperf: .. iter= %d\n", timeTaken, timeNs, (int32_t)loopCount);
  u_printf("leperf: DATA|\"%s\"|%.2f|\n", U_ICU_VERSION, timeNs);
  u_printf("leperf: glyphs=%d\n", p.glyphCount);
  return 0;
}

// hack - #include these for easier build.
#include "SimpleFontInstance.cpp"
#include "PortableFontInstance.cpp"
#include "cmaps.cpp"
#include "FontTableCache.cpp"
