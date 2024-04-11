// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2002-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

/* Created by weiv 05/09/2002 */

#include <stdarg.h>

#include "unicode/tstdtmod.h"
#include "cmemory.h"
#include <stdio.h>
#include "cstr.h"
#include "cstring.h"

TestLog::~TestLog() {}

IcuTestErrorCode::~IcuTestErrorCode() {
    // Safe because our errlog() does not throw exceptions.
    if(isFailure()) {
        errlog(false, u"destructor: expected success", nullptr);
    }
}

UBool IcuTestErrorCode::errIfFailureAndReset() {
    if(isFailure()) {
        errlog(false, u"expected success", nullptr);
        reset();
        return true;
    } else {
        reset();
        return false;
    }
}

UBool IcuTestErrorCode::errIfFailureAndReset(const char *fmt, ...) {
    if(isFailure()) {
        char buffer[4000];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, ap);
        va_end(ap);
        errlog(false, u"expected success", buffer);
        reset();
        return true;
    } else {
        reset();
        return false;
    }
}

UBool IcuTestErrorCode::errDataIfFailureAndReset() {
    if(isFailure()) {
        errlog(true, u"data: expected success", nullptr);
        reset();
        return true;
    } else {
        reset();
        return false;
    }
}

UBool IcuTestErrorCode::errDataIfFailureAndReset(const char *fmt, ...) {
    if(isFailure()) {
        char buffer[4000];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, ap);
        va_end(ap);
        errlog(true, u"data: expected success", buffer);
        reset();
        return true;
    } else {
        reset();
        return false;
    }
}

UBool IcuTestErrorCode::expectErrorAndReset(UErrorCode expectedError) {
    if(get() != expectedError) {
        errlog(false, UnicodeString(u"expected: ") + u_errorName(expectedError), nullptr);
    }
    UBool retval = isFailure();
    reset();
    return retval;
}

UBool IcuTestErrorCode::expectErrorAndReset(UErrorCode expectedError, const char *fmt, ...) {
    if(get() != expectedError) {
        char buffer[4000];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buffer, sizeof(buffer), fmt, ap);
        va_end(ap);
        errlog(false, UnicodeString(u"expected: ") + u_errorName(expectedError), buffer);
    }
    UBool retval = isFailure();
    reset();
    return retval;
}

void IcuTestErrorCode::setScope(const char* message) {
    scopeMessage.remove().append({ message, -1, US_INV });
}

void IcuTestErrorCode::setScope(const UnicodeString& message) {
    scopeMessage = message;
}

void IcuTestErrorCode::handleFailure() const {
    errlog(false, u"(handleFailure)", nullptr);
}

void IcuTestErrorCode::errlog(UBool dataErr, const UnicodeString& mainMessage, const char* extraMessage) const {
    UnicodeString msg(testName, -1, US_INV);
    msg.append(u' ').append(mainMessage);
    msg.append(u" but got error: ").append(UnicodeString(errorName(), -1, US_INV));

    if (!scopeMessage.isEmpty()) {
        msg.append(u" scope: ").append(scopeMessage);
    }

    if (extraMessage != nullptr) {
        msg.append(u" - ").append(UnicodeString(extraMessage, -1, US_INV));
    }

    if (dataErr || get() == U_MISSING_RESOURCE_ERROR || get() == U_FILE_ACCESS_ERROR) {
        testClass.dataerrln(msg);
    } else {
        testClass.errln(msg);
    }
}

TestDataModule *TestDataModule::getTestDataModule(const char* name, TestLog& log, UErrorCode &status)
{
  if(U_FAILURE(status)) {
    return nullptr;
  }
  TestDataModule *result = nullptr;

  // TODO: probe for resource bundle and then for XML.
  // According to that, construct an appropriate driver object

  result = new RBTestDataModule(name, log, status);
  if(U_SUCCESS(status)) {
    return result;
  } else {
    delete result;
    return nullptr;
  }
}

TestDataModule::TestDataModule(const char* name, TestLog& log, UErrorCode& /*status*/)
: testName(name),
fInfo(nullptr),
fLog(log)
{
}

TestDataModule::~TestDataModule() {
  delete fInfo;
}

const char * TestDataModule::getName() const
{
  return testName;
}



RBTestDataModule::~RBTestDataModule()
{
  ures_close(fTestData);
  ures_close(fModuleBundle);
  ures_close(fInfoRB);
  uprv_free(tdpath);
}

RBTestDataModule::RBTestDataModule(const char* name, TestLog& log, UErrorCode& status) 
: TestDataModule(name, log, status),
  fModuleBundle(nullptr),
  fTestData(nullptr),
  fInfoRB(nullptr),
  tdpath(nullptr)
{
  fNumberOfTests = 0;
  fDataTestValid = true;
  fModuleBundle = getTestBundle(name, status);
  if(fDataTestValid) {
    fTestData = ures_getByKey(fModuleBundle, "TestData", nullptr, &status);
    fNumberOfTests = ures_getSize(fTestData);
    fInfoRB = ures_getByKey(fModuleBundle, "Info", nullptr, &status);
    if(status != U_ZERO_ERROR) {
      log.errln(UNICODE_STRING_SIMPLE("Unable to initialize test data - missing mandatory description resources!"));
      fDataTestValid = false;
    } else {
      fInfo = new RBDataMap(fInfoRB, status);
    }
  }
}

UBool RBTestDataModule::getInfo(const DataMap *& info, UErrorCode &/*status*/) const
{
    info = fInfo;
    if(fInfo) {
        return true;
    } else {
        return false;
    }
}

TestData* RBTestDataModule::createTestData(int32_t index, UErrorCode &status) const 
{
  TestData *result = nullptr;
  UErrorCode intStatus = U_ZERO_ERROR;

  if(fDataTestValid == true) {
    // Both of these resources get adopted by a TestData object.
    UResourceBundle *DataFillIn = ures_getByIndex(fTestData, index, nullptr, &status); 
    UResourceBundle *headers = ures_getByKey(fInfoRB, "Headers", nullptr, &intStatus);
  
    if(U_SUCCESS(status)) {
      result = new RBTestData(DataFillIn, headers, status);

      if(U_SUCCESS(status)) {
        return result;
      } else {
        delete result;
      }
    } else {
      ures_close(DataFillIn);
      ures_close(headers);
    }
  } else {
    status = U_MISSING_RESOURCE_ERROR;
  }
  return nullptr;
}

TestData* RBTestDataModule::createTestData(const char* name, UErrorCode &status) const
{
  TestData *result = nullptr;
  UErrorCode intStatus = U_ZERO_ERROR;

  if(fDataTestValid == true) {
    // Both of these resources get adopted by a TestData object.
    UResourceBundle *DataFillIn = ures_getByKey(fTestData, name, nullptr, &status); 
    UResourceBundle *headers = ures_getByKey(fInfoRB, "Headers", nullptr, &intStatus);
   
    if(U_SUCCESS(status)) {
      result = new RBTestData(DataFillIn, headers, status);
      if(U_SUCCESS(status)) {
        return result;
      } else {
        delete result;
      }
    } else {
      ures_close(DataFillIn);
      ures_close(headers);
    }
  } else {
    status = U_MISSING_RESOURCE_ERROR;
  }
  return nullptr;
}



//Get test data from ResourceBundles
UResourceBundle* 
RBTestDataModule::getTestBundle(const char* bundleName, UErrorCode &status) 
{
  if(U_SUCCESS(status)) {
    UResourceBundle *testBundle = nullptr;
    const char* icu_data = fLog.getTestDataPath(status);
    if (testBundle == nullptr) {
        testBundle = ures_openDirect(icu_data, bundleName, &status);
        if (status != U_ZERO_ERROR) {
            fLog.dataerrln(UNICODE_STRING_SIMPLE("Could not load test data from resourcebundle: ") + UnicodeString(bundleName, -1, US_INV));
            fDataTestValid = false;
        }
    }
    return testBundle;
  } else {
    return nullptr;
  }
}

