/*
 **********************************************************************
 * Copyright (c) 2011-2012,International Business Machines
 * Corporation and others.  All Rights Reserved.
 **********************************************************************
 */
#include <stdio.h>
#include "sieve.h"
#include "unicode/utimer.h"
#include "udbgutil.h"
#include "unicode/decimfmt.h"
void runTests(void);

FILE *out = NULL;
UErrorCode setupStatus = U_ZERO_ERROR;

int main(int argc, const char* argv[]){
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

  if(argc==2) {
    out=fopen(argv[1],"w");
    if(out==NULL) {
      fprintf(stderr,"Err: can't open %s for writing.\n", argv[1]);
      return 1;
    }
    fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n");
    fprintf(out, "<tests icu=\"%s\">\n", U_ICU_VERSION);
    fprintf(out, "<!-- %s -->\n", U_COPYRIGHT_STRING);
  } else if(argc>2) {
    fprintf(stderr, "Err: usage: %s [ output-file.xml ]\n", argv[0]);
    return 1;
  }

  runTests();
  

  if(out!=NULL) {
    udbg_writeIcuInfo(out);
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
    #define ITERATIONS 5
    double times[ITERATIONS];
    int subIterations = 0;
    for(int i=0;i<ITERATIONS;i++) {
      subIterations = runTest(&times[i]);
#if U_DEBUG
      fprintf(stderr, "trial: %d/%d = %.9fs\n", i, ITERATIONS,times[i]);
      fflush(stderr);
#endif
    }
    *subTime = uprv_getMeanTime(times,ITERATIONS,marginOfError);
    return subIterations;
  }
public:
  const char *fName;
  const char *fFile;
  int32_t fLine;
  int32_t fIterations;
};

void runTestOn(HowExpensiveTest &t) {
  fprintf(stderr, "%s:%d: Running: %s\n", t.fFile, t.fLine, t.fName);
  double sieveTime = uprv_getSieveTime(NULL);
  double st;
  double me;
  
  fflush(stdout);
  fflush(stderr);
  int32_t iter = t.runTests(&st,&me);
  fflush(stdout);
  fflush(stderr);
  
  double stn = st/sieveTime;

  printf("%s\t%.9f\t%.9f +/- %.9f,  @ %d iter\n", t.fName,stn,st,me,iter);

  if(out!=NULL) {
    fprintf(out, "   <test name=\"%s\" standardizedTime=\"%f\" realDuration=\"%f\" marginOfError=\"%f\" iterations=\"%d\" />\n",
            t.fName,stn,st,me,iter);
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
    return unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, "en_US", 0, &setupStatus);
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
    return unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, "en_US", 0, &setupStatus);
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
private:
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
    return unum_open(UNUM_PATTERN_DECIMAL, fPat.getTerminatedBuffer(), -1, "en_US", 0, &setupStatus);
  }
  virtual const char *getClassName() {
    return "NumFmtInt64Test";
  }
public:
  NumFmtInt64Test(const char *pat, const char *num, int64_t expect, const char *FILE, int LINE) 
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

#define DO_NumFmtInt64Test(p,n,x) { NumFmtInt64Test t(p,n,x,__FILE__,__LINE__); runTestOn(t); }


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
    int32_t trial;
    int i;
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

UNumberFormat *NumParseTest_fmt;

// TODO: de-uglify.
QuickTest(NumParseTest,{    static UChar pattern[] = { 0x23 };    NumParseTest_fmt = unum_open(UNUM_PATTERN_DECIMAL,         pattern,                    1,                    "en_US",                    0,                    &setupStatus);  },{    int32_t i;    static UChar str[] = { 0x31 };double val;    for(i=0;i<U_LOTS_OF_TIMES;i++) {      val=unum_parse(NumParseTest_fmt,str,1,NULL,&setupStatus);    }    return i;  },{unum_close(NumParseTest_fmt);})


QuickTest(NullTest,{},{int j=U_LOTS_OF_TIMES;while(--j);return U_LOTS_OF_TIMES;},{})
OpenCloseTest(pattern,unum,open,{},(UNUM_PATTERN_DECIMAL,pattern,1,"en_US",0,&setupStatus),{})
OpenCloseTest(default,unum,open,{},(UNUM_DEFAULT,NULL,-1,"en_US",0,&setupStatus),{})
#include "unicode/ucnv.h"
OpenCloseTest(gb18030,ucnv,open,{},("gb18030",&setupStatus),{})
#include "unicode/ures.h"
OpenCloseTest(root,ures,open,{},(NULL,"root",&setupStatus),{})

void runTests() {
  {
    SieveTest t;
    runTestOn(t);
  }
  {
    NullTest t;
    runTestOn(t);
  }
  {
    NumParseTest t;
    runTestOn(t);
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
    
#if 1
    DO_NumFmtTest("#","0",0.0);
    DO_NumFmtTest("#","12345",12345);
    DO_NumFmtTest("#","-2",-2);
    DO_NumFmtTest("+#","+2",2);
    DO_NumFmtInt64Test("#","-682",-682);
    DO_NumFmtInt64Test("#","0",0);
    DO_NumFmtInt64Test("#","12345",12345);
    DO_NumFmtInt64Test("#","1234",1234);
    DO_NumFmtInt64Test("#","123",123);
    DO_NumFmtInt64Test("#","-2",-2);
    DO_NumFmtInt64Test("+#","+2",2);
  }
#endif

  {
    Test_unum_opendefault t;
    runTestOn(t);
  }
  {
    Test_ucnv_opengb18030 t;
    runTestOn(t);
  }
  {
    Test_unum_openpattern t;
    runTestOn(t);
  }
  {
    Test_ures_openroot t;
    runTestOn(t);
  }
}
