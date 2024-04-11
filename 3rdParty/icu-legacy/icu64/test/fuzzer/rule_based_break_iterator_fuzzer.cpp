// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <cstring>

#include "fuzzer_utils.h"
#include "unicode/localpointer.h"
#include "unicode/locid.h"
#include "unicode/rbbi.h"

IcuEnvironment* env = new IcuEnvironment();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  UErrorCode status = U_ZERO_ERROR;

  size_t unistr_size = size/2;
  std::unique_ptr<char16_t[]> fuzzbuff(new char16_t[unistr_size]);
  std::memcpy(fuzzbuff.get(), data, unistr_size * 2);
  icu::UnicodeString fuzzstr(false, fuzzbuff.get(), unistr_size);

  UParseError parse;
  icu::LocalPointer<icu::RuleBasedBreakIterator> brk(
      new icu::RuleBasedBreakIterator(fuzzstr, parse, status));

  return 0;
}
