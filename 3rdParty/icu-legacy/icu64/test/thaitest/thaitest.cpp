// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 ******************************************************************************
 * Copyright (C) 1998-2003, 2006, International Business Machines Corporation *
 * and others. All Rights Reserved.                                           *
 ******************************************************************************
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "unicode/utypes.h"
#include "unicode/uchar.h"
#include "unicode/uchriter.h"
#include "unicode/brkiter.h"
#include "unicode/locid.h"
#include "unicode/unistr.h"
#include "unicode/uniset.h"
#include "unicode/ustring.h"

using icu::BreakIterator;
using icu::Locale;
using icu::UCharCharacterIterator;
using icu::UnicodeSet;
using icu::UnicodeString;

/*
 * This program takes a Unicode text file containing Thai text with
 * spaces inserted where the word breaks are. It computes a copy of
 * the text without spaces and uses a word instance of a Thai BreakIterator
 * to compute the word breaks. The program reports any differences in the
 * breaks.
 *
 * NOTE: by it's very nature, Thai word breaking is not exact, so it is
 * expected that this program will always report some differences.
 */

/*
 * This class is a break iterator that counts words and spaces.
 */
class SpaceBreakIterator
{
public:
    // The constructor:
    // text  - pointer to an array of UChars to iterate over
    // count - the number of UChars in text
    SpaceBreakIterator(const char16_t *text, int32_t count);

    // the destructor
    ~SpaceBreakIterator();

    // return next break position
    int32_t next();

    // return current word count
    int32_t getWordCount();

    // return current space count
    int32_t getSpaceCount();

private:
    // No arg constructor: private so clients can't call it.
    SpaceBreakIterator();

    // The underlying BreakIterator
    BreakIterator *fBreakIter;

    // address of the char16_t array
    const char16_t *fText;

    // number of UChars in fText
    int32_t fTextCount;

    // current word count
    int32_t fWordCount;

    // current space count
    int32_t fSpaceCount;
    
    // UnicodeSet of SA characters
    UnicodeSet fComplexContext;

    // true when fBreakIter has returned DONE
    UBool fDone;
};

/*
 * This is the main class. It compares word breaks and reports the differences.
 */
class ThaiWordbreakTest
{
public:
    // The main constructor:
    // spaces       - pointer to a char16_t array for the text with spaces
    // spaceCount   - the number of characters in the spaces array
    // noSpaces     - pointer to a char16_t array for the text without spaces
    // noSpaceCount - the number of characters in the noSpaces array
    // verbose      - report all breaks if true, otherwise just report differences
    ThaiWordbreakTest(const char16_t *spaces, int32_t spaceCount, const char16_t *noSpaces, int32_t noSpaceCount, UBool verbose);
    ~ThaiWordbreakTest();

    // returns the number of breaks that are in the spaces array
    // but aren't found in the noSpaces array
    int32_t getBreaksNotFound();

    // returns the number of breaks which are found in the noSpaces
    // array but aren't in the spaces array
    int32_t getInvalidBreaks();

    // returns the number of words found in the spaces array
    int32_t getWordCount();

    // reads the input Unicode text file:
    // fileName  - the path name of the file
    // charCount - set to the number of UChars read from the file
    // returns   - the address of the char16_t array containing the characters
    static const char16_t *readFile(char *fileName, int32_t &charCount);

    // removes spaces form the input char16_t array:
    // spaces        - pointer to the input char16_t array
    // count         - number of UChars in the spaces array
    // nonSpaceCount - the number of UChars in the result array
    // returns       - the address of the char16_t array with spaces removed
    static const char16_t *crunchSpaces(const char16_t *spaces, int32_t count, int32_t &nonSpaceCount);

private:
    // The no arg constructor - private so clients can't call it
    ThaiWordbreakTest();

    // This does the actual comparison:
    // spaces - the address of the char16_t array for the text with spaces
    // spaceCount - the number of UChars in the spaces array
    // noSpaces   - the address of the char16_t array for the text without spaces
    // noSpaceCount - the number of UChars in the noSpaces array
    // returns      - true if all breaks match, false otherwise
    UBool compareWordBreaks(const char16_t *spaces, int32_t spaceCount,
                            const char16_t *noSpaces, int32_t noSpaceCount);

    // helper method to report a break in the spaces
    // array that's not found in the noSpaces array
    void breakNotFound(int32_t br);

    // helper method to report a break that's found in
    // the noSpaces array that's not in the spaces array
    void foundInvalidBreak(int32_t br);

    // count of breaks in the spaces array that
    // aren't found in the noSpaces array
    int32_t fBreaksNotFound;

    // count of breaks found in the noSpaces array
    // that aren't in the spaces array
    int32_t fInvalidBreaks;

    // number of words found in the spaces array
    int32_t fWordCount;

    // report all breaks if true, otherwise just report differences
    UBool fVerbose;
};

/*
 * The main constructor: it calls compareWordBreaks and reports any differences
 */
ThaiWordbreakTest::ThaiWordbreakTest(const char16_t *spaces, int32_t spaceCount,
                                     const char16_t *noSpaces, int32_t noSpaceCount, UBool verbose)
: fBreaksNotFound(0), fInvalidBreaks(0), fWordCount(0), fVerbose(verbose)
{
    compareWordBreaks(spaces, spaceCount, noSpaces, noSpaceCount);
}

/*
 * The no arg constructor
 */
ThaiWordbreakTest::ThaiWordbreakTest()
{
    // nothing
}

/*
 * The destructor
 */
ThaiWordbreakTest::~ThaiWordbreakTest()
{
    // nothing?
}

/*
 * returns the number of breaks in the spaces array
 * that aren't found in the noSpaces array
 */
inline int32_t ThaiWordbreakTest::getBreaksNotFound()
{
    return fBreaksNotFound;
}

/*
 * Returns the number of breaks found in the noSpaces
 * array that aren't in the spaces array
 */
inline int32_t ThaiWordbreakTest::getInvalidBreaks()
{
    return fInvalidBreaks;
}

/*
 * Returns the number of words found in the spaces array
 */
inline int32_t ThaiWordbreakTest::getWordCount()
{
    return fWordCount;
}

/*
 * This method does the actual break comparison and reports the results.
 * It uses a SpaceBreakIterator to iterate over the text with spaces,
 * and a word instance of a Thai BreakIterator to iterate over the text
 * without spaces.
 */
UBool ThaiWordbreakTest::compareWordBreaks(const char16_t *spaces, int32_t spaceCount,
                                           const char16_t *noSpaces, int32_t noSpaceCount)
{
    UBool result = true;
    Locale thai("th");
    UCharCharacterIterator *noSpaceIter = new UCharCharacterIterator(noSpaces, noSpaceCount);
    UErrorCode status = U_ZERO_ERROR;
    
    BreakIterator *breakIter = BreakIterator::createWordInstance(thai, status);
    breakIter->adoptText(noSpaceIter);
    
    SpaceBreakIterator spaceIter(spaces, spaceCount);
    
    int32_t nextBreak = 0;
    int32_t nextSpaceBreak = 0;
    int32_t iterCount = 0;
    
    while (true) {
        nextSpaceBreak = spaceIter.next();
        nextBreak = breakIter->next();
        
        if (nextSpaceBreak == BreakIterator::DONE || nextBreak == BreakIterator::DONE) {
            if (nextBreak != BreakIterator::DONE) {
                fprintf(stderr, "break iterator didn't end.\n");
            } else if (nextSpaceBreak != BreakIterator::DONE) {
                fprintf(stderr, "premature break iterator end.\n");
            }
            
            break;
        }
        
        while (nextSpaceBreak != nextBreak &&
               nextSpaceBreak != BreakIterator::DONE && nextBreak != BreakIterator::DONE) {
            if (nextSpaceBreak < nextBreak) {
                breakNotFound(nextSpaceBreak);
                result = false;
                nextSpaceBreak = spaceIter.next();
            } else if (nextSpaceBreak > nextBreak) {
                foundInvalidBreak(nextBreak);
                result = false;
                nextBreak = breakIter->next();
            }
        }
        
        if (fVerbose) {
            printf("%d   %d\n", nextSpaceBreak, nextBreak);
        }
    }
        
   
    fWordCount = spaceIter.getWordCount();
    
    delete breakIter;

    return result;
}

/*
 * Report a break that's in the text with spaces but
 * not found in the text without spaces.
 */
void ThaiWordbreakTest::breakNotFound(int32_t br)
{
    if (fVerbose) {
        printf("%d   ****\n", br);
    } else {
        fprintf(stderr, "break not found: %d\n", br);
    }
    
    fBreaksNotFound += 1;
}

/*
 * Report a break that's found in the text without spaces
 * that isn't in the text with spaces.
 */
void ThaiWordbreakTest::foundInvalidBreak(int32_t br)
{
    if (fVerbose) {
        printf("****   %d\n", br);
    } else {
        fprintf(stderr, "found invalid break: %d\n", br);
    }
    
    fInvalidBreaks += 1;
}

/*
 * Read the text from a file. The text must start with a Unicode Byte
 * Order Mark (BOM) so that we know what order to read the bytes in.
 */
const char16_t *ThaiWordbreakTest::readFile(char *fileName, int32_t &charCount)
{
    FILE *f;
    int32_t fileSize;
    
    char16_t *buffer;
    char *bufferChars;
    
    f = fopen(fileName, "rb");
    
    if( f == nullptr ) {
        fprintf(stderr,"Couldn't open %s reason: %s \n", fileName, strerror(errno));
        return nullptr;
    }
    
    fseek(f, 0, SEEK_END);
    fileSize = ftell(f);
    
    fseek(f, 0, SEEK_SET);
    bufferChars = new char[fileSize];

    if (bufferChars == nullptr) {
        fprintf(stderr,"Couldn't get memory for reading %s reason: %s \n", fileName, strerror(errno));
        fclose(f);
        return nullptr;
    }
    
    fread(bufferChars, sizeof(char), fileSize, f);
    if( ferror(f) ) {
        fprintf(stderr,"Couldn't read %s reason: %s \n", fileName, strerror(errno));
        fclose(f);
        delete[] bufferChars;
        return nullptr;
    }
    fclose(f);
    
    UnicodeString myText(bufferChars, fileSize, "UTF-8");

    delete[] bufferChars;
    
    charCount = myText.length();
    buffer = new char16_t[charCount];
    if (buffer == nullptr) {
        fprintf(stderr,"Couldn't get memory for reading %s reason: %s \n", fileName, strerror(errno));
        return nullptr;
    }
    
    myText.extract(1, myText.length(), buffer);
    charCount--;  // skip the BOM
    buffer[charCount] = 0;    // NUL terminate for easier reading in the debugger
    
    return buffer;
}

/*
 * Remove spaces from the input char16_t array.
 *
 * We check explicitly for a Unicode code value of 0x0020
 * because Unicode::isSpaceChar returns true for CR, LF, etc.
 *
 */
const char16_t *ThaiWordbreakTest::crunchSpaces(const char16_t *spaces, int32_t count, int32_t &nonSpaceCount)
{
    int32_t i, out, spaceCount;

    spaceCount = 0;
    for (i = 0; i < count; i += 1) {
        if (spaces[i] == 0x0020 /*Unicode::isSpaceChar(spaces[i])*/) {
            spaceCount += 1;
        }
    }

    nonSpaceCount = count - spaceCount;
    char16_t *noSpaces = new char16_t[nonSpaceCount];

    if (noSpaces == nullptr) {
        fprintf(stderr, "Couldn't allocate memory for the space stripped text.\n");
        return nullptr;
    }

    for (out = 0, i = 0; i < count; i += 1) {
        if (spaces[i] != 0x0020 /*! Unicode::isSpaceChar(spaces[i])*/) {
            noSpaces[out++] = spaces[i];
        }
    }

    return noSpaces;
}

/*
 * Generate a text file with spaces in it from a file without.
 */
int generateFile(const char16_t *chars, int32_t length) {
    Locale root("");
    UCharCharacterIterator *noSpaceIter = new UCharCharacterIterator(chars, length);
    UErrorCode status = U_ZERO_ERROR;
    
    UnicodeSet complexContext(UNICODE_STRING_SIMPLE("[:LineBreak=SA:]"), status);
    BreakIterator *breakIter = BreakIterator::createWordInstance(root, status);
    breakIter->adoptText(noSpaceIter);
    char outbuf[1024];
    int32_t strlength;
    char16_t bom = 0xFEFF;
    
    printf("%s", u_strToUTF8(outbuf, sizeof(outbuf), &strlength, &bom, 1, &status));
    int32_t prevbreak = 0;
    while (U_SUCCESS(status)) {
        int32_t nextbreak = breakIter->next();
        if (nextbreak == BreakIterator::DONE) {
            break;
        }
        printf("%s", u_strToUTF8(outbuf, sizeof(outbuf), &strlength, &chars[prevbreak],
                                    nextbreak-prevbreak, &status));
        if (nextbreak > 0 && complexContext.contains(chars[nextbreak-1])
            && complexContext.contains(chars[nextbreak])) {
            printf(" ");
        }
        prevbreak = nextbreak;
    }
    
    if (U_FAILURE(status)) {
        fprintf(stderr, "generate failed: %s\n", u_errorName(status));
        return status;
    }
    else {
        return 0;
    }
}

/*
 * The main routine. Read the command line arguments, read the text file,
 * remove the spaces, do the comparison and report the final results
 */
int main(int argc, char **argv)
{
    char *fileName = "space.txt";
    int arg = 1;
    UBool verbose = false;
    UBool generate = false;

    if (argc >= 2 && strcmp(argv[1], "-generate") == 0) {
        generate = true;
        arg += 1;
    }

    if (argc >= 2 && strcmp(argv[1], "-verbose") == 0) {
        verbose = true;
        arg += 1;
    }

    if (arg == argc - 1) {
        fileName = argv[arg++];
    }

    if (arg != argc) {
        fprintf(stderr, "Usage: %s [-verbose] [<file>]\n", argv[0]);
        return 1;
    }

    int32_t spaceCount, nonSpaceCount;
    const char16_t *spaces, *noSpaces;

    spaces = ThaiWordbreakTest::readFile(fileName, spaceCount);

    if (spaces == nullptr) {
        return 1;
    }
    
    if (generate) {
        return generateFile(spaces, spaceCount);
    }

    noSpaces = ThaiWordbreakTest::crunchSpaces(spaces, spaceCount, nonSpaceCount);

    if (noSpaces == nullptr) {
        return 1;
    }

    ThaiWordbreakTest test(spaces, spaceCount, noSpaces, nonSpaceCount, verbose);

    printf("word count: %d\n", test.getWordCount());
    printf("breaks not found: %d\n", test.getBreaksNotFound());
    printf("invalid breaks found: %d\n", test.getInvalidBreaks());

    return 0;
}

/*
 * The main constructor. Clear all the counts and construct a default
 * word instance of a BreakIterator.
 */
SpaceBreakIterator::SpaceBreakIterator(const char16_t *text, int32_t count)
  : fBreakIter(nullptr), fText(text), fTextCount(count), fWordCount(0), fSpaceCount(0), fDone(false)
{
    UCharCharacterIterator *iter = new UCharCharacterIterator(text, count);
    UErrorCode status = U_ZERO_ERROR;
    fComplexContext.applyPattern(UNICODE_STRING_SIMPLE("[:LineBreak=SA:]"), status);
    Locale root("");

    fBreakIter = BreakIterator::createWordInstance(root, status);
    fBreakIter->adoptText(iter);
}

SpaceBreakIterator::SpaceBreakIterator()
{
    // nothing
}

/*
 * The destructor. delete the underlying BreakIterator
 */
SpaceBreakIterator::~SpaceBreakIterator()
{
    delete fBreakIter;
}

/*
 * Return the next break, counting words and spaces.
 */
int32_t SpaceBreakIterator::next()
{
    if (fDone) {
        return BreakIterator::DONE;
    }
    
    int32_t nextBreak;
    do {
        nextBreak = fBreakIter->next();
        
        if (nextBreak == BreakIterator::DONE) {
            fDone = true;
            return BreakIterator::DONE;
        }
    }
    while(nextBreak > 0 && fComplexContext.contains(fText[nextBreak-1])
            && fComplexContext.contains(fText[nextBreak]));
    
   int32_t result = nextBreak - fSpaceCount;
    
    if (nextBreak < fTextCount) {
        if (fText[nextBreak] == 0x0020 /*Unicode::isSpaceChar(fText[nextBreak])*/) {
            fSpaceCount += fBreakIter->next() - nextBreak;
        }
    }
    
    fWordCount += 1;

    return result;
}

/*
 * Returns the current space count
 */
int32_t SpaceBreakIterator::getSpaceCount()
{
    return fSpaceCount;
}

/*
 * Returns the current word count
 */
int32_t SpaceBreakIterator::getWordCount()
{
    return fWordCount;
}


