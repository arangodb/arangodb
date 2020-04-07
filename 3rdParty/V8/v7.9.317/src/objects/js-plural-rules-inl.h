// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_PLURAL_RULES_INL_H_
#define V8_OBJECTS_JS_PLURAL_RULES_INL_H_

#include "src/api/api-inl.h"
#include "src/objects/js-plural-rules.h"
#include "src/objects/objects-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

OBJECT_CONSTRUCTORS_IMPL(JSPluralRules, JSObject)

ACCESSORS(JSPluralRules, locale, String, kLocaleOffset)
SMI_ACCESSORS(JSPluralRules, flags, kFlagsOffset)
ACCESSORS(JSPluralRules, icu_plural_rules, Managed<icu::PluralRules>,
          kIcuPluralRulesOffset)
ACCESSORS(JSPluralRules, icu_number_formatter,
          Managed<icu::number::LocalizedNumberFormatter>,
          kIcuNumberFormatterOffset)

inline void JSPluralRules::set_type(Type type) {
  DCHECK_LE(type, TypeBits::kMax);
  int hints = flags();
  hints = TypeBits::update(hints, type);
  set_flags(hints);
}

inline JSPluralRules::Type JSPluralRules::type() const {
  return TypeBits::decode(flags());
}

CAST_ACCESSOR(JSPluralRules)

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PLURAL_RULES_INL_H_
