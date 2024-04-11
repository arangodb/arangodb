// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

// Fuzzer for ICU Unicode Property.

#include <cstring>

#include "fuzzer_utils.h"

#include "unicode/uchar.h"
#include "unicode/locid.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    UProperty prop;
    UChar32 c32;

    if (size < sizeof(prop) + sizeof(c32)) return 0;

    icu::StringPiece fuzzData(reinterpret_cast<const char *>(data), size);

    std::memcpy(&prop, fuzzData.data(), sizeof(prop));
    fuzzData.remove_prefix(sizeof(prop));

    std::memcpy(&c32, fuzzData.data(), sizeof(c32));
    fuzzData.remove_prefix(sizeof(c32));

    u_hasBinaryProperty(c32, prop);

    UErrorCode status = U_ZERO_ERROR;
    u_getBinaryPropertySet(prop, &status);

    u_getIntPropertyValue(c32, prop);
    u_getIntPropertyMinValue(prop);
    u_getIntPropertyMaxValue(prop);

    status = U_ZERO_ERROR;
    u_getIntPropertyMap(prop, &status);

    size_t unistr_size = fuzzData.length()/2;
    const UChar* p = (const UChar*)(fuzzData.data());
    u_stringHasBinaryProperty(p, unistr_size, prop);

    return 0;
}
