// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2009, International Business Machines Corporation and
 * others. All Rights Reserved. 
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#ifndef _COLL
#include "unicode/coll.h"
#endif

#ifndef _TBLCOLL
#include "unicode/tblcoll.h"
#endif

#ifndef _UNISTR
#include "unicode/unistr.h"
#endif

#ifndef _SORTKEY
#include "unicode/sortkey.h"
#endif

#include "lcukocol.h"

#include "sfwdchit.h"

LotusCollationKoreanTest::LotusCollationKoreanTest()
: myCollation(nullptr)
{
    UErrorCode status = U_ZERO_ERROR;
    myCollation = Collator::createInstance("ko_kr", status);
    if(U_SUCCESS(status)) {
      myCollation->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);
    } else {
      errcheckln(status, "Couldn't instantiate the collator with %s", u_errorName(status));
      delete myCollation;
      myCollation = nullptr;
    }

}

LotusCollationKoreanTest::~LotusCollationKoreanTest()
{
    delete myCollation;
}

const char16_t LotusCollationKoreanTest::testSourceCases[][LotusCollationKoreanTest::MAX_TOKEN_LEN] = {
    {0xac00, 0}
    
};

const char16_t LotusCollationKoreanTest::testTargetCases[][LotusCollationKoreanTest::MAX_TOKEN_LEN] = {
    {0xac01, 0}
};

const Collator::EComparisonResult LotusCollationKoreanTest::results[] = {
    Collator::LESS
};

void LotusCollationKoreanTest::TestTertiary(/* char* par */)
{
    int32_t i = 0;
    myCollation->setStrength(Collator::TERTIARY);

    for (i = 0; i < 1; i++) {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }
}

void LotusCollationKoreanTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite LotusCollationKoreanTest: ");
    if(myCollation) {
      switch (index) {
          case 0: name = "TestTertiary";  if (exec)   TestTertiary(/* par */); break;
          default: name = ""; break;
      }
    } else {
      dataerrln("Class collator not instantiated");
      name = "";
    }
}

#endif /* #if !UCONFIG_NO_COLLATION */
