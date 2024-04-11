// © 2019 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Fuzzer for ICU Locales.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "unicode/locid.h"
#include "unicode/localpointer.h"
#include "unicode/stringpiece.h"

#include "locale_util.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size < 1) return 0;
    icu::StringPiece fuzzData(reinterpret_cast<const char *>(data), size);
    uint8_t rnd = *fuzzData.data();
    fuzzData.remove_prefix(1);
    const std::string input = MakeZeroTerminatedInput(
        (const uint8_t*)(fuzzData.data()), fuzzData.length());

    icu::Locale locale(input.c_str());
    UErrorCode status = U_ZERO_ERROR;
    switch(rnd % 8) {
        case 0:
            locale.addLikelySubtags(status);
            break;
        case 1:
            locale.minimizeSubtags(status);
            break;
        case 2:
            locale.canonicalize(status);
            break;
        case 3:
            {
                icu::LocalPointer<icu::StringEnumeration> senum(
                    locale.createKeywords(status), status);
                while (U_SUCCESS(status) &&
                       (senum->next(nullptr, status)) != nullptr) {
                    // noop
                }
            }
            break;
        case 4:
            {
                icu::LocalPointer<icu::StringEnumeration> senum(
                    locale.createUnicodeKeywords(status), status);
                while (U_SUCCESS(status) &&
                       (senum->next(nullptr, status)) != nullptr) {
                    // noop
                }
            }
            break;
        case 5:
            {
                char buf[256];
                icu::CheckedArrayByteSink sink(buf, rnd);
                locale.toLanguageTag(sink, status);
            }
            break;
        case 6:
            {
                std::set<std::string> keys;
                locale.getKeywords<std::string>(
                    std::insert_iterator<decltype(keys)>(keys, keys.begin()),
                    status);
                if (U_SUCCESS(status)) {
                    char buf[256];
                    icu::CheckedArrayByteSink sink(buf, rnd);
                    for (std::set<std::string>::iterator it=keys.begin();
                         it!=keys.end();
                         ++it) {
                        locale.getKeywordValue(
                            icu::StringPiece(it->c_str(), it->length()), sink,
                            status);
                    }
                }
            }
            break;
        case 7:
            {
                std::set<std::string> keys;
                locale.getUnicodeKeywords<std::string>(
                    std::insert_iterator<decltype(keys)>(keys, keys.begin()),
                    status);
                if (U_SUCCESS(status)) {
                    char buf[256];
                    icu::CheckedArrayByteSink sink(buf, rnd);
                    for (std::set<std::string>::iterator it=keys.begin();
                         it!=keys.end();
                         ++it) {
                        locale.getUnicodeKeywordValue(
                            icu::StringPiece(it->c_str(), it->length()), sink,
                            status);
                    }
                }
            }
            break;
        default:
          break;
    }
    return EXIT_SUCCESS;
}
