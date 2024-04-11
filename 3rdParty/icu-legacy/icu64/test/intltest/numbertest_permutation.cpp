// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <fstream>
#include <iostream>
#include <vector>

#include "numbertest.h"
#include "ucbuf.h"
#include "unicode/numberformatter.h"

void NumberPermutationTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite NumberPermutationTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testPermutations);
    TESTCASE_AUTO_END;
}

static const char16_t* kSkeletonParts[] = {
    // Notation
    u"compact-short",
    u"scientific/+ee/sign-always",
    nullptr,
    // Unit
    u"percent",
    u"currency/EUR",
    u"measure-unit/length-furlong",
    nullptr,
    // Unit Width
    u"unit-width-narrow",
    u"unit-width-full-name",
    nullptr,
    // Precision
    u"precision-integer",
    u".000",
    u".##/@@@+",
    u"@@",
    nullptr,
    // Rounding Mode
    u"rounding-mode-floor",
    nullptr,
    // Integer Width
    u"integer-width/##00",
    nullptr,
    // Scale
    u"scale/0.5",
    nullptr,
    // Grouping
    u"group-on-aligned",
    nullptr,
    // Symbols
    u"latin",
    nullptr,
    // Sign Display
    u"sign-accounting-except-zero",
    nullptr,
    // Decimal Separator Display
    u"decimal-always",
    nullptr,
};

static const double kNumbersToTest[]{0, 91827.3645, -0.22222};

/**
 * Test permutations of 3 orthogonal skeleton parts from the list above.
 * Compare the results against the golden data file:
 *     numberpermutationtest.txt
 * To regenerate that file, run intltest with the -e and -G options.
 * On Linux, from icu4c/source:
 *     make -j -l2.5 tests && (cd test/intltest && LD_LIBRARY_PATH=../../lib:../../tools/ctestfw ./intltest -e -G format/NumberTest/NumberPermutationTest)
 * After re-generating the file, copy it into icu4j:
 *     cp test/testdata/numberpermutationtest.txt ../../icu4j/main/core/src/test/resources/com/ibm/icu/dev/data/numberpermutationtest.txt
 */
void NumberPermutationTest::testPermutations() {
    IcuTestErrorCode status(*this, "testPermutations");

    const struct LocaleData {
        Locale locale;
        const char16_t* ustring;
    } localesToTest[] = {
        {"es-MX", u"es-MX"},
        {"zh-TW", u"zh-TW"},
        {"bn-BD", u"bn-BD"},
    };

    // Convert kSkeletonParts to a more convenient data structure
    auto skeletonParts = std::vector<std::vector<const char16_t*>>();
    auto currentSection = std::vector<const char16_t*>();
    for (int32_t i = 0; i < UPRV_LENGTHOF(kSkeletonParts); i++) {
        const char16_t* skeletonPart = kSkeletonParts[i];
        if (skeletonPart == nullptr) {
            skeletonParts.push_back(currentSection);
            currentSection.clear();
        } else {
            currentSection.push_back(skeletonPart);
        }
    }

    // Build up the golden data string as we evaluate all permutations
    std::vector<UnicodeString> resultLines;
    resultLines.emplace_back(u"# © 2019 and later: Unicode, Inc. and others.");
    resultLines.emplace_back(u"# License & terms of use: http://www.unicode.org/copyright.html");
    resultLines.emplace_back();

    // Take combinations of 3 orthogonal options
    for (size_t i = 0; i < skeletonParts.size() - 2; i++) {
        const auto& skeletons1 = skeletonParts[i];
        for (size_t j = i + 1; j < skeletonParts.size() - 1; j++) {
            const auto& skeletons2 = skeletonParts[j];
            for (size_t k = j + 1; k < skeletonParts.size(); k++) {
                const auto& skeletons3 = skeletonParts[k];

                // Evaluate all combinations of skeletons for these options
                for (const auto& skel1 : skeletons1) {
                    for (const auto& skel2 : skeletons2) {
                        for (const auto& skel3 : skeletons3) {
                            // Compute the skeleton
                            UnicodeString skeleton;
                            skeleton
                                .append(skel1)  //
                                .append(u' ')   //
                                .append(skel2)  //
                                .append(u' ')   //
                                .append(skel3);
                            resultLines.push_back(skeleton);

                            // Check several locales and several numbers in each locale
                            for (const auto& locData : localesToTest) {
                                auto lnf = NumberFormatter::forSkeleton(skeleton, status)
                                               .locale(locData.locale);
                                resultLines.push_back(UnicodeString(u"  ").append(locData.ustring));
                                for (const auto& input : kNumbersToTest) {
                                    resultLines.push_back(UnicodeString(u"    ").append(
                                        lnf.formatDouble(input, status).toTempString(status)));
                                }
                            }

                            resultLines.emplace_back();
                        }
                    }
                }
            }

            // Quick mode: test all fields at least once but stop early.
            if (quick) {
                infoln(u"Quick mode: stopped after " + Int64ToUnicodeString(resultLines.size()) +
                       u" lines");
                goto outerEnd;
            }
        }
    }
outerEnd:
    void();

    CharString goldenFilePath(getSourceTestData(status), status);
    goldenFilePath.appendPathPart("numberpermutationtest.txt", status);

    // Compare it to the golden file
    const char* codePage = "UTF-8";
    LocalUCHARBUFPointer f(ucbuf_open(goldenFilePath.data(), &codePage, true, false, status));
    if (!assertSuccess("Can't open data file", status)) {
        return;
    }

    int32_t lineNumber = 1;
    int32_t lineLength;
    for (const auto& actualLine : resultLines) {
        const char16_t* lineBuf = ucbuf_readline(f.getAlias(), &lineLength, status);
        if (lineBuf == nullptr) {
            errln("More lines generated than are in the data file!");
            break;
        }
        UnicodeString expectedLine(lineBuf, lineLength - 1);
        assertEquals(u"Line #" + Int64ToUnicodeString(lineNumber) + u" differs",  //
            expectedLine, actualLine);
        lineNumber++;
    }
    // Quick mode: test all fields at least once but stop early.
    if (!quick && ucbuf_readline(f.getAlias(), &lineLength, status) != nullptr) {
        errln("Fewer lines generated than are in the data file!");
    }

    // Overwrite the golden data if requested
    if (write_golden_data) {
        std::ofstream outFile;
        outFile.open(goldenFilePath.data());
        for (const auto& uniLine : resultLines) {
            std::string byteLine;
            uniLine.toUTF8String(byteLine);
            outFile << byteLine << std::endl;
        }
        outFile.close();
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
