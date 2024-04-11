// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "datadrivennumberformattestsuite.h"

#if !UCONFIG_NO_FORMATTING

#include "charstr.h"
#include "ucbuf.h"
#include "unicode/localpointer.h"
#include "ustrfmt.h"

static UBool isCROrLF(char16_t c) { return c == 0xa || c == 0xd; }
static UBool isSpace(char16_t c) { return c == 9 || c == 0x20 || c == 0x3000; }

void DataDrivenNumberFormatTestSuite::run(const char *fileName, UBool runAllTests) {
    fFileLineNumber = 0;
    fFormatTestNumber = 0;
    UErrorCode status = U_ZERO_ERROR;
    for (int32_t i = 0; i < UPRV_LENGTHOF(fPreviousFormatters); ++i) {
        delete fPreviousFormatters[i];
        fPreviousFormatters[i] = newFormatter(status);
    }
    if (!assertSuccess("Can't create previous formatters", status)) {
        return;
    }
    CharString path(getSourceTestData(status), status);
    path.appendPathPart(fileName, status);
    const char *codePage = "UTF-8";
    LocalUCHARBUFPointer f(ucbuf_open(path.data(), &codePage, true, false, &status));
    if (!assertSuccess("Can't open data file", status)) {
        return;
    }
    UnicodeString columnValues[kNumberFormatTestTupleFieldCount];
    ENumberFormatTestTupleField columnTypes[kNumberFormatTestTupleFieldCount];
    int32_t columnCount = 0;
    int32_t state = 0;
    while(U_SUCCESS(status)) {
        // Read a new line if necessary.
        if(fFileLine.isEmpty()) {
            if(!readLine(f.getAlias(), status)) { break; }
            if (fFileLine.isEmpty() && state == 2) {
                state = 0;
            }
            continue;
        }
        if (fFileLine.startsWith("//")) {
            fFileLine.remove();
            continue;
        }
        // Initial setup of test.
        if (state == 0) {
            if (fFileLine.startsWith(UNICODE_STRING("test ", 5))) {
                fFileTestName = fFileLine;
                fTuple.clear();
            } else if(fFileLine.startsWith(UNICODE_STRING("set ", 4))) {
                setTupleField(status);
            } else if(fFileLine.startsWith(UNICODE_STRING("begin", 5))) {
                state = 1;
            } else {
                showError("Unrecognized verb.");
                return;
            }
        // column specification
        } else if (state == 1) {
            columnCount = splitBy(columnValues, UPRV_LENGTHOF(columnValues), 0x9);
            for (int32_t i = 0; i < columnCount; ++i) {
                columnTypes[i] = NumberFormatTestTuple::getFieldByName(
                    columnValues[i]);
                if (columnTypes[i] == kNumberFormatTestTupleFieldCount) {
                    showError("Unrecognized field name.");
                    return;
                }
            }
            state = 2;
        // run the tests
        } else {
            int32_t columnsInThisRow = splitBy(columnValues, columnCount, 0x9);
            for (int32_t i = 0; i < columnsInThisRow; ++i) {
                fTuple.setField(
                        columnTypes[i], columnValues[i].unescape(), status);
            }
            for (int32_t i = columnsInThisRow; i < columnCount; ++i) {
                fTuple.clearField(columnTypes[i], status);
            }
            if (U_FAILURE(status)) {
                showError("Invalid column values");
                return;
            }
            if (runAllTests || !breaksC()) {
                UnicodeString errorMessage;
                UBool shouldFail = (NFTT_GET_FIELD(fTuple, output, "") == "fail")
                        ? !breaksC()
                        : breaksC();
                UBool actualSuccess = isPass(fTuple, errorMessage, status);
                if (shouldFail && actualSuccess) {
                    showFailure("Expected failure, but passed: " + errorMessage);
                    break;
                } else if (!shouldFail && !actualSuccess) {
                    showFailure(errorMessage);
                    break;
                }
                status = U_ZERO_ERROR;
            }
        }
        fFileLine.remove();
    }
}

DataDrivenNumberFormatTestSuite::~DataDrivenNumberFormatTestSuite() {
    for (int32_t i = 0; i < UPRV_LENGTHOF(fPreviousFormatters); ++i) {
        delete fPreviousFormatters[i];
    }
}

UBool DataDrivenNumberFormatTestSuite::breaksC() {
    return (NFTT_GET_FIELD(fTuple, breaks, "").toUpper().indexOf((char16_t)0x43) != -1);
}

void DataDrivenNumberFormatTestSuite::setTupleField(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    UnicodeString parts[3];
    int32_t partCount = splitBy(parts, UPRV_LENGTHOF(parts), 0x20);
    if (partCount < 3) {
        showError("Set expects 2 parameters");
        status = U_PARSE_ERROR;
        return;
    }
    if (!fTuple.setField(
            NumberFormatTestTuple::getFieldByName(parts[1]),
            parts[2].unescape(),
            status)) {
        showError("Invalid field value");
    }
}


int32_t
DataDrivenNumberFormatTestSuite::splitBy(
        UnicodeString *columnValues,
        int32_t columnValuesCount,
        char16_t delimiter) {
    int32_t colIdx = 0;
    int32_t colStart = 0;
    int32_t len = fFileLine.length();
    for (int32_t idx = 0; colIdx < columnValuesCount - 1 && idx < len; ++idx) {
        char16_t ch = fFileLine.charAt(idx);
        if (ch == delimiter) {
            columnValues[colIdx++] = 
                    fFileLine.tempSubString(colStart, idx - colStart);
            colStart = idx + 1;
        }
    }
    columnValues[colIdx++] = 
            fFileLine.tempSubString(colStart, len - colStart);
    return colIdx;
}

void DataDrivenNumberFormatTestSuite::showLineInfo() {
    UnicodeString indent("    ");
    infoln(indent + fFileTestName);
    infoln(indent + fFileLine);
}

void DataDrivenNumberFormatTestSuite::showError(const char *message) {
    errln("line %d: %s", (int) fFileLineNumber, message);
    showLineInfo();
}

void DataDrivenNumberFormatTestSuite::showFailure(const UnicodeString &message) {
    char16_t lineStr[20];
    uprv_itou(
            lineStr, UPRV_LENGTHOF(lineStr), (uint32_t) fFileLineNumber, 10, 1);
    UnicodeString fullMessage("line ");
    dataerrln(fullMessage.append(lineStr).append(": ")
            .append(prettify(message)));
    showLineInfo();
}

UBool DataDrivenNumberFormatTestSuite::readLine(
        UCHARBUF *f, UErrorCode &status) {
    int32_t lineLength;
    const char16_t *line = ucbuf_readline(f, &lineLength, &status);
    if(line == nullptr || U_FAILURE(status)) {
        if (U_FAILURE(status)) {
            errln("Error reading line from file.");
        }
        fFileLine.remove();
        return false;
    }
    ++fFileLineNumber;
    // Strip trailing CR/LF, comments, and spaces.
    while(lineLength > 0 && isCROrLF(line[lineLength - 1])) { --lineLength; }
    fFileLine.setTo(false, line, lineLength);
    while(lineLength > 0 && isSpace(line[lineLength - 1])) { --lineLength; }
    if (lineLength == 0) {
        fFileLine.remove();
    }
    return true;
}

UBool DataDrivenNumberFormatTestSuite::isPass(
        const NumberFormatTestTuple &tuple,
        UnicodeString &appendErrorMessage,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    UBool result = false;
    if (tuple.formatFlag && tuple.outputFlag) {
        ++fFormatTestNumber;
        result = isFormatPass(
                tuple,
                fPreviousFormatters[
                        fFormatTestNumber % UPRV_LENGTHOF(fPreviousFormatters)],
                appendErrorMessage,
                status);
    }
    else if (tuple.toPatternFlag || tuple.toLocalizedPatternFlag) {
        result = isToPatternPass(tuple, appendErrorMessage, status);
    } else if (tuple.parseFlag && tuple.outputFlag && tuple.outputCurrencyFlag) {
        result = isParseCurrencyPass(tuple, appendErrorMessage, status);

    } else if (tuple.parseFlag && tuple.outputFlag) {
        result = isParsePass(tuple, appendErrorMessage, status);
    } else if (tuple.pluralFlag) {
        result = isSelectPass(tuple, appendErrorMessage, status);
    } else {
        appendErrorMessage.append("Unrecognized test type.");
        status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    if (!result) {
        if (appendErrorMessage.length() > 0) {
            appendErrorMessage.append(": ");
        }
        if (U_FAILURE(status)) {
            appendErrorMessage.append(u_errorName(status));
            appendErrorMessage.append(": ");
        }
        tuple.toString(appendErrorMessage);
    }
    return result;
}

UBool DataDrivenNumberFormatTestSuite::isFormatPass(
        const NumberFormatTestTuple & /* tuple */,
        UnicodeString & /*appendErrorMessage*/,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    return true;
}
    
UBool DataDrivenNumberFormatTestSuite::isFormatPass(
        const NumberFormatTestTuple &tuple,
        UObject * /* somePreviousFormatter */,
        UnicodeString &appendErrorMessage,
        UErrorCode &status) {
    return isFormatPass(tuple, appendErrorMessage, status);
}

UObject *DataDrivenNumberFormatTestSuite::newFormatter(
        UErrorCode & /*status*/) {
    return nullptr;
}

UBool DataDrivenNumberFormatTestSuite::isToPatternPass(
        const NumberFormatTestTuple & /* tuple */,
        UnicodeString & /*appendErrorMessage*/,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    return true;
}

UBool DataDrivenNumberFormatTestSuite::isParsePass(
        const NumberFormatTestTuple & /* tuple */,
        UnicodeString & /*appendErrorMessage*/,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    return true;
}

UBool DataDrivenNumberFormatTestSuite::isParseCurrencyPass(
        const NumberFormatTestTuple & /* tuple */,
        UnicodeString & /*appendErrorMessage*/,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    return true;
}

UBool DataDrivenNumberFormatTestSuite::isSelectPass(
        const NumberFormatTestTuple & /* tuple */,
        UnicodeString & /*appendErrorMessage*/,
        UErrorCode &status) {
    if (U_FAILURE(status)) {
        return false;
    }
    return true;
}
#endif /* !UCONFIG_NO_FORMATTING */
