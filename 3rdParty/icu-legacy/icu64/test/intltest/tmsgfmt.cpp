// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************
 * File TMSGFMT.CPP
 *
 * Modification History:
 *
 *   Date        Name        Description
 *   03/24/97    helena      Converted from Java.
 *   07/11/97    helena      Updated to work on AIX.
 *   08/04/97    jfitz       Updated to intltest
 *******************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "tmsgfmt.h"
#include "cmemory.h"
#include "loctest.h"

#include "unicode/format.h"
#include "unicode/decimfmt.h"
#include "unicode/localpointer.h"
#include "unicode/locid.h"
#include "unicode/msgfmt.h"
#include "unicode/numfmt.h"
#include "unicode/choicfmt.h"
#include "unicode/messagepattern.h"
#include "unicode/selfmt.h"
#include "unicode/gregocal.h"
#include "unicode/strenum.h"
#include <stdio.h>

void
TestMessageFormat::runIndexedTest(int32_t index, UBool exec,
                                  const char* &name, char* /*par*/) {
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testBug1);
    TESTCASE_AUTO(testBug2);
    TESTCASE_AUTO(sample);
    TESTCASE_AUTO(PatternTest);
    TESTCASE_AUTO(testStaticFormat);
    TESTCASE_AUTO(testSimpleFormat);
    TESTCASE_AUTO(testMsgFormatChoice);
    TESTCASE_AUTO(testCopyConstructor);
    TESTCASE_AUTO(testAssignment);
    TESTCASE_AUTO(testClone);
    TESTCASE_AUTO(testEquals);
    TESTCASE_AUTO(testNotEquals);
    TESTCASE_AUTO(testSetLocale);
    TESTCASE_AUTO(testFormat);
    TESTCASE_AUTO(testParse);
    TESTCASE_AUTO(testAdopt);
    TESTCASE_AUTO(testCopyConstructor2);
    TESTCASE_AUTO(TestUnlimitedArgsAndSubformats);
    TESTCASE_AUTO(TestRBNF);
    TESTCASE_AUTO(TestTurkishCasing);
    TESTCASE_AUTO(testAutoQuoteApostrophe);
    TESTCASE_AUTO(testMsgFormatPlural);
    TESTCASE_AUTO(testMsgFormatSelect);
    TESTCASE_AUTO(testApostropheInPluralAndSelect);
    TESTCASE_AUTO(TestApostropheMode);
    TESTCASE_AUTO(TestCompatibleApostrophe);
    TESTCASE_AUTO(testCoverage);
    TESTCASE_AUTO(testGetFormatNames);
    TESTCASE_AUTO(TestTrimArgumentName);
    TESTCASE_AUTO(TestSelectOrdinal);
    TESTCASE_AUTO(TestDecimals);
    TESTCASE_AUTO(TestArgIsPrefixOfAnother);
    TESTCASE_AUTO(TestMessageFormatNumberSkeleton);
    TESTCASE_AUTO(TestMessageFormatDateSkeleton);
    TESTCASE_AUTO(TestMessageFormatTimeSkeleton);
    TESTCASE_AUTO_END;
}

void TestMessageFormat::testBug3()
{
    double myNumber = -123456;
    DecimalFormat *form = 0;
    Locale locale[] = {
        Locale("ar", "", ""),
        Locale("be", "", ""),
        Locale("bg", "", ""),
        Locale("ca", "", ""),
        Locale("cs", "", ""),
        Locale("da", "", ""),
        Locale("de", "", ""),
        Locale("de", "AT", ""),
        Locale("de", "CH", ""),
        Locale("el", "", ""),       // 10
        Locale("en", "CA", ""),
        Locale("en", "GB", ""),
        Locale("en", "IE", ""),
        Locale("en", "US", ""),
        Locale("es", "", ""),
        Locale("et", "", ""),
        Locale("fi", "", ""),
        Locale("fr", "", ""),
        Locale("fr", "BE", ""),
        Locale("fr", "CA", ""),     // 20
        Locale("fr", "CH", ""),
        Locale("he", "", ""),
        Locale("hr", "", ""),
        Locale("hu", "", ""),
        Locale("is", "", ""),
        Locale("it", "", ""),
        Locale("it", "CH", ""),
        Locale("ja", "", ""),
        Locale("ko", "", ""),
        Locale("lt", "", ""),       // 30
        Locale("lv", "", ""),
        Locale("mk", "", ""),
        Locale("nl", "", ""),
        Locale("nl", "BE", ""),
        Locale("no", "", ""),
        Locale("pl", "", ""),
        Locale("pt", "", ""),
        Locale("ro", "", ""),
        Locale("ru", "", ""),
        Locale("sh", "", ""),       // 40
        Locale("sk", "", ""),
        Locale("sl", "", ""),
        Locale("sq", "", ""),
        Locale("sr", "", ""),
        Locale("sv", "", ""),
        Locale("tr", "", ""),
        Locale("uk", "", ""),
        Locale("zh", "", ""),
        Locale("zh", "TW", "")      // 49
    };
    int32_t i;
    for (i= 0; i < 49; i++) {
        UnicodeString buffer;
        logln(locale[i].getDisplayName(buffer));
        UErrorCode success = U_ZERO_ERROR;
//        form = (DecimalFormat*)NumberFormat::createCurrencyInstance(locale[i], success);
        form = (DecimalFormat*)NumberFormat::createInstance(locale[i], success);
        if (U_FAILURE(success)) {
            errln("Err: Number Format ");
            logln("Number format creation failed.");
            continue;
        }
        Formattable result;
        FieldPosition pos(FieldPosition::DONT_CARE);
        buffer.remove();
        form->format(myNumber, buffer, pos);
        success = U_ZERO_ERROR;
        ParsePosition parsePos;
        form->parse(buffer, result, parsePos);
        logln(UnicodeString(" -> ") /* + << dec*/ + toString(result) + UnicodeString("[supposed output for result]"));
        if (U_FAILURE(success)) {
            errln("Err: Number Format parse");
            logln("Number format parse failed.");
        }
        delete form;
    }
}

void TestMessageFormat::testBug1()
{
    const double limit[] = {0.0, 1.0, 2.0};
    const UnicodeString formats[] = {"0.0<=Arg<1.0",
                               "1.0<=Arg<2.0",
                               "2.0<-Arg"};
    ChoiceFormat *cf = new ChoiceFormat(limit, formats, 3);
    FieldPosition status(FieldPosition::DONT_CARE);
    UnicodeString toAppendTo;
    cf->format((int32_t)1, toAppendTo, status);
    if (toAppendTo != "1.0<=Arg<2.0") {
        errln("ChoiceFormat cmp in testBug1");
    }
    logln(toAppendTo);
    delete cf;
}

void TestMessageFormat::testBug2()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString result;
    // {sfb} use double format in pattern, so result will match (not strictly necessary)
    const UnicodeString pattern = "There {0,choice,0#are no files|1#is one file|1<are {0, number} files} on disk {1}. ";
    logln("The input pattern : " + pattern);
    MessageFormat *fmt = new MessageFormat(pattern, status);
    if (U_FAILURE(status)) {
        dataerrln("MessageFormat pattern creation failed. - %s", u_errorName(status));
        return;
    }
    logln("The output pattern is : " + fmt->toPattern(result));
    if (pattern != result) {
        errln("MessageFormat::toPattern() failed.");
    }
    delete fmt;
}

#if 0
#if defined(_DEBUG)
//----------------------------------------------------
// console I/O
//----------------------------------------------------

#include <iostream>
std::ostream& operator<<(std::ostream& stream,  const Formattable&   obj);

#include "unicode/datefmt.h"
#include <stdlib.h>
#include <string.h>

IntlTest&
operator<<( IntlTest&           stream,
            const Formattable&  obj)
{
    static DateFormat *defDateFormat = 0;

    UnicodeString buffer;
    switch(obj.getType()) {
        case Formattable::kDate : 
            if (defDateFormat == 0) {
                defDateFormat = DateFormat::createInstance();
            }
            defDateFormat->format(obj.getDate(), buffer);
            stream << buffer;
            break;
        case Formattable::kDouble :
            char convert[20];
            sprintf( convert, "%lf", obj.getDouble() );
            stream << convert << "D";
            break;
        case Formattable::kLong :
            stream << obj.getLong() << "L";
            break;
        case Formattable::kString:
            stream << "\"" << obj.getString(buffer) << "\"";
            break;
        case Formattable::kArray:
            int32_t i, count;
            const Formattable* array;
            array = obj.getArray(count);
            stream << "[";
            for (i=0; i<count; ++i) stream << array[i] << ( (i==(count-1)) ? "" : ", " );
            stream << "]";
            break;
        default:
            stream << "INVALID_Formattable";
    }
    return stream;
}
#endif /* defined(_DEBUG) */
#endif

void TestMessageFormat::PatternTest() 
{
    Formattable testArgs[] = {
        Formattable(double(1)), Formattable(double(3456)),
            Formattable("Disk"), Formattable(UDate((int32_t)1000000000L), Formattable::kIsDate)
    };
    UnicodeString testCases[] = {
       "Quotes '', '{', 'a' {0} '{0}'",
       "Quotes '', '{', 'a' {0,number} '{0}'",
       "'{'1,number,'#',##} {1,number,'#',##}",
       "There are {1} files on {2} at {3}.",
       "On {2}, there are {1} files, with {0,number,currency}.",
       "'{1,number,percent}', {1,number,percent},",
       "'{1,date,full}', {1,date,full},",
       "'{3,date,full}', {3,date,full},",
       "'{1,number,#,##}' {1,number,#,##}",
    };

    // ICU 4.8 returns the original pattern (testCases),
    // rather than toPattern() reconstituting a new, equivalent pattern string (testResultPatterns).
    /*UnicodeString testResultPatterns[] = {
        "Quotes '', '{', a {0} '{'0}",
        "Quotes '', '{', a {0,number} '{'0}",
        "'{'1,number,#,##} {1,number,'#'#,##}",
        "There are {1} files on {2} at {3}.",
        "On {2}, there are {1} files, with {0,number,currency}.",
        "'{'1,number,percent}, {1,number,percent},",
        "'{'1,date,full}, {1,date,full},",
        "'{'3,date,full}, {3,date,full},",
        "'{'1,number,#,##} {1,number,#,##}"
    };*/

    UnicodeString testResultStrings[] = {
        "Quotes ', {, 'a' 1 {0}",
        "Quotes ', {, 'a' 1 {0}",
        "{1,number,'#',##} #34,56",
        "There are 3,456 files on Disk at 1/12/70, 5:46 AM.",
        "On Disk, there are 3,456 files, with $1.00.",
        "{1,number,percent}, 345,600%,",
        "{1,date,full}, Wednesday, December 31, 1969,",
        "{3,date,full}, Monday, January 12, 1970,",
        "{1,number,#,##} 34,56"
    };


    for (int32_t i = 0; i < 9; ++i) {
        //it_out << "\nPat in:  " << testCases[i]);

        MessageFormat *form = 0;
        UErrorCode success = U_ZERO_ERROR;
        UnicodeString buffer;
        form = new MessageFormat(testCases[i], Locale::getUS(), success);
        if (U_FAILURE(success)) {
            dataerrln("MessageFormat creation failed.#1 - %s", u_errorName(success));
            logln(((UnicodeString)"MessageFormat for ") + testCases[i] + " creation failed.\n");
            continue;
        }
        // ICU 4.8 returns the original pattern (testCases),
        // rather than toPattern() reconstituting a new, equivalent pattern string (testResultPatterns).
        if (form->toPattern(buffer) != testCases[i]) {
            // Note: An alternative test would be to build MessagePattern objects for
            // both the input and output patterns and compare them, taking SKIP_SYNTAX etc.
            // into account.
            // (Too much trouble...)
            errln(UnicodeString("TestMessageFormat::PatternTest failed test #2, i = ") + i);
            //form->toPattern(buffer);
            errln(((UnicodeString)" Orig: ") + testCases[i]);
            errln(((UnicodeString)" Exp:  ") + testCases[i]);
            errln(((UnicodeString)" Got:  ") + buffer);
        }

        //it_out << "Pat out: " << form->toPattern(buffer));
        UnicodeString result;
        int32_t count = 4;
        FieldPosition fieldpos(FieldPosition::DONT_CARE);
        form->format(testArgs, count, result, fieldpos, success);
        if (U_FAILURE(success)) {
            dataerrln("MessageFormat failed test #3 - %s", u_errorName(success));
            logln("TestMessageFormat::PatternTest failed test #3");
            continue;
        }
        if (result != testResultStrings[i]) {
            errln("TestMessageFormat::PatternTest failed test #4");
            logln("TestMessageFormat::PatternTest failed #4.");
            logln(UnicodeString("    Result: ") + result );
            logln(UnicodeString("  Expected: ") + testResultStrings[i] );
        }


        //it_out << "Result:  " << result);
#if 0
        /* TODO: Look at this test and see if this is still a valid test */
        logln("---------------- test parse ----------------");

        form->toPattern(buffer);
        logln("MSG pattern for parse: " + buffer);

        int32_t parseCount = 0;
        Formattable* values = form->parse(result, parseCount, success);
        if (U_FAILURE(success)) {
            errln("MessageFormat failed test #5");
            logln(UnicodeString("MessageFormat failed test #5 with error code ")+(int32_t)success);
        } else if (parseCount != count) {
            errln("MSG count not %d as expected. Got %d", count, parseCount);
        }
        UBool failed = FALSE;
        for (int32_t j = 0; j < parseCount; ++j) {
             if (values == 0 || testArgs[j] != values[j]) {
                errln(((UnicodeString)"MSG testargs[") + j + "]: " + toString(testArgs[j]));
                errln(((UnicodeString)"MSG values[") + j + "]  : " + toString(values[j]));
                failed = TRUE;
             }
        }
        if (failed)
            errln("MessageFormat failed test #6");
#endif
        delete form;
    }
}

void TestMessageFormat::sample() 
{
    MessageFormat *form = 0;
    UnicodeString buffer1, buffer2;
    UErrorCode success = U_ZERO_ERROR;
    form = new MessageFormat("There are {0} files on {1}", success);
    if (U_FAILURE(success)) {
        errln("Err: Message format creation failed");
        logln("Sample message format creation failed.");
        return;
    }
    UnicodeString abc("abc");
    UnicodeString def("def");
    Formattable testArgs1[] = { abc, def };
    FieldPosition fieldpos(FieldPosition::DONT_CARE);
    assertEquals("format",
                 "There are abc files on def",
                 form->format(testArgs1, 2, buffer2, fieldpos, success));
    assertSuccess("format", success);
    delete form;
}

void TestMessageFormat::testStaticFormat()
{
    UErrorCode err = U_ZERO_ERROR;
    Formattable arguments[] = {
        (int32_t)7,
        Formattable(UDate(8.71068e+011), Formattable::kIsDate),
        "a disturbance in the Force"
        };

    UnicodeString result;
    result = MessageFormat::format(
        "At {1,time} on {1,date}, there was {2} on planet {0,number,integer}.",
        arguments,
        3,
        result,
        err);

    if (U_FAILURE(err)) {
        dataerrln("TestMessageFormat::testStaticFormat #1 - %s", u_errorName(err));
        logln(UnicodeString("TestMessageFormat::testStaticFormat failed test #1 with error code ")+(int32_t)err);
        return;
    }

    const UnicodeString expected(
            "At 12:20:00 PM on Aug 8, 1997, there was a disturbance in the Force on planet 7.", "");
    if (result != expected) {
        errln("TestMessageFormat::testStaticFormat failed on test");
        logln( UnicodeString("     Result: ") + result );
        logln( UnicodeString("   Expected: ") + expected );
    }
}

/* When the default locale is tr, make sure that the pattern can still be parsed. */
void TestMessageFormat::TestTurkishCasing()
{
    UErrorCode err = U_ZERO_ERROR;
    Locale  saveDefaultLocale;
    Locale::setDefault( Locale("tr"), err );

    Formattable arguments[] = {
        (int32_t)7,
        Formattable(UDate(8.71068e+011), Formattable::kIsDate),
        "a disturbance in the Force"
        };

    UnicodeString result;
    result = MessageFormat::format(
        "At {1,TIME} on {1,DATE,SHORT}, there was {2} on planet {0,NUMBER,INTEGER}.",
        arguments,
        3,
        result,
        err);

    if (U_FAILURE(err)) {
        dataerrln("TestTurkishCasing #1 with error code %s", u_errorName(err));
        return;
    }

    const UnicodeString expected(
            "At 12:20:00 on 8.08.1997, there was a disturbance in the Force on planet 7.", "");
    if (result != expected) {
        errln("TestTurkishCasing failed on test");
        errln( UnicodeString("     Result: ") + result );
        errln( UnicodeString("   Expected: ") + expected );
    }
    Locale::setDefault( saveDefaultLocale, err );
}

void TestMessageFormat::testSimpleFormat(/* char* par */)
{
    logln("running TestMessageFormat::testSimpleFormat");

    UErrorCode err = U_ZERO_ERROR;

    Formattable testArgs1[] = {(int32_t)0, "MyDisk"};
    Formattable testArgs2[] = {(int32_t)1, "MyDisk"};
    Formattable testArgs3[] = {(int32_t)12, "MyDisk"};
   
    MessageFormat* form = new MessageFormat(
        "The disk \"{1}\" contains {0} file(s).", err);
    
    UnicodeString string;
    FieldPosition ignore(FieldPosition::DONT_CARE);
    form->format(testArgs1, 2, string, ignore, err);
    if (U_FAILURE(err) || string != "The disk \"MyDisk\" contains 0 file(s).") {
        dataerrln(UnicodeString("TestMessageFormat::testSimpleFormat failed on test #1 - ") + u_errorName(err));
    }
 
    ignore.setField(FieldPosition::DONT_CARE);
    string.remove();
    form->format(testArgs2, 2, string, ignore, err);
    if (U_FAILURE(err) || string != "The disk \"MyDisk\" contains 1 file(s).") {
        logln(string);
        dataerrln(UnicodeString("TestMessageFormat::testSimpleFormat failed on test #2")+string + " - " + u_errorName(err));
    }
 
    ignore.setField(FieldPosition::DONT_CARE);
    string.remove();
    form->format(testArgs3, 2, string, ignore, err);
    if (U_FAILURE(err) || string != "The disk \"MyDisk\" contains 12 file(s).") {
        dataerrln(UnicodeString("TestMessageFormat::testSimpleFormat failed on test #3")+string + " - " + u_errorName(err));
    }

    delete form;
 }

void TestMessageFormat::testMsgFormatChoice(/* char* par */)
{
    logln("running TestMessageFormat::testMsgFormatChoice");

    UErrorCode err = U_ZERO_ERROR;

    MessageFormat* form = new MessageFormat("The disk \"{1}\" contains {0}.", err);
    double filelimits[] = {0,1,2};
    UnicodeString filepart[] = {"no files","one file","{0,number} files"};
    ChoiceFormat* fileform = new ChoiceFormat(filelimits, filepart, 3);
    form->setFormat(1,*fileform); // NOT zero, see below
        //is the format adopted?

    FieldPosition ignore(FieldPosition::DONT_CARE);
    UnicodeString string;
    Formattable testArgs1[] = {(int32_t)0, "MyDisk"};    
    form->format(testArgs1, 2, string, ignore, err);
    if (string != "The disk \"MyDisk\" contains no files.") {
        errln("TestMessageFormat::testMsgFormatChoice failed on test #1");
    }
 
    ignore.setField(FieldPosition::DONT_CARE);
    string.remove();
    Formattable testArgs2[] = {(int32_t)1, "MyDisk"};    
    form->format(testArgs2, 2, string, ignore, err);
    if (string != "The disk \"MyDisk\" contains one file.") {
        errln("TestMessageFormat::testMsgFormatChoice failed on test #2");
    }

    ignore.setField(FieldPosition::DONT_CARE);
    string.remove();
    Formattable testArgs3[] = {(int32_t)1273, "MyDisk"};    
    form->format(testArgs3, 2, string, ignore, err);
    if (string != "The disk \"MyDisk\" contains 1,273 files.") {
        dataerrln("TestMessageFormat::testMsgFormatChoice failed on test #3 - %s", u_errorName(err));
    }

    delete form;
    delete fileform;
}


void TestMessageFormat::testMsgFormatPlural(/* char* par */)
{
    logln("running TestMessageFormat::testMsgFormatPlural");

    UErrorCode err = U_ZERO_ERROR;
    UnicodeString t1("{0, plural, one{C''est # fichier} other{Ce sont # fichiers}} dans la liste."); 
    UnicodeString t2("{argument, plural, one{C''est # fichier} other {Ce sont # fichiers}} dans la liste.");
    UnicodeString t3("There {0, plural, one{is # zavod}few{are {0, number,###.0} zavoda} other{are # zavodov}} in the directory.");
    UnicodeString t4("There {argument, plural, one{is # zavod}few{are {argument, number,###.0} zavoda} other{are #zavodov}} in the directory.");
    UnicodeString t5("{0, plural, one {{0, number,C''est #,##0.0# fichier}} other {Ce sont # fichiers}} dans la liste.");
    MessageFormat* mfNum = new MessageFormat(t1, Locale("fr"), err);
    if (U_FAILURE(err)) {
        dataerrln("TestMessageFormat::testMsgFormatPlural #1 - argumentIndex - %s", u_errorName(err));
        logln(UnicodeString("TestMessageFormat::testMsgFormatPlural #1 with error code ")+(int32_t)err);
        return;
    }
    Formattable testArgs1((int32_t)0);
    FieldPosition ignore(FieldPosition::DONT_CARE);
    UnicodeString numResult1;
    mfNum->format(&testArgs1, 1, numResult1, ignore, err);
   
    MessageFormat* mfAlpha = new MessageFormat(t2, Locale("fr"), err);
    UnicodeString argName[] = {UnicodeString("argument")};
    UnicodeString argNameResult;
    mfAlpha->format(argName, &testArgs1, 1, argNameResult, err);
    if (U_FAILURE(err)) {
        dataerrln("TestMessageFormat::testMsgFormatPlural #1 - argumentName - %s", u_errorName(err));
        logln(UnicodeString("TestMessageFormat::testMsgFormatPlural #1 with error code ")+(int32_t)err);
        delete mfNum;
        return;
    }
    if ( numResult1 != argNameResult){
        errln("TestMessageFormat::testMsgFormatPlural #1");
        logln(UnicodeString("The results of argumentName and argumentIndex are not the same."));
    }
    if ( numResult1 != UnicodeString("C\'est 0 fichier dans la liste.")) {
        errln("TestMessageFormat::testMsgFormatPlural #1");
        logln(UnicodeString("The results of argumentName and argumentIndex are not the same."));
    }
    err = U_ZERO_ERROR;
  
    delete mfNum;
    delete mfAlpha;

    MessageFormat* mfNum2 = new MessageFormat(t3, Locale("uk"), err);
    numResult1.remove();
    Formattable testArgs2((int32_t)4);
    mfNum2->format(&testArgs2, 1, numResult1, ignore, err);
    MessageFormat* mfAlpha2 = new MessageFormat(t4, Locale("uk"), err);
    argNameResult.remove();
    mfAlpha2->format(argName, &testArgs2, 1, argNameResult, err);

    if (U_FAILURE(err)) {
        errln("TestMessageFormat::testMsgFormatPlural #2 - argumentName");
        logln(UnicodeString("TestMessageFormat::testMsgFormatPlural #2 with error code ")+(int32_t)err);
        delete mfNum2;
        return;
    }
    if ( numResult1 != argNameResult){
        errln("TestMessageFormat::testMsgFormatPlural #2");
        logln(UnicodeString("The results of argumentName and argumentIndex are not the same."));
    }
    if ( numResult1 != UnicodeString("There are 4,0 zavoda in the directory.")) {
        errln("TestMessageFormat::testMsgFormatPlural #2");
        logln(UnicodeString("The results of argumentName and argumentIndex are not the same."));
    }

    delete mfNum2;
    delete mfAlpha2;
    
    // nested formats
    err = U_ZERO_ERROR;
    MessageFormat* msgFmt = new MessageFormat(t5, Locale("fr"), err);
    if (U_FAILURE(err)) {
        errln("TestMessageFormat::test nested PluralFormat with argumentName");
        logln(UnicodeString("TestMessageFormat::test nested PluralFormat with error code ")+(int32_t)err);
        delete msgFmt;
        return;
    }
    Formattable testArgs3((int32_t)0);
    argNameResult.remove();
    msgFmt->format(&testArgs3, 1, argNameResult, ignore, err);
    if (U_FAILURE(err)) {
        errln("TestMessageFormat::test nested PluralFormat with argumentName");
    }
    if ( argNameResult!= UnicodeString("C'est 0,0 fichier dans la liste.")) {
        errln(UnicodeString("TestMessageFormat::test nested named PluralFormat: ") + argNameResult);
        logln(UnicodeString("The unexpected nested named PluralFormat."));
    }
    delete msgFmt;
}

void TestMessageFormat::testApostropheInPluralAndSelect() {
    UErrorCode errorCode = U_ZERO_ERROR;
    MessageFormat msgFmt(UNICODE_STRING_SIMPLE(
        "abc_{0,plural,other{#'#'#'{'#''}}_def_{1,select,other{sel'}'ect''}}_xyz"),
        Locale::getEnglish(),
        errorCode);
    if (U_FAILURE(errorCode)) {
        errln("MessageFormat constructor failed - %s\n", u_errorName(errorCode));
        return;
    }
    UnicodeString expected = UNICODE_STRING_SIMPLE("abc_3#3{3'_def_sel}ect'_xyz");
    Formattable args[] = { (int32_t)3, UNICODE_STRING_SIMPLE("x") };
    internalFormat(
        &msgFmt, args, 2, expected,
        "MessageFormat with apostrophes in plural/select arguments failed:\n");
}

void TestMessageFormat::internalFormat(MessageFormat* msgFmt , 
        Formattable* args , int32_t numOfArgs , 
        UnicodeString expected, const char* errMsg)
{
        UnicodeString result;
        FieldPosition ignore(FieldPosition::DONT_CARE);
        UErrorCode status = U_ZERO_ERROR;

        //Format with passed arguments
        msgFmt->format( args , numOfArgs , result, ignore, status);
        if (U_FAILURE(status)) {
            dataerrln( "%s error while formatting with ErrorCode as %s" ,errMsg, u_errorName(status) );
        }
        //Compare expected with obtained result
        if ( result!= expected ) {
            UnicodeString err = UnicodeString(errMsg);
            err+= UnicodeString(":Unexpected Result \n Expected: " + expected + "\n Obtained: " + result + "\n");
            dataerrln(err);
        }
}

MessageFormat* TestMessageFormat::internalCreate(
        UnicodeString pattern ,Locale locale ,UErrorCode &status ,  char* errMsg)
{
    //Create the MessageFormat with simple SelectFormat
    MessageFormat* msgFmt = new MessageFormat(pattern, locale, status);
    if (U_FAILURE(status)) {
        dataerrln( "%s error while constructing with ErrorCode as %s" ,errMsg, u_errorName(status) );
        logln(UnicodeString("TestMessageFormat::testMsgFormatSelect #1 with error code ")+(int32_t)status);
        return NULL;
    }
    return msgFmt;
}

void TestMessageFormat::testMsgFormatSelect(/* char* par */)
{
    logln("running TestMessageFormat::testMsgFormatSelect");

    UErrorCode err = U_ZERO_ERROR;
    //French Pattern
    UnicodeString t1("{0} est {1, select, female {all\\u00E9e} other {all\\u00E9}} \\u00E0 Paris.");

    err = U_ZERO_ERROR;
    //Create the MessageFormat with simple French pattern
    MessageFormat* msgFmt1 = internalCreate(t1.unescape(), Locale("fr"),err,(char*)"From TestMessageFormat::TestSelectFormat create t1");
    if (!U_FAILURE(err)) {
        //Arguments 
        Formattable testArgs10[] = {"Kirti","female"};    
        Formattable testArgs11[] = {"Victor","other"};    
        Formattable testArgs12[] = {"Ash","unknown"};    
        Formattable* testArgs[] = {testArgs10,testArgs11,testArgs12};    
        UnicodeString exp[] = {
            "Kirti est all\\u00E9e \\u00E0 Paris." ,
            "Victor est all\\u00E9 \\u00E0 Paris.", 
            "Ash est all\\u00E9 \\u00E0 Paris."}; 
        //Format
        for( int i=0; i< 3; i++){
            internalFormat( msgFmt1 , testArgs[i], 2, exp[i].unescape() ,(char*)"From TestMessageFormat::testSelectFormat format t1");
        }
    }
    delete msgFmt1;

    //Quoted French Pattern
    UnicodeString t2("{0} est {1, select, female {all\\u00E9e c''est} other {all\\u00E9 c''est}} \\u00E0 Paris.");
    err = U_ZERO_ERROR;
    //Create the MessageFormat with Quoted French pattern 
    MessageFormat* msgFmt2 = internalCreate(t2.unescape(), Locale("fr"),err,(char*)"From TestMessageFormat::TestSelectFormat create t2");
    if (!U_FAILURE(err)) {
        //Arguments 
        Formattable testArgs10[] = {"Kirti","female"};    
        Formattable testArgs11[] = {"Victor","other"};    
        Formattable testArgs12[] = {"Ash","male"};    
        Formattable* testArgs[] = {testArgs10,testArgs11,testArgs12};    
        UnicodeString exp[] = {
            "Kirti est all\\u00E9e c'est \\u00E0 Paris." ,
            "Victor est all\\u00E9 c'est \\u00E0 Paris.", 
            "Ash est all\\u00E9 c'est \\u00E0 Paris."}; 
        //Format
        for( int i=0; i< 3; i++){
            internalFormat( msgFmt2 , testArgs[i], 2, exp[i].unescape() ,(char*)"From TestMessageFormat::testSelectFormat format t2");
        }
    }
    delete msgFmt2;
    
    //English Pattern
    UnicodeString t3("{0, select , male {MALE FR company} female {FEMALE FR company} other {FR otherValue}} published new books.");
    err = U_ZERO_ERROR;
    //Create the MessageFormat with English pattern 
    MessageFormat* msgFmt3 = internalCreate(t3, Locale("en"),err,(char*)"From TestMessageFormat::TestSelectFormat create t3");
    if (!U_FAILURE(err)) {
        //Arguments 
        Formattable testArgs10[] = {"female"};
        Formattable testArgs11[] = {"other"};
        Formattable testArgs12[] = {"male"};
        Formattable* testArgs[] = {testArgs10,testArgs11,testArgs12};
        UnicodeString exp[] = {
            "FEMALE FR company published new books." ,
            "FR otherValue published new books.",
            "MALE FR company published new books."};
        //Format
        for( int i=0; i< 3; i++){
            internalFormat( msgFmt3 , testArgs[i], 1, exp[i] ,(char*)"From TestMessageFormat::testSelectFormat format t3");
        }
    }
    delete msgFmt3;

    //Nested patterns with plural, number ,choice ,select format etc.
    //Select Format with embedded number format
    UnicodeString t4("{0} est {1, select, female {{2,number,integer} all\\u00E9e} other {all\\u00E9}} \\u00E0 Paris.");
    err = U_ZERO_ERROR;
    //Create the MessageFormat with Select Format with embedded number format (nested pattern)
    MessageFormat* msgFmt4 = internalCreate(t4.unescape(), Locale("fr"),err,(char*)"From TestMessageFormat::TestSelectFormat create t4");
    if (!U_FAILURE(err)) {
        //Arguments 
        Formattable testArgs10[] = {"Kirti","female",(int32_t)6};    
        Formattable testArgs11[] = {"Kirti","female",100.100};    
        Formattable testArgs12[] = {"Kirti","other",(int32_t)6};    
        Formattable* testArgs[] = {testArgs10,testArgs11,testArgs12};
        UnicodeString exp[] = {
            "Kirti est 6 all\\u00E9e \\u00E0 Paris." ,
            "Kirti est 100 all\\u00E9e \\u00E0 Paris.",
            "Kirti est all\\u00E9 \\u00E0 Paris."};
        //Format
        for( int i=0; i< 3; i++){
            internalFormat( msgFmt4 , testArgs[i], 3, exp[i].unescape() ,(char*)"From TestMessageFormat::testSelectFormat format t4");
        }
    }
    delete msgFmt4;

    //Plural format with embedded select format
    UnicodeString t5("{0} {1, plural, one {est {2, select, female {all\\u00E9e} other {all\\u00E9}}} other {sont {2, select, female {all\\u00E9es} other {all\\u00E9s}}}} \\u00E0 Paris.");
    err = U_ZERO_ERROR;
    //Create the MessageFormat with Plural format with embedded select format(nested pattern)
    MessageFormat* msgFmt5 = internalCreate(t5.unescape(), Locale("fr"),err,(char*)"From TestMessageFormat::TestSelectFormat create t5");
    // with no data the above should fail but it seems to construct an invalid MessageFormat with no reported error. See #13079
    if (!U_FAILURE(err)) {
        //Arguments 
        Formattable testArgs10[] = {"Kirti",(int32_t)6,"female"};  
        Formattable testArgs11[] = {"Kirti",(int32_t)1,"female"};  
        Formattable testArgs12[] = {"Ash",(int32_t)1,"other"};
        Formattable testArgs13[] = {"Ash",(int32_t)5,"other"};  
        Formattable* testArgs[] = {testArgs10,testArgs11,testArgs12,testArgs13};
        UnicodeString exp[] = {
            "Kirti sont all\\u00E9es \\u00E0 Paris." ,
            "Kirti est all\\u00E9e \\u00E0 Paris.",
            "Ash est all\\u00E9 \\u00E0 Paris.",
            "Ash sont all\\u00E9s \\u00E0 Paris."};
        //Format
        for( int i=0; i< 4; i++){
            internalFormat( msgFmt5 , testArgs[i], 3, exp[i].unescape() ,(char*)"From TestMessageFormat::testSelectFormat format t5");
        }
    }
    delete msgFmt5;

    err = U_ZERO_ERROR;
    //Select, plural, and number formats heavily nested 
    UnicodeString t6("{0} und {1, select, female {{2, plural, one {{3, select, female {ihre Freundin} other {ihr Freund}} } other {ihre {2, number, integer} {3, select, female {Freundinnen} other {Freunde}} } }} other{{2, plural, one {{3, select, female {seine Freundin} other {sein Freund}}} other {seine {2, number, integer} {3, select, female {Freundinnen} other {Freunde}}}}} } gingen nach Paris.");
    //Create the MessageFormat with Select, plural, and number formats heavily nested  
    MessageFormat* msgFmt6 = internalCreate(t6, Locale("de"),err,(char*)"From TestMessageFormat::TestSelectFormat create t6");
    if (!U_FAILURE(err)) {
        //Arguments 
        Formattable testArgs10[] = {"Kirti","other",(int32_t)1,"other"}; 
        Formattable testArgs11[] = {"Kirti","other",(int32_t)6,"other"};
        Formattable testArgs12[] = {"Kirti","other",(int32_t)1,"female"};
        Formattable testArgs13[] = {"Kirti","other",(int32_t)3,"female"};
        Formattable testArgs14[] = {"Kirti","female",(int32_t)1,"female"};
        Formattable testArgs15[] = {"Kirti","female",(int32_t)5,"female"};
        Formattable testArgs16[] = {"Kirti","female",(int32_t)1,"other"};
        Formattable testArgs17[] = {"Kirti","female",(int32_t)5,"other"};
        Formattable testArgs18[] = {"Kirti","mixed",(int32_t)1,"mixed"};
        Formattable testArgs19[] = {"Kirti","mixed",(int32_t)1,"other"};
        Formattable testArgs20[] = {"Kirti","female",(int32_t)1,"mixed"};
        Formattable testArgs21[] = {"Kirti","mixed",(int32_t)5,"mixed"};
        Formattable testArgs22[] = {"Kirti","mixed",(int32_t)5,"other"};
        Formattable testArgs23[] = {"Kirti","female",(int32_t)5,"mixed"};
        Formattable* testArgs[] = {testArgs10,testArgs11,testArgs12,testArgs13,
                                   testArgs14,testArgs15,testArgs16,testArgs17,
                                   testArgs18,testArgs19,testArgs20,testArgs21,
                                   testArgs22,testArgs23 };
        UnicodeString exp[] = {
            "Kirti und sein Freund gingen nach Paris." ,
            "Kirti und seine 6 Freunde gingen nach Paris." ,
            "Kirti und seine Freundin gingen nach Paris.",
            "Kirti und seine 3 Freundinnen gingen nach Paris.",
            "Kirti und ihre Freundin  gingen nach Paris.",
            "Kirti und ihre 5 Freundinnen  gingen nach Paris.",
            "Kirti und ihr Freund  gingen nach Paris.",
            "Kirti und ihre 5 Freunde  gingen nach Paris.",
            "Kirti und sein Freund gingen nach Paris.", 
            "Kirti und sein Freund gingen nach Paris.", 
            "Kirti und ihr Freund  gingen nach Paris.",
            "Kirti und seine 5 Freunde gingen nach Paris." ,
            "Kirti und seine 5 Freunde gingen nach Paris." ,
            "Kirti und ihre 5 Freunde  gingen nach Paris."
        };
        //Format
        for( int i=0; i< 14; i++){
            internalFormat( msgFmt6 , testArgs[i], 4, exp[i] ,(char*)"From TestMessageFormat::testSelectFormat format t6");
        }
    }
    delete msgFmt6;
}

//---------------------------------
//  API Tests
//---------------------------------

void TestMessageFormat::testCopyConstructor() 
{
    UErrorCode success = U_ZERO_ERROR;
    MessageFormat *x = new MessageFormat("There are {0} files on {1}", success);
    MessageFormat *z = new MessageFormat("There are {0} files on {1} created", success);
    MessageFormat *y = 0;
    y = new MessageFormat(*x);
    if ( (*x == *y) && 
         (*x != *z) && 
         (*y != *z) )
         logln("First test (operator ==): Passed!");
    else {
        errln("TestMessageFormat::testCopyConstructor failed #1");
        logln("First test (operator ==): Failed!");
    }
    if ( ((*x == *y) && (*y == *x)) &&
         ((*x != *z) && (*z != *x)) &&
         ((*y != *z) && (*z != *y)) )
        logln("Second test (equals): Passed!");
    else {
        errln("TestMessageFormat::testCopyConstructor failed #2");
        logln("Second test (equals): Failed!");
    }

    delete x;
    delete y;
    delete z;
}


void TestMessageFormat::testAssignment() 
{
    UErrorCode success = U_ZERO_ERROR;
    MessageFormat *x = new MessageFormat("There are {0} files on {1}", success);
    MessageFormat *z = new MessageFormat("There are {0} files on {1} created", success);
    MessageFormat *y = new MessageFormat("There are {0} files on {1} created", success);
    *y = *x;
    if ( (*x == *y) && 
         (*x != *z) && 
         (*y != *z) )
        logln("First test (operator ==): Passed!");
    else {
        errln( "TestMessageFormat::testAssignment failed #1");
        logln("First test (operator ==): Failed!");
    }
    if ( ((*x == *y) && (*y == *x)) &&
         ((*x != *z) && (*z != *x)) &&
         ((*y != *z) && (*z != *y)) )
        logln("Second test (equals): Passed!");
    else {
        errln("TestMessageFormat::testAssignment failed #2");
        logln("Second test (equals): Failed!");
    }

    delete x;
    delete y;
    delete z;
}

void TestMessageFormat::testClone() 
{
    UErrorCode success = U_ZERO_ERROR;
    MessageFormat *x = new MessageFormat("There are {0} files on {1}", success);
    MessageFormat *z = new MessageFormat("There are {0} files on {1} created", success);
    MessageFormat *y = 0;
    y = (MessageFormat*)x->clone();
    if ( (*x == *y) && 
         (*x != *z) && 
         (*y != *z) )
        logln("First test (operator ==): Passed!");
    else {
        errln("TestMessageFormat::testClone failed #1");
        logln("First test (operator ==): Failed!");
    }
    if ( ((*x == *y) && (*y == *x)) &&
         ((*x != *z) && (*z != *x)) &&
         ((*y != *z) && (*z != *y)) )
        logln("Second test (equals): Passed!");
    else {
        errln("TestMessageFormat::testClone failed #2");
        logln("Second test (equals): Failed!");
    }

    delete x;
    delete y;
    delete z;
}

void TestMessageFormat::testEquals() 
{
    UErrorCode success = U_ZERO_ERROR;
    MessageFormat x("There are {0} files on {1}", success);
    MessageFormat y("There are {0} files on {1}", success);
    if (!(x == y)) {
        errln( "TestMessageFormat::testEquals failed #1");
        logln("First test (operator ==): Failed!");
    }

}

void TestMessageFormat::testNotEquals() 
{
    UErrorCode success = U_ZERO_ERROR;
    MessageFormat x("There are {0} files on {1}", success);
    MessageFormat y(x);
    y.setLocale(Locale("fr"));
    if (!(x != y)) {
        errln( "TestMessageFormat::testEquals failed #1");
        logln("First test (operator !=): Failed!");
    }
    y = x;
    y.applyPattern("There are {0} files on {1} the disk", success);
    if (!(x != y)) {
        errln( "TestMessageFormat::testEquals failed #1");
        logln("Second test (operator !=): Failed!");
    }
}


void TestMessageFormat::testSetLocale()
{
    UErrorCode err = U_ZERO_ERROR;
    GregorianCalendar cal(err);   
    Formattable arguments[] = {
        456.83,
        Formattable(UDate(8.71068e+011), Formattable::kIsDate),
        "deposit"
        };
   
    UnicodeString result;

    //UnicodeString formatStr = "At {1,time} on {1,date}, you made a {2} of {0,number,currency}.";
    UnicodeString formatStr = "At <time> on {1,date}, you made a {2} of {0,number,currency}.";
    // {sfb} to get $, would need Locale::US, not Locale::ENGLISH
    // Just use unlocalized currency symbol.
    //UnicodeString compareStrEng = "At <time> on Aug 8, 1997, you made a deposit of $456.83.";
    UnicodeString compareStrEng = "At <time> on Aug 8, 1997, you made a deposit of ";
    compareStrEng += (UChar) 0x00a4;
    compareStrEng += "456.83.";
    // {sfb} to get DM, would need Locale::GERMANY, not Locale::GERMAN
    // Just use unlocalized currency symbol.
    //UnicodeString compareStrGer = "At <time> on 08.08.1997, you made a deposit of 456,83 DM.";
    UnicodeString compareStrGer = "At <time> on 08.08.1997, you made a deposit of ";
    compareStrGer += "456,83";
    compareStrGer += (UChar) 0x00a0;
    compareStrGer += "XXX.";

    MessageFormat msg( formatStr, err);
    result = "";
    FieldPosition pos(FieldPosition::DONT_CARE);
    result = msg.format(
        arguments,
        3,
        result,
        pos,
        err);

    logln(result);
    if (result != compareStrEng) {
        char bbuf[96];
        result.extract(0, result.length(), bbuf, sizeof(bbuf));
        dataerrln("***  MSG format err. - %s; result was %s", u_errorName(err), bbuf);
    }

    msg.setLocale(Locale::getEnglish());
    UBool getLocale_ok = TRUE;
    if (msg.getLocale() != Locale::getEnglish()) {
        errln("*** MSG getLocale err.");
        getLocale_ok = FALSE;
    }

    msg.setLocale(Locale::getGerman());

    if (msg.getLocale() != Locale::getGerman()) {
        errln("*** MSG getLocal err.");
        getLocale_ok = FALSE;
    }

    msg.applyPattern( formatStr, err);

    pos.setField(0);
    result = "";
    result = msg.format(
        arguments,
        3,
        result,
        pos,
        err);

    logln(result);
    if (result == compareStrGer) {
        logln("MSG setLocale tested.");
    }else{
        dataerrln( "*** MSG setLocale err. - %s", u_errorName(err));
    }

    if (getLocale_ok) { 
        logln("MSG getLocale tested.");
    }
}

void TestMessageFormat::testFormat()
{
    UErrorCode err = U_ZERO_ERROR;
    GregorianCalendar cal(err);   

    const Formattable ftarray[] = 
    {
        Formattable( UDate(8.71068e+011), Formattable::kIsDate )
    };
    const int32_t ft_cnt = UPRV_LENGTHOF(ftarray);
    Formattable ft_arr( ftarray, ft_cnt );

    Formattable* fmt = new Formattable(UDate(8.71068e+011), Formattable::kIsDate);
   
    UnicodeString result;

    //UnicodeString formatStr = "At {1,time} on {1,date}, you made a {2} of {0,number,currency}.";
    UnicodeString formatStr = "On {0,date}, it began.";
    UnicodeString compareStr = "On Aug 8, 1997, it began.";

    err = U_ZERO_ERROR;
    MessageFormat msg( formatStr, err);
    FieldPosition fp(FieldPosition::DONT_CARE);

    result = "";
    fp = 0;
    result = msg.format(
        *fmt,
        result,
        //FieldPosition(FieldPosition::DONT_CARE),
        fp,
        err);

    if (err != U_ILLEGAL_ARGUMENT_ERROR) {
        dataerrln("*** MSG format without expected error code. - %s", u_errorName(err));
    }
    err = U_ZERO_ERROR;

    result = "";
    fp = 0;
    result = msg.format(
        ft_arr,
        result,
        //FieldPosition(FieldPosition::DONT_CARE),
        fp,
        err);

    logln("MSG format( Formattable&, ... ) expected:" + compareStr);
    logln("MSG format( Formattable&, ... )   result:" + result);
    if (result != compareStr) {
        dataerrln("***  MSG format( Formattable&, .... ) err. - %s", u_errorName(err));
    }else{
        logln("MSG format( Formattable&, ... ) tested.");
    }

    delete fmt;

}

void TestMessageFormat::testParse()
{
    UErrorCode err = U_ZERO_ERROR;
    int32_t count;
    UnicodeString msgFormatString = "{0} =sep= {1}";
    MessageFormat msg( msgFormatString, err);
    UnicodeString source = "abc =sep= def";
    UnicodeString tmp1, tmp2;

    Formattable* fmt_arr = msg.parse( source, count, err );
    if (U_FAILURE(err) || (!fmt_arr)) {
        errln("*** MSG parse (ustring, count, err) error.");
    }else{
        logln("MSG parse -- count: %d", count);
        if (count != 2) {
            errln("*** MSG parse (ustring, count, err) count err.");
        }else{
            if ((fmt_arr[0].getType() == Formattable::kString)
             && (fmt_arr[1].getType() == Formattable::kString)
             && (fmt_arr[0].getString(tmp1) == "abc")
             && (fmt_arr[1].getString(tmp2) == "def")) {
                logln("MSG parse (ustring, count, err) tested.");
            }else{
                errln("*** MSG parse (ustring, count, err) result err.");
            }
        }
    }
    delete[] fmt_arr;

    ParsePosition pp(0);

    fmt_arr = msg.parse( source, pp, count );
    if ((pp == 0) || (!fmt_arr)) {
        errln("*** MSG parse (ustring, parsepos., count) error.");
    }else{
        logln("MSG parse -- count: %d", count);
        if (count != 2) {
            errln("*** MSG parse (ustring, parsepos., count) count err.");
        }else{
            if ((fmt_arr[0].getType() == Formattable::kString)
             && (fmt_arr[1].getType() == Formattable::kString)
             && (fmt_arr[0].getString(tmp1) == "abc")
             && (fmt_arr[1].getString(tmp2) == "def")) {
                logln("MSG parse (ustring, parsepos., count) tested.");
            }else{
                errln("*** MSG parse (ustring, parsepos., count) result err.");
            }
        }
    }
    delete[] fmt_arr;

    pp = 0;
    Formattable fmta;

    msg.parseObject( source, fmta, pp );
    if (pp == 0) {
        errln("*** MSG parse (ustring, Formattable, parsepos ) error.");
    }else{
        logln("MSG parse -- count: %d", count);
        fmta.getArray(count);
        if (count != 2) {
            errln("*** MSG parse (ustring, Formattable, parsepos ) count err.");
        }else{
            if ((fmta[0].getType() == Formattable::kString)
             && (fmta[1].getType() == Formattable::kString)
             && (fmta[0].getString(tmp1) == "abc")
             && (fmta[1].getString(tmp2) == "def")) {
                logln("MSG parse (ustring, Formattable, parsepos ) tested.");
            }else{
                errln("*** MSG parse (ustring, Formattable, parsepos ) result err.");
            }
        }
    }
}


void TestMessageFormat::testAdopt()
{
    UErrorCode err = U_ZERO_ERROR;

    UnicodeString formatStr("{0,date},{1},{2,number}", "");
    UnicodeString formatStrChange("{0,number},{1,number},{2,date}", "");
    err = U_ZERO_ERROR;
    MessageFormat msg( formatStr, err);
    MessageFormat msgCmp( formatStr, err);
    if (U_FAILURE(err)) {
        dataerrln("Unable to instantiate MessageFormat - %s", u_errorName(err));
        return;
    }
    int32_t count, countCmp;
    const Format** formats = msg.getFormats(count);
    const Format** formatsCmp = msgCmp.getFormats(countCmp);
    const Format** formatsChg = 0;
    const Format** formatsAct = 0;
    int32_t countAct;
    const Format* a;
    const Format* b;
    UnicodeString patCmp;
    UnicodeString patAct;
    Format** formatsToAdopt;

    if (!formats || !formatsCmp || (count <= 0) || (count != countCmp)) {
        dataerrln("Error getting Formats");
        return;
    }

    int32_t i;

    for (i = 0; i < count; i++) {
        a = formats[i];
        b = formatsCmp[i];
        if ((a != NULL) && (b != NULL)) {
            if (*a != *b) {
                errln("a != b");
                return;
            }
        }else if ((a != NULL) || (b != NULL)) {
            errln("(a != NULL) || (b != NULL)");
            return;
        }
    }

    msg.applyPattern( formatStrChange, err ); //set msg formats to something different
    int32_t countChg;
    formatsChg = msg.getFormats(countChg); // tested function
    if (!formatsChg || (countChg != count)) {
        errln("Error getting Formats");
        return;
    }

    UBool diff;
    diff = TRUE;
    for (i = 0; i < count; i++) {
        a = formatsChg[i];
        b = formatsCmp[i];
        if ((a != NULL) && (b != NULL)) {
            if (*a == *b) {
                logln("formatsChg == formatsCmp at index %d", i);
                diff = FALSE;
            }
        }
    }
    if (!diff) {
        errln("*** MSG getFormats diff err.");
        return;
    }

    logln("MSG getFormats tested.");

    msg.setFormats( formatsCmp, countCmp ); //tested function

    formatsAct = msg.getFormats(countAct);
    if (!formatsAct || (countAct <=0) || (countAct != countCmp)) {
        errln("Error getting Formats");
        return;
    }

    assertEquals("msgCmp.toPattern()", formatStr, msgCmp.toPattern(patCmp.remove()));
    // ICU 4.8 does not support toPattern() when there are custom formats (from setFormat() etc.).
    // assertEquals("msg.toPattern()", formatStr, msg.toPattern(patAct.remove()));
    msg.toPattern(patCmp.remove());
    if (!patCmp.isBogus()) {
      errln("msg.setFormat().toPattern() succeeds.");
    }

    for (i = 0; i < countAct; i++) {
        a = formatsAct[i];
        b = formatsCmp[i];
        if ((a != NULL) && (b != NULL)) {
            if (*a != *b) {
                logln("formatsAct != formatsCmp at index %d", i);
                errln("a != b");
                return;
            }
        }else if ((a != NULL) || (b != NULL)) {
            errln("(a != NULL) || (b != NULL)");
            return;
        }
    }
    logln("MSG setFormats tested.");

    //----

    msg.applyPattern( formatStrChange, err ); //set msg formats to something different

    formatsToAdopt = new Format* [countCmp];
    if (!formatsToAdopt) {
        errln("memory allocation error");
        return;
    }

    for (i = 0; i < countCmp; i++) {
        if (formatsCmp[i] == NULL) {
            formatsToAdopt[i] = NULL;
        }else{
            formatsToAdopt[i] = formatsCmp[i]->clone();
            if (!formatsToAdopt[i]) {
                errln("Can't clone format at index %d", i);
                return;
            }
        }
    }
    msg.adoptFormats( formatsToAdopt, countCmp ); // function to test
    delete[] formatsToAdopt;

    assertEquals("msgCmp.toPattern()", formatStr, msgCmp.toPattern(patCmp.remove()));
    // ICU 4.8 does not support toPattern() when there are custom formats (from setFormat() etc.).
    // assertEquals("msg.toPattern()", formatStr, msg.toPattern(patAct.remove()));

    formatsAct = msg.getFormats(countAct);
    if (!formatsAct || (countAct <=0) || (countAct != countCmp)) {
        errln("Error getting Formats");
        return;
    }

    for (i = 0; i < countAct; i++) {
        a = formatsAct[i];
        b = formatsCmp[i];
        if ((a != NULL) && (b != NULL)) {
            if (*a != *b) {
                errln("a != b");
                return;
            }
        }else if ((a != NULL) || (b != NULL)) {
            errln("(a != NULL) || (b != NULL)");
            return;
        }
    }
    logln("MSG adoptFormats tested.");

    //---- adoptFormat

    msg.applyPattern( formatStrChange, err ); //set msg formats to something different

    formatsToAdopt = new Format* [countCmp];
    if (!formatsToAdopt) {
        errln("memory allocation error");
        return;
    }

    for (i = 0; i < countCmp; i++) {
        if (formatsCmp[i] == NULL) {
            formatsToAdopt[i] = NULL;
        }else{
            formatsToAdopt[i] = formatsCmp[i]->clone();
            if (!formatsToAdopt[i]) {
                errln("Can't clone format at index %d", i);
                return;
            }
        }
    }

    for ( i = 0; i < countCmp; i++ ) {
        msg.adoptFormat( i, formatsToAdopt[i] ); // function to test
    }
    delete[] formatsToAdopt; // array itself not needed in this case;

    assertEquals("msgCmp.toPattern()", formatStr, msgCmp.toPattern(patCmp.remove()));
    // ICU 4.8 does not support toPattern() when there are custom formats (from setFormat() etc.).
    // assertEquals("msg.toPattern()", formatStr, msg.toPattern(patAct.remove()));

    formatsAct = msg.getFormats(countAct);
    if (!formatsAct || (countAct <=0) || (countAct != countCmp)) {
        errln("Error getting Formats");
        return;
    }

    for (i = 0; i < countAct; i++) {
        a = formatsAct[i];
        b = formatsCmp[i];
        if ((a != NULL) && (b != NULL)) {
            if (*a != *b) {
                errln("a != b");
                return;
            }
        }else if ((a != NULL) || (b != NULL)) {
            errln("(a != NULL) || (b != NULL)");
            return;
        }
    }
    logln("MSG adoptFormat tested.");
}

// This test is a regression test for a fixed bug in the copy constructor.
// It is kept as a global function rather than as a method since the test depends on memory values.
// (At least before the bug was fixed, whether it showed up or not depended on memory contents,
// which is probably why it didn't show up in the regular test for the copy constructor.)
// For this reason, the test isn't changed even though it contains function calls whose results are
// not tested and had no problems. Actually, the test failed by *crashing*.
static void _testCopyConstructor2()
{
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString formatStr("Hello World on {0,date,full}", "");
    UnicodeString resultStr(" ", "");
    UnicodeString result;
    FieldPosition fp(FieldPosition::DONT_CARE);
    UDate d = Calendar::getNow();
    const Formattable fargs( d, Formattable::kIsDate );

    MessageFormat* fmt1 = new MessageFormat( formatStr, status );
    MessageFormat* fmt2 = NULL;
    MessageFormat* fmt3 = NULL;
    MessageFormat* fmt4 = NULL;

    if (fmt1 == NULL) {
        it_err("testCopyConstructor2: (fmt1 != NULL)");
        goto cleanup;
    }

    fmt2 = new MessageFormat( *fmt1 );
    result = fmt1->format( &fargs, 1, resultStr, fp, status );

    if (fmt2 == NULL) {
        it_err("testCopyConstructor2: (fmt2 != NULL)");
        goto cleanup;
    }

    fmt3 = (MessageFormat*) fmt1->clone();
    fmt4 = (MessageFormat*) fmt2->clone();

    if (fmt3 == NULL) {
        it_err("testCopyConstructor2: (fmt3 != NULL)");
        goto cleanup;
    }
    if (fmt4 == NULL) {
        it_err("testCopyConstructor2: (fmt4 != NULL)");
        goto cleanup;
    }

    result = fmt1->format( &fargs, 1, resultStr, fp, status );
    result = fmt2->format( &fargs, 1, resultStr, fp, status );
    result = fmt3->format( &fargs, 1, resultStr, fp, status );
    result = fmt4->format( &fargs, 1, resultStr, fp, status );

cleanup:
    delete fmt1;
    delete fmt2;
    delete fmt3;
    delete fmt4;
}

void TestMessageFormat::testCopyConstructor2() {
    _testCopyConstructor2();
}

/**
 * Verify that MessageFormat accomodates more than 10 arguments and
 * more than 10 subformats.
 */
void TestMessageFormat::TestUnlimitedArgsAndSubformats() {
    UErrorCode ec = U_ZERO_ERROR;
    const UnicodeString pattern =
        "On {0,date} (aka {0,date,short}, aka {0,date,long}) "
        "at {0,time} (aka {0,time,short}, aka {0,time,long}) "
        "there were {1,number} werjes "
        "(a {3,number,percent} increase over {2,number}) "
        "despite the {4}''s efforts "
        "and to delight of {5}, {6}, {7}, {8}, {9}, and {10} {11}.";
    MessageFormat msg(pattern, ec);
    if (U_FAILURE(ec)) {
        dataerrln("FAIL: constructor failed - %s", u_errorName(ec));
        return;
    }

    const Formattable ARGS[] = {
        Formattable(UDate(1e13), Formattable::kIsDate),
        Formattable((int32_t)1303),
        Formattable((int32_t)1202),
        Formattable(1303.0/1202 - 1),
        Formattable("Glimmung"),
        Formattable("the printers"),
        Formattable("Nick"),
        Formattable("his father"),
        Formattable("his mother"),
        Formattable("the spiddles"),
        Formattable("of course"),
        Formattable("Horace"),
    };
    const int32_t ARGS_LENGTH = UPRV_LENGTHOF(ARGS);
    Formattable ARGS_OBJ(ARGS, ARGS_LENGTH);

    UnicodeString expected =
        "On Nov 20, 2286 (aka 11/20/86, aka November 20, 2286) "
        "at 9:46:40 AM (aka 9:46 AM, aka 9:46:40 AM PST) "
        "there were 1,303 werjes "
        "(a 8% increase over 1,202) "
        "despite the Glimmung's efforts "
        "and to delight of the printers, Nick, his father, "
        "his mother, the spiddles, and of course Horace.";
    UnicodeString result;
    msg.format(ARGS_OBJ, result, ec);
    if (result == expected) {
        logln(result);
    } else {
        errln((UnicodeString)"FAIL: Got " + result +
              ", expected " + expected);
    }
}

// test RBNF extensions to message format
void TestMessageFormat::TestRBNF(void) {
    // WARNING: this depends on the RBNF formats for en_US
    Locale locale("en", "US", "");

    UErrorCode ec = U_ZERO_ERROR;

    UnicodeString values[] = {
        // decimal values do not format completely for ordinal or duration, and 
        // do not always parse, so do not include them
        "0", "1", "12", "100", "123", "1001", "123,456", "-17",
    };
    int32_t values_count = UPRV_LENGTHOF(values);

    UnicodeString formats[] = {
        "There are {0,spellout} files to search.",
        "There are {0,spellout,%simplified} files to search.",
        "The bogus spellout {0,spellout,%BOGUS} files behaves like the default.",
        "This is the {0,ordinal} file to search.",
        "Searching this file will take {0,duration} to complete.",
        "Searching this file will take {0,duration,%with-words} to complete.",
    };
    int32_t formats_count = UPRV_LENGTHOF(formats);

    Formattable args[1];

    NumberFormat* numFmt = NumberFormat::createInstance(locale, ec);
    if (U_FAILURE(ec)) {
        dataerrln("Error calling NumberFormat::createInstance()");
        return;
    }

    for (int i = 0; i < formats_count; ++i) {
        MessageFormat* fmt = new MessageFormat(formats[i], locale, ec);
        logln((UnicodeString)"Testing format pattern: '" + formats[i] + "'");

        for (int j = 0; j < values_count; ++j) {
            ec = U_ZERO_ERROR;
            numFmt->parse(values[j], args[0], ec);
            if (U_FAILURE(ec)) {
                errln((UnicodeString)"Failed to parse test argument " + values[j]);
            } else {
                FieldPosition fp(FieldPosition::DONT_CARE);
                UnicodeString result;
                fmt->format(args, 1, result, fp, ec);
                logln((UnicodeString)"value: " + toString(args[0]) + " --> " + result + UnicodeString(" ec: ") + u_errorName(ec));
               
                int32_t count = 0;
                Formattable* parseResult = fmt->parse(result, count, ec);
                if (count != 1) {
                    errln((UnicodeString)"parse returned " + count + " args");
                } else if (parseResult[0] != args[0]) {
                    errln((UnicodeString)"parsed argument " + toString(parseResult[0]) + " != " + toString(args[0]));
                }
                delete []parseResult;
            }
        }
        delete fmt;
    }
    delete numFmt;
}

UnicodeString TestMessageFormat::GetPatternAndSkipSyntax(const MessagePattern& pattern) {
    UnicodeString us(pattern.getPatternString());
    int count = pattern.countParts();
    for (int i = count; i > 0;) {
        const MessagePattern::Part& part = pattern.getPart(--i);
        if (part.getType() == UMSGPAT_PART_TYPE_SKIP_SYNTAX) {
            us.remove(part.getIndex(), part.getLimit() - part.getIndex());
        }
    }
    return us;
}

void TestMessageFormat::TestApostropheMode() {
    UErrorCode status = U_ZERO_ERROR;
    MessagePattern *ado_mp = new MessagePattern(UMSGPAT_APOS_DOUBLE_OPTIONAL, status);
    MessagePattern *adr_mp = new MessagePattern(UMSGPAT_APOS_DOUBLE_REQUIRED, status);
    if (ado_mp->getApostropheMode() != UMSGPAT_APOS_DOUBLE_OPTIONAL) {
      errln("wrong value from ado_mp->getApostropheMode().");
    }
    if (adr_mp->getApostropheMode() != UMSGPAT_APOS_DOUBLE_REQUIRED) {
      errln("wrong value from adr_mp->getApostropheMode().");
    }


    UnicodeString tuples[] = {
        // Desired output
        // DOUBLE_OPTIONAL pattern
        // DOUBLE_REQUIRED pattern (empty=same as DOUBLE_OPTIONAL)
        "I see {many}", "I see '{many}'", "",
        "I said {'Wow!'}", "I said '{''Wow!''}'", "",
        "I dont know", "I dont know", "I don't know",
        "I don't know", "I don't know", "I don''t know",
        "I don't know", "I don''t know", "I don''t know"
    };
    int32_t tuples_count = UPRV_LENGTHOF(tuples);

    for (int i = 0; i < tuples_count; i += 3) {
      UnicodeString& desired = tuples[i];
      UnicodeString& ado_pattern = tuples[i + 1];
      UErrorCode status = U_ZERO_ERROR;
      assertEquals("DOUBLE_OPTIONAL failure",
                   desired,
                   GetPatternAndSkipSyntax(ado_mp->parse(ado_pattern, NULL, status)));
      UnicodeString& adr_pattern = tuples[i + 2].isEmpty() ? ado_pattern : tuples[i + 2];
      assertEquals("DOUBLE_REQUIRED failure", desired,
          GetPatternAndSkipSyntax(adr_mp->parse(adr_pattern, NULL, status)));
    }
    delete adr_mp;
    delete ado_mp;
}


// Compare behavior of DOUBLE_OPTIONAL (new default) and DOUBLE_REQUIRED JDK-compatibility mode.
void TestMessageFormat::TestCompatibleApostrophe() {
    // Message with choice argument which does not contain another argument.
    // The JDK performs only one apostrophe-quoting pass on this pattern.
    UnicodeString pattern = "ab{0,choice,0#1'2''3'''4''''.}yz";

    UErrorCode ec = U_ZERO_ERROR;
    MessageFormat compMsg("", Locale::getUS(), ec);
    compMsg.applyPattern(pattern, UMSGPAT_APOS_DOUBLE_REQUIRED, NULL, ec);
    if (compMsg.getApostropheMode() != UMSGPAT_APOS_DOUBLE_REQUIRED) {
        errln("wrong value from  compMsg.getApostropheMode().");
    }

    MessageFormat icuMsg("", Locale::getUS(), ec);
    icuMsg.applyPattern(pattern, UMSGPAT_APOS_DOUBLE_OPTIONAL, NULL, ec);
    if (icuMsg.getApostropheMode() != UMSGPAT_APOS_DOUBLE_OPTIONAL) {
        errln("wrong value from  icuMsg.getApostropheMode().");
    }

    Formattable zero0[] = { (int32_t)0 };
    FieldPosition fieldpos(FieldPosition::DONT_CARE);
    UnicodeString buffer1, buffer2;
    assertEquals("incompatible ICU MessageFormat compatibility-apostrophe behavior",
            "ab12'3'4''.yz",
            compMsg.format(zero0, 1, buffer1, fieldpos, ec));
    assertEquals("unexpected ICU MessageFormat double-apostrophe-optional behavior",
            "ab1'2'3''4''.yz",
            icuMsg.format(zero0, 1, buffer2, fieldpos, ec));

    // Message with choice argument which contains a nested simple argument.
    // The DOUBLE_REQUIRED version performs two apostrophe-quoting passes.
    buffer1.remove();
    buffer2.remove();
    pattern = "ab{0,choice,0#1'2''3'''4''''.{0,number,'#x'}}yz";
    compMsg.applyPattern(pattern, ec);
    icuMsg.applyPattern(pattern, ec);
    if (U_FAILURE(ec)) {
        dataerrln("Unable to applyPattern - %s", u_errorName(ec));
    } else {
        assertEquals("incompatible ICU MessageFormat compatibility-apostrophe behavior",
                "ab1234'.0xyz",
                compMsg.format(zero0, 1, buffer1, fieldpos, ec));
        assertEquals("unexpected ICU MessageFormat double-apostrophe-optional behavior",
                "ab1'2'3''4''.#x0yz",
                icuMsg.format(zero0, 1, buffer2, fieldpos, ec));
    }

    // This part is copied over from Java tests but cannot be properly tested here
    // because we do not have a live reference implementation with JDK behavior.
    // The JDK ChoiceFormat itself always performs one apostrophe-quoting pass.
    /*
    ChoiceFormat choice = new ChoiceFormat("0#1'2''3'''4''''.");
    assertEquals("unexpected JDK ChoiceFormat apostrophe behavior",
            "12'3'4''.",
            choice.format(0));
    choice.applyPattern("0#1'2''3'''4''''.{0,number,'#x'}");
    assertEquals("unexpected JDK ChoiceFormat apostrophe behavior",
            "12'3'4''.{0,number,#x}",
            choice.format(0));
    */
}

void TestMessageFormat::testAutoQuoteApostrophe(void) {
    const char* patterns[] = { // pattern, expected pattern
        "'", "''",
        "''", "''",
        "'{", "'{'",
        "' {", "'' {",
        "'a", "''a",
        "'{'a", "'{'a",
        "'{a'", "'{a'",
        "'{}", "'{}'",
        "{'", "{'",
        "{'a", "{'a",
        "{'a{}'a}'a", "{'a{}'a}''a",
        "'}'", "'}'",
        "'} '{'}'", "'} '{'}''",
        "'} {{{''", "'} {{{'''",
    };
    int32_t pattern_count = UPRV_LENGTHOF(patterns);

    for (int i = 0; i < pattern_count; i += 2) {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString result = MessageFormat::autoQuoteApostrophe(patterns[i], status);
        UnicodeString target(patterns[i+1]);
        if (target != result) {
            const int BUF2_LEN = 64;
            char buf[256];
            char buf2[BUF2_LEN];
            int32_t len = result.extract(0, result.length(), buf2, BUF2_LEN);
            if (len >= BUF2_LEN) {
                buf2[BUF2_LEN-1] = 0;
            }
            sprintf(buf, "[%2d] test \"%s\": target (\"%s\") != result (\"%s\")\n", i/2, patterns[i], patterns[i+1], buf2);
            errln(buf);
        }
    }
}

void TestMessageFormat::testCoverage(void) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString testformat("{argument, plural, one{C''est # fichier} other {Ce sont # fichiers}} dans la liste.");
    MessageFormat *msgfmt = new MessageFormat(testformat, Locale("fr"), status);
    if (msgfmt == NULL || U_FAILURE(status)) {
        dataerrln("FAIL: Unable to create MessageFormat.: %s", u_errorName(status));
        return;
    }
    if (!msgfmt->usesNamedArguments()) {
        errln("FAIL: Unable to detect usage of named arguments.");
    }
    const double limit[] = {0.0, 1.0, 2.0};
    const UnicodeString formats[] = {"0.0<=Arg<1.0",
                                   "1.0<=Arg<2.0",
                                   "2.0<-Arg"};
    ChoiceFormat cf(limit, formats, 3);

    msgfmt->setFormat("set", cf, status);

    StringEnumeration *en = msgfmt->getFormatNames(status);
    if (en == NULL || U_FAILURE(status)) {
        errln("FAIL: Unable to get format names enumeration.");
    } else {
        int32_t count = 0;
        en->reset(status);
        count = en->count(status);
        if (U_FAILURE(status)) {
            errln("FAIL: Unable to get format name enumeration count.");
        } else {
            for (int32_t i = 0; i < count; i++) {
                en->snext(status);
                if (U_FAILURE(status)) {
                    errln("FAIL: Error enumerating through names.");
                    break;
                }
            }
        }
    }

    // adoptFormat() takes ownership of the input Format object.
    // We need to clone the stack-allocated cf so that we do not attempt to delete cf.
    Format *cfClone = cf.clone();
    msgfmt->adoptFormat("adopt", cfClone, status);

    delete en;
    delete msgfmt;

    msgfmt = new MessageFormat("'", status);
    if (msgfmt == NULL || U_FAILURE(status)) {
        errln("FAIL: Unable to create MessageFormat.");
        return;
    }
    if (msgfmt->usesNamedArguments()) {
        errln("FAIL: Unable to detect usage of named arguments.");
    }

    // Starting with ICU 4.8, we support setFormat(name, ...) and getFormatNames()
    // on a MessageFormat without named arguments.
    msgfmt->setFormat("formatName", cf, status);
    if (U_FAILURE(status)) {
        errln("FAIL: Should work to setFormat(name, ...) regardless of pattern.");
    }
    status = U_ZERO_ERROR;
    en = msgfmt->getFormatNames(status);
    if (U_FAILURE(status)) {
        errln("FAIL: Should work to get format names enumeration regardless of pattern.");
    }

    delete en;
    delete msgfmt;
}

void TestMessageFormat::testGetFormatNames() {
    IcuTestErrorCode errorCode(*this, "testGetFormatNames");
    MessageFormat msgfmt("Hello, {alice,number} {oops,date,full}  {zip,spellout} World.", Locale::getRoot(), errorCode);
    if(errorCode.errDataIfFailureAndReset("MessageFormat() failed")) {
        return;
    }
    LocalPointer<StringEnumeration> names(msgfmt.getFormatNames(errorCode));
    if(errorCode.errIfFailureAndReset("msgfmt.getFormatNames() failed")) {
        return;
    }
    const UnicodeString *name;
    name = names->snext(errorCode);
    if (name == NULL || errorCode.isFailure()) {
        errln("msgfmt.getFormatNames()[0] failed: %s", errorCode.errorName());
        errorCode.reset();
        return;
    }
    if (!assertEquals("msgfmt.getFormatNames()[0]", UNICODE_STRING_SIMPLE("alice"), *name)) {
        return;
    }
    name = names->snext(errorCode);
    if (name == NULL || errorCode.isFailure()) {
        errln("msgfmt.getFormatNames()[1] failed: %s", errorCode.errorName());
        errorCode.reset();
        return;
    }
    if (!assertEquals("msgfmt.getFormatNames()[1]", UNICODE_STRING_SIMPLE("oops"), *name)) {
        return;
    }
    name = names->snext(errorCode);
    if (name == NULL || errorCode.isFailure()) {
        errln("msgfmt.getFormatNames()[2] failed: %s", errorCode.errorName());
        errorCode.reset();
        return;
    }
    if (!assertEquals("msgfmt.getFormatNames()[2]", UNICODE_STRING_SIMPLE("zip"), *name)) {
        return;
    }
    name = names->snext(errorCode);
    if (name != NULL) {
        errln(UnicodeString("msgfmt.getFormatNames()[3] should be NULL but is: ") + *name);
        return;
    }
}

void TestMessageFormat::TestTrimArgumentName() {
    // ICU 4.8 allows and ignores white space around argument names and numbers.
    IcuTestErrorCode errorCode(*this, "TestTrimArgumentName");
    MessageFormat m("a { 0 , number , '#,#'#.0 } z", Locale::getEnglish(), errorCode);
    if (errorCode.errDataIfFailureAndReset("Unable to instantiate MessageFormat")) {
        return;
    }
    Formattable args[1] = { (int32_t)2 };
    FieldPosition ignore(FieldPosition::DONT_CARE);
    UnicodeString result;
    assertEquals("trim-numbered-arg format() failed", "a  #,#2.0  z",
                 m.format(args, 1, result, ignore, errorCode));

    m.applyPattern("x { _oOo_ , number , integer } y", errorCode);
    UnicodeString argName = UNICODE_STRING_SIMPLE("_oOo_");
    args[0].setLong(3);
    result.remove();
    assertEquals("trim-named-arg format() failed", "x 3 y",
                  m.format(&argName, args, 1, result, errorCode));
}

void TestMessageFormat::TestSelectOrdinal() {
    IcuTestErrorCode errorCode(*this, "TestSelectOrdinal");
    // Test plural & ordinal together,
    // to make sure that we get the correct cached PluralSelector for each.
    MessageFormat m(
        "{0,plural,one{1 file}other{# files}}, "
        "{0,selectordinal,one{#st file}two{#nd file}few{#rd file}other{#th file}}",
        Locale::getEnglish(), errorCode);
    if (errorCode.errDataIfFailureAndReset("Unable to instantiate MessageFormat")) {
        return;
    }
    Formattable args[1] = { (int32_t)21 };
    FieldPosition ignore(FieldPosition::DONT_CARE);
    UnicodeString result;
    assertEquals("plural-and-ordinal format(21) failed", "21 files, 21st file",
                 m.format(args, 1, result, ignore, errorCode), TRUE);

    args[0].setLong(2);
    assertEquals("plural-and-ordinal format(2) failed", "2 files, 2nd file",
                 m.format(args, 1, result.remove(), ignore, errorCode), TRUE);

    args[0].setLong(1);
    assertEquals("plural-and-ordinal format(1) failed", "1 file, 1st file",
                 m.format(args, 1, result.remove(), ignore, errorCode), TRUE);

    args[0].setLong(3);
    assertEquals("plural-and-ordinal format(3) failed", "3 files, 3rd file",
                 m.format(args, 1, result.remove(), ignore, errorCode), TRUE);

    errorCode.errDataIfFailureAndReset("");
}

void TestMessageFormat::TestDecimals() {
    IcuTestErrorCode errorCode(*this, "TestDecimals");
    // Simple number replacement.
    MessageFormat m(
            "{0,plural,one{one meter}other{# meters}}",
            Locale::getEnglish(), errorCode);
    Formattable args[1] = { (int32_t)1 };
    FieldPosition ignore;
    UnicodeString result;
    assertEquals("simple format(1)", "one meter",
            m.format(args, 1, result, ignore, errorCode), TRUE);

    args[0] = (double)1.5;
    result.remove();
    assertEquals("simple format(1.5)", "1.5 meters",
            m.format(args, 1, result, ignore, errorCode), TRUE);

    // Simple but explicit.
    MessageFormat m0(
            "{0,plural,one{one meter}other{{0} meters}}",
            Locale::getEnglish(), errorCode);
    args[0] = (int32_t)1;
    result.remove();
    assertEquals("explicit format(1)", "one meter",
            m0.format(args, 1, result, ignore, errorCode), TRUE);

    args[0] = (double)1.5;
    result.remove();
    assertEquals("explicit format(1.5)", "1.5 meters",
            m0.format(args, 1, result, ignore, errorCode), TRUE);

    // With offset and specific simple format with optional decimals.
    MessageFormat m1(
            "{0,plural,offset:1 one{another meter}other{{0,number,00.#} meters}}",
            Locale::getEnglish(), errorCode);
    args[0] = (int32_t)1;
    result.remove();
    assertEquals("offset format(1)", "01 meters",
            m1.format(args, 1, result, ignore, errorCode), TRUE);

    args[0] = (int32_t)2;
    result.remove();
    assertEquals("offset format(1)", "another meter",
            m1.format(args, 1, result, ignore, errorCode), TRUE);

    args[0] = (double)2.5;
    result.remove();
    assertEquals("offset format(1)", "02.5 meters",
            m1.format(args, 1, result, ignore, errorCode), TRUE);

    // With offset and specific simple format with forced decimals.
    MessageFormat m2(
            "{0,plural,offset:1 one{another meter}other{{0,number,0.0} meters}}",
            Locale::getEnglish(), errorCode);
    args[0] = (int32_t)1;
    result.remove();
    assertEquals("offset-decimals format(1)", "1.0 meters",
            m2.format(args, 1, result, ignore, errorCode), TRUE);

    args[0] = (int32_t)2;
    result.remove();
    assertEquals("offset-decimals format(1)", "2.0 meters",
            m2.format(args, 1, result, ignore, errorCode), TRUE);

    args[0] = (double)2.5;
    result.remove();
    assertEquals("offset-decimals format(1)", "2.5 meters",
            m2.format(args, 1, result, ignore, errorCode), TRUE);
    errorCode.reset();
}

void TestMessageFormat::TestArgIsPrefixOfAnother() {
    IcuTestErrorCode errorCode(*this, "TestArgIsPrefixOfAnother");
    // Ticket #11952
    MessageFormat mf1("{0,select,a{A}ab{AB}abc{ABC}other{?}}", Locale::getEnglish(), errorCode);
    Formattable args[3];
    FieldPosition ignore;
    UnicodeString result;
    args[0].setString("a");
    assertEquals("a", "A", mf1.format(args, 1, result, ignore, errorCode));
    args[0].setString("ab");
    assertEquals("ab", "AB", mf1.format(args, 1, result.remove(), ignore, errorCode));
    args[0].setString("abc");
    assertEquals("abc", "ABC", mf1.format(args, 1, result.remove(), ignore, errorCode));

    // Ticket #12172
    MessageFormat mf2("{a} {aa} {aaa}", Locale::getEnglish(), errorCode);
    UnicodeString argNames[3] = { "a", "aa", "aaa" };
    args[0].setString("A");
    args[1].setString("AB");
    args[2].setString("ABC");
    assertEquals("a aa aaa", "A AB ABC", mf2.format(argNames, args, 3, result.remove(), errorCode));

    // Ticket #12172
    MessageFormat mf3("{aa} {aaa}", Locale::getEnglish(), errorCode);
    assertEquals("aa aaa", "AB ABC", mf3.format(argNames + 1, args + 1, 2, result.remove(), errorCode));
}

void TestMessageFormat::TestMessageFormatNumberSkeleton() {
    IcuTestErrorCode status(*this, "TestMessageFormatNumberSkeleton");

    static const struct TestCase {
        const char16_t* messagePattern;
        const char* localeName;
        double arg;
        const char16_t* expected;
    } cases[] = {
            { u"{0,number,::percent}", "en", 50, u"50%" },
            { u"{0,number,::percent scale/100}", "en", 0.5, u"50%" },
            { u"{0,number,   ::   percent   scale/100   }", "en", 0.5, u"50%" },
            { u"{0,number,::currency/USD}", "en", 23, u"$23.00" },
            { u"{0,number,::precision-integer}", "en", 514.23, u"514" },
            { u"{0,number,::.000}", "en", 514.23, u"514.230" },
            { u"{0,number,::.}", "en", 514.23, u"514" },
            { u"{0,number,::}", "fr", 514.23, u"514,23" },
            { u"Cost: {0,number,::currency/EUR}.", "en", 4.3, u"Cost: €4.30." },
            { u"{0,number,'::'0.00}", "en", 50, u"::50.00" }, // pattern literal
    };

    for (auto& cas : cases) {
        status.setScope(cas.messagePattern);
        MessageFormat msgf(cas.messagePattern, cas.localeName, status);
        UnicodeString sb;
        FieldPosition fpos(FieldPosition::DONT_CARE);
        Formattable argsArray[] = {{cas.arg}};
        Formattable args(argsArray, 1);
        msgf.format(args, sb, status);

        assertEquals(cas.messagePattern, cas.expected, sb);
    }
}

void TestMessageFormat::doTheRealDateTimeSkeletonTesting(UDate testDate,
        const char16_t* messagePattern, const char* localeName, const char16_t* expected,
        IcuTestErrorCode& status) {

    status.setScope(messagePattern);
    MessageFormat msgf(messagePattern, localeName, status);
    UnicodeString sb;
    FieldPosition fpos(FieldPosition::DONT_CARE);
    Formattable argsArray[] = { Formattable(testDate, Formattable::kIsDate) };
    Formattable args(argsArray, 1);
    msgf.format(args, sb, status);

    assertEquals(messagePattern, expected, sb);
}

void TestMessageFormat::TestMessageFormatDateSkeleton() {
    IcuTestErrorCode status(*this, "TestMessageFormatDateSkeleton");

    UDate date = LocaleTest::date(2021-1900, UCAL_NOVEMBER, 23, 16, 42, 55);

    doTheRealDateTimeSkeletonTesting(date, u"{0,date,::MMMMd}", "en", u"November 23", status);
    doTheRealDateTimeSkeletonTesting(date, u"{0,date,::yMMMMdjm}", "en", u"November 23, 2021, 4:42 PM", status);
    doTheRealDateTimeSkeletonTesting(date, u"{0,date,   ::   yMMMMd   }", "en", u"November 23, 2021", status);
    doTheRealDateTimeSkeletonTesting(date, u"{0,date,::yMMMMd}", "fr", u"23 novembre 2021", status);
    doTheRealDateTimeSkeletonTesting(date, u"Expiration: {0,date,::yMMM}!", "en", u"Expiration: Nov 2021!", status);
    // pattern literal
    doTheRealDateTimeSkeletonTesting(date, u"{0,date,'::'yMMMMd}", "en", u"::2021November23", status);
}

void TestMessageFormat::TestMessageFormatTimeSkeleton() {
    IcuTestErrorCode status(*this, "TestMessageFormatTimeSkeleton");

    UDate date = LocaleTest::date(2021-1900, UCAL_NOVEMBER, 23, 16, 42, 55);

    doTheRealDateTimeSkeletonTesting(date, u"{0,time,::MMMMd}", "en", u"November 23", status);
    doTheRealDateTimeSkeletonTesting(date, u"{0,time,::yMMMMdjm}", "en", u"November 23, 2021, 4:42 PM", status);
    doTheRealDateTimeSkeletonTesting(date, u"{0,time,   ::   yMMMMd   }", "en", u"November 23, 2021", status);
    doTheRealDateTimeSkeletonTesting(date, u"{0,time,::yMMMMd}", "fr", u"23 novembre 2021", status);
    doTheRealDateTimeSkeletonTesting(date, u"Expiration: {0,time,::yMMM}!", "en", u"Expiration: Nov 2021!", status);
    // pattern literal
    doTheRealDateTimeSkeletonTesting(date, u"{0,time,'::'yMMMMd}", "en", u"::2021November23", status);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
