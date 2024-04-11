/*
 ***********************************************************************
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html#License
 ***********************************************************************
 ***********************************************************************
 * Copyright (c) 2011-2016,International Business Machines
 * Corporation and others.  All Rights Reserved.
 ***********************************************************************
 */
#include <stdio.h>
#include <string.h>

#include "cmemory.h"
#include "sieve.h"
#include "unicode/utimer.h"
#include "udbgutil.h"
#include "unicode/ustring.h"
#include "unicode/decimfmt.h"
#include "unicode/udat.h"
U_NAMESPACE_USE

#if U_PLATFORM_IMPLEMENTS_POSIX
#include <unistd.h>

static void usage(const char *prog) {
  fprintf(stderr, "Usage: %s [ -f outfile.xml ] [ -t 'TestName' ]\n", prog);
}
#endif

void runTests(void);

#ifndef ITERATIONS
#define ITERATIONS 5
#endif

#ifndef TEST_LOCALE
#define TEST_LOCALE "en_US"
#endif

FILE *out = NULL;
UErrorCode setupStatus = U_ZERO_ERROR;
const char *outName = NULL;
int listmode = 0;
const char *testName = NULL;
const char *progname = NULL;
int errflg = 0;
int testhit = 0;

int testMatch(const char *aName) {
  if(testName==NULL) return 1;
  int len = strlen(testName);
  if(testName[len-1]=='*') {
    return strncmp(testName,aName,len-1);
  } else {
    return strcmp(testName,aName);
  }
}

int main(int argc, char * const * argv){
#if U_DEBUG
  fprintf(stderr,"%s: warning: U_DEBUG is on.\n", argv[0]);
#endif
#if U_DEBUG
  {
    double m;
    double s = uprv_getSieveTime(&m);
    fprintf(stderr, "** Standard sieve time: %.9fs +/- %.9fs (%d iterations)\n", s,m, (int)U_LOTS_OF_TIMES);
  }
#endif

#if U_PLATFORM_IMPLEMENTS_POSIX
  int c;
  //extern int optind;
  extern char *optarg;
  while((c=getopt(argc,argv,"lf:t:")) != EOF) {
    switch(c) {
    case 'f':
      outName = optarg;
      break;
    case 'l':
      listmode++;
      break;
    case 't':
      testName = optarg;
      break;
    case '?':
      errflg++;
    }
    if(errflg) {
      usage(progname);
      return 0;
    }
  }
  /* for ( ; optind < argc; optind++) {     ... argv[optind] } */
#else
  if(argc==2) {
    outName = argv[1];
  } else if(argc>2) {
    fprintf(stderr, "Err: usage: %s [ output-file.xml ]\n", argv[0]);
  }
#endif

    if(listmode && outName != NULL ) {
      fprintf(stderr, "Warning: no output when list mode\n");
      outName=NULL;
    }

  if(outName != NULL) {


    out=fopen(outName,"w");
    if(out==NULL) {
      fprintf(stderr,"Err: can't open %s for writing.\n", outName);
      return 1;
    } else {
      fprintf(stderr, "# writing results to %s\n", outName);
    }
    fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    fprintf(out, "<tests icu=\"%s\">\n", U_ICU_VERSION);
    fprintf(out, "<!-- %s -->\n", U_COPYRIGHT_STRING);
  } else {
    fprintf(stderr, "# (no output)\n");
  }

  if(listmode && testName!=NULL) {
    fprintf(stderr, "ERR: no -l mode when specific test with -t\n");
    usage(progname);
    return 1;
  }


  runTests();


  if(out!=NULL) {
#ifndef SKIP_INFO
    udbg_writeIcuInfo(out);
#endif
    fprintf(out, "</tests>\n");
    fclose(out);
  }

  if(U_FAILURE(setupStatus)) {
    fprintf(stderr, "Error in tests: %s\n", u_errorName(setupStatus));
    return 1;
  }

  return 0;
}

class HowExpensiveTest {
public:
  virtual ~HowExpensiveTest(){}
protected:
  HowExpensiveTest(const char *name, const char *file, int32_t line) : fName(name), fFile(file), fLine(line) {}
protected:
  /**
   * @return number of iterations
   */
  virtual int32_t run() = 0;
  virtual void warmup() {  run(); }
public:
  virtual const char *getName() { return fName; }
public:
  virtual int32_t runTest(double *subTime) {
    UTimer a,b;
    utimer_getTime(&a);
    int32_t iter = run();
    utimer_getTime(&b);
    *subTime = utimer_getDeltaSeconds(&a,&b);
    return iter;
  }

  virtual int32_t runTests(double *subTime, double *marginOfError) {
    warmup(); /* warmup */
    double times[ITERATIONS];
    int subIterations = 0;
    for(int i=0;i<ITERATIONS;i++) {
      subIterations = runTest(&times[i]);
#if U_DEBUG
      fprintf(stderr, "trial: %d/%d = %.9fs\n", i, ITERATIONS,times[i]);
      fflush(stderr);
#endif
    }
    uint32_t iterations = ITERATIONS;
    *subTime = uprv_getMeanTime(times,&iterations,marginOfError);
    return subIterations;
  }
public:
  const char *fName;
  const char *fFile;
  int32_t fLine;
  int32_t fIterations;
};

void runTestOn(HowExpensiveTest &t) {
  if(U_FAILURE(setupStatus)) return; // silently
  const char *tn = t.getName();
  if(testName!=NULL && testMatch(tn)) return; // skipped.
  if(listmode) {
    fprintf(stderr, "%s:%d:\t%s\n", t.fFile, t.fLine, t.getName());
    testhit++;
    return;
  } else {
    fprintf(stderr, "%s:%d: Running: %s\n", t.fFile, t.fLine, t.getName());
    testhit++;
  }
  double sieveTime = uprv_getSieveTime(NULL);
  double st;
  double me;

  fflush(stdout);
  fflush(stderr);
  int32_t iter = t.runTests(&st,&me);
  if(U_FAILURE(setupStatus)) {
    fprintf(stderr, "Error in tests: %s\n", u_errorName(setupStatus));
    return;
  }
  fflush(stdout);
  fflush(stderr);

  double stn = st/sieveTime;

  printf("%s\t%.9f\t%.9f +/- %.9f,  @ %d iter\n", t.getName(),stn,st,me,iter);

  if(out!=NULL) {
    fprintf(out, "   <test name=\"%s\" standardizedTime=\"%f\" realDuration=\"%f\" marginOfError=\"%f\" iterations=\"%d\" />\n",
            tn,stn,st,me,iter);
    fflush(out);
  }
}

/* ------------------- test code here --------------------- */

class SieveTest : public HowExpensiveTest {
public:
  virtual ~SieveTest(){}
  SieveTest():HowExpensiveTest("SieveTest",__FILE__,__LINE__){}
  virtual int32_t run(){return 0;} // dummy
  int32_t runTest(double *subTime) {
    *subTime = uprv_getSieveTime(NULL);
    return U_LOTS_OF_TIMES;
  }
  virtual int32_t runTests(double *subTime, double *marginOfError) {
    *subTime = uprv_getSieveTime(marginOfError);
    return U_LOTS_OF_TIMES;
  }
};


/* ------- NumParseTest ------------- */
#include "unicode/unum.h"
/* open and close tests */
#define OCName(svc,ub,testn,suffix,n) testn ## svc ## ub ## suffix ## n
#define OCStr(svc,ub,suffix,n) "Test_" # svc # ub # suffix # n
#define OCRun(svc,ub,suffix) svc ## ub ## suffix
// TODO: run away screaming
#define OpenCloseTest(n, svc,suffix,c,a,d) class OCName(svc,_,Test_,suffix,n) : public HowExpensiveTest { public: OCName(svc,_,Test_,suffix,n)():HowExpensiveTest(OCStr(svc,_,suffix,n),__FILE__,__LINE__) c int32_t run() { int32_t i; for(i=0;i<U_LOTS_OF_TIMES;i++){ OCRun(svc,_,close) (  OCRun(svc,_,suffix) a );  } return i; }   void warmup() { OCRun(svc,_,close) ( OCRun(svc,_,suffix) a); } virtual ~ OCName(svc,_,Test_,suffix,n) () d };
#define QuickTest(n,c,r,d)  class n : public HowExpensiveTest { public: n():HowExpensiveTest(#n,__FILE__,__LINE__) c int32_t run() r virtual ~n () d };

class NumTest : public HowExpensiveTest {
private:
  double fExpect;
  UNumberFormat *fFmt;
  UnicodeString fPat;
  UnicodeString fString;
  const UChar *fStr;
  int32_t fLen;
  const char *fFile;
  int fLine;
  const char *fCPat;
  const char *fCStr;
  char name[100];
public:
  virtual const char *getName() {
    if(name[0]==0) {
      sprintf(name,"%s:p=|%s|,str=|%s|",getClassName(),fCPat,fCStr);
    }
    return name;
  }
protected:
  virtual UNumberFormat* initFmt() {
    return unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, TEST_LOCALE, 0, &setupStatus);
  }
  virtual const char *getClassName() {
    return "NumTest";
  }
public:
  NumTest(const char *pat, const char *num, double expect, const char *FILE, int LINE)
    : HowExpensiveTest("(n/a)",FILE, LINE),
      fExpect(expect),
      fFmt(0),
      fPat(pat, -1, US_INV),
      fString(num,-1,US_INV),
      fStr(fString.getTerminatedBuffer()),
      fLen(u_strlen(fStr)),
      fFile(FILE),
      fLine(LINE),
      fCPat(pat),
      fCStr(num)
  {
    name[0]=0;
  }
  void warmup() {
    fFmt = initFmt();
    if(U_SUCCESS(setupStatus)) {
      double trial = unum_parseDouble(fFmt,fStr,fLen, NULL, &setupStatus);
      if(U_SUCCESS(setupStatus) && trial!=fExpect) {
        setupStatus = U_INTERNAL_PROGRAM_ERROR;
        printf("%s:%d: warmup() %s got %.8f expected %.8f\n",
               fFile,fLine,getName(),trial,fExpect);
      }
    }
  }
  int32_t run() {
    double trial=0.0;
    int i;
    for(i=0;i<U_LOTS_OF_TIMES;i++){
      trial = unum_parse(fFmt,fStr,fLen, NULL, &setupStatus);
    }
    return i;
  }
  virtual ~NumTest(){}
};

#define DO_NumTest(p,n,x) { NumTest t(p,n,x,__FILE__,__LINE__); runTestOn(t); }


class AttrNumTest : public NumTest
{
private:
  UNumberFormatAttribute fAttr;
  int32_t fAttrValue;
  char name2[100];
protected:
  virtual const char *getClassName() {
    sprintf(name2,"AttrNumTest:%d=%d", fAttr,fAttrValue);
    return name2;
  }
public:
  AttrNumTest(const char *pat, const char *num, double expect, const char *FILE, int LINE, UNumberFormatAttribute attr, int32_t newValue)
    : NumTest(pat,num,expect,FILE,LINE),
      fAttr(attr),
      fAttrValue(newValue)
  {
  }
  virtual UNumberFormat* initFmt() {
    UNumberFormat *fmt = NumTest::initFmt();
    unum_setAttribute(fmt, fAttr,fAttrValue);
    return fmt;
  }
};

#define DO_AttrNumTest(p,n,x,a,v) { AttrNumTest t(p,n,x,__FILE__,__LINE__,a,v); runTestOn(t); }


class NOXNumTest : public NumTest
{
private:
  UNumberFormatAttribute fAttr;
  int32_t fAttrValue;
  char name2[100];
protected:
  virtual const char *getClassName() {
    sprintf(name2,"NOXNumTest:%d=%d", fAttr,fAttrValue);
    return name2;
  }
public:
  NOXNumTest(const char *pat, const char *num, double expect, const char *FILE, int LINE /*, UNumberFormatAttribute attr, int32_t newValue */)
    : NumTest(pat,num,expect,FILE,LINE) /* ,
      fAttr(attr),
      fAttrValue(newValue) */
  {
  }
  virtual UNumberFormat* initFmt() {
    UNumberFormat *fmt = NumTest::initFmt();
    //unum_setAttribute(fmt, fAttr,fAttrValue);
    return fmt;
  }
};

#define DO_NOXNumTest(p,n,x) { NOXNumTest t(p,n,x,__FILE__,__LINE__); runTestOn(t); }

#define DO_TripleNumTest(p,n,x) DO_AttrNumTest(p,n,x,UNUM_PARSE_ALL_INPUT,UNUM_YES) \
                                DO_AttrNumTest(p,n,x,UNUM_PARSE_ALL_INPUT,UNUM_NO) \
                                DO_AttrNumTest(p,n,x,UNUM_PARSE_ALL_INPUT,UNUM_MAYBE)


class NumFmtTest : public HowExpensiveTest {
private:
  double fExpect;
  UNumberFormat *fFmt;
  UnicodeString fPat;
  UnicodeString fString;
  const UChar *fStr;
  int32_t fLen;
  const char *fFile;
  int fLine;
  const char *fCPat;
  const char *fCStr;
  char name[100];
public:
  virtual const char *getName() {
    if(name[0]==0) {
      sprintf(name,"%s:p=|%s|,str=|%s|",getClassName(),fCPat,fCStr);
    }
    return name;
  }
protected:
  virtual UNumberFormat* initFmt() {
    return unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, TEST_LOCALE, 0, &setupStatus);
  }
  virtual const char *getClassName() {
    return "NumFmtTest";
  }
public:
  NumFmtTest(const char *pat, const char *num, double expect, const char *FILE, int LINE)
    : HowExpensiveTest("(n/a)",FILE, LINE),
      fExpect(expect),
      fFmt(0),
      fPat(pat, -1, US_INV),
      fString(num,-1,US_INV),
      fStr(fString.getTerminatedBuffer()),
      fLen(u_strlen(fStr)),
      fFile(FILE),
      fLine(LINE),
      fCPat(pat),
      fCStr(num)
  {
    name[0]=0;
  }
  void warmup() {
    fFmt = initFmt();
    UChar buf[100];
    if(U_SUCCESS(setupStatus)) {
      int32_t trial = unum_formatDouble(fFmt,fExpect, buf, 100, NULL, &setupStatus);
      if(!U_SUCCESS(setupStatus)
         || trial!=fLen
         ||trial<=0
         || u_strncmp(fStr,buf,trial)  ) {
        char strBuf[200];
        u_strToUTF8(strBuf,200,NULL,buf,trial+1,&setupStatus);
        printf("%s:%d: warmup() %s got %s expected %s, err %s\n",
               fFile,fLine,getName(),strBuf,fCStr, u_errorName(setupStatus));
        setupStatus = U_INTERNAL_PROGRAM_ERROR;
      }
    }
  }
  int32_t run() {
    int32_t trial;
    int i;
    UChar buf[100];
    if(U_SUCCESS(setupStatus)) {
      for(i=0;i<U_LOTS_OF_TIMES;i++){
        trial = unum_formatDouble(fFmt,fExpect, buf, 100, NULL, &setupStatus);
      }
    }
    return i;
  }
  virtual ~NumFmtTest(){}
};

#define DO_NumFmtTest(p,n,x) { NumFmtTest t(p,n,x,__FILE__,__LINE__); runTestOn(t); }

class NumFmtInt64Test : public HowExpensiveTest {
public:
  enum EMode {
    kDefault,
    kPattern,
    kApplyPattern,
    kGroupOff,
    kApplyGroupOff
  };
private:
  EMode   fMode;
  int64_t fExpect;
  UNumberFormat *fFmt;
  UnicodeString fPat;
  UnicodeString fString;
  const UChar *fStr;
  int32_t fLen;
  const char *fFile;
  int fLine;
  const char *fCPat;
  const char *fCStr;
  char name[100];
public:
  virtual const char *getName() {
    if(name[0]==0) {
      sprintf(name,"%s:p=|%s|,str=|%s|",getClassName(),fCPat,fCStr);
    }
    return name;
  }
protected:
  virtual UNumberFormat* initFmt() {
    switch(fMode) {
    case kPattern:
      return unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, TEST_LOCALE, 0, &setupStatus);
    case kApplyPattern:
      {
        UNumberFormat *fmt = unum_open(UNUM_DECIMAL, NULL, -1, TEST_LOCALE, 0, &setupStatus);
        unum_applyPattern(fmt, FALSE, fPat.getTerminatedBuffer(), -1, NULL, &setupStatus);
        return fmt;
      }
    case kGroupOff:
      {
        UNumberFormat *fmt = unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, TEST_LOCALE, 0, &setupStatus);
        unum_setAttribute(fmt, UNUM_GROUPING_USED, UNUM_NO);
        return fmt;
      }
    case kApplyGroupOff:
      {
        UNumberFormat *fmt = unum_open(UNUM_DECIMAL, NULL, -1, TEST_LOCALE, 0, &setupStatus);
        unum_applyPattern(fmt, FALSE, fPat.getTerminatedBuffer(), -1, NULL, &setupStatus);
        unum_setAttribute(fmt, UNUM_GROUPING_USED, UNUM_NO);
        return fmt;
      }
    default:
    case kDefault:
      return unum_open(UNUM_DEFAULT, NULL, -1, TEST_LOCALE, 0, &setupStatus);
    }
  }
  virtual const char *getClassName() {
    switch(fMode) {
    case EMode::kDefault:
      return "NumFmtInt64Test (default)";
    case EMode::kPattern:
      return "NumFmtInt64Test (pattern)";
    case EMode::kApplyPattern:
      return "NumFmtInt64Test (applypattern)";
    case EMode::kGroupOff:
      return "NumFmtInt64Test (pattern, group=off)";
    case EMode::kApplyGroupOff:
      return "NumFmtInt64Test (applypattern, group=off)";
    default:
      return "NumFmtInt64Test (? ? ?)";
    }
  }
public:
  NumFmtInt64Test(const char *pat, const char *num, int64_t expect, const char *FILE, int LINE, EMode mode)
    : HowExpensiveTest("(n/a)",FILE, LINE),
      fMode(mode),
      fExpect(expect),
      fFmt(0),
      fPat(pat, -1, US_INV),
      fString(num,-1,US_INV),
      fStr(fString.getTerminatedBuffer()),
      fLen(u_strlen(fStr)),
      fFile(FILE),
      fLine(LINE),
      fCPat(pat),
      fCStr(num)
  {
    name[0]=0;
  }
  void warmup() {
    fFmt = initFmt();
    UChar buf[100];
    if(U_SUCCESS(setupStatus)) {
      int32_t trial = unum_formatInt64(fFmt,fExpect, buf, 100, NULL, &setupStatus);
      if(!U_SUCCESS(setupStatus)
         || trial!=fLen
         ||trial<=0
         || u_strncmp(fStr,buf,trial)  ) {
        char strBuf[200];
        u_strToUTF8(strBuf,200,NULL,buf,trial+1,&setupStatus);
        printf("%s:%d: warmup() %s got %s (len %d) expected %s (len %d), err %s\n",
               fFile,fLine,getName(),strBuf,trial,fCStr,fLen, u_errorName(setupStatus));
        setupStatus = U_INTERNAL_PROGRAM_ERROR;
      }
    }
  }
  int32_t run() {
    int32_t trial;
    int i;
    UChar buf[100];
    if(U_SUCCESS(setupStatus)) {
      for(i=0;i<U_LOTS_OF_TIMES;i++){
        trial = unum_formatInt64(fFmt,fExpect, buf, 100, NULL, &setupStatus);
      }
    }
    return i;
  }
  virtual ~NumFmtInt64Test(){}
};

/**
 * unum_open .. with pattern, == new DecimalFormat(pattern)
 */
#define DO_NumFmtInt64Test(p,n,x) { NumFmtInt64Test t(p,n,x,__FILE__,__LINE__,NumFmtInt64Test::EMode::kPattern); runTestOn(t); }
/**
 * unum_open(UNUM_DECIMAL), then 
 */
#define DO_NumFmtInt64Test_apply(p,n,x) { NumFmtInt64Test t(p,n,x,__FILE__,__LINE__,NumFmtInt64Test::EMode::kApplyPattern); runTestOn(t); }

#define DO_NumFmtInt64Test_default(p,n,x) { NumFmtInt64Test t(p,n,x,__FILE__,__LINE__,NumFmtInt64Test::EMode::kDefault); runTestOn(t); }
#define DO_NumFmtInt64Test_gr0(p,n,x) { NumFmtInt64Test t(p,n,x,__FILE__,__LINE__,NumFmtInt64Test::EMode::kGroupOff); runTestOn(t); }
#define DO_NumFmtInt64Test_applygr0(p,n,x) { NumFmtInt64Test t(p,n,x,__FILE__,__LINE__,NumFmtInt64Test::EMode::kApplyGroupOff); runTestOn(t); }


class NumFmtStringPieceTest : public HowExpensiveTest {
private:
  const StringPiece &fExpect;
  UNumberFormat *fFmt;
  UnicodeString fPat;
  UnicodeString fString;
  const UChar *fStr;
  int32_t fLen;
  const char *fFile;
  int fLine;
  const char *fCPat;
  const char *fCStr;
  char name[100];
public:
  virtual const char *getName() {
    if(name[0]==0) {
      sprintf(name,"%s:p=|%s|,str=|%s|,sp=|%s|",getClassName(),fCPat,fCStr, fExpect.data());
    }
    return name;
  }
protected:
  virtual UNumberFormat* initFmt() {
    DecimalFormat *d = new DecimalFormat(setupStatus);
    UParseError pe;
    d->applyPattern(fPat, pe, setupStatus);
    return (UNumberFormat*) d;
  }
  virtual const char *getClassName() {
    return "NumFmtStringPieceTest";
  }
public:
  NumFmtStringPieceTest(const char *pat, const char *num, const StringPiece& expect, const char *FILE, int LINE)
    : HowExpensiveTest("(n/a)",FILE, LINE),
      fExpect(expect),
      fFmt(0),
      fPat(pat, -1, US_INV),
      fString(num,-1,US_INV),
      fStr(fString.getTerminatedBuffer()),
      fLen(u_strlen(fStr)),
      fFile(FILE),
      fLine(LINE),
      fCPat(pat),
      fCStr(num)
  {
    name[0]=0;
  }
  void warmup() {
    fFmt = initFmt();
    UnicodeString buf;
    if(U_SUCCESS(setupStatus)) {
      buf.remove();
      ((const DecimalFormat*)fFmt)->format(fExpect, buf, NULL, setupStatus);
      if(!U_SUCCESS(setupStatus)
         || fString!=buf
         ) {
        char strBuf[200];
        u_strToUTF8(strBuf,200,NULL,buf.getTerminatedBuffer(),buf.length()+1,&setupStatus);
        printf("%s:%d: warmup() %s got %s (len %d) expected %s (len %d), err %s\n",
               fFile,fLine,getName(),strBuf,buf.length(),fCStr,fLen, u_errorName(setupStatus));
        setupStatus = U_INTERNAL_PROGRAM_ERROR;
      }
    }
  }

  int32_t run() {
#if U_DEBUG
    int32_t trial;
#endif
    int i=0;
    UnicodeString buf;
    if(U_SUCCESS(setupStatus)) {
      for(i=0;i<U_LOTS_OF_TIMES;i++){
        buf.remove();
        ((const DecimalFormat*)fFmt)->format(fExpect, buf, NULL, setupStatus);
      }
    }
    return i;
  }
  virtual ~NumFmtStringPieceTest(){}
};

#define DO_NumFmtStringPieceTest(p,n,x) { NumFmtStringPieceTest t(p,n,x,__FILE__,__LINE__); runTestOn(t); }

// TODO: move, scope.
static UChar pattern[] = { 0x23 }; // '#'
static UChar strdot[] = { '2', '.', '0', 0 };
static UChar strspc[] = { '2', ' ', 0 };
static UChar strgrp[] = {'2',',','2','2','2', 0 };
static UChar strbeng[] = {0x09E8,0x09E8,0x09E8,0x09E8, 0 };

UNumberFormat *NumParseTest_fmt;

// TODO: de-uglify.
QuickTest(NumParseTest,{    static UChar pattern[] = { 0x23 };    NumParseTest_fmt = unum_open(UNUM_PATTERN_DECIMAL,         pattern,                    1,                    TEST_LOCALE,                    0,                    &setupStatus);  },{    int32_t i;    static UChar str[] = { 0x31 };double val;    for(i=0;i<U_LOTS_OF_TIMES;i++) {      val=unum_parse(NumParseTest_fmt,str,1,NULL,&setupStatus);    }    return i;  },{unum_close(NumParseTest_fmt);})

QuickTest(NumParseTestdot,{    static UChar pattern[] = { 0x23 };    NumParseTest_fmt = unum_open(UNUM_PATTERN_DECIMAL,         pattern,                    1,                    TEST_LOCALE,                    0,                    &setupStatus);  },{    int32_t i;  double val;    for(i=0;i<U_LOTS_OF_TIMES;i++) {      val=unum_parse(NumParseTest_fmt,strdot,1,NULL,&setupStatus);    }    return i;  },{unum_close(NumParseTest_fmt);})
QuickTest(NumParseTestspc,{    static UChar pattern[] = { 0x23 };    NumParseTest_fmt = unum_open(UNUM_PATTERN_DECIMAL,         pattern,                    1,                    TEST_LOCALE,                    0,                    &setupStatus);  },{    int32_t i;    double val;    for(i=0;i<U_LOTS_OF_TIMES;i++) {      val=unum_parse(NumParseTest_fmt,strspc,1,NULL,&setupStatus);    }    return i;  },{unum_close(NumParseTest_fmt);})
QuickTest(NumParseTestgrp,{    static UChar pattern[] = { 0x23 };    NumParseTest_fmt = unum_open(UNUM_PATTERN_DECIMAL,         pattern,                    1,                    TEST_LOCALE,                    0,                    &setupStatus);  },{    int32_t i;    double val;    for(i=0;i<U_LOTS_OF_TIMES;i++) {      val=unum_parse(NumParseTest_fmt,strgrp,-1,NULL,&setupStatus);    }    return i;  },{unum_close(NumParseTest_fmt);})

QuickTest(NumParseTestbeng,{    static UChar pattern[] = { 0x23 };    NumParseTest_fmt = unum_open(UNUM_PATTERN_DECIMAL,         pattern,                    1,                    TEST_LOCALE,                    0,                    &setupStatus);  },{    int32_t i;    double val;    for(i=0;i<U_LOTS_OF_TIMES;i++) {      val=unum_parse(NumParseTest_fmt,strbeng,-1,NULL,&setupStatus);    }    return i;  },{unum_close(NumParseTest_fmt);})

UDateFormat *DateFormatTest_fmt = NULL;
UDate sometime = 100000000.0;
UChar onekbuf[1024];
const int32_t onekbuf_len = UPRV_LENGTHOF(onekbuf);


QuickTest(DateFormatTestBasic, \
          { \
            DateFormatTest_fmt = udat_open(UDAT_DEFAULT, UDAT_DEFAULT, NULL, NULL, -1, NULL, -1, &setupStatus); \
          }, \
          { \
            int i; \
            for(i=0;i<U_LOTS_OF_TIMES;i++)  \
            { \
              udat_format(DateFormatTest_fmt, sometime, onekbuf, onekbuf_len, NULL, &setupStatus); \
            } \
            return i; \
          }, \
          { \
            udat_close(DateFormatTest_fmt); \
          } \
      )


QuickTest(NullTest,{},{int j=U_LOTS_OF_TIMES;while(--j);return U_LOTS_OF_TIMES;},{})

#if 0
#include <time.h>

QuickTest(RandomTest,{},{timespec ts; ts.tv_sec=rand()%4; int j=U_LOTS_OF_TIMES;while(--j) { ts.tv_nsec=100000+(rand()%10000)*1000000; nanosleep(&ts,NULL); return j;} return U_LOTS_OF_TIMES;},{})
#endif

OpenCloseTest(pattern,unum,open,{},(UNUM_PATTERN_DECIMAL,pattern,1,TEST_LOCALE,0,&setupStatus),{})
OpenCloseTest(default,unum,open,{},(UNUM_DEFAULT,NULL,-1,TEST_LOCALE,0,&setupStatus),{})
#if !UCONFIG_NO_CONVERSION
#include "unicode/ucnv.h"
OpenCloseTest(gb18030,ucnv,open,{},("gb18030",&setupStatus),{})
#endif
#include "unicode/ures.h"
OpenCloseTest(root,ures,open,{},(NULL,"root",&setupStatus),{})

void runTests() {
  {
    SieveTest t;
    runTestOn(t);
  }
#if 0
  {
    RandomTest t;
    runTestOn(t);
  }
#endif
  {
    NullTest t;
    runTestOn(t);
  }

#ifndef SKIP_DATEFMT_TESTS
  {
    DateFormatTestBasic t;
    runTestOn(t);
  }
#endif

#ifndef SKIP_NUMPARSE_TESTS
  {
    // parse tests

    DO_NumTest("#","0",0.0);
    DO_NumTest("#","2.0",2.0);
    DO_NumTest("#","2 ",2);
    DO_NumTest("#","-2 ",-2);
    DO_NumTest("+#","+2",2);
    DO_NumTest("#,###.0","2222.0",2222.0);
    DO_NumTest("#.0","1.000000000000000000000000000000000000000000000000000000000000000000000000000000",1.0);
    DO_NumTest("#","123456",123456);

    // attr
#ifdef HAVE_UNUM_MAYBE
    DO_AttrNumTest("#","0",0.0,UNUM_PARSE_ALL_INPUT,UNUM_YES);
    DO_AttrNumTest("#","0",0.0,UNUM_PARSE_ALL_INPUT,UNUM_NO);
    DO_AttrNumTest("#","0",0.0,UNUM_PARSE_ALL_INPUT,UNUM_MAYBE);
    DO_TripleNumTest("#","2.0",2.0);
    DO_AttrNumTest("#.0","1.000000000000000000000000000000000000000000000000000000000000000000000000000000",1.0,UNUM_PARSE_ALL_INPUT,UNUM_NO);
#endif


    //  {    NumParseTestgrp t;    runTestOn(t);  }
    {    NumParseTestbeng t;    runTestOn(t);  }

  }
#endif

#ifndef SKIP_NUMFORMAT_TESTS
  // format tests
  {

    DO_NumFmtInt64Test("0000","0001",1);
    DO_NumFmtInt64Test("0000","0000",0);
    StringPiece sp3456("3456");
    DO_NumFmtStringPieceTest("0000","3456",sp3456);
    DO_NumFmtStringPieceTest("#","3456",sp3456);
    StringPiece sp3("3");
    DO_NumFmtStringPieceTest("0000","0003",sp3);
    DO_NumFmtStringPieceTest("#","3",sp3);
    StringPiece spn3("-3");
    DO_NumFmtStringPieceTest("0000","-0003",spn3);
    DO_NumFmtStringPieceTest("#","-3",spn3);
    StringPiece spPI("123.456");
    DO_NumFmtStringPieceTest("#.0000","123.4560",spPI);
    DO_NumFmtStringPieceTest("#.00","123.46",spPI);

    DO_NumFmtTest("#","0",0.0);
    DO_NumFmtTest("#","12345",12345);
    DO_NumFmtTest("#","-2",-2);
    DO_NumFmtTest("+#","+2",2);

    DO_NumFmtInt64Test("#","-682",-682);
    DO_NumFmtInt64Test("#","0",0);
    DO_NumFmtInt64Test("#","12345",12345);
    DO_NumFmtInt64Test("#,###","12,345",12345);
    DO_NumFmtInt64Test("#","1234",1234);
    DO_NumFmtInt64Test("#","123",123);
    DO_NumFmtInt64Test("#,###","123",123);
    DO_NumFmtInt64Test_apply("#","123",123);
    DO_NumFmtInt64Test_apply("#","12345",12345);
    DO_NumFmtInt64Test_apply("#,###","123",123);
    DO_NumFmtInt64Test_apply("#,###","12,345",12345);
    DO_NumFmtInt64Test_default("","123",123);
    DO_NumFmtInt64Test_default("","12,345",12345);
    DO_NumFmtInt64Test_applygr0("#","123",123);
    DO_NumFmtInt64Test_applygr0("#","12345",12345);
    DO_NumFmtInt64Test_applygr0("#,###","123",123);
    DO_NumFmtInt64Test_applygr0("#,###","12345",12345);
    DO_NumFmtInt64Test_gr0("#","123",123);
    DO_NumFmtInt64Test_gr0("#","12345",12345);
    DO_NumFmtInt64Test_gr0("#,###","123",123);
    DO_NumFmtInt64Test_gr0("#,###","12345",12345);
    DO_NumFmtInt64Test("#","-2",-2);
    DO_NumFmtInt64Test("+#","+2",2);
  }

#ifndef SKIP_NUM_OPEN_TEST
  {
    Test_unum_opendefault t;
    runTestOn(t);
  }
  {
    Test_unum_openpattern t;
    runTestOn(t);
  }
#endif

#endif /* skip numformat tests */
#if !UCONFIG_NO_CONVERSION
  {
    Test_ucnv_opengb18030 t;
    runTestOn(t);
  }
#endif
  {
    Test_ures_openroot t;
    runTestOn(t);
  }

  if(testhit==0) {
    fprintf(stderr, "ERROR: no tests matched.\n");
  }
}
